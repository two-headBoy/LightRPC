#pragma once

#include <assert.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>

namespace rpc {

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8, size_t maxQueueSize = 1024)
        : pool_(std::make_shared<Pool>()), maxQueueSize_(maxQueueSize) {
        assert(threadCount > 0);
        pool_->workers.reserve(threadCount);
        for (size_t i = 0; i < threadCount; ++i) {
            pool_->workers.emplace_back([pool = pool_] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(pool->mtx);
                        pool->cond.wait(lock, [pool] { return pool->isClosed || !pool->tasks.empty(); });
                        if (pool->isClosed && pool->tasks.empty()) {
                            return;
                        }
                        task = std::move(pool->tasks.front());
                        pool->tasks.pop();
                        pool->activeTasks++;
                    }

                    try {
                        task();
                    } catch (const std::exception &e) {
                    } catch (...) {}

                    {
                        std::lock_guard<std::mutex> lock(pool->mtx);
                        pool->activeTasks--;
                        if (pool->activeTasks == 0 && pool->tasks.empty()) {
                            pool->condEmpty.notify_all();
                        }
                    }
                }
            });
        }
    }

    ~ThreadPool() {
        if (pool_) {
            {
                std::lock_guard<std::mutex> lock(pool_->mtx);
                pool_->isClosed = true;
            }

            pool_->cond.notify_all();

            for (auto &t : pool_->workers) {
                if (t.joinable()) t.join();
            }
        }
    }

    template <class F>
    void AddTask(F &&task) {
        {
            std::lock_guard<std::mutex> lock(pool_->mtx);
            if (pool_->isClosed) throw std::runtime_error("pool is closed");
            if (pool_->tasks.size() >= maxQueueSize_) throw std::runtime_error("queue full");
            pool_->tasks.emplace(std::forward<F>(task));
        }
        pool_->cond.notify_one();
    }

    template <class F>
    bool TryAddTask(F &&task) {
        {
            std::lock_guard<std::mutex> lock(pool_->mtx);
            if (pool_->isClosed) return false;
            if (pool_->tasks.size() >= maxQueueSize_) return false;
            pool_->tasks.emplace(std::forward<F>(task));
        }
        pool_->cond.notify_one();
        return true;
    }

    void WaitForTasks() {
        std::unique_lock<std::mutex> lock(pool_->mtx);
        pool_->condEmpty.wait(lock, [this] { return pool_->tasks.empty() && pool_->activeTasks == 0; });
    }

private:
    struct Pool {
        std::mutex mtx;
        std::condition_variable cond;
        std::condition_variable condEmpty;
        std::atomic<bool> isClosed{false};
        std::queue<std::function<void()>> tasks;
        std::vector<std::thread> workers;
        size_t activeTasks{0};
    };
    std::shared_ptr<Pool> pool_;
    size_t maxQueueSize_;
};

}  // namespace rpc