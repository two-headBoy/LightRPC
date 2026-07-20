#include "epoller.h"

#include <cassert>

#include "network/core/channel.h"

namespace rpc {

Epoller::Epoller(int maxEventCount) : epollFd_(epoll_create1(EPOLL_CLOEXEC)), events_(maxEventCount) {
    assert(epollFd_ >= 0 && events_.size() > 0);
}

Epoller::~Epoller() {
    if (epollFd_ >= 0) {
        close(epollFd_);
    }
}

bool Epoller::AddFd(int fd, uint32_t events, Channel *channel) {
    if (fd < 0) {
        return false;
    }
    epoll_event ev;
    ev.events = events | EPOLLRDHUP;
    ev.data.fd = fd;
    ev.data.ptr = channel;
    int ret = epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
    return ret == 0;
}

bool Epoller::ModFd(int fd, uint32_t events, Channel *channel) {
    if (fd < 0) {
        return false;
    }
    epoll_event ev;
    ev.events = events | EPOLLRDHUP;
    ev.data.fd = fd;
    ev.data.ptr = channel;
    int ret = epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
    if (ret != 0 && errno == ENOENT) {
        ret = epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
    }
    return ret == 0;
}

bool Epoller::DelFd(int fd) {
    if (fd < 0) {
        return false;
    }
    return epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr) == 0;  // 使用nullptr
}

int Epoller::Wait(int timeoutMs) {
    return epoll_wait(epollFd_, events_.data(), static_cast<int>(events_.size()), timeoutMs);
}

int Epoller::GetEventFd(size_t i) const {
    assert(i < events_.size());
    return events_[i].data.fd;
}

uint32_t Epoller::GetEvents(size_t i) const {
    assert(i < events_.size());
    return events_[i].events;
}

Channel *Epoller::GetChannel(size_t i) const {
    assert(i < events_.size());
    return static_cast<Channel *>(events_[i].data.ptr);
}

bool Epoller::SetNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

}  // namespace rpc