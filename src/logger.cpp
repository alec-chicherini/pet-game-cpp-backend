
#include "logger.h"

namespace logger {
    bool LOG::is_init = { false };

    LOG::LOG() {
        severity = NONE;
        data_type = JSON;
    }

    LOG::LOG(DATA_TYPE type) :LOG() {
        severity = NONE;
        data_type = type;
    }

    LOG::LOG(MESSAGE_SEVERITY sev) : LOG(JSON) {
        severity = sev;
        data_type = JSON;
    }

    LOG::LOG(MESSAGE_SEVERITY sev, DATA_TYPE type) : LOG(type) {
        severity = sev;
        data_type = type;
    }

    void LOG::Init() {
        logging::add_common_attributes();
        logging::add_console_log(
            std::cout,
            keywords::format = "%Message%",
            keywords::auto_flush = true
        );
        is_init = true;
    }


    LOG& LOG::operator<<(std::string msg) {
        if (severity == LOG::MESSAGE_DATA) {
            map_to_send["message"] = msg;
        }
        else {
            str_to_send.append(msg);
        }
        return *this;
    };
    LOG& LOG::operator<<(std::string_view msg) {
        if (severity == LOG::MESSAGE_DATA) {
            map_to_send["message"] = std::string(msg);
        }
        else {
            str_to_send.append(msg);
        }
        return *this;
    };
    LOG& LOG::operator<<(const char* msg) {
        if (severity == LOG::MESSAGE_DATA) {
            map_to_send["message"] = std::string(msg);
        }
        else {
            str_to_send.append(msg);
        }
        return *this;
    };
    LOG& LOG::operator<<(const int& msg) {
        if (severity == LOG::MESSAGE_DATA) {
            map_to_send["message"] = std::to_string(msg);
        }
        else {
            str_to_send.append(std::to_string(msg));
        }
        return *this;
    }
    LOG& LOG::operator<<(json::value&& msg) {
        if (severity == LOG::MESSAGE_DATA) {
            map_to_send["data"] = msg;
        }
        else {
            std::stringstream ss;
            ss << msg;
            str_to_send.append(ss.str());
        }
        return *this;
    };
    LOG& LOG::operator<<(SERVICE msg) {
        if (is_init == false) {
            Init();
        }

        switch (msg)
        {
        case LOG::Flush:
        {
            switch (severity)
            {
            case LOG::TRACE:
            case LOG::DEBUG:
            case LOG::INFO:
            case LOG::WARNING:
            case LOG::ERROR_:
            case LOG::FATAL:
            {
                if (data_type == JSON) {

                    json::object jv_full_msg;
                    jv_full_msg.emplace("timestamp", boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time()));
                    jv_full_msg.emplace("data", str_to_send);
                    jv_full_msg.emplace("message", severity_strings[static_cast<int>(severity)]);
                    std::stringstream ss;
                    ss << jv_full_msg;
                    BOOST_LOG(my_logger::get()) << ss.str();
                }
                else {
                    BOOST_LOG(my_logger::get()) << str_to_send;
                }
            }
            break;
            case LOG::NONE:
                break;
                {
                    BOOST_LOG(my_logger::get()) << str_to_send;
                }
            case LOG::MESSAGE_DATA:
            {
                json::object jv_full_msg;
                jv_full_msg.emplace("timestamp", boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::universal_time()));
                jv_full_msg.emplace("data", map_to_send["data"]);
                jv_full_msg.emplace("message", map_to_send["message"]);
                std::stringstream ss;
                ss << jv_full_msg;
                BOOST_LOG(my_logger::get()) << ss.str();
            }
            break;
            }
        }
        break;
        }
        return *this;
    };
}