#include <fcntl.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <thread>

#include "base/log/log.h"
#include "rpc/rpc_client.h"

namespace rpc {

class RpcClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        Log::Init({.level = LogLevel::WARN, .path = "./test_client_log", .suffix = ".log", .max_queue_size = 1024});
    }

    void TearDown() override {}

    int startEchoServer(uint16_t port) {
        int server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (server_fd < 0) return -1;

        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(server_fd, (sockaddr *)&addr, sizeof(addr)) < 0) {
            close(server_fd);
            return -1;
        }

        if (listen(server_fd, 10) < 0) {
            close(server_fd);
            return -1;
        }

        std::thread([server_fd]() {
            while (true) {
                sockaddr_in client_addr{};
                socklen_t len = sizeof(client_addr);
                int conn_fd = accept4(server_fd, (sockaddr *)&client_addr, &len, SOCK_NONBLOCK);
                if (conn_fd >= 0) {
                    char buf[1024];
                    while (read(conn_fd, buf, sizeof(buf)) > 0) {}
                    close(conn_fd);
                }
            }
        }).detach();

        return server_fd;
    }
};

TEST_F(RpcClientTest, StartAndStop) {
    RpcClient client("127.0.0.1", 19999);

    EXPECT_FALSE(client.IsConnected());

    client.Start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_FALSE(client.IsConnected());

    client.Stop();
}

TEST_F(RpcClientTest, StopBeforeConnected) {
    RpcClient client("127.0.0.1", 19999);

    client.Start();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    client.Stop();

    EXPECT_FALSE(client.IsConnected());
}

TEST_F(RpcClientTest, StartStopMultipleTimes) {
    RpcClient client("127.0.0.1", 19999);

    client.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    client.Stop();

    client.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    client.Stop();

    EXPECT_FALSE(client.IsConnected());
}

TEST_F(RpcClientTest, CallWhenNotConnected) {
    RpcClient client("127.0.0.1", 19999);

    bool callback_called = false;
    int response_code = -1;

    client.Start();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    client.CallRaw(
        "TestService", "TestMethod", "test_body",
        [&callback_called, &response_code](const RpcResponse &resp) {
            callback_called = true;
            response_code = resp.code();
        },
        1000);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(response_code, static_cast<int>(RpcErrorCode::CLIENT_NOT_RUNNING));

    client.Stop();
}

TEST_F(RpcClientTest, CallTimeout) {
    int server_fd = startEchoServer(19997);
    ASSERT_NE(server_fd, -1);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    RpcClient client("127.0.0.1", 19997);

    bool callback_called = false;
    int response_code = -1;

    client.Start();

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    ASSERT_TRUE(client.IsConnected());

    client.CallRaw(
        "TestService", "TestMethod", "test_body",
        [&callback_called, &response_code](const RpcResponse &resp) {
            callback_called = true;
            response_code = resp.code();
        },
        200);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(response_code, static_cast<int>(RpcErrorCode::CLIENT_REQUEST_TIMEOUT));

    client.Stop();
    close(server_fd);
}

TEST_F(RpcClientTest, IsConnectedReturnsFalseWhenNotConnected) {
    RpcClient client("127.0.0.1", 19999);

    EXPECT_FALSE(client.IsConnected());

    client.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(client.IsConnected());

    client.Stop();
}

}  // namespace rpc

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}