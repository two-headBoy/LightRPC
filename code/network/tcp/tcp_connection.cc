#include "tcp_connection.h"

#include <unistd.h>

namespace rpc {

TcpConnection::TcpConnection(EventLoop *loop, int fd, const std::string &peer_ip, uint16_t peer_port)
    : input_buffer_(std::make_unique<Buffer>()), output_buffer_(std::make_unique<Buffer>()), loop_(loop), sockfd_(fd),
      state_(Connected), channel_(std::make_unique<Channel>(loop, fd)), peer_ip_(peer_ip), peer_port_(peer_port) {
    channel_->SetReadCallback(std::bind(&TcpConnection::handleRead, this));
    channel_->SetWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->SetCloseCallback(std::bind(&TcpConnection::handleClose, this));

    channel_->EnableReading();
}

TcpConnection::~TcpConnection() {
    if (sockfd_ >= 0) {
        ::close(sockfd_);
    }
}

void TcpConnection::send(const std::byte *data, size_t len) {
    if (state_ != Connected) {
        return;
    }

    if (loop_->IsInLoopThread()) {
        sendInLoop(data, len);
    } else {
        std::string copy(reinterpret_cast<const char *>(data), len);
        auto self = shared_from_this();
        loop_->RunInLoop([self, copy]() {
            if (self->state_ != Connected) {
                return;
            }
            self->sendInLoop(reinterpret_cast<const std::byte *>(copy.data()), copy.size());
        });
    }
}

void TcpConnection::sendInLoop(const std::byte *data, size_t len) {
    if (state_ != Connected) {
        return;
    }

    if (output_buffer_->ReadableBytes() == 0) {
        ssize_t n = ::write(sockfd_, data, len);
        if (n >= 0) {
            if (static_cast<size_t>(n) < len) {
                output_buffer_->Append(data + n, len - n);
                channel_->EnableWriting();
            }
            return;
        }
        output_buffer_->Append(data, len);
        channel_->EnableWriting();
        return;
    }

    output_buffer_->Append(data, len);
    channel_->EnableWriting();
}

void TcpConnection::close() {
    if (loop_->IsInLoopThread()) {
        handleClose();
    } else {
        auto self = shared_from_this();
        loop_->RunInLoop([self]() { self->handleClose(); });
    }
}

void TcpConnection::handleRead() {
    if (state_ != Connected) {
        return;
    }

    int saved_errno = 0;
    ssize_t n = input_buffer_->ReadFd(sockfd_, &saved_errno);

    if (n > 0) {
        if (messageCallback_) {
            messageCallback_(shared_from_this(), input_buffer_.get());
        }
    } else if (n == 0) {
        handleClose();
    } else {
        if (saved_errno != EAGAIN && saved_errno != EWOULDBLOCK) {
            handleClose();
        }
    }
}

void TcpConnection::handleWrite() {
    if (state_ != Connected) {
        return;
    }

    int saved_errno = 0;
    ssize_t n = output_buffer_->WriteFd(sockfd_, &saved_errno);

    if (n > 0) {
        output_buffer_->Retrieve(n);
    }

    if (output_buffer_->ReadableBytes() == 0) {
        channel_->DisableWriting();
    }
}

void TcpConnection::handleClose() {
    if (state_ == Disconnected) {
        return;
    }

    state_ = Disconnected;

    channel_->DisableAll();
    channel_->Remove();

    if (connectionCallback_) {
        connectionCallback_(shared_from_this());
    }

    if (closeCallback_) {
        closeCallback_(shared_from_this());
    }
}

}  // namespace rpc
