#include "request_file.h"

namespace http_handler {
    std::string_view ContentTypeFromExtention(const std::string& ext) noexcept {
        std::string ext_ = ext;
        std::transform(ext_.begin(), ext_.end(), ext_.begin(), [](unsigned char c) {return std::tolower(c); });

        for (const auto& p : file_formats) {
            if (p.first == ext_) {
                return p.second;
            }
        }

        return ContentType::BINARY_DATA;
    }

}