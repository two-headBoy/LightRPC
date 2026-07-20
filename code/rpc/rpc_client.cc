#include "rpc/rpc_client.h"

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>

namespace rpc {

RpcClient::RpcClient(const std::string &ip, uint16_t port)
    : ip_(ip), port_(port), loop_(std::make_unique<EventLoop>()) {
    client_ = std::make_unique<TcpClient>(loop_.get(), ip_, port_);

    client_->SetConnectionCallback(std::bind(&RpcClient::onConnection, this, std::placeholders::_1));

    client_->SetMessageCallback(std::bind(&RpcClient::onMessage, this, std::placeholders::_1, std::placeholders::_2));

    client_->SetErrorCallback(std::bind(&RpcClient::onError, this, std::placeholders::_1));
}

RpcClient::~RpcClient() { Stop(); }

void RpcClient::Start() {
    if (stopped_) {
        return;
    }

    LOG_INFO("RpcClient starting");

    loop_thread_ = std::thread([this]() { loop_->Loop(); });

    loop_->RunInLoop([this]() { client_->Connect(); });

    LOG_INFO("RpcClient started");
}

void RpcClient::Stop() {
    bool expected = false;
    if (stopped_.compare_exchange_strong(expected, true)) {
        LOG_INFO("RpcClient stopping");
        DropAllPending(RpcErrorCode::CLIENT_NOT_RUNNING, "rpc client stopped");
        client_->Disconnect();
        loop_->Quit();
        if (loop_thread_.joinable()) {
            loop_thread_.join();
        }
        client_->Stop();
        LOG_INFO("RpcClient stopped");
    }
}

void RpcClient::CallRaw(const std::string &service_name, const std::string &method_name,
                        const std::string &request_body, ResponseCallback callback, int timeout_ms) {
    std::string request_id = GenerateRequestId();

    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        if (stopped_ || !client_->IsConnected()) {
            LOG_WARN("Call failed: client stopped or not connected, reqId=%s", request_id.c_str());
            RpcResponse resp;
            resp.set_request_id(request_id);
            resp.set_code(static_cast<int32_t>(RpcErrorCode::CLIENT_NOT_RUNNING));
            resp.set_msg("client stopped/disconnected");
            callback(resp);
            return;
        }
        pending_calls_[request_id] = std::move(callback);
    }

    int timer_id = std::hash<std::string>{}(request_id);
    loop_->AddTimer(
        timer_id,
        [this, request_id, service_name, method_name]() {
            ResponseCallback cb;
            {
                std::lock_guard<std::mutex> lock(pending_mutex_);
                auto it = pending_calls_.find(request_id);
                if (it != pending_calls_.end()) {
                    cb = std::move(it->second);
                    pending_calls_.erase(it);
                }
            }
            if (cb) {
                RpcResponse resp;
                resp.set_request_id(request_id);
                resp.set_code(static_cast<int32_t>(RpcErrorCode::CLIENT_REQUEST_TIMEOUT));
                resp.set_msg("request timeout");
                LOG_WARN("Request timeout: id=%s, service=%s, method=%s", request_id.c_str(), service_name.c_str(),
                         method_name.c_str());
                cb(resp);
            }
        },
        timeout_ms);

    RpcRequest request;
    request.set_service_name(service_name);
    request.set_method_name(method_name);
    request.set_request_id(request_id);
    request.set_body(request_body);

    LOG_DEBUG("Sending request: id=%s, service=%s, method=%s, timeout=%dms", request_id.c_str(), service_name.c_str(),
              method_name.c_str(), timeout_ms);

    auto encoded = Codec::encode(request);
    if (!encoded.has_value()) {
        LOG_ERROR("Encode request failed: id=%s, service=%s, method=%s", request_id.c_str(), service_name.c_str(),
                  method_name.c_str());
        ResponseCallback cb;
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            auto it = pending_calls_.find(request_id);
            if (it != pending_calls_.end()) {
                cb = std::move(it->second);
                pending_calls_.erase(it);
            }
        }
        int timer_id = std::hash<std::string>{}(request_id);
        loop_->RemoveTimer(timer_id);
        if (cb) {
            RpcResponse resp;
            resp.set_request_id(request_id);
            resp.set_code(static_cast<int32_t>(RpcErrorCode::CLIENT_ENCODE_FAILED));
            resp.set_msg("encode request failed");
            cb(resp);
        }
        return;
    }
    client_->Send(reinterpret_cast<const std::byte *>(encoded->data()), encoded->size());
}

bool RpcClient::IsConnected() const { return client_->IsConnected(); }

void RpcClient::onConnection(const std::shared_ptr<TcpConnection> &conn) {
    if (conn->state() == TcpConnection::State::Connected) {
        LOG_INFO("Connected to server %s:%d, fd=%d", ip_.c_str(), port_, conn->fd());
    } else {
        LOG_INFO("Disconnected from server, fd=%d", conn->fd());
        if (!stopped_) {
            DropAllPending(RpcErrorCode::CLIENT_CONNECTION_DISCONNECTED, "tcp connection disconnected");
        }
    }
}

void RpcClient::onMessage(const std::shared_ptr<TcpConnection> &conn, Buffer *buffer) {
    (void)conn;
    while (buffer->ReadableBytes() >= 4) {
        RpcResponse response;
        if (!Codec::decode(buffer->PeekStringView(), response)) {
            break;
        }

        uint32_t len;
        memcpy(&len, buffer->Peek(), 4);
        len = ntohl(len);
        buffer->Retrieve(4 + len);

        std::string request_id = response.request_id();

        LOG_DEBUG("Received response: id=%s, code=%d", request_id.c_str(), response.code());

        ResponseCallback callback;
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            auto it = pending_calls_.find(request_id);
            if (it != pending_calls_.end()) {
                callback = std::move(it->second);
                pending_calls_.erase(it);
            }
        }

        int timer_id = std::hash<std::string>{}(request_id);
        loop_->RemoveTimer(timer_id);

        if (callback) {
            callback(response);
        }
    }
}

void RpcClient::onError(int err) {
    LOG_ERROR("Client error occurred: err=%d", err);
    if (!stopped_) {
        DropAllPending(RpcErrorCode::CLIENT_NETWORK_ERROR, "tcp client io error, err=" + std::to_string(err));
    }
}

void RpcClient::DropAllPending(RpcErrorCode err_code, std::string err_msg) {
    std::unordered_map<std::string, ResponseCallback> tmp;
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        tmp.swap(pending_calls_);
    }

    LOG_WARN("Drop all pending rpc calls, count=%zu, code=%d msg=%s", tmp.size(), static_cast<int>(err_code),
             err_msg.c_str());

    for (auto &[req_id, cb] : tmp) {
        int timer_id = std::hash<std::string>{}(req_id);
        loop_->RemoveTimer(timer_id);

        RpcResponse resp;
        resp.set_request_id(req_id);
        resp.set_code(static_cast<int32_t>(err_code));
        resp.set_msg(err_msg);
        if (cb) {
            cb(resp);
        }
    }
}

std::string RpcClient::GenerateRequestId() {
    static std::mutex rng_mutex;
    static std::mt19937 gen([] {
        std::random_device rd;
        return rd();
    }());
    static std::uniform_int_distribution<uint64_t> dis;

    uint64_t random;
    {
        std::lock_guard<std::mutex> lock(rng_mutex);
        random = dis(gen);
    }

    auto now = std::chrono::system_clock::now();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << millis << std::setw(16) << random;
    return ss.str();
}

}  // namespace rpc
