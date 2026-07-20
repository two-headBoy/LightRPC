#include "channel.h"

namespace rpc {

Channel::Channel(EventLoop *loop, int fd) : loop_(loop), fd_(fd), events_(0) { assert(fd >= 0); }

void Channel::HandleEvent(uint32_t revents) {
    if (events_ == 0) {
        return;
    }

    if ((revents & EPOLLERR) || (revents & EPOLLHUP)) {
        if (error_callback_) {
            error_callback_();
        }
        return;
    }

    if (revents & EPOLLRDHUP) {
        if (close_callback_) {
            close_callback_();
        }
        return;
    }

    if (revents & EPOLLIN) {
        if (read_callback_) {
            read_callback_();
        }
    }

    if (revents & EPOLLOUT) {
        if (write_callback_) {
            write_callback_();
        }
    }
}

void Channel::Update() { loop_->UpdateChannel(this); }

void Channel::Remove() {
    read_callback_ = nullptr;
    write_callback_ = nullptr;
    close_callback_ = nullptr;
    error_callback_ = nullptr;
    loop_->RemoveChannel(this);
}

}  // namespace rpc
