#include "event_loop.h"

#include <unistd.h>

namespace rpc {

EventLoop::EventLoop()
    : epoller_(std::make_unique<Epoller>()), timer_(std::make_unique<TimeWheel>(10)), quit_(false),
      wakeup_fd_(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {
    Epoller::SetNonBlocking(wakeup_fd_);
    epoller_->AddFd(wakeup_fd_, EPOLLIN | EPOLLRDHUP, nullptr);
}

EventLoop::~EventLoop() {
    Quit();
    epoller_->DelFd(wakeup_fd_);
    close(wakeup_fd_);
}

void EventLoop::Loop() {
    quit_ = false;
    thread_id_ = std::this_thread::get_id();

    while (!quit_) {
        int n = epoller_->Wait(100);

        for (int i = 0; i < n; ++i) {
            int fd = epoller_->GetEventFd(i);
            if (fd == wakeup_fd_) {
                uint64_t one;
                ssize_t __attribute__((unused)) ret = read(wakeup_fd_, &one, sizeof(one));
                (void)ret;
            }
            Channel *channel = epoller_->GetChannel(i);
            if (channel && !channel->is_none_event()) {
                channel->HandleEvent(epoller_->GetEvents(i));
            }
        }

        timer_->Tick();
        HandlePendingFunctors();
    }
}

void EventLoop::AddChannel(Channel *channel) {
    assert(IsInLoopThread());
    int fd = channel->fd();
    Epoller::SetNonBlocking(fd);
    epoller_->AddFd(fd, channel->events(), channel);
}

void EventLoop::RemoveChannel(Channel *channel) {
    assert(IsInLoopThread());
    int fd = channel->fd();
    epoller_->DelFd(fd);
}

void EventLoop::UpdateChannel(Channel *channel) {
    assert(IsInLoopThread());
    epoller_->ModFd(channel->fd(), channel->events(), channel);
}

void EventLoop::RunInLoop(std::function<void()> cb) {
    if (IsInLoopThread()) {
        cb();
    } else {
        QueueInLoop(std::move(cb));
    }
}

void EventLoop::QueueInLoop(std::function<void()> cb) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_functors_.push(std::move(cb));
    }
    Wakeup();
}

void EventLoop::Wakeup() {
    uint64_t one = 1;
    ssize_t __attribute__((unused)) ret = write(wakeup_fd_, &one, sizeof(one));
    (void)ret;
}

void EventLoop::HandlePendingFunctors() {
    std::queue<std::function<void()>> functors;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pending_functors_);
    }

    while (!functors.empty()) {
        functors.front()();
        functors.pop();
    }
}

bool EventLoop::IsInLoopThread() const { return std::this_thread::get_id() == thread_id_; }

void EventLoop::Quit() {
    quit_ = true;
    Wakeup();
}

void EventLoop::AddTimer(int id, std::function<void()> cb, int delay_ms) {
    RunInLoop([this, id, cb = std::move(cb), delay_ms]() { timer_->AddTimer(id, std::move(cb), delay_ms); });
}

void EventLoop::RemoveTimer(int id) {
    RunInLoop([this, id]() { timer_->RemoveTimer(id); });
}

}  // namespace rpc
