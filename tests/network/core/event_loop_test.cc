#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "network/core/channel.h"
#include "network/core/event_loop.h"

namespace rpc {

class EventLoopTest : public ::testing::Test {
protected:
    void SetUp() override {}

    void TearDown() override {
        for (int fd : fds_) {
            close(fd);
        }
    }

    int createTestSocket() {
        int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (fd >= 0) {
            fds_.push_back(fd);
        }
        return fd;
    }

    std::vector<int> fds_;
};

TEST_F(EventLoopTest, CreateAndDestroy) {
    EventLoop loop;
    EXPECT_TRUE(true);
}

TEST_F(EventLoopTest, LoopAndQuit) {
    EventLoop loop;

    std::thread t([&loop]() { loop.Loop(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    loop.Quit();
    t.join();
}

TEST_F(EventLoopTest, IsInLoopThread) {
    EventLoop loop;

    std::atomic<bool> in_loop_thread = false;

    std::thread t([&loop, &in_loop_thread]() { loop.Loop(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    loop.RunInLoop([&in_loop_thread]() { in_loop_thread = true; });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    loop.Quit();
    t.join();

    EXPECT_TRUE(in_loop_thread);
}

TEST_F(EventLoopTest, RunInLoop) {
    EventLoop loop;

    std::atomic<int> counter = 0;

    std::thread t([&loop]() { loop.Loop(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    loop.RunInLoop([&counter]() { counter++; });
    loop.RunInLoop([&counter]() { counter++; });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    loop.Quit();
    t.join();

    EXPECT_EQ(counter, 2);
}

TEST_F(EventLoopTest, QueueInLoop) {
    EventLoop loop;

    std::atomic<int> counter = 0;

    std::thread t([&loop]() { loop.Loop(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    loop.QueueInLoop([&counter]() { counter++; });
    loop.QueueInLoop([&counter]() { counter++; });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    loop.Quit();
    t.join();

    EXPECT_EQ(counter, 2);
}

TEST_F(EventLoopTest, AddTimer) {
    EventLoop loop;

    std::atomic<bool> timer_called = false;

    std::thread t([&loop]() { loop.Loop(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    loop.AddTimer(
        1, [&timer_called]() { timer_called = true; }, 50);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    loop.Quit();
    t.join();

    EXPECT_TRUE(timer_called);
}

TEST_F(EventLoopTest, RemoveTimer) {
    EventLoop loop;

    std::atomic<bool> timer_called = false;

    std::thread t([&loop]() { loop.Loop(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    loop.AddTimer(
        1, [&timer_called]() { timer_called = true; }, 100);

    loop.RemoveTimer(1);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    loop.Quit();
    t.join();

    EXPECT_FALSE(timer_called);
}

TEST_F(EventLoopTest, AddAndRemoveChannel) {
    EventLoop loop;

    int fd = createTestSocket();
    ASSERT_NE(fd, -1);

    Channel channel(&loop, fd);
    channel.EnableReading();

    std::thread t([&loop]() { loop.Loop(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    loop.RemoveChannel(&channel);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    loop.Quit();
    t.join();
}

}  // namespace rpc

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}