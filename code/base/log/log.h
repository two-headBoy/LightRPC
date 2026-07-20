#pragma once

#include <assert.h>
#include <stdarg.h>
#include <sys/stat.h>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "../buffer/buffer.h"

namespace rpc {

enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

struct LogConfig {
    LogLevel level = LogLevel::INFO;
    std::string path = "./log";
    std::string suffix = ".log";
    int max_queue_size = 1024;
};

class Log {
public:
    static void Init(const LogConfig &config);

    [[deprecated("Use Log::Init(const LogConfig&) instead")]] void
    init(int level, const char *path = "./log", const char *suffix = ".log", int maxQueueCapacity = 1024);

    static Log *Instance();
    static void FlushLogThread();

    void write(int level, const char *format, ...);
    void flush();

    int GetLevel();
    void SetLevel(int level);
    bool IsOpen() { return isOpen_; }

private:
    Log();
    void AppendLogLevelTitle_(int level);
    virtual ~Log();
    void AsyncWrite_();

private:
    class AsyncQueue {
    public:
        explicit AsyncQueue(size_t capacity) : capacity_(capacity), closed_(false) {}

        bool push(const std::string &item) {
            std::unique_lock<std::mutex> lock(mutex_);
            if (closed_) return false;
            while (queue_.size() >= capacity_) {
                cond_producer_.wait(lock);
            }
            queue_.push_back(item);
            cond_consumer_.notify_one();
            return true;
        }

        std::optional<std::string> pop() {
            std::unique_lock<std::mutex> lock(mutex_);
            while (queue_.empty()) {
                cond_consumer_.wait(lock);
                if (closed_) return std::nullopt;
            }
            auto item = std::move(queue_.front());
            queue_.pop_front();
            cond_producer_.notify_one();
            return item;
        }

        void close() {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                queue_.clear();
                closed_ = true;
            }
            cond_producer_.notify_all();
            cond_consumer_.notify_all();
        }

        bool empty() {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.empty();
        }

        bool full() {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.size() >= capacity_;
        }

        void flush() { cond_consumer_.notify_one(); }

    private:
        std::deque<std::string> queue_;
        size_t capacity_;
        std::mutex mutex_;
        std::condition_variable cond_producer_;
        std::condition_variable cond_consumer_;
        bool closed_;
    };

private:
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;

    std::string path_;
    std::string suffix_;

    int lineCount_;
    int toDay_;

    bool isOpen_;

    Buffer buff_;
    int level_;
    bool isAsync_;

    FILE *fp_;
    std::unique_ptr<AsyncQueue> queue_;
    std::unique_ptr<std::thread> writeThread_;
    std::mutex mtx_;
};

}  // namespace rpc

#define LOG_BASE(level, format, ...)                                                                                   \
    do {                                                                                                               \
        Log *log = Log::Instance();                                                                                    \
        if (log->IsOpen() && log->GetLevel() <= level) {                                                               \
            log->write(level, format, ##__VA_ARGS__);                                                                  \
        }                                                                                                              \
    } while (0);

#define LOG_DEBUG(format, ...)                                                                                         \
    do {                                                                                                               \
        LOG_BASE(0, format, ##__VA_ARGS__)                                                                             \
    } while (0);
#define LOG_INFO(format, ...)                                                                                          \
    do {                                                                                                               \
        LOG_BASE(1, format, ##__VA_ARGS__)                                                                             \
    } while (0);
#define LOG_WARN(format, ...)                                                                                          \
    do {                                                                                                               \
        LOG_BASE(2, format, ##__VA_ARGS__)                                                                             \
    } while (0);
#define LOG_ERROR(format, ...)                                                                                         \
    do {                                                                                                               \
        LOG_BASE(3, format, ##__VA_ARGS__)                                                                             \
    } while (0);