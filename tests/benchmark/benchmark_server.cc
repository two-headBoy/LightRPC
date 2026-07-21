#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>

#include "base/log/log.h"
#include "proto/benchmark.pb.h"
#include "rpc/rpc_server.h"

static std::atomic<bool> g_running{true};

static void SignalHandler(int) { g_running = false; }

int main(int argc, char *argv[]) {
    rpc::Log::Init(
        {.level = rpc::LogLevel::DEBUG, .path = "./benchmark_server_log", .suffix = ".log", .max_queue_size = 1024});

    std::string ip = "127.0.0.1";
    uint16_t port = 8080;
    if (argc > 1) {
        try {
            port = static_cast<uint16_t>(std::stoi(argv[1]));
        } catch (...) {
            std::cerr << "Invalid port: " << argv[1] << ", using default 8080" << std::endl;
        }
    }
    int thread_num = 4;
    int max_queue_size = 65536;

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    rpc::RpcServer server(ip, port, thread_num, max_queue_size);

    server.Register<benchmark::EchoRequest, benchmark::EchoResponse>(
        "BenchmarkService", "Echo", [](const benchmark::EchoRequest &req, benchmark::EchoResponse &resp) {
            resp.set_message(req.message());
            resp.set_processed_time(req.timestamp());
        });

    server.Register<benchmark::EmptyRequest, benchmark::EmptyResponse>(
        "BenchmarkService", "Ping",
        [](const benchmark::EmptyRequest &req, benchmark::EmptyResponse &resp) { (void)req; });

    std::cout << "Benchmark server starting on " << ip << ":" << port << std::endl;
    std::cout << "Thread pool size: " << thread_num << std::endl;
    std::cout << "Max queue size: " << max_queue_size << std::endl;

    server.Start();

    std::cout << "Server running. Send SIGINT/SIGTERM to stop." << std::endl;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    server.Stop();

    std::cout << "Server stopped" << std::endl;

    return 0;
}