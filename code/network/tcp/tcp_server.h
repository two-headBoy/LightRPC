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
    // 仅停止接受新连接，不动已有连接
    void StopAccepting();

    void SetConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void SetMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }

    // 在 loop 线程内对每个连接执行回调，用于广播 GOAWAY 等场景
    // 调用方必须保证在 loop 线程内调用（connections_ 无锁）
    template <typename F>
    void ForEachConnectionInLoop(F &&f) {
        for (auto &pair : connections_) {
            f(pair.second);
        }
    }

private:
    void HandleAccept();
    void RemoveConnection(const std::shared_ptr<TcpConnection> &conn);
    void StopInLoop();
    void StopAcceptingInLoop();
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
