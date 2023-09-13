#pragma once
#include <iostream>
#include <vector>
#include <optional>
#include <sstream>
#include <array>
#include <variant>
#include <concepts>
#include <filesystem>

#include <boost/log/trivial.hpp>     // для BOOST_LOG_TRIVIAL
#include <boost/log/core.hpp>        // для logging::core
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/date_time.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/utility/setup/file.hpp>

#include <boost/json.hpp>

namespace json = boost::json;
namespace sys = boost::system;
namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;

template<typename T>
concept StringConvertible = std::constructible_from<std::string, T>;

template<typename T>
concept IntConvertible = requires(T x)
{
    { uint64_t(x) };
};

template<typename T>
concept GoodJSONType = StringConvertible<T> || IntConvertible<T>;

BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(my_logger, logging::sources::logger_mt);
namespace logger{
    class LOG {
    public:
        enum DATA_TYPE {
            JSON
        };

        enum MESSAGE_SEVERITY {
            TRACE = logging::trivial::severity_level::trace, //=0
            DEBUG = logging::trivial::severity_level::debug,
            INFO = logging::trivial::severity_level::info,
            WARNING = logging::trivial::severity_level::warning,
            ERROR_ = logging::trivial::severity_level::error,
            FATAL = logging::trivial::severity_level::fatal,
            NONE,
            MESSAGE_DATA
        };

        inline static const std::array<std::string, 8> severity_strings
        {
                std::string("trace"),
                std::string("debug"),
                std::string("info"),
                std::string("warning"),
                std::string("error"),
                std::string("fatal"),
                std::string("NONE"),
                std::string("MESSAGE_DATA")
        };

        enum SERVICE {
            Flush
        };

        LOG();
        LOG(DATA_TYPE type);
        LOG(MESSAGE_SEVERITY sev);
        LOG(MESSAGE_SEVERITY sev, DATA_TYPE type);
        static void Init();

        template<GoodJSONType... ARGS>
        static json::value ToJson(ARGS&&... args) {
            static_assert(sizeof...(ARGS) % 2 == 0, "ERROR:the number of arguments must be divided by two ");
            json::object arr;
            using VariantLog = std::variant<std::string, uint64_t>;
            std::vector<VariantLog> vec;
            ([&vec, &args] {
                if constexpr (StringConvertible<decltype(args)>) {
                    vec.push_back(std::string(args));
                }
                else if constexpr (IntConvertible<decltype(args)>) {
                    vec.push_back(static_cast<uint64_t>(args));
                }
                }(), ...);
            for (int i = 0; i < vec.size(); i += 2) {
                std::string key = std::get<std::string>(vec[i]);
                if (std::holds_alternative< std::string >(vec[i + 1])) {
                    arr.emplace(key, std::get<std::string>(vec[i + 1]));
                }
                else if (std::holds_alternative < uint64_t >(vec[i + 1])) {
                    arr.emplace(key, std::get<uint64_t>(vec[i + 1]));
                }
            }
            return json::value(arr);
        };

        LOG& operator<<(std::string msg) ;
        LOG& operator<<(std::string_view msg) ;
        LOG& operator<<(const char* msg) ;
        LOG& operator<<(const int& msg);
        LOG& operator<<(json::value&& msg);
        LOG& operator<<(SERVICE msg);

    private:
        std::map<std::string, json::value> map_to_send;
        std::string str_to_send;
        static bool is_init;
        MESSAGE_SEVERITY severity;
        DATA_TYPE data_type;
    };  // class LOG

}

