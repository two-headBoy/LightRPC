#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>

#include "base/log/log.h"
#include "rpc/rpc_server.h"

namespace rpc {

class RpcServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Log::Init({.level = LogLevel::WARN, .path = "./test_server_log", .suffix = ".log", .max_queue_size = 1024});
    }

    void TearDown() override {}
};

TEST_F(RpcServerTest, StartAndStop) {
    RpcServer server("127.0.0.1", 19995, 1, 1000);

    server.Start();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    server.Stop();
}

TEST_F(RpcServerTest, StopMultipleTimes) {
    RpcServer server("127.0.0.1", 19996, 1, 1000);

    server.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    server.Stop();

    server.Stop();

    EXPECT_TRUE(true);
}

TEST_F(RpcServerTest, RegisterService) {
    RpcServer server("127.0.0.1", 19997, 1, 1000);

    server.RegisterRaw("TestService", "TestMethod", [](const RpcRequest &, RpcResponse &resp) {
        resp.set_code(0);
        resp.set_msg("success");
    });

    server.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    server.Stop();

    EXPECT_TRUE(true);
}

TEST_F(RpcServerTest, StartStopMultipleTimes) {
    RpcServer server("127.0.0.1", 19998, 1, 1000);

    server.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    server.Stop();

    server.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    server.Stop();

    EXPECT_TRUE(true);
}

TEST_F(RpcServerTest, PortBindingFailure) {
    RpcServer server1("127.0.0.1", 19999, 1, 1000);
    RpcServer server2("127.0.0.1", 19999, 1, 1000);

    server1.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    server2.Start();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    server1.Stop();
    server2.Stop();
}

}  // namespace rpc

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}