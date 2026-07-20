#pragma once

#include <sys/epoll.h>

#include <cassert>
#include <functional>
#include <memory>

#include "network/core/event_loop.h"

namespace rpc {

class EventLoop;

class Channel {
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop *loop, int fd);

    Channel(const Channel &) = delete;
    Channel &operator=(const Channel &) = delete;

    ~Channel() = default;

    void HandleEvent(uint32_t revents);

    void SetReadCallback(EventCallback cb) { read_callback_ = std::move(cb); }
    void SetWriteCallback(EventCallback cb) { write_callback_ = std::move(cb); }
    void SetCloseCallback(EventCallback cb) { close_callback_ = std::move(cb); }
    void SetErrorCallback(EventCallback cb) { error_callback_ = std::move(cb); }

    void EnableReading() {
        events_ |= EPOLLIN;
        Update();
    }
    void EnableWriting() {
        events_ |= EPOLLOUT;
        Update();
    }
    void DisableReading() {
        events_ &= ~EPOLLIN;
        Update();
    }
    void DisableWriting() {
        events_ &= ~EPOLLOUT;
        Update();
    }
    void DisableAll() {
        events_ = 0;
        Update();
    }

    int fd() const { return fd_; }
    uint32_t events() const { return events_; }
    bool is_none_event() const { return events_ == 0; }
    EventLoop *OwnerLoop() const { return loop_; }
    void Remove();

private:
    void Update();

    EventLoop *loop_;
    int fd_;
    uint32_t events_;  // 当前关注的事件

    EventCallback read_callback_;
    EventCallback write_callback_;
    EventCallback close_callback_;
    EventCallback error_callback_;
};

}  // namespace rpc
