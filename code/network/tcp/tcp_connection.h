#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "base/buffer/buffer.h"
#include "network/core/event_loop.h"

namespace rpc {

class TcpConnection;

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    enum State { Disconnected, Connected };

    using ConnectionCallback = std::function<void(const std::shared_ptr<TcpConnection> &)>;
    using MessageCallback = std::function<void(const std::shared_ptr<TcpConnection> &, Buffer *)>;
    using CloseCallback = std::function<void(const std::shared_ptr<TcpConnection> &)>;

    TcpConnection(EventLoop *loop, int fd, const std::string &peer_ip, uint16_t peer_port);
    ~TcpConnection();

    TcpConnection(const TcpConnection &) = delete;
    TcpConnection &operator=(const TcpConnection &) = delete;

    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }

    void send(const std::byte *data, size_t len);
    void sendInLoop(const std::byte *data, size_t len);
    void close();

    int fd() const { return sockfd_; }
    State state() const { return state_; }
    const std::string &peer_ip() const { return peer_ip_; }
    uint16_t peer_port() const { return peer_port_; }

private:
    void handleRead();
    void handleWrite();
    void handleClose();

    std::unique_ptr<Buffer> input_buffer_;
    std::unique_ptr<Buffer> output_buffer_;

    EventLoop *loop_;
    int sockfd_;
    State state_;

    std::unique_ptr<Channel> channel_;

    std::string peer_ip_;
    uint16_t peer_port_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    CloseCallback closeCallback_;
};

}  // namespace rpc
