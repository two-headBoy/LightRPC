#pragma once

#include <functional>
#include <memory>
#include <thread>

#include "base/buffer/buffer.h"
#include "base/error_code/rpc_error_code.h"
#include "base/log/log.h"
#include "base/pool/threadpool.h"
#include "network/core/event_loop.h"
#include "network/tcp/tcp_connection.h"
#include "network/tcp/tcp_server.h"
#include "protocol/codec.h"
#include "protocol/proto_traits.h"
#include "protocol/rpc.pb.h"

namespace rpc {

class RpcService;

class RpcServer {
public:
    using ServiceCallback = std::function<void(const RpcRequest &, RpcResponse &)>;

    RpcServer(std::string ip, uint16_t port, int threadNum, int maxQueueSize);
    ~RpcServer();

    RpcServer(const RpcServer &) = delete;
    RpcServer &operator=(const RpcServer &) = delete;

    void Start();
    void Stop();

    void RegisterRaw(const std::string &service_name, const std::string &method_name, ServiceCallback callback);

    template <typename ReqT, typename RspT>
    void Register(const std::string &service_name, const std::string &method_name,
                  std::function<void(const ReqT &, RspT &)> handler) {
        static_assert(is_proto_message_v<ReqT>, "ReqT must be a protobuf message");
        static_assert(is_proto_message_v<RspT>, "RspT must be a protobuf message");

        ServiceCallback raw_cb = [h = std::move(handler)](const RpcRequest &req, RpcResponse &resp) {
            resp.set_request_id(req.request_id());

            ReqT request;
            if (!request.ParseFromString(req.body())) {
                resp.set_code(static_cast<int32_t>(RpcErrorCode::SERVER_REQUEST_PARSE_FAILED));
                resp.set_msg("Failed to parse request body");
                return;
            }
            RspT response;
            h(request, response);

            std::string out;
            if (!response.SerializeToString(&out)) {
                resp.set_code(static_cast<int32_t>(RpcErrorCode::SERVER_RESPONSE_SERIALIZE_FAILED));
                resp.set_msg("Failed to serialize response body");
                return;
            }
            resp.set_code(static_cast<int32_t>(RpcErrorCode::OK));
            resp.set_msg("success");
            resp.set_body(out);
        };

        RegisterRaw(service_name, method_name, std::move(raw_cb));
    }

private:
    void onMessage(const std::shared_ptr<TcpConnection> &conn, Buffer *buffer);
    void onConnection(const std::shared_ptr<TcpConnection> &conn);

    std::string ip_;
    uint16_t port_;

    std::unique_ptr<ThreadPool> pool_;
    std::unique_ptr<EventLoop> loop_;
    std::unique_ptr<TcpServer> tcp_server_;
    std::unique_ptr<RpcService> service_;
    std::atomic<bool> stopped_{false};
    std::thread loop_thread_;
};

}  // namespace rpc
