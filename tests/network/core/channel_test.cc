#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>

#include "network/core/channel.h"
#include "network/core/event_loop.h"

namespace rpc {

class ChannelTest : public ::testing::Test {
protected:
    void SetUp() override { loop_ = std::make_unique<EventLoop>(); }

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

    std::unique_ptr<EventLoop> loop_;
    std::vector<int> fds_;
};

TEST_F(ChannelTest, CreateAndDestroy) {
    int fd = createTestSocket();
    ASSERT_NE(fd, -1);

    Channel channel(loop_.get(), fd);
    EXPECT_EQ(channel.fd(), fd);
    EXPECT_EQ(channel.events(), 0);
    EXPECT_TRUE(channel.is_none_event());
}

TEST_F(ChannelTest, EnableReading) {
    int fd = createTestSocket();
    ASSERT_NE(fd, -1);

    Channel channel(loop_.get(), fd);
    channel.EnableReading();

    EXPECT_EQ(channel.events(), EPOLLIN);
    EXPECT_FALSE(channel.is_none_event());
}

TEST_F(ChannelTest, EnableWriting) {
    int fd = createTestSocket();
    ASSERT_NE(fd, -1);

    Channel channel(loop_.get(), fd);
    channel.EnableWriting();

    EXPECT_EQ(channel.events(), EPOLLOUT);
    EXPECT_FALSE(channel.is_none_event());
}

TEST_F(ChannelTest, EnableBoth) {
    int fd = createTestSocket();
    ASSERT_NE(fd, -1);

    Channel channel(loop_.get(), fd);
    channel.EnableReading();
    channel.EnableWriting();

    EXPECT_EQ(channel.events(), EPOLLIN | EPOLLOUT);
}

TEST_F(ChannelTest, DisableReading) {
    int fd = createTestSocket();
    ASSERT_NE(fd, -1);

    Channel channel(loop_.get(), fd);
    channel.EnableReading();
    channel.DisableReading();

    EXPECT_EQ(channel.events(), 0);
    EXPECT_TRUE(channel.is_none_event());
}

TEST_F(ChannelTest, DisableWriting) {
    int fd = createTestSocket();
    ASSERT_NE(fd, -1);

    Channel channel(loop_.get(), fd);
    channel.EnableWriting();
    channel.DisableWriting();

    EXPECT_EQ(channel.events(), 0);
    EXPECT_TRUE(channel.is_none_event());
}

TEST_F(ChannelTest, DisableAll) {
    int fd = createTestSocket();
    ASSERT_NE(fd, -1);

    Channel channel(loop_.get(), fd);
    channel.EnableReading();
    channel.EnableWriting();
    channel.DisableAll();

    EXPECT_EQ(channel.events(), 0);
    EXPECT_TRUE(channel.is_none_event());
}

TEST_F(ChannelTest, OwnerLoop) {
    int fd = createTestSocket();
    ASSERT_NE(fd, -1);

    Channel channel(loop_.get(), fd);
    EXPECT_EQ(channel.OwnerLoop(), loop_.get());
}

TEST_F(ChannelTest, SetCallbacks) {
    int fd = createTestSocket();
    ASSERT_NE(fd, -1);

    Channel channel(loop_.get(), fd);
    channel.EnableReading();
    channel.EnableWriting();

    bool read_called = false;
    bool write_called = false;
    bool close_called = false;
    bool error_called = false;

    channel.SetReadCallback([&read_called]() { read_called = true; });
    channel.SetWriteCallback([&write_called]() { write_called = true; });
    channel.SetCloseCallback([&close_called]() { close_called = true; });
    channel.SetErrorCallback([&error_called]() { error_called = true; });

    channel.HandleEvent(EPOLLIN);
    EXPECT_TRUE(read_called);

    channel.HandleEvent(EPOLLOUT);
    EXPECT_TRUE(write_called);

    channel.HandleEvent(EPOLLRDHUP);
    EXPECT_TRUE(close_called);

    channel.HandleEvent(EPOLLERR);
    EXPECT_TRUE(error_called);
}

}  // namespace rpc

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}