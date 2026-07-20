#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "network/core/event_loop.h"
#include "network/tcp/tcp_connection.h"

namespace rpc {

class TcpServer {
public:
    using ConnectionCallback = std::function<void(const std::shared_ptr<TcpConnection> &)>;
    using MessageCallback = std::function<void(const std::shared_ptr<TcpConnection> &, Buffer *)>;

    TcpServer(EventLoop *loop, const std::string &ip, uint16_t port);
    ~TcpServer();

    void Start();
    void Stop();

    void SetConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void SetMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }

private:
    void HandleAccept();
    void RemoveConnection(const std::shared_ptr<TcpConnection> &conn);
    void StopInLoop();
    void ServerThreadFunc();

    EventLoop *loop_;
    std::string ip_;
    uint16_t port_;
    int listen_fd_;
    std::unique_ptr<Channel> accept_channel_;

    std::unordered_map<int, std::shared_ptr<TcpConnection>> connections_;

    std::atomic<bool> stopped_{false};

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
};

}  // namespace rpc
