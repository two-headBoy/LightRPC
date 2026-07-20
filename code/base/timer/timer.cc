#include "timer.h"

namespace rpc {

TimeWheel::TimeWheel(int tick_ms)
    : wheel_(WHEEL_SIZE), current_slot_(0), tick_ms_(tick_ms), last_tick_time_(Clock::now()) {}

void TimeWheel::AddTimer(int id, TimeoutCallBack cb, int delay_ms) {
    if (delay_ms < 0) {
        return;
    }
    if (timer_map_.count(id)) {
        return;
    }

    TimeStamp expire = Clock::now() + MS(delay_ms);
    int total_slots = delay_ms / tick_ms_;
    int target_slot = (current_slot_ + total_slots) % WHEEL_SIZE;

    TimerNode node{id, std::move(cb), expire, false};
    auto it = wheel_[target_slot].insert(wheel_[target_slot].end(), std::move(node));
    timer_map_[id] = {target_slot, it};
}

void TimeWheel::Tick() {
    auto now = Clock::now();
    int elapsed_ms = std::chrono::duration_cast<MS>(now - last_tick_time_).count();
    int ticks_to_advance = elapsed_ms / tick_ms_;

    if (ticks_to_advance == 0) {
        return;
    }

    for (int t = 0; t < ticks_to_advance; ++t) {
        auto &current_list = wheel_[current_slot_];

        for (auto it = current_list.begin(); it != current_list.end();) {
            if (it->isDeleted) {
                timer_map_.erase(it->id);
                it = current_list.erase(it);
            } else if (Clock::now() >= it->expire) {
                it->cb();
                timer_map_.erase(it->id);
                it = current_list.erase(it);
            } else {
                ++it;
            }
        }

        current_slot_ = (current_slot_ + 1) % WHEEL_SIZE;
    }

    last_tick_time_ = now;
}

void TimeWheel::RemoveTimer(int id) {
    auto it = timer_map_.find(id);
    if (it != timer_map_.end()) {
        auto &list_it = it->second.second;
        list_it->isDeleted = true;
        timer_map_.erase(it);
    }
}

}  // namespace rpc
