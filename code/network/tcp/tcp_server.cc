#include "tcp_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace rpc {

TcpServer::TcpServer(EventLoop *loop, const std::string &ip, uint16_t port)
    : loop_(loop), ip_(ip), port_(port), listen_fd_(-1) {
    listen_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = htons(port);

    bind(listen_fd_, (sockaddr *)&addr, sizeof(addr));
}

TcpServer::~TcpServer() {
    if (!stopped_) {
        Stop();
    }

    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
    }
}

void TcpServer::Start() {
    listen(listen_fd_, 1024);

    accept_channel_ = std::make_unique<Channel>(loop_, listen_fd_);
    accept_channel_->SetReadCallback(std::bind(&TcpServer::HandleAccept, this));
    accept_channel_->EnableReading();
}

void TcpServer::Stop() {
    bool expected = false;
    if (stopped_.compare_exchange_strong(expected, true)) {
        if (loop_->IsInLoopThread()) {
            StopInLoop();
        } else {
            loop_->RunInLoop(std::bind(&TcpServer::StopInLoop, this));
        }
    }
}

void TcpServer::StopInLoop() {
    if (accept_channel_) {
        accept_channel_->DisableAll();
        accept_channel_.reset();
    }

    std::vector<std::shared_ptr<TcpConnection>> conns;
    conns.reserve(connections_.size());

    for (auto &pair : connections_) {
        conns.push_back(pair.second);
    }

    for (auto &conn : conns) {
        conn->close();
    }

    connections_.clear();

    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);
    }
}

void TcpServer::HandleAccept() {
    if (stopped_) {
        return;
    }

    sockaddr_in peer_addr;
    socklen_t len = sizeof(peer_addr);
    int conn_fd = accept4(listen_fd_, (sockaddr *)&peer_addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);

    if (conn_fd < 0) {
        return;
    }

    std::string peer_ip = inet_ntoa(peer_addr.sin_addr);
    uint16_t peer_port = ntohs(peer_addr.sin_port);

    auto conn = std::make_shared<TcpConnection>(loop_, conn_fd, peer_ip, peer_port);
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setCloseCallback(std::bind(&TcpServer::RemoveConnection, this, std::placeholders::_1));

    connections_[conn_fd] = conn;

    if (connectionCallback_) {
        connectionCallback_(conn);
    }
}

void TcpServer::RemoveConnection(const std::shared_ptr<TcpConnection> &conn) {
    if (stopped_) {
        return;
    }

    if (loop_->IsInLoopThread()) {
        if (!stopped_) {
            connections_.erase(conn->fd());
        }
    } else {
        loop_->RunInLoop([this, conn]() {
            if (!stopped_) {
                connections_.erase(conn->fd());
            }
        });
    }
}

}  // namespace rpc