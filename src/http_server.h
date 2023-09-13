#pragma once
#include "sdk.h"

#include <iostream>
#include <atomic>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "logger.h"

namespace http_server {
    namespace{
    }
    namespace net = boost::asio;
    namespace sys = boost::system;
    using tcp = net::ip::tcp;
    namespace beast = boost::beast;
    namespace http = beast::http;
    using namespace std::literals;
    using namespace logger;
    namespace {
        void ReportError(beast::error_code ec, std::string_view what) {
            LOG(LOG::MESSAGE_DATA)
                << "error"sv
                << LOG::ToJson("code"sv, std::to_string(ec.value()),
                               "text"sv, ec.message(),
                               "where "sv, what)
                << LOG::Flush;
        }
    }
    class SessionBase {
    public:
        // Запрещаем копирование и присваивание объектов SessionBase и его наследников
        SessionBase(const SessionBase&) = delete;
        SessionBase& operator=(const SessionBase&) = delete;
        void Run();

    protected:
        using HttpRequest = http::request<http::string_body>;
        explicit SessionBase(tcp::socket&& socket);

        template <typename Body, typename Fields>
        void Write(http::response<Body, Fields>&& response) {
            // Запись выполняется асинхронно, поэтому response перемещаем в область кучи
            auto safe_response = std::make_shared<http::response<Body, Fields>>(std::move(response));
            auto self = GetSharedThis();
            http::async_write(stream_, *safe_response,
                [safe_response, self,this](beast::error_code ec, std::size_t bytes_written) {
                    self->OnWrite(safe_response->need_eof(), ec, bytes_written);
                });
        }


    private:
        void Read();
        void OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read);
        void OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written);
        void Close();

        virtual std::shared_ptr<SessionBase> GetSharedThis() = 0;
        // Обработку запроса делегируем подклассу
        virtual void HandleRequest(HttpRequest&& request, const tcp::endpoint& remote_endpoint) = 0;

    private:
        // tcp_stream содержит внутри себя сокет и добавляет поддержку таймаутов
        beast::tcp_stream stream_;
        beast::flat_buffer buffer_;
        HttpRequest request_;
    };

    template <typename RequestHandler>
    class Session : public SessionBase, public std::enable_shared_from_this<Session<RequestHandler>> {
    public:
        template <typename Handler>
        Session(tcp::socket&& socket, Handler&& request_handler)
            : SessionBase(std::move(socket))
            , request_handler_(std::forward<Handler>(request_handler)) {
        }
    private:
        std::shared_ptr<SessionBase> GetSharedThis() override {
            return this->shared_from_this();
        }
        void HandleRequest(HttpRequest&& request, const tcp::endpoint& remote_endpoint) override {
            {
                // Захватываем умный указатель на текущий объект Session в лямбде,
                // чтобы продлить время жизни сессии до вызова лямбды.
                // Используется generic-лямбда функция, способная принять response произвольного типа
                request_handler_(std::move(request), remote_endpoint, [self = this->shared_from_this()](auto&& response) {
                    self->Write(std::move(response));
                    });
            }
        };

    private:
        RequestHandler request_handler_;
    };

    template <typename RequestHandler>
    class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
    public:
        template <typename Handler>
        Listener(net::io_context& ioc, const tcp::endpoint& endpoint, Handler&& request_handler) : ioc_(ioc)
            // Обработчики асинхронных операций acceptor_ будут вызываться в своём strand
            , acceptor_(net::make_strand(ioc))
            , request_handler_(std::forward<Handler>(request_handler)) {
            // Открываем acceptor, используя протокол (IPv4 или IPv6), указанный в endpoint
            acceptor_.open(endpoint.protocol());

            // После закрытия TCP-соединения сокет некоторое время может считаться занятым,
            // чтобы компьютеры могли обменяться завершающими пакетами данных.
            // Однако это может помешать повторно открыть сокет в полузакрытом состоянии.
            // Флаг reuse_address разрешает открыть сокет, когда он "наполовину закрыт"
            acceptor_.set_option(net::socket_base::reuse_address(true));
            // Привязываем acceptor к адресу и порту endpoint
            acceptor_.bind(endpoint);
            // Переводим acceptor в состояние, в котором он способен принимать новые соединения
            // Благодаря этому новые подключения будут помещаться в очередь ожидающих соединений
            acceptor_.listen(net::socket_base::max_listen_connections);
        };
        void Run() {
            DoAccept();
        }

    private:
        int id;
        void DoAccept() {
            acceptor_.async_accept(
                // Передаём последовательный исполнитель, в котором будут вызываться обработчики
                // асинхронных операций сокета
                net::make_strand(ioc_),
                // С помощью bind_front_handler создаём обработчик, привязанный к методу OnAccept
                // текущего объекта.
                // Так как Listener — шаблонный класс, нужно подсказать компилятору, что
                // shared_from_this — метод класса, а не свободная функция.
                // Для этого вызываем его, используя this
                // Этот вызов bind_front_handler аналогичен
                // namespace ph = std::placeholders;
                // std::bind(&Listener::OnAccept, this->shared_from_this(), ph::_1, ph::_2)
                beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
        };
        // Метод socket::async_accept создаст сокет и передаст его в OnAccept
        void OnAccept(sys::error_code ec, tcp::socket socket) {
            using namespace std::literals;
            if (ec) {
                return ReportError(ec, "accept"sv);
            }
            // Асинхронно обрабатываем сессию
            AsyncRunSession(std::move(socket));
            // Принимаем новое соединение
            DoAccept();
        };
        void AsyncRunSession(tcp::socket&& socket) {
            auto session = std::make_shared<Session<RequestHandler>>(std::move(socket), request_handler_);
            session->Run();
        };

        net::io_context& ioc_;
        tcp::acceptor acceptor_;
        RequestHandler request_handler_;
    };

    template <typename RequestHandler>
    void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
        // При помощи decay_t исключим ссылки из типа RequestHandler,
        // чтобы Listener хранил RequestHandler по значению
        using MyListener = Listener<std::decay_t<RequestHandler>>;
        std::make_shared<MyListener>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
    }

}  // namespace http_server
