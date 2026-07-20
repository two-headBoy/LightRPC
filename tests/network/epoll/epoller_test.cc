#include <fcntl.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "network/epoll/epoller.h"

namespace rpc {

class EpollerTest : public ::testing::Test {
protected:
    void SetUp() override { epoller_ = std::make_unique<Epoller>(); }

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

    std::unique_ptr<Epoller> epoller_;
    std::vector<int> fds_;
};

TEST_F(EpollerTest, CreateAndDestroy) {
    Epoller epoller;
    EXPECT_TRUE(true);
}

TEST_F(EpollerTest, AddAndRemoveFd) {
    int fd = createTestSocket();
    ASSERT_NE(fd, -1);

    bool result = epoller_->AddFd(fd, EPOLLIN, nullptr);
    EXPECT_TRUE(result);

    result = epoller_->DelFd(fd);
    EXPECT_TRUE(result);
}

TEST_F(EpollerTest, ModFd) {
    int fd = createTestSocket();
    ASSERT_NE(fd, -1);

    bool result = epoller_->AddFd(fd, EPOLLIN, nullptr);
    EXPECT_TRUE(result);

    result = epoller_->ModFd(fd, EPOLLIN | EPOLLOUT, nullptr);
    EXPECT_TRUE(result);

    result = epoller_->DelFd(fd);
    EXPECT_TRUE(result);
}

TEST_F(EpollerTest, WaitWithTimeout) {
    int fd = createTestSocket();
    ASSERT_NE(fd, -1);

    epoller_->AddFd(fd, EPOLLIN, nullptr);

    int num_events = epoller_->Wait(100);

    epoller_->DelFd(fd);

    EXPECT_LE(num_events, 1);
}

TEST_F(EpollerTest, SetNonBlocking) {
    int fd = createTestSocket();
    ASSERT_NE(fd, -1);

    bool result = Epoller::SetNonBlocking(fd);
    EXPECT_TRUE(result);

    int flags = fcntl(fd, F_GETFL, 0);
    EXPECT_TRUE(flags & O_NONBLOCK);
}

}  // namespace rpc

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}