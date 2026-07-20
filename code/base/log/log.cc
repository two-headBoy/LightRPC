#include "log.h"

namespace rpc {

Log::Log() {
    lineCount_ = 0;
    isAsync_ = false;
    writeThread_ = nullptr;
    queue_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}

Log::~Log() {
    if (writeThread_ && writeThread_->joinable()) {
        while (!queue_->empty()) {
            queue_->flush();
        };
        queue_->close();
        writeThread_->join();
    }
    if (fp_) {
        std::lock_guard<std::mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

int Log::GetLevel() {
    std::lock_guard<std::mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level) {
    std::lock_guard<std::mutex> locker(mtx_);
    level_ = level;
}

void Log::Init(const LogConfig &config) {
    Log *log = Instance();
    log->isOpen_ = true;
    log->level_ = static_cast<int>(config.level);

    if (config.max_queue_size > 0) {
        log->isAsync_ = true;
        if (!log->queue_) {
            log->queue_ = std::make_unique<AsyncQueue>(config.max_queue_size);
            log->writeThread_ = std::make_unique<std::thread>(FlushLogThread);
        }
    } else {
        log->isAsync_ = false;
    }

    log->lineCount_ = 0;

    time_t timer = time(nullptr);
    struct tm *sysTime = localtime(&timer);
    struct tm t = *sysTime;

    log->path_ = config.path;
    log->suffix_ = config.suffix;

    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", log->path_.c_str(), t.tm_year + 1900, t.tm_mon + 1,
             t.tm_mday, log->suffix_.c_str());
    log->toDay_ = t.tm_mday;

    {
        std::lock_guard<std::mutex> locker(log->mtx_);
        log->buff_.RetrieveAll();
        if (log->fp_) {
            log->flush();
            fclose(log->fp_);
        }

        log->fp_ = fopen(fileName, "a");
        if (log->fp_ == nullptr) {
            mkdir(log->path_.c_str(), 0777);
            log->fp_ = fopen(fileName, "a");
        }
        assert(log->fp_ != nullptr);
    }
}

void Log::init(int level, const char *path, const char *suffix, int maxQueueSize) {
    LogConfig config;
    config.level = static_cast<LogLevel>(level);
    config.path = path;
    config.suffix = suffix;
    config.max_queue_size = maxQueueSize;
    Init(config);
}

void Log::write(int level, const char *format, ...) {
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
    time_t tSec = std::chrono::system_clock::to_time_t(now);
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list vaList;

    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_ % MAX_LINES == 0))) {
        std::lock_guard<std::mutex> locker(mtx_);

        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (toDay_ != t.tm_mday) {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_.c_str(), tail, suffix_.c_str());
            toDay_ = t.tm_mday;
            lineCount_ = 0;
        } else {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_.c_str(), tail, (lineCount_ / MAX_LINES),
                     suffix_.c_str());
        }

        flush();
        fclose(fp_);
        fp_ = fopen(newFile, "a");
        assert(fp_ != nullptr);
    }

    {
        std::unique_lock<std::mutex> locker(mtx_);
        lineCount_++;
        int n = snprintf(reinterpret_cast<char *>(buff_.WriteBegin()), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                         t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec,
                         now_ms.count() % 1000000);

        buff_.HasWritten(n);
        AppendLogLevelTitle_(level);

        va_start(vaList, format);
        size_t writable = buff_.WritableBytes();
        int m = vsnprintf(reinterpret_cast<char *>(buff_.WriteBegin()), writable, format, vaList);
        va_end(vaList);

        if (m < 0) {
            m = 0;
        } else if (static_cast<size_t>(m) > writable) {
            m = static_cast<int>(writable);
        }

        buff_.HasWritten(m);
        buff_.Append("\n\0", 2);

        if (isAsync_ && queue_ && !queue_->full()) {
            queue_->push(buff_.RetrieveAllToStr());
        } else {
            fputs(reinterpret_cast<const char *>(buff_.Peek()), fp_);
        }
        buff_.RetrieveAll();
    }
}

void Log::AppendLogLevelTitle_(int level) {
    switch (level) {
        case 0:
            buff_.Append("[debug]: ", 9);
            break;
        case 1:
            buff_.Append("[info] : ", 9);
            break;
        case 2:
            buff_.Append("[warn] : ", 9);
            break;
        case 3:
            buff_.Append("[error]: ", 9);
            break;
        default:
            buff_.Append("[info] : ", 9);
            break;
    }
}

void Log::flush() {
    if (isAsync_) {
        queue_->flush();
    }
    fflush(fp_);
}

void Log::AsyncWrite_() {
    while (auto str = queue_->pop()) {
        std::lock_guard<std::mutex> locker(mtx_);
        fputs(str->c_str(), fp_);
    }
}

Log *Log::Instance() {
    static Log inst;
    return &inst;
}

void Log::FlushLogThread() { Log::Instance()->AsyncWrite_(); }

}  // namespace rpc
