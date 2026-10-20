#include "rpc/rpc_server.h"

#include <string>

#include "rpc/rpc_service.h"

namespace rpc {

RpcServer::RpcServer(std::string ip, uint16_t port, int threadNum, int maxQueueSize)
    : ip_(ip), port_(port), loop_(std::make_unique<EventLoop>()) {
    pool_ = std::make_unique<ThreadPool>(threadNum, maxQueueSize);
    tcp_server_ = std::make_unique<TcpServer>(loop_.get(), ip_, port_);
    service_ = std::make_unique<RpcService>();

    tcp_server_->SetConnectionCallback(std::bind(&RpcServer::onConnection, this, std::placeholders::_1));

    tcp_server_->SetMessageCallback(
        std::bind(&RpcServer::onMessage, this, std::placeholders::_1, std::placeholders::_2));
}

RpcServer::~RpcServer() { Stop(); }

void RpcServer::Start() {
    if (stopped_) {
        return;
    }

    LOG_INFO("RpcServer starting on %s:%d", ip_.c_str(), port_);
    loop_thread_ = std::thread([this]() { loop_->Loop(); });

    tcp_server_->Start();

    LOG_INFO("RpcServer started successfully");
}

void RpcServer::Stop() {
    bool expected = false;
    if (stopped_.compare_exchange_strong(expected, true)) {
        LOG_INFO("RpcServer stopping");
        // 关闭顺序很关键：
        // 1. 先停 listener，新连接不再进来（同步生效）
        tcp_server_->StopAccepting();
        // 2. 在 loop 线程中广播 GOAWAY 给所有已建立的连接
        //    告诉客户端别再发新请求，发了的会被 onMessage 闸门回 SERVER_SHUTTING_DOWN
        loop_->RunInLoop([this]() {
            tcp_server_->ForEachConnectionInLoop(
                [](const std::shared_ptr<TcpConnection> &conn) {
                    RpcResponse resp;
                    resp.set_code(static_cast<int32_t>(RpcErrorCode::SERVER_SHUTTING_DOWN));
                    resp.set_msg("server shutting down");
                    auto encoded = Codec::encode(resp);
                    if (encoded.has_value()) {
                        conn->send(reinterpret_cast<const std::byte *>(encoded->data()), encoded->size());
                    }
                });
        });
        // 3. 等已投递任务跑完，期间响应仍能通过 conn->send 发出去
        pool_->WaitForTasks();
        // 4. 关闭所有已建立的连接（FIN 通知客户端）
        tcp_server_->Stop();
        // 5. 停事件循环 + 析构线程池
        loop_->Quit();
        if (loop_thread_.joinable()) {
            loop_thread_.join();
        }
        pool_.reset();
        LOG_INFO("RpcServer stopped");
    }
}

void RpcServer::RegisterRaw(const std::string &service_name, const std::string &method_name, ServiceCallback callback) {
    service_->RegisterMethod(service_name, method_name, std::move(callback));
    LOG_DEBUG("Registered service: %s, method: %s", service_name.c_str(), method_name.c_str());
}

void RpcServer::onConnection(const std::shared_ptr<TcpConnection> &conn) {
    if (conn->state() == TcpConnection::State::Connected) {
        LOG_INFO("New connection, fd=%d", conn->fd());
    } else {
        LOG_INFO("Connection closed, fd=%d", conn->fd());
    }
}

void RpcServer::onMessage(const std::shared_ptr<TcpConnection> &conn, Buffer *buffer) {
    while (buffer->ReadableBytes() >= 4) {
        RpcRequest request;
        size_t consumed = 0;
        auto status = Codec::decode(buffer->PeekStringView(), request, consumed);
        if (status == Codec::DecodeStatus::Incomplete) {
            break;
        }
        if (status == Codec::DecodeStatus::Corrupted) {
            LOG_WARN("Corrupted frame from peer %s:%d, closing connection fd=%d", conn->peer_ip().c_str(),
                     conn->peer_port(), conn->fd());
            conn->close();
            return;
        }

        buffer->Retrieve(consumed);

        LOG_DEBUG("Received request: id=%s, service=%s, method=%s", request.request_id().c_str(),
                  request.service_name().c_str(), request.method_name().c_str());

        // 闸门：服务端正在关闭，直接同步拒绝，不再投递到线程池
        // 避免 WaitForTasks 期间新请求涌入导致永远等不完
        if (stopped_.load()) {
            LOG_INFO("Reject request while shutting down: id=%s", request.request_id().c_str());
            RpcResponse response;
            response.set_request_id(request.request_id());
            response.set_code(static_cast<int32_t>(RpcErrorCode::SERVER_SHUTTING_DOWN));
            response.set_msg("server shutting down");
            auto encoded = Codec::encode(response);
            if (encoded.has_value()) {
                conn->send(reinterpret_cast<const std::byte *>(encoded->data()), encoded->size());
            }
            continue;
        }

        std::weak_ptr<TcpConnection> weak_conn = conn;
        if (!pool_->TryAddTask([this, weak_conn, request]() {
                auto conn = weak_conn.lock();
                if (!conn) {
                    return;
                }

                RpcResponse response;

                service_->Invoke(request, response);

                LOG_DEBUG("Sending response: id=%s, code=%d", response.request_id().c_str(), response.code());

                auto encoded = Codec::encode(response);
                if (encoded.has_value()) {
                    conn->send(reinterpret_cast<const std::byte *>(encoded->data()), encoded->size());
                }
            })) {
            LOG_WARN("ThreadPool queue full, drop request: id=%s, service=%s, method=%s", request.request_id().c_str(),
                     request.service_name().c_str(), request.method_name().c_str());
            RpcResponse response;
            response.set_request_id(request.request_id());
            response.set_code(static_cast<int32_t>(RpcErrorCode::SERVER_BUSY));
            response.set_msg("Server busy, try again later");
            auto encoded = Codec::encode(response);
            if (encoded.has_value()) {
                conn->send(reinterpret_cast<const std::byte *>(encoded->data()), encoded->size());
            }
        }
    }
}

}  // namespace rpc
