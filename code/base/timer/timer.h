#pragma once

#include <chrono>
#include <functional>
#include <list>
#include <unordered_map>
#include <vector>

namespace rpc {

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

struct TimerNode {
    int id;
    TimeoutCallBack cb;
    TimeStamp expire;
    bool isDeleted;
};

class TimeWheel {
public:
    TimeWheel(int tick_ms = 10);
    void AddTimer(int id, TimeoutCallBack cb, int delay_ms);
    void Tick();
    void RemoveTimer(int id);

private:
    static const int WHEEL_SIZE = 1024;
    std::vector<std::list<TimerNode>> wheel_;
    std::unordered_map<int, std::pair<int, std::list<TimerNode>::iterator>> timer_map_;
    int current_slot_;
    int tick_ms_;
    TimeStamp last_tick_time_;
};

}  // namespace rpc
