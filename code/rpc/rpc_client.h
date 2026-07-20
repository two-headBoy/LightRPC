#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "base/error_code/rpc_error_code.h"
#include "base/log/log.h"
#include "network/core/event_loop.h"
#include "network/tcp/tcp_client.h"
#include "protocol/codec.h"
#include "protocol/proto_traits.h"
#include "protocol/rpc.pb.h"

namespace rpc {

class RpcClient {
public:
    using ResponseCallback = std::function<void(const RpcResponse &)>;

    RpcClient(const std::string &ip, uint16_t port);
    ~RpcClient();

    RpcClient(const RpcClient &) = delete;
    RpcClient &operator=(const RpcClient &) = delete;

    void Start();
    void Stop();

    void CallRaw(const std::string &service_name, const std::string &method_name, const std::string &request_body,
                 ResponseCallback callback, int timeout_ms = 5000);

    template <typename ReqT, typename RspT>
    void Call(const std::string &service_name, const std::string &method_name, const ReqT &req,
              std::function<void(const RspT &)> callback, int timeout_ms = 5000) {
        static_assert(is_proto_message_v<ReqT>, "ReqT must be a protobuf message");
        static_assert(is_proto_message_v<RspT>, "RspT must be a protobuf message");

        std::string body;
        if (!req.SerializeToString(&body)) {
            LOG_ERROR("Serialize request failed: service=%s, method=%s", service_name.c_str(), method_name.c_str());
            if (callback) {
                RspT rsp;
                callback(rsp);
            }
            return;
        }

        ResponseCallback raw_cb = [cb = std::move(callback)](const RpcResponse &resp) {
            RspT rsp;
            if (resp.code() != static_cast<int32_t>(RpcErrorCode::OK)) {
                LOG_WARN("RPC call failed: code=%d, msg=%s", resp.code(), resp.msg().c_str());
                if (cb) cb(rsp);
                return;
            }
            if (!rsp.ParseFromString(resp.body())) {
                LOG_ERROR("Parse response failed: code=%d, msg=%s", resp.code(), resp.msg().c_str());
                if (cb) cb(rsp);
                return;
            }
            if (cb) cb(rsp);
        };

        CallRaw(service_name, method_name, body, std::move(raw_cb), timeout_ms);
    }

    bool IsConnected() const;

private:
    void onConnection(const std::shared_ptr<TcpConnection> &conn);
    void onMessage(const std::shared_ptr<TcpConnection> &conn, Buffer *buffer);
    void onError(int err);

    void DropAllPending(RpcErrorCode err_code, std::string err_msg);

    std::string GenerateRequestId();

    std::string ip_;
    uint16_t port_;

    std::unique_ptr<EventLoop> loop_;
    std::unique_ptr<TcpClient> client_;
    std::atomic<bool> stopped_{false};
    std::thread loop_thread_;

    std::mutex pending_mutex_;
    std::unordered_map<std::string, ResponseCallback> pending_calls_;
};

}  // namespace rpc
