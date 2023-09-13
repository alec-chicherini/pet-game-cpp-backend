#pragma once

#include "response_utils.h"
#include "application.h"
#include <optional>
#include <atomic>

std::atomic<int> number_of_ticks{ 0 };
namespace http_handler {
    using namespace app;
    class RequestAPI {
        RequestAPI() = delete;
    public:
        template<typename REQUEST_T>
        static StringResponse process(REQUEST_T&& req, model::Game& game_, const tcp::endpoint& remote_endpoint) {
            auto begin = std::chrono::high_resolution_clock::now();
            LOG(LOG::MESSAGE_DATA)
                << "request received"sv
                << LOG::ToJson("ip"sv, remote_endpoint.address().to_string(),
                    "URI"sv, req.target(),
                    "method"sv, req.method_string())
                << LOG::Flush;

            unsigned http_version = req.version();
            bool keep_alive = req.keep_alive();
            StringResponse response;

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
                        std::make_pair(http::field::cache_control, FreqStr::no_cache));
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
                        std::make_pair(http::field::content_type, ContentType::APPLICATION_JSON),
                        std::make_pair(http::field::cache_control, FreqStr::no_cache));
            };

             auto make_response_error_405 = [&](std::string_view allowed_methods, std::string_view target_)->StringResponse 
             {       
                 std::string message = "Only ";
                 message.append(allowed_methods).append(" is expected in ").append(target_);
                 std::string body = json_loader::ToJsonAsString(FreqStr::code, "invalidMethod"sv,
                                                                FreqStr::message, message);
                 return ResponseUtils::MakeResponse<StringResponse>(
                     http::int_to_status(405u),
                     body,
                     body.size(),
                     http_version,
                     keep_alive,
                     std::make_pair(http::field::content_type, ContentType::APPLICATION_JSON),
                     std::make_pair(http::field::allow, allowed_methods),
                     std::make_pair(http::field::cache_control, FreqStr::no_cache));
             };

             auto this_is_bad_autorization = [&req](std::string& Authorization) {
                 bool bad = true;
                 try {
                     auto AutorizationB = req.at("Authorization"sv);
                     Authorization = std::string(AutorizationB.data(), AutorizationB.size());
                     bad = false;
                 }
                 catch (std::exception&) {
                     try {
                         auto AutorizationB = req.at("authorization"sv);
                         Authorization = std::string(AutorizationB.data(), AutorizationB.size());
                         bad = false;
                     }
                     catch (std::exception&) {}
                 }

                 std::string b = req.body();

                 if (json_loader::isJsonValid(b) && b.find("authorization"sv) != std::string::npos) {
                     Authorization = json_loader::GetValueAsString(b, "authorization"s);
                     bad = false;
                 }
                 else if (json_loader::isJsonValid(b) && b.find("Authorization"sv) != std::string::npos) {
                     Authorization = json_loader::GetValueAsString(b, "Authorization"s);
                     bad = false;
                 }

                 if (bad == true) {
                     return true;
                 }
                 const std::string str_bearer("Bearer");
                 auto pos = Authorization.find(str_bearer);
                 if (pos == std::string::npos) {
                     return true;
                 }
                 Authorization = Authorization.erase(0, pos + str_bearer.size());
                 std::erase(Authorization, ' ');
                 if (Authorization.size() != 32) {
                     return true;
                 }

                 for (auto& ch : Authorization) {
                     if (std::isxdigit(ch) == false) {
                         return true;
                     }
                 }

                 return false;
             };

            std::string target = FromUrlEncoding(static_cast<std::string>(req.target()));
            auto target_is = [&target](std::string_view target_)->bool {
                return target == target_;
            };

            auto target_is_records = [&target]()->bool {
                std::string str(TargetAPI::TARGET_RECORDS);
                return target.find(str) != std::string::npos;
                
            };

            auto target_is_single_map = [&target]()->bool {
                std::string str(TargetAPI::TARGET_MAPS);
                return target.find(str + "/") != std::string::npos;
            };

            auto target_is_game_join = [&target,&req]()->bool {
                bool is_application_json; 
                bool have_userName_and_mapId=json_loader::isJsonHaveKeys(req.body(),"userName"s,"mapId"s); 
                bool is_target_game_join = (target == TargetAPI::TARGET_JOIN);

                try{
                    auto content_type_req = req.at(http::field::content_type);
                    if (content_type_req == ContentType::APPLICATION_JSON) {
                        is_application_json = true;
                    }
                    else {
                        is_application_json = false;
                    }
                }
                catch(std::exception&){
                    is_application_json = false;
                }
             
                return is_application_json && have_userName_and_mapId && is_target_game_join;
            };

            auto target_is_action = [&target, &req]()->bool {
                bool is_application_json;
                bool have_move = json_loader::isJsonHaveKeys(req.body(), "move"s);
                bool is_target_action = (target == TargetAPI::TARGET_ACTION);

                try {
                    auto content_type_req = req.at(http::field::content_type);
                    if (content_type_req == ContentType::APPLICATION_JSON) {
                        is_application_json = true;
                    }
                    else {
                        is_application_json = false;
                    }
                }
                catch (std::exception&) {
                    is_application_json = false;
                }

                return is_application_json && have_move && is_target_action;
            };

            auto target_is_tick = [&target, &req]()->bool {
                bool is_application_json;
                bool have_timeDelta = json_loader::isJsonHaveKeys(req.body(), "timeDelta"s);
                bool is_target_tick = (target == TargetAPI::TARGET_TICK);

                try {
                    auto content_type_req = req.at(http::field::content_type);
                    if (content_type_req == ContentType::APPLICATION_JSON) {
                        is_application_json = true;
                    }
                    else {
                        is_application_json = false;
                    }
                }
                catch (std::exception&) {
                    is_application_json = false;
                }

                return is_application_json && have_timeDelta && is_target_tick;
            };

            auto get_single_map_name = [&target]()->std::string {
                return  target.erase(0, target.rfind("/") + 1);
            };

            auto verb = req.method();
            switch (verb) {
            case http::verb::get:
            case http::verb::head:
            {
                if (target_is(TargetAPI::TARGET_JOIN)) {
                    response = make_response_error_405(ResponseAllowedMethods::POST, TargetAPI::TARGET_JOIN);
                }
                else if (target_is(TargetAPI::TARGET_TICK)) {
                    response = make_response_error_405(ResponseAllowedMethods::POST, TargetAPI::TARGET_TICK);
                }
                else if (target_is(TargetAPI::TARGET_PLAYERS)) {
                    std::string Authorization;

                    if (this_is_bad_autorization(Authorization)) {
                        std::string body = json_loader::ToJsonAsString(FreqStr::code, FreqStr::invalidToken,
                                                                       FreqStr::message, "Error: Authorization header is wrong"sv);
                        response = make_response_error(http::int_to_status(401u), body, body.size());
                    }
                    else if (Players::FindPlayerByToken(Token(Authorization)) == std::nullopt) {
                        std::string body = json_loader::ToJsonAsString(FreqStr::code, FreqStr::unknownToken,
                                                                       FreqStr::message, "Error: Player token has not been found"sv);
                        response = make_response_error(http::int_to_status(401u), body, body.size());
                    }
                    else {//if everething is ok
                        Token token(Authorization);
                        std::string body = json_loader::GetPlayersCase(Players::FindPlayersInSessionWithToken(token));
                        response = ResponseUtils::MakeResponse<StringResponse>(
                            http::status::ok,
                            verb == http::verb::get ? body : ""sv,
                            body.size(),
                            http_version,
                            keep_alive,
                            std::make_pair(http::field::content_type, ContentType::APPLICATION_JSON),
                            std::make_pair(http::field::cache_control, FreqStr::no_cache));
                    }
                }
                else if (target_is(TargetAPI::TARGET_STATE)) {
                    std::string Authorization;
                    if (this_is_bad_autorization(Authorization)) {
                        std::string body = json_loader::ToJsonAsString(FreqStr::code, FreqStr::invalidToken,
                            FreqStr::message, "Error: Authorization header is wrong"sv);
                        response = make_response_error(http::int_to_status(401u), body, body.size());
                    }
                    else if (Players::FindPlayerByToken(Token(Authorization)) == std::nullopt) {
                        std::string body = json_loader::ToJsonAsString(FreqStr::code, FreqStr::unknownToken,
                            FreqStr::message, "Error: Player token has not been found"sv);
                        response = make_response_error(http::int_to_status(401u), body, body.size());
                    }
                    else {//if everething is ok
                        Token token(Authorization);
                        std::string body = json_loader::GetStateCase(Players::FindPlayersInSessionWithToken(token),
                                                                     Players::FindLootsInSessionWithPlayerToken(token));
                        response = ResponseUtils::MakeResponse<StringResponse>(
                            http::status::ok,
                            verb == http::verb::get ? body : ""sv,
                            body.size(),
                            http_version,
                            keep_alive,
                            std::make_pair(http::field::content_type, ContentType::APPLICATION_JSON),
                            std::make_pair(http::field::cache_control, FreqStr::no_cache));
                    }
                }
                else if (target_is(TargetAPI::TARGET_MAPS)) {
                    std::string body = json_loader::GetMapsList(game_);
                    response = make_response_json(http::status::ok,
                        verb == http::verb::get ? body : ""sv,
                        body.size());
                }
                else if (target_is_records()) {
                     auto ParseToPair = [](const std::string& str) {
                         std::optional<std::pair<std::string, std::string>> result;
                         
                         auto pos_eq = str.find('=');
                        if (pos_eq == std::string::npos) {
                            result = std::nullopt;
                            return result;
                        }
                         auto left = str.substr(0, pos_eq);
                         auto right = str.substr(pos_eq + 1, str.size() - pos_eq);
                         result = std::make_pair(left, right);
                         return result;
                     };

                    auto GetURIParams = [&ParseToPair](std::string target) {
                        std::vector<std::pair<std::string, std::string>> result;
                        auto pos_left = target.find('?');
                        if (pos_left == std::string::npos)return result;
                        target.erase(0, pos_left + 1);
                        
                        std::string pair;
                        for (int i = 0; i < target.size(); i++) {
                            if (target[i] == '&') {
                                auto parse = ParseToPair(pair);
                                if (parse) {
                                    result.push_back(parse.value());
                                    pair.clear();
                                }
                                else return result;
                            }
                            else {
                                pair += target[i];
                            }
                        }
                        
                        auto parse = ParseToPair(pair);
                        if (parse)result.push_back(parse.value());
                        else return result;
                    
                        return result;
                    };///end of getURI param
                    
                    int start(0);
                    int maxItems(100);
                    try {
                        auto params = GetURIParams(target);
                        for (auto p : params) {
                            if (p.first == "start") {
                                start = stoi(p.second);
                            }
                            if (p.first == "maxItems") {
                                maxItems = stoi(p.second);
                            }
                        }
                    }
                    catch (std::exception&) {
                        start = 0;
                        maxItems = 100;
                    }

                    if (start < 0)start = 0;
                    if (maxItems > 100) {
                        std::string body = json_loader::ToJsonAsString(FreqStr::code, FreqStr::invalidArgument,
                                                                       FreqStr::message, "Error maxItems must be less then 100"sv);
                        response = make_response_error(http::int_to_status(400u), body, body.size());
                    }else{
                        std::string body = json_loader::GetRecordsCase(game_,start,maxItems);
                        response = make_response_json(http::status::ok,
                            verb == http::verb::get ? body : ""sv,
                            body.size());
                    }
                }
                else if (target_is_single_map()) {
                    auto body = json_loader::GetMap(game_, get_single_map_name());
                    if (body != std::nullopt) {
                        response = make_response_json(http::status::ok,
                            verb == http::verb::get ? body.value() : ""sv,
                            body.value().size());
                    }
                    else {
                        std::string body = json_loader::MakeError("mapNotFound"s, "Map not found"s);
                        response = make_response_json(http::int_to_status(404u), body, body.size());
                    }
                }
                break;
            } 
            case http::verb::post:
            {
                if (target_is(TargetAPI::TARGET_PLAYERS)) {
                    response = make_response_error_405(ResponseAllowedMethods::GET_HEAD, TargetAPI::TARGET_PLAYERS);
                }
                else if (target_is(TargetAPI::TARGET_STATE)) {
                    response = make_response_error_405(ResponseAllowedMethods::GET_HEAD, TargetAPI::TARGET_STATE);
                }
                else if (target_is_single_map()) {
                    response = make_response_error_405(ResponseAllowedMethods::GET_HEAD, "/maps/{map_id}");
                }
                else if (target_is_tick()) {
                    auto timeDelta = json_loader::GetValueAsUInt64(req.body(), "timeDelta");
                    if (game_.GetTickPeriod() != Game::TICK_TESTING_MODE) {
                        std::string body = json_loader::ToJsonAsString(FreqStr::code, "badRequest"sv,
                                                                       FreqStr::message, "Error: server was launched with --tick_period. Now the server decides when the tick will occur."sv);
                        response = make_response_error(http::int_to_status(400u), body, body.size());
                    }
                    else if ((json_loader::isJsonValid(req.body()) == false) || (timeDelta.has_value() == false)) {
                        std::string body = json_loader::ToJsonAsString(FreqStr::code, FreqStr::invalidArgument,
                                  FreqStr::message, "Error in parsing JSON or timeDelta field is error"sv);
                        response = make_response_error(http::int_to_status(400u), body, body.size());
                    }
                    else {//if everething is ok
                        game_.Update(timeDelta.value());
                        std::string body = "{}";
                        response = ResponseUtils::MakeResponse<StringResponse>(
                            http::status::ok,
                            body,
                            body.size(),
                            http_version,
                            keep_alive,
                            std::make_pair(http::field::content_type, ContentType::APPLICATION_JSON),
                            std::make_pair(http::field::cache_control, FreqStr::no_cache));
                    }
                }
                else if (target_is_action()) {
                    std::string Authorization;
                    std::string move = json_loader::GetValueAsString(req.body(), "move");

                    auto is_move_valid = [&move] {
                        if (move == "L" ||
                            move == "R" ||
                            move == "U" ||
                            move == "D" ||
                            move == "") {
                            return true;
                        }
                        return false;
                    };

                    if (this_is_bad_autorization(Authorization)) {
                        std::string body = json_loader::ToJsonAsString(FreqStr::code, FreqStr::invalidToken,
                            FreqStr::message, "Error: Authorization header is wrong"sv);
                        response = make_response_error(http::int_to_status(401u), body, body.size());
                    }
                    else if (Players::FindPlayerByToken(Token(Authorization)) == std::nullopt) {
                        std::string body = json_loader::ToJsonAsString(FreqStr::code, FreqStr::unknownToken,
                            FreqStr::message, "Error: Player token has not been found"sv);
                        response = make_response_error(http::int_to_status(401u), body, body.size());
                    }
                    else if ((json_loader::isJsonValid(req.body()) == false) && is_move_valid()) {
                        std::string body = json_loader::ToJsonAsString(FreqStr::code, FreqStr::invalidArgument,
                            FreqStr::message, "Error in parsing JSON or move field is wrong"sv);
                        response = make_response_error(http::int_to_status(400u), body, body.size());
                    }
                    else {//if everething is ok
                        Token token(Authorization);
                        Players::FindPlayerByToken(token).value()->DoAction(ACTIONS::MOVE, move);
                        std::string body = "{}";
                        response = ResponseUtils::MakeResponse<StringResponse>(
                            http::status::ok,
                            body,
                            body.size(),
                            http_version,
                            keep_alive,
                            std::make_pair(http::field::content_type, ContentType::APPLICATION_JSON),
                            std::make_pair(http::field::cache_control, FreqStr::no_cache));
                    }
                }
                else if (target_is_game_join()) {
                    Map::Id mapId(json_loader::GetValueAsString(req.body(), "mapId"));
                    std::string userName = json_loader::GetValueAsString(req.body(), "userName");

                    if(json_loader::isJsonValid(req.body()) == false){
                        std::string body = json_loader::ToJsonAsString(FreqStr::code, FreqStr::invalidArgument,
                                                                       FreqStr::message, "Error in parsing JSON"sv);
                        response = make_response_error(http::int_to_status(400u), body, body.size());
                    }
                    else if (userName.size() == 0) {
                        std::string body = json_loader::ToJsonAsString(FreqStr::code, FreqStr::invalidArgument,
                                                                       FreqStr::message, "Error in recived JSON: userName is empty"sv);
                        response = make_response_error(http::int_to_status(400u), body, body.size());
                    }
                    else if (game_.FindMap(mapId) == nullptr) {
                        std::string body (json_loader::ToJsonAsString(FreqStr::code, "mapNotFound"sv,
                                                                      FreqStr::message, "Map not found"sv));
                        response = make_response_error(http::int_to_status(404u), body, body.size());
                    }
                    else  {//if everything is ok
                        std::shared_ptr<Player> newPlayer = Players::AddPlayer(game_, userName, mapId);
                        std::string body = json_loader::ToJsonAsString("authToken"sv, *newPlayer->GetToken(),
                                                                       "playerId"sv, newPlayer->GetIdAsUInt64());
                        response = ResponseUtils::MakeResponse<StringResponse>(
                            http::status::ok,
                            body,
                            body.size(),
                            http_version,
                            keep_alive,
                            std::make_pair(http::field::content_type, ContentType::APPLICATION_JSON),
                            std::make_pair(http::field::cache_control, FreqStr::no_cache));
                    }
                }
                else {
                    std::string body = json_loader::ToJsonAsString(FreqStr::code, FreqStr::invalidArgument,
                                                                   FreqStr::message, "General request error. Logic was not satisfyed."sv);
                    response = make_response_error(http::int_to_status(400u), body, body.size());
                }
                break;
            }
            default:
            {
                if (target_is(TargetAPI::TARGET_PLAYERS)) {
                    response = make_response_error_405(ResponseAllowedMethods::GET_HEAD, TargetAPI::TARGET_PLAYERS);
                }
                else if (target_is(TargetAPI::TARGET_STATE)) {
                    response = make_response_error_405(ResponseAllowedMethods::GET_HEAD, TargetAPI::TARGET_STATE);
                }
                else  if (target_is(TargetAPI::TARGET_JOIN)) {
                    response = make_response_error_405(ResponseAllowedMethods::POST, TargetAPI::TARGET_JOIN);
                }
                else  if (target_is(TargetAPI::TARGET_TICK)) {
                    response = make_response_error_405(ResponseAllowedMethods::POST, TargetAPI::TARGET_TICK);
                }
                else if (target_is(TargetAPI::TARGET_ACTION)) {
                    response = make_response_error_405(ResponseAllowedMethods::GET_HEAD, TargetAPI::TARGET_ACTION);
                }
                else if (target_is(TargetAPI::TARGET_RECORDS)) {
                    response = make_response_error_405(ResponseAllowedMethods::GET_HEAD, TargetAPI::TARGET_RECORDS);
                }
                else if (target_is_single_map()) {
                    response = make_response_error_405(ResponseAllowedMethods::GET_HEAD, "/maps/{map_id}"sv);
                }
                else{ ///for all other targets all methods in app expected
                    response = make_response_error_405(ResponseAllowedMethods::GET_HEAD_POST, "/");
                }
            }
            };
            auto end = std::chrono::high_resolution_clock::now();
            auto response_time_ms=std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
            
            int response_code;
            std::string content_type;
            try {
                content_type = response.at(http::field::content_type);
            }
            catch (std::exception&) {
                content_type = "null";
            }
            response_code =response.result_int();

            std::string msg_sent("response sent");
            if (target_is_tick()) {
                number_of_ticks++;
                msg_sent += "(";
                msg_sent += std::to_string(number_of_ticks.load());
                msg_sent += ")";
            }

            LOG(LOG::MESSAGE_DATA)
                << msg_sent
                << LOG::ToJson("response_time"sv, std::to_string(response_time_ms.count()),
                               FreqStr::code, response_code,
                               "content_type"sv, content_type)
                << LOG::Flush;
            return response;
        };

    };
}///namespace http_handler