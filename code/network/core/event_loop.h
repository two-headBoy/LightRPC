#pragma once

#include <sys/eventfd.h>

#include <atomic>
#include <cassert>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include "base/timer/timer.h"
#include "network/core/channel.h"
#include "network/epoll/epoller.h"

namespace rpc {

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop &) = delete;
    EventLoop &operator=(const EventLoop &) = delete;

    void Loop();
    void Quit();

    void AddChannel(Channel *channel);
    void RemoveChannel(Channel *channel);
    void UpdateChannel(Channel *channel);

    bool IsInLoopThread() const;

    void RunInLoop(std::function<void()> cb);
    void QueueInLoop(std::function<void()> cb);

    void AddTimer(int id, std::function<void()> cb, int delay_ms);
    void RemoveTimer(int id);

private:
    void Wakeup();
    void HandlePendingFunctors();

    std::unique_ptr<Epoller> epoller_;
    std::unique_ptr<TimeWheel> timer_;

    std::atomic<bool> quit_ = false;
    std::thread::id thread_id_;

    int wakeup_fd_;

    mutable std::mutex mutex_;
    std::queue<std::function<void()>> pending_functors_;
};
}  // namespace rpc
