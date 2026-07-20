#include "rpc/rpc_server.h"
#include "protocol/rpc.pb.h"
#include "proto/user.pb.h"
#include "proto/math.pb.h"
#include "base/log/log.h"
#include "network/core/event_loop.h"

#include <iostream>
#include <string>
#include <thread>

int main() {
    rpc::Log::Init({
        .level = rpc::LogLevel::DEBUG,
        .path = "./server_log",
        .suffix = ".log",
        .max_queue_size = 1024
    });

    std::string ip = "127.0.0.1";
    uint16_t port = 8080;

    rpc::RpcServer server(ip, port, 4, 1024);

    server.Register<user::GetUserRequest, user::GetUserResponse>(
        "UserService", "GetUser",
        [](const user::GetUserRequest& req, user::GetUserResponse& resp) {
            std::cout << "Received GetUser request, user_id=" << req.user_id() << std::endl;

            resp.set_user_id(req.user_id());
            resp.set_name("John");
            resp.set_email("john@example.com");
        });

    server.Register<user::CreateUserRequest, user::CreateUserResponse>(
        "UserService", "CreateUser",
        [](const user::CreateUserRequest& req, user::CreateUserResponse& resp) {
            std::cout << "Received CreateUser request, name=" << req.name()
                      << ", email=" << req.email() << std::endl;

            resp.set_user_id(2);
        });

    server.Register<math::AddRequest, math::AddResponse>(
        "MathService", "Add",
        [](const math::AddRequest& req, math::AddResponse& resp) {
            std::cout << "Received Add request: a=" << req.a()
                      << ", b=" << req.b() << std::endl;

            resp.set_result(req.a() + req.b());
        });

    std::cout << "Server starting on " << ip << ":" << port << std::endl;

    server.Start();


    std::cout << "Press Enter to stop server..." << std::endl;
    std::cin.get();

    server.Stop();

    std::cout << "Server stopped" << std::endl;

    return 0;
}
