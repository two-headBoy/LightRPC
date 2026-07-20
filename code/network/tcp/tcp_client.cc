#include "tcp_client.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace rpc {

TcpClient::TcpClient(EventLoop *loop, const std::string &ip, uint16_t port)
    : loop_(loop), ip_(ip), port_(port), sock_fd_(-1), connecting_(false), stopped_(false) {}

TcpClient::~TcpClient() {
    if (!stopped_) {
        Stop();
    }
}

void TcpClient::Start() {}

void TcpClient::Stop() {
    bool expected = false;
    if (stopped_.compare_exchange_strong(expected, true)) {
        if (loop_->IsInLoopThread()) {
            StopInLoop();
        } else {
            loop_->RunInLoop(std::bind(&TcpClient::StopInLoop, this));
        }
    }
}

void TcpClient::StopInLoop() { Disconnect(); }

void TcpClient::Connect() {
    if (sock_fd_ != -1 || connecting_) {
        return;
    }

    connecting_ = true;

    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd == -1) {
        connecting_ = false;
        if (errorCallback_) {
            errorCallback_(errno);
        }
        return;
    }

    sock_fd_ = fd;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip_.c_str());
    addr.sin_port = htons(port_);

    int ret = connect(fd, (sockaddr *)&addr, sizeof(addr));

    if (ret == 0) {
        connecting_ = false;
        loop_->RunInLoop([this, fd]() {
            connection_ = std::make_shared<TcpConnection>(loop_, fd, ip_, port_);
            connection_->setConnectionCallback(connectionCallback_);
            connection_->setMessageCallback(messageCallback_);
            SetConnected(true);

            if (connectionCallback_) {
                connectionCallback_(connection_);
            }
        });
    } else if (errno == EINPROGRESS) {
        loop_->RunInLoop([this, fd]() {
            channel_ = std::make_unique<Channel>(loop_, fd);
            channel_->SetWriteCallback(std::bind(&TcpClient::HandleWrite, this));
            channel_->SetErrorCallback(std::bind(&TcpClient::HandleError, this));
            channel_->EnableWriting();
        });
    } else {
        connecting_ = false;
        ::close(fd);
        sock_fd_ = -1;

        if (errorCallback_) {
            errorCallback_(errno);
        }
    }
}

void TcpClient::Disconnect() {
    if (loop_->IsInLoopThread()) {
        DisconnectInLoop();
    } else {
        loop_->RunInLoop(std::bind(&TcpClient::DisconnectInLoop, this));
    }
}

void TcpClient::DisconnectInLoop() {
    SetConnected(false);

    if (channel_) {
        channel_->DisableAll();
        channel_.reset();
    }

    if (connection_) {
        connection_->close();
        connection_.reset();
    }

    if (sock_fd_ >= 0) {
        ::close(sock_fd_);
        sock_fd_ = -1;
    }

    connecting_ = false;
}

void TcpClient::HandleWrite() {
    connecting_ = false;

    channel_->DisableWriting();

    int err = 0;
    socklen_t len = sizeof(err);
    getsockopt(sock_fd_, SOL_SOCKET, SO_ERROR, &err, &len);

    if (err == 0) {
        connection_ = std::make_shared<TcpConnection>(loop_, sock_fd_, ip_, port_);
        connection_->setConnectionCallback(connectionCallback_);
        connection_->setMessageCallback(messageCallback_);
        SetConnected(true);

        if (connectionCallback_) {
            connectionCallback_(connection_);
        }
    } else {
        ::close(sock_fd_);
        sock_fd_ = -1;

        if (errorCallback_) {
            errorCallback_(err);
        }
    }

    channel_.reset();
}

void TcpClient::HandleError() {
    connecting_ = false;

    if (channel_) {
        channel_->DisableAll();
    }

    if (sock_fd_ >= 0) {
        int err = 0;
        socklen_t len = sizeof(err);
        getsockopt(sock_fd_, SOL_SOCKET, SO_ERROR, &err, &len);

        ::close(sock_fd_);
        sock_fd_ = -1;

        if (errorCallback_) {
            errorCallback_(err);
        }
    }

    channel_.reset();
}

void TcpClient::Send(const std::byte *data, size_t len) {
    if (connection_) {
        connection_->send(data, len);
    }
}

void TcpClient::ClientThreadFunc() { loop_->Loop(); }

void TcpClient::SetConnected(bool connected) { connected_.store(connected); }

}  // namespace rpc