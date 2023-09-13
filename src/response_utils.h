#pragma once

#include "http_server.h"
#include "model.h"
#include "json_loader.h"
#include "logger.h"

#include <optional>
#include <vector>
#include <tuple>
#include <string_view>
#include <algorithm>
#include <memory>
#include <filesystem>
#include <variant>
#include <chrono>
#include <ratio>
#include <iostream>
#include <memory>

#include <stdlib.h>

namespace http_handler {
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace sys = boost::system;
    namespace net = boost::asio;
    using tcp = net::ip::tcp;
    using namespace std::literals;
    using namespace logger;
    namespace fs = std::filesystem;
    // Запрос, тело которого представлено в виде строки
    using StringRequest = http::request<http::string_body>;
    // Ответ, тело которого представлено в виде строки
    using StringResponse = http::response<http::string_body>;
    using FileResponse = http::response<http::file_body>;
    using ErrorResponse = sys::error_code;
    using VariantResponse = std::variant<StringResponse, FileResponse, ErrorResponse>;

    void DumpRequest(const StringRequest& req);

    template<typename ResponseType>
    ResponseType& operator<< (ResponseType& RT, const std::pair<http::field, std::string_view>& field) {
        RT.set(field.first, field.second);
        return RT;
    };

    class ResponseUtils {
        ResponseUtils() = delete;

    public:
        template<typename ResponseType,
            typename BodyType,
            typename ... Fields>
        static ResponseType MakeResponse(http::status status,
            BodyType body,
            std::size_t content_lenght,
            unsigned http_version,
            bool keep_alive,
            Fields&&... fields) {
            ResponseType response(status, http_version);
            response.body() = std::move(body);
            response.content_length(content_lenght);
            response.keep_alive(keep_alive);
            (response << ... << fields);
            return response;
        }
    };///class ResponseUtils

    struct ContentType {
        ContentType() = delete;

        constexpr static std::string_view TEXT_HTML = "text/html"sv;
        constexpr static std::string_view TEXT_CSS = "text/css"sv;
        constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;
        constexpr static std::string_view TEXT_JAVASCRIPT = "text/javascript"sv;

        constexpr static std::string_view APPLICATION_JSON = "application/json"sv;
        constexpr static std::string_view APPLICATION_XML = "application/xml"sv;
        constexpr static std::string_view BINARY_DATA = "application/octet-stream"sv;

        constexpr static std::string_view IMAGE_PNG = "image/png"sv;
        constexpr static std::string_view IMAGE_JPEG = "image/jpeg"sv;
        constexpr static std::string_view IMAGE_GIF = "image/gif"sv;
        constexpr static std::string_view IMAGE_BMP = "image/bmp"sv;
        constexpr static std::string_view IMAGE_ICO = "image/vnd.microsoft.icon"sv;
        constexpr static std::string_view IMAGE_TIFF = "image/tiff"sv;
        constexpr static std::string_view IMAGE_SVG = "image/svg+xml"sv;

        constexpr static std::string_view AUDIO_MP3 = "audio/mpeg"sv;
    };

    struct ResponseAllowedMethods {
        ResponseAllowedMethods() = delete;
        constexpr static std::string_view GET_HEAD = "GET,HEAD"sv;
        constexpr static std::string_view POST = "POST"sv;
        constexpr static std::string_view GET_HEAD_POST = "GET,HEAD,POST"sv;
    };

    /// frequently repeating strings
    struct FreqStr {
        FreqStr() = delete;
        constexpr static std::string_view code = "code"sv;
        constexpr static std::string_view message = "message"sv;
        constexpr static std::string_view invalidArgument = "invalidArgument"sv;
        constexpr static std::string_view unknownToken = "unknownToken"sv;
        constexpr static std::string_view invalidToken = "invalidToken"sv;
        constexpr static std::string_view no_cache = "no-cache"sv;
    };

    struct TargetAPI {
        TargetAPI() = delete;

        constexpr static std::string_view TARGET_MAPS = "/api/v1/maps"sv;
        constexpr static std::string_view TARGET_JOIN = "/api/v1/game/join"sv;
        constexpr static std::string_view TARGET_PLAYERS = "/api/v1/game/players"sv;
        constexpr static std::string_view TARGET_STATE = "/api/v1/game/state"sv;
        constexpr static std::string_view TARGET_ACTION = "/api/v1/game/player/action"sv;
        constexpr static std::string_view TARGET_TICK = "/api/v1/game/tick"sv;
        constexpr static std::string_view TARGET_RECORDS = "/api/v1/game/records"sv;
        constexpr static std::string_view TARGET_BAD = "/api/"sv;
    };
    std::string FromUrlEncoding(const std::string& str) noexcept;

    class FileManager {
        FileManager() = delete;
    public:
        static bool IsFileInRootDir(const std::string& file);
        static bool IsFileReadyToWork(const std::string& file);
        static void SetRootPath(const std::string& p);
        static std::string GetAbsoluteFilePath(const std::string& file);
        static std::string GetExtension(const std::string& file);

    private:
        static fs::path CanonicalFullPath(const std::string& file);
        static bool CheckInitialized();

    private:
        static fs::path root_path;
        static bool is_initialized;
    };
}