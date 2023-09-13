#include "sdk.h"
//
#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>

#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

#include "json_loader.h"
#include "request_handler.h"
#include "response_utils.h"
#include "logger.h"
#include "ticker.h"
#include "postgres.h"

using namespace std::literals;
using namespace logger;
namespace fs = std::filesystem;
namespace net = boost::asio;
namespace sys = boost::system;
namespace po = boost::program_options;
namespace krn = std::chrono;

#ifdef __cpp_lib_jthread
using thread_type = std::jthread;
#else
using thread_type = std::thread;
#endif

namespace {

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void ExecuteThreaded(unsigned num_threads, const Fn& fn) {
    num_threads = std::max(1u, num_threads);
    std::vector<thread_type> workers;
    workers.reserve(num_threads - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--num_threads) {
        workers.emplace_back(fn);
    }
    fn();

#ifndef __cpp_lib_jthread
    for (std::thread & t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }
#endif
}

}  // namespace

struct Args {
    int tick_period;
    std::string config_file_path;
    std::string www_root_path;
    std::string state_file_path;
    int save_state_period;
    bool randomize_spawn_points;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    po::options_description desc{"All options"s};
    Args args;
    desc.add_options()
        ("help,h", "Show help")
        ("tick-period,t", po::value<int>(&args.tick_period), "Tick period of server update")
        ("config-file,c", po::value(&args.config_file_path), "Config file path")
        ("www-root,w", po::value(&args.www_root_path), "Static www files path")
        ("randomize-spawn-points", "Spawn dogs in random map point")
        ("state-file,s", po::value(&args.state_file_path), "State file path")
        ("save-state-period,p", po::value<int>(&args.save_state_period), "Auto save period in ms");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        BOOST_LOG(my_logger::get()) << desc<<std::endl;
        return std::nullopt;
    }

    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("ERROR:Config file must be specified."s);
    }

    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("ERROR:Source root must be specified."s);
    }

    if (!vm.contains("state-file"s)) {
        BOOST_LOG(my_logger::get()) << "--state-file was not set. Server will launch in default values." << std::endl;
        args.save_state_period = Game::STATE_FILE_WAS_NOT_SET;
    }
    else {
        if (!vm.contains("save-state-period"s)) {
            BOOST_LOG(my_logger::get()) << "--save-state-period was not set. Server state will be saved at SIGINT and SIGTERM" << std::endl;
            args.save_state_period = Game::STATE_PERIOD_WAS_NOT_SET;
        }
    } 

    if (vm.count("tick-period"s) == false) {
        BOOST_LOG(my_logger::get()) << "--tick-period was not set. Server will launch in test mode." << std::endl;
        args.tick_period = Game::TICK_TESTING_MODE;
    }

    if (vm.count("randomize-spawn-points"s) == false) {
        args.randomize_spawn_points = false;
        BOOST_LOG(my_logger::get()) << "--randomize-spawn-points was not set. Dog will be spawn in Start position at first road." << std::endl;
    }
    else {
        args.randomize_spawn_points = true;
    }
    return args;
};

int main(int argc, const char* argv[]) {
    Args args;
    try {
        auto args_opt = ParseCommandLine(argc, argv);
        if (args_opt.has_value()) {
            args = args_opt.value();
        }else  throw std::runtime_error("ERROR:Not enough options to start server."s);
    }
    catch (const std::exception& e) {
        BOOST_LOG(my_logger::get()) << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    LOG::Init();
    std::vector<std::string> argss(argv + 1, argv + argc);
    std::string args_str;
    for (auto& a : argss) {
        args_str += a;
        args_str += " ";
    }
    try {
        const unsigned num_threads = std::thread::hardware_concurrency();
        // 1. Загружаем карту из файла чтобы построить модель игры. Устанавливаем корневую дирректорию статических файлов.
        model::Game game = json_loader::LoadGame(args.config_file_path);
        game.SetSaveFilePath(args.state_file_path);
        if ((args.state_file_path.size() != 0) && (fs::exists(fs::path(args.state_file_path)) == true)) {
            try {
                game.LoadState();
                LOG(LOG::INFO) << "Game state was loaded from file: "sv<< args.state_file_path << LOG::Flush;
            }
            catch (std::exception& e) {
                LOG(LOG::ERROR_) << "Game::LoadState exception: "sv << e.what() << LOG::Flush;
                return EXIT_FAILURE;
            }
        }
        game.SetTickPeriod(args.tick_period);
        game.SetRandomizeSpawnPoint(args.randomize_spawn_points);
        http_handler::FileManager::SetRootPath(args.www_root_path);
        // 1.5 Подключение к БД.
        const char* db_url = std::getenv("GAME_DB_URL");
        if (!db_url) {
            throw std::runtime_error("GAME_DB_URL is not specified");
        }
        auto conn_pool = std::make_shared< postgres::ConnectionPool >( std::max(1u, num_threads), [db_url] {
            return std::make_shared<pqxx::connection>(db_url);
        });
        try{
            postgres::Database::Init(conn_pool);
            game.SetDBConnectionPool(conn_pool);
        }
        catch (std::exception& e) {
            LOG(LOG::ERROR_) << "postgres::Database::Init exception: "sv << e.what() << LOG::Flush;
            return EXIT_FAILURE;
        }
        // 2. Инициализируем io_context
        net::io_context ioc(num_threads);

        // 2.5 Инциализиурем Ticker
        auto ticker_strand = net::make_strand(ioc);
        using TickerHandler = std::function<void(std::chrono::milliseconds delta)>;
        std::shared_ptr<Ticker<TickerHandler>> ticker_game_update;
        if (auto update_period = game.GetTickPeriod(); update_period != Game::TICK_TESTING_MODE) {
            ticker_game_update = std::make_shared<Ticker<TickerHandler>>(ticker_strand, krn::milliseconds(update_period), [&game](krn::milliseconds ms) {
                game.Update(ms);
                });
            ticker_game_update->Start();
        }
        // 2.75 Инциализиурем сохранение состояния
        game.SetSaveStatePeriod(args.save_state_period);
        game.PrepareSaveStateCycle(ticker_strand);
                
        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc, args, &game](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                if (args.save_state_period != Game::STATE_FILE_WAS_NOT_SET) {
                    game.SaveState();
                }
               LOG(LOG::ERROR_) << "Signal "sv << signal_number << " received"sv << LOG::Flush;
               ioc.stop();
            }
            });

        // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
        //http_handler::RequestHandler handler{game, ticker_strand};
        auto api_strand = net::make_strand(ioc);
        auto handler = std::make_shared<http_handler::RequestHandler>(game, api_strand);

        // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, const auto& remote_endpoint, auto&& send) {
            (*handler)(std::forward<decltype(req)>(req), remote_endpoint, std::forward<decltype(send)>(send));
        });
        
        // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
        LOG(LOG::MESSAGE_DATA) << "server started"sv << LOG::ToJson("port"sv, port, "address"sv, address.to_string()) << LOG::Flush;

        // 6. Запускаем обработку асинхронных операций
        ExecuteThreaded(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    } catch (const std::exception& ex) {
        LOG(LOG::ERROR_) << "main.cpp exception: "sv << ex.what() << LOG::Flush;
        LOG(LOG::MESSAGE_DATA) << "server exited"sv << LOG::ToJson("code"sv, std::to_string(EXIT_FAILURE)) << LOG::Flush;
        return EXIT_FAILURE;
    }
    LOG(LOG::MESSAGE_DATA) << "server exited"sv << LOG::ToJson("code"sv, std::to_string(EXIT_SUCCESS)) << LOG::Flush;
    return EXIT_SUCCESS;
}
