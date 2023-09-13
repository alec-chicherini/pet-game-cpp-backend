#pragma once

#include "response_utils.h"

namespace http_handler {
    static constexpr size_t NUM_FILE_FORMATS = 19;
    static constexpr std::array<std::pair<std::string_view, std::string_view>, NUM_FILE_FORMATS> file_formats = {
        std::make_pair(".htm"sv,  ContentType::TEXT_HTML),
        std::make_pair(".html"sv, ContentType::TEXT_HTML),
        std::make_pair(".css"sv,  ContentType::TEXT_PLAIN),
        std::make_pair(".txt"sv,  ContentType::TEXT_PLAIN),
        std::make_pair(".js"sv,   ContentType::TEXT_JAVASCRIPT),

        std::make_pair(".json"sv, ContentType::APPLICATION_JSON),
        std::make_pair(".xml"sv,  ContentType::APPLICATION_XML),

        std::make_pair(".png"sv,  ContentType::IMAGE_PNG),
        std::make_pair(".jpg"sv,  ContentType::IMAGE_JPEG),
        std::make_pair(".jpe"sv,  ContentType::IMAGE_JPEG),
        std::make_pair(".jpeg"sv, ContentType::IMAGE_JPEG),
        std::make_pair(".gif"sv,  ContentType::IMAGE_GIF),
        std::make_pair(".bmp"sv,  ContentType::IMAGE_BMP),
        std::make_pair(".ico"sv,  ContentType::IMAGE_ICO),
        std::make_pair(".tiff"sv, ContentType::IMAGE_TIFF),
        std::make_pair(".tif"sv,  ContentType::IMAGE_TIFF),
        std::make_pair(".svg"sv,  ContentType::IMAGE_SVG),
        std::make_pair(".svgz"sv, ContentType::IMAGE_SVG),

        std::make_pair(".mp3"sv,  ContentType::AUDIO_MP3),
    };
    static_assert(file_formats[0].first == ".htm"sv);
    static_assert(file_formats[NUM_FILE_FORMATS - 1].second == ContentType::AUDIO_MP3);

    std::string_view ContentTypeFromExtention(const std::string& ext) noexcept;

    class RequestFile {
        RequestFile() = delete;
    public:
        template<typename REQUEST_T>
        static VariantResponse process(REQUEST_T&& req, model::Game& game_, const tcp::endpoint& remote_endpoint) {
            auto begin = std::chrono::high_resolution_clock::now();

            LOG(LOG::MESSAGE_DATA)
                << "request received"sv
                << LOG::ToJson("ip"sv, remote_endpoint.address().to_string(),
                    "URI"sv, req.target(),
                    "method"sv, req.method_string())
                << LOG::Flush;

            unsigned http_version = req.version();
            bool keep_alive = req.keep_alive();
            VariantResponse response;

            auto make_response_json = [&](http::status status,
                std::string_view body,
                size_t body_size)->StringResponse {
                    return ResponseUtils::MakeResponse<StringResponse>(
                        status,
                        body,
                        body_size,
                        http_version,
                        keep_alive,
                        std::make_pair(http::field::content_type, ContentType::APPLICATION_JSON),
                        std::make_pair(http::field::cache_control, "no-cache"sv));
            };

            auto make_response_error = [&](http::status status,
                std::string_view body,
                size_t body_size)->StringResponse {
                    return ResponseUtils::MakeResponse<StringResponse>(
                        status,
                        body,
                        body_size,
                        http_version,
                        keep_alive,
                        std::make_pair(http::field::content_type, ContentType::TEXT_PLAIN),
                        std::make_pair(http::field::cache_control, "no-cache"sv));
            };

            auto make_response_file = [&](http::status status,
                http::file_body::value_type& body,
                size_t body_size,
                std::string_view content_type
                )->FileResponse {
                    return ResponseUtils::MakeResponse<FileResponse>(
                        status,
                        std::move(body),
                        body_size,
                        http_version,
                        keep_alive,
                        std::make_pair(http::field::content_type, content_type),
                        std::make_pair(http::field::cache_control, "no-cache"sv));
            };

            std::string target = FromUrlEncoding(static_cast<std::string>(req.target()));
            if (target == "/") {
                target = "/index.html";
            }

            auto target_is_bad = [&target] {
                bool result = ((target == TargetAPI::TARGET_BAD) ||
                    ((target.size() > TargetAPI::TARGET_BAD.size()) &&
                        (target.substr(0, TargetAPI::TARGET_BAD.size()) == TargetAPI::TARGET_BAD)));
                return result;
            };

            auto get_single_map_name = [&target]()->std::string {
                return  target.erase(0, target.rfind("/") + 1);
            };

            auto target_file_in_root_dir = [&target]()->bool {
                return FileManager::IsFileInRootDir(target);
            };

            auto target_file_ready_to_work = [&target]()->bool {
                return FileManager::IsFileReadyToWork(target);
            };

            std::string_view content_type;
            auto verb = req.method();
            switch (verb) {
            case http::verb::get:
            case http::verb::head:
            {
                if (target_is_bad()) {
                    content_type = ContentType::APPLICATION_JSON;
                    std::string body = json_loader::MakeError("badRequest"s, "Bad request"s);
                    response = make_response_json(http::int_to_status(400u), body, body.size());
                }
                else if (target_file_in_root_dir() == false) {
                    content_type = ContentType::TEXT_PLAIN;
                    std::string_view body = "Bad Request"sv;
                    response = make_response_error(http::int_to_status(400u), body, body.size());
                }
                else if (target_file_ready_to_work() == false) {
                    content_type = ContentType::TEXT_PLAIN;
                    std::string_view body = "Not Found"sv;
                    response = make_response_error(http::int_to_status(404u), body, body.size());
                }
                else {
                    http::file_body::value_type file_get;
                    http::file_body::value_type file_head;
                    auto path = FileManager::GetAbsoluteFilePath(target);
                    sys::error_code ec;
                    file_get.open(path.data(), beast::file_mode::read, ec);
                    if (ec) {
                        LOG(LOG::ERROR_) << "ERROR: failed to open file :" << path << " ec: " << ec.what() << LOG::Flush;
                        return ErrorResponse();
                    }

                    content_type = ContentTypeFromExtention(FileManager::GetExtension(target));
                    response = make_response_file(http::status::ok,
                        verb == http::verb::get ? file_get : file_head,
                        file_get.size(),
                        content_type);
                }

                break;
            }
            default:
            {
                std::string_view body = "Invalid method"sv;
                content_type = ContentType::TEXT_HTML;
                response = ResponseUtils::MakeResponse<StringResponse>(
                    http::int_to_status(405u),
                    body,
                    body.size(),
                    http_version,
                    keep_alive,
                    std::make_pair(http::field::content_type, content_type),
                    std::make_pair(http::field::cache_control, "no-cache"sv),
                    std::make_pair(http::field::allow, ResponseAllowedMethods::GET_HEAD));
            }
            };

            auto end = std::chrono::high_resolution_clock::now();
            auto response_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);

            int response_code;
            if (std::holds_alternative<StringResponse>(response)) {
                response_code = std::get<StringResponse>(response).result_int();
            }
            else if (std::holds_alternative<FileResponse>(response)) {
                response_code = std::get<FileResponse>(response).result_int();
            }
            

            LOG(LOG::MESSAGE_DATA)
                << "response sent"sv
                << LOG::ToJson("response_time"sv, std::to_string(response_time_ms.count()),
                    "code"sv, response_code,
                    "content_type"sv, content_type)
                << LOG::Flush;

            return response;
        }
    };

}
