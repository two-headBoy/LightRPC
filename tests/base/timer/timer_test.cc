#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "base/timer/timer.h"

namespace rpc {

class TimerTest : public ::testing::Test {
protected:
    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(TimerTest, CreateAndDestroy) {
    TimeWheel timer;
    EXPECT_TRUE(true);
}

TEST_F(TimerTest, AddTimer) {
    TimeWheel timer(10);

    std::atomic<bool> callback_called = false;

    timer.AddTimer(
        1, [&callback_called]() { callback_called = true; }, 50);

    for (int i = 0; i < 10; ++i) {
        timer.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(callback_called);
}

TEST_F(TimerTest, AddMultipleTimers) {
    TimeWheel timer(10);

    std::atomic<int> counter = 0;

    timer.AddTimer(
        1, [&counter]() { counter++; }, 30);
    timer.AddTimer(
        2, [&counter]() { counter++; }, 50);
    timer.AddTimer(
        3, [&counter]() { counter++; }, 70);

    for (int i = 0; i < 15; ++i) {
        timer.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(counter, 3);
}

TEST_F(TimerTest, RemoveTimer) {
    TimeWheel timer(10);

    std::atomic<bool> callback_called = false;

    timer.AddTimer(
        1, [&callback_called]() { callback_called = true; }, 100);

    timer.RemoveTimer(1);

    for (int i = 0; i < 20; ++i) {
        timer.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_FALSE(callback_called);
}

TEST_F(TimerTest, RemoveNonExistentTimer) {
    TimeWheel timer(10);

    timer.RemoveTimer(999);

    EXPECT_TRUE(true);
}

TEST_F(TimerTest, TimerWithDifferentDelay) {
    TimeWheel timer(10);

    std::atomic<int> order = 0;
    std::atomic<int> first_timer = 0;
    std::atomic<int> second_timer = 0;

    timer.AddTimer(
        1, [&order, &first_timer]() { first_timer = ++order; }, 50);

    timer.AddTimer(
        2, [&order, &second_timer]() { second_timer = ++order; }, 30);

    for (int i = 0; i < 15; ++i) {
        timer.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(first_timer, 2);
    EXPECT_EQ(second_timer, 1);
}

}  // namespace rpc

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}