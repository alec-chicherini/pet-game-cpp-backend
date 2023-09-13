#pragma once

#include "response_utils.h"
#include "request_file.h"
#include "request_api.h"

namespace http_handler {
class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    explicit RequestHandler(model::Game& game, Strand& api_strand)
        : game_{ game }, api_strand_{ api_strand }{
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req,
                    const tcp::endpoint& remote_endpoint,
                    Send&& send) {
        // Обработать запрос request и отправить ответ, используя send

        std::string target = FromUrlEncoding(static_cast<std::string>(req.target()));

        auto target_is = [&target](std::string_view target_)->bool {
            return target == target_;
        };

        auto target_is_single_map = [&target]()->bool {
            std::string str(TargetAPI::TARGET_MAPS);
            return target.find(str + "/") != std::string::npos;
        };

        auto target_is_records = [&target]()->bool {
            std::string str(TargetAPI::TARGET_RECORDS);
            return target.find(str) != std::string::npos;

        };

        auto target_is_api = [=] {
            if (target_is(TargetAPI::TARGET_MAPS) ||
                target_is_single_map() || 
                target_is(TargetAPI::TARGET_JOIN) ||
                target_is(TargetAPI::TARGET_PLAYERS) ||
                target_is(TargetAPI::TARGET_STATE) || 
                target_is(TargetAPI::TARGET_ACTION) || 
                target_is_records() ||
                target_is(TargetAPI::TARGET_TICK))
                return true;
            else
                return false;
        };

        auto execute_send = [&send] (VariantResponse&& response)->void{
            if (std::holds_alternative<StringResponse>(response)) {
                send(std::get<StringResponse>(std::move(response)));
            }
            else if (std::holds_alternative<FileResponse>(response)) {
                send(std::get<FileResponse>(std::move(response)));
            }
        };
        try {
            if (target_is_api()) {
                auto handle = [self = shared_from_this(),
                               req = std::forward<decltype(req)>(req),
                               send,
                               this,
                               remote_endpoint] {
                    assert(self->api_strand_.running_in_this_thread());
                    send(RequestAPI::process(std::move(req), game_, remote_endpoint));
                };
                
                net::dispatch(api_strand_, handle);
            }
            else { //if (target_is_file())
                execute_send(RequestFile::process(std::move(req), game_, remote_endpoint));
            }
        }
        catch(std::exception & ex){
            BOOST_LOG(my_logger::get()) << "ERROR: in process request " << ex.what() << std::endl;
        }
    }

private:
    model::Game& game_;
    Strand& api_strand_;
};

}  // namespace http_handler
