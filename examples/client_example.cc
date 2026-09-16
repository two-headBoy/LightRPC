#include "rpc/rpc_client.h"
#include "protocol/rpc.pb.h"
#include "proto/user.pb.h"
#include "proto/math.pb.h"
#include "base/log/log.h"
#include "network/core/event_loop.h"

#include <ctime>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

int main() {
    rpc::Log::Init({
        .level = rpc::LogLevel::DEBUG,
        .path = "./client_log",
        .suffix = ".log",
        .max_queue_size = 1024
    });

    std::string ip = "127.0.0.1";
    uint16_t port = 8080;

    rpc::RpcClient client(ip, port);

    std::cout << "Starting client..." << std::endl;

    client.Start();

    int timeout = 500;
    while (!client.IsConnected() && timeout > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        timeout--;
    }

    if (!client.IsConnected()) {
        std::cout << "Failed to connect to server!" << std::endl;
        std::cout << "Please make sure server is running on " << ip << ":" << port << std::endl;
        client.Stop();
        return 1;
    }

    std::cout << "Connected to server!" << std::endl;

    // GetUser
    user::GetUserRequest get_req;
    get_req.set_user_id(1);
    client.Call<user::GetUserRequest, user::GetUserResponse>(
        "UserService", "GetUser", get_req,
        [](int32_t code, const std::string &msg, const user::GetUserResponse& resp) {
            std::cout << "\n=== GetUser Response ===" << std::endl;
            if (code != 0) {
                std::cout << "RPC failed: code=" << code << ", msg=" << msg << std::endl;
                return;
            }
            std::cout << "user_id: " << resp.user_id() << std::endl;
            std::cout << "name:    " << resp.name() << std::endl;
            std::cout << "email:   " << resp.email() << std::endl;
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 空闲等待测试：sleep 10s 后再发数据，验证连接是否还活着
    {
        auto now_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::cout << "\n=== Idle wait test: sleep 10s ===" << std::endl;
        std::cout << "sleep start at:        " << std::ctime(&now_t);
        std::cout << "IsConnected (before):  " << (client.IsConnected() ? "true" : "false") << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(10));
        auto end_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::cout << "sleep end at:          " << std::ctime(&end_t);
        std::cout << "IsConnected (after):   " << (client.IsConnected() ? "true" : "false") << std::endl;
    }

    // CreateUser
    user::CreateUserRequest create_req;
    create_req.set_name("Alice");
    create_req.set_email("alice@example.com");
    client.Call<user::CreateUserRequest, user::CreateUserResponse>(
        "UserService", "CreateUser", create_req,
        [](int32_t code, const std::string &msg, const user::CreateUserResponse& resp) {
            std::cout << "\n=== CreateUser Response ===" << std::endl;
            if (code != 0) {
                std::cout << "RPC failed: code=" << code << ", msg=" << msg << std::endl;
                return;
            }
            std::cout << "new user_id: " << resp.user_id() << std::endl;
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Add
    math::AddRequest add_req;
    add_req.set_a(100);
    add_req.set_b(200);
    client.Call<math::AddRequest, math::AddResponse>(
        "MathService", "Add", add_req,
        [](int32_t code, const std::string &msg, const math::AddResponse& resp) {
            std::cout << "\n=== Add Response ===" << std::endl;
            if (code != 0) {
                std::cout << "RPC failed: code=" << code << ", msg=" << msg << std::endl;
                return;
            }
            std::cout << "result: " << resp.result() << std::endl;
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Non-existent method - should timeout
    math::AddRequest timeout_req;
    timeout_req.set_a(1);
    timeout_req.set_b(2);
    client.Call<math::AddRequest, math::AddResponse>(
        "MathService", "NonExistentMethod", timeout_req,
        [](int32_t code, const std::string &msg, const math::AddResponse& resp) {
            std::cout << "\n=== NonExistentMethod Response ===" << std::endl;
            if (code != 0) {
                std::cout << "RPC failed: code=" << code << ", msg=" << msg << std::endl;
                return;
            }
            std::cout << "result: " << resp.result() << std::endl;
        },
        1000);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "\nPress Enter to stop client..." << std::endl;
    std::cin.get();

    client.Stop();

    std::cout << "Client stopped" << std::endl;

    return 0;
}
