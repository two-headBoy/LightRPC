#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include "network/core/event_loop.h"
#include "network/tcp/tcp_connection.h"

namespace rpc {

class TcpClient {
public:
    using ConnectionCallback = std::function<void(const std::shared_ptr<TcpConnection> &)>;
    using MessageCallback = std::function<void(const std::shared_ptr<TcpConnection> &, Buffer *)>;
    using ErrorCallback = std::function<void(int err)>;

    TcpClient(EventLoop *loop, const std::string &ip, uint16_t port);
    ~TcpClient();

    void Start();
    void Stop();
    void Connect();
    void Disconnect();

    void Send(const std::byte *data, size_t len);

    void SetConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void SetMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void SetErrorCallback(const ErrorCallback &cb) { errorCallback_ = cb; }

    bool IsConnected() const { return connected_.load(); }
    int GetFd() const { return sock_fd_; }

private:
    void HandleWrite();
    void HandleError();
    void ClientThreadFunc();
    void StopInLoop();
    void DisconnectInLoop();
    void SetConnected(bool connected);

    EventLoop *loop_;
    std::string ip_;
    uint16_t port_;
    int sock_fd_;

    std::shared_ptr<TcpConnection> connection_;
    std::unique_ptr<Channel> channel_;
    std::atomic<bool> connecting_{false};
    std::atomic<bool> stopped_{false};
    std::atomic<bool> connected_{false};

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    ErrorCallback errorCallback_;
};

}  // namespace rpc
