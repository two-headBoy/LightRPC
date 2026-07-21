#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "proto/benchmark.pb.h"
#include "rpc/rpc_client.h"

// 测试模式：
//   QPS          - 开环压测：固定发送速率（QPS），与响应快慢无关
//   CONCURRENCY  - 闭环并发：固定在途请求数，响应回来一个立即补发一个
enum class TestMode { QPS, CONCURRENCY };

struct BenchmarkResult {
    std::atomic<int64_t> sent_requests{0};   // 已发送请求数
    std::atomic<int64_t> total_requests{0};  // 已收到响应数
    std::atomic<int64_t> success_requests{0};
    std::atomic<int64_t> failed_requests{0};
    std::atomic<int64_t> total_latency_us{0};

    // 错误分类统计
    std::atomic<int64_t> timeout_errors{0};
    std::atomic<int64_t> connection_errors{0};
    std::atomic<int64_t> network_errors{0};
    std::atomic<int64_t> server_errors{0};
    std::atomic<int64_t> other_errors{0};

    std::atomic<int64_t> pending_requests{0};
    std::mutex finish_mutex;
    std::condition_variable finish_cv;
};

struct WorkerLatencyData {
    std::vector<int64_t> latency_samples;
};

void PrintUsage(const char *prog) {
    std::cout << "Usage: " << prog << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  -server <ip:port>    Server address (default: 127.0.0.1:8080)\n";
    std::cout << "  -mode <mode>         Test mode: qps | concurrency (default: qps)\n";
    std::cout << "  -threads <n>         Number of client threads (default: 4)\n";
    std::cout << "  -connections <n>     Number of connections (default: 1)\n";
    std::cout << "  -qps <n>             Target QPS in qps mode (default: 1000)\n";
    std::cout << "  -concurrency <n>     Target in-flight requests in concurrency mode (default: 100)\n";
    std::cout << "  -duration <seconds>  Test duration in seconds (default: 30)\n";
    std::cout << "  -timeout <ms>        RPC timeout in milliseconds (default: 1000)\n";
    std::cout << "  -warmup <seconds>    Warmup duration in seconds (default: 5)\n";
    std::cout << "  -help                Show this help message\n";
}

int ParseInt(const char *str, int default_val) {
    if (!str) return default_val;
    try {
        return std::stoi(str);
    } catch (...) {
        return default_val;
    }
}

// 余数补偿计算每线程 QPS，避免整数除法丢请求
inline int QpsPerThread(int total_qps, int threads, int thread_id) {
    int base = total_qps / threads;
    int remainder = total_qps % threads;
    return base + (thread_id < remainder ? 1 : 0);
}

// 并发模式下每线程维持的在途请求数，余数补偿避免整数除法丢槽位
inline int ConcurrencyPerThread(int total_concurrency, int threads, int thread_id) {
    int base = total_concurrency / threads;
    int remainder = total_concurrency % threads;
    return base + (thread_id < remainder ? 1 : 0);
}

// CPU relax 指令：busy-wait 时降低功耗与流水线冲突
inline void CpuRelax() {
#if defined(__x86_64__) || defined(__i386__)
    asm volatile("pause" ::: "memory");
#elif defined(__aarch64__)
    asm volatile("yield" ::: "memory");
#endif
}

// 节流到目标时间点。
// std::this_thread::sleep_for 在 Linux 上对 < 1ms 的睡眠精度极差（实际往往睡 ~1ms），
// 高 QPS（单线程 > 1000 QPS，间隔 < 1ms）时会让实际 QPS 远低于目标，差一个数量级。
// 采用三档自适应策略：
//   - 剩余 > 10ms：纯 sleep_until，不浪费 CPU（低 QPS 场景）
//   - 剩余 > 1ms ：sleep_until 粗对齐 + 末段 1ms busy-wait 兜底
//   - 剩余 <= 1ms：纯 busy-wait，保证精度（高 QPS 场景）
inline void ThrottleUntil(std::chrono::steady_clock::time_point target) {
    auto now = std::chrono::steady_clock::now();
    if (now >= target) {
        return;
    }

    auto remaining_us = std::chrono::duration_cast<std::chrono::microseconds>(target - now).count();

    constexpr int64_t kBusyWaitBudgetUs = 1000;
    constexpr int64_t kPureSleepThresholdUs = 10000;

    if (remaining_us > kPureSleepThresholdUs) {
        std::this_thread::sleep_until(target);
    } else if (remaining_us > kBusyWaitBudgetUs) {
        std::this_thread::sleep_until(target - std::chrono::microseconds(kBusyWaitBudgetUs));
        while (std::chrono::steady_clock::now() < target) {
            CpuRelax();
        }
    } else {
        while (std::chrono::steady_clock::now() < target) {
            CpuRelax();
        }
    }
}

// 统一记录一次响应：计数、延迟采样、错误分类。两种模式共用，保证统计口径一致。
// 延迟采样覆盖所有响应（含超时/失败），否则 P99 会被排除的尾部延迟严重拉低。
inline void RecordResponse(BenchmarkResult *result, int32_t code, int64_t latency_us, WorkerLatencyData *latency_data) {
    result->total_requests.fetch_add(1);
    result->total_latency_us.fetch_add(latency_us);
    latency_data->latency_samples.push_back(latency_us);

    if (code == static_cast<int32_t>(rpc::RpcErrorCode::OK)) {
        result->success_requests.fetch_add(1);
    } else {
        result->failed_requests.fetch_add(1);
        if (code == static_cast<int32_t>(rpc::RpcErrorCode::CLIENT_REQUEST_TIMEOUT)) {
            result->timeout_errors.fetch_add(1);
        } else if (code == static_cast<int32_t>(rpc::RpcErrorCode::CLIENT_CONNECTION_DISCONNECTED)) {
            result->connection_errors.fetch_add(1);
        } else if (code == static_cast<int32_t>(rpc::RpcErrorCode::CLIENT_NETWORK_ERROR)) {
            result->network_errors.fetch_add(1);
        } else if (code >= static_cast<int32_t>(rpc::RpcErrorCode::SERVER_SERVICE_NOT_FOUND)) {
            result->server_errors.fetch_add(1);
        } else {
            result->other_errors.fetch_add(1);
        }
    }
}

void RunBenchmarkWorker(int thread_id, std::vector<rpc::RpcClient *> &clients, int total_qps, int threads,
                        int duration_ms, int timeout_ms, BenchmarkResult *result, WorkerLatencyData *latency_data) {
    auto start_time = std::chrono::steady_clock::now();
    int qps_per_thread = QpsPerThread(total_qps, threads, thread_id);
    int64_t requests_per_thread = static_cast<int64_t>(qps_per_thread) * duration_ms / 1000;

    // 处理边界情况：防止除零
    int64_t interval_us = 0;
    if (requests_per_thread > 0) {
        interval_us = static_cast<int64_t>(duration_ms) * 1000LL / requests_per_thread;
    }

    // 如果QPS配置过低，至少每个线程发送一个请求
    if (requests_per_thread <= 0) {
        requests_per_thread = 1;
    }

    int conn_idx = thread_id % clients.size();
    rpc::RpcClient *client = clients[conn_idx];

    // 预分配延迟样本空间，避免 push_back 时的 reallocate 污染延迟测量
    latency_data->latency_samples.reserve(requests_per_thread);

    for (int64_t i = 0; i < requests_per_thread; ++i) {
        auto req_start = std::chrono::steady_clock::now();

        benchmark::EmptyRequest req;
        std::string request_body;
        req.SerializeToString(&request_body);

        result->pending_requests.fetch_add(1);
        result->sent_requests.fetch_add(1);

        client->CallRaw(
            "BenchmarkService", "Ping", request_body,
            [result, req_start, latency_data](const rpc::RpcResponse &resp) {
                auto req_end = std::chrono::steady_clock::now();
                int64_t latency_us = std::chrono::duration_cast<std::chrono::microseconds>(req_end - req_start).count();

                RecordResponse(result, resp.code(), latency_us, latency_data);

                if (result->pending_requests.fetch_sub(1) == 1) {
                    result->finish_cv.notify_all();
                }
            },
            timeout_ms);

        if (interval_us > 0) {
            auto target = start_time + std::chrono::microseconds((i + 1) * interval_us);
            ThrottleUntil(target);
        }
    }
}

// 闭环并发测试的 per-worker 状态。
// 每个线程维持固定数量的在途请求（in_flight），响应回来后立即补发一个，从而保持并发度恒定。
struct ConcurrencyWorkerState {
    rpc::RpcClient *client;
    int timeout_ms;
    BenchmarkResult *result;
    WorkerLatencyData *latency_data;
    std::chrono::steady_clock::time_point end_time;

    std::atomic<int64_t> in_flight{0};  // 当前在途请求数
    std::atomic<bool> sending_done{false};
    std::mutex mtx;
    std::condition_variable cv;  // in_flight 归零时唤醒 worker 线程
};

// 发送一个请求并占用一个在途槽位（in_flight++）。
// 其回调在响应到达时无条件释放该槽位（in_flight--），随后决定是否补发：
//   - 补发：再 SendOneRequest 占回刚释放的槽位（in_flight++），净变化 0，并发度恒定
//   - 不补发：槽位保持释放状态（净 -1），in_flight 最终归零时唤醒 worker
// 递归是事件驱动的（CallRaw 异步返回，回调稍后在 EventLoop 线程触发），不会栈递归；
// 仅当连接已断开时 CallRaw 会同步触发回调，此时 IsConnected() 为 false 走 drain 分支，避免死循环。
void SendOneRequest(ConcurrencyWorkerState *ws) {
    ws->in_flight.fetch_add(1);
    ws->result->pending_requests.fetch_add(1);
    ws->result->sent_requests.fetch_add(1);

    auto req_start = std::chrono::steady_clock::now();

    benchmark::EmptyRequest req;
    std::string request_body;
    req.SerializeToString(&request_body);

    ws->client->CallRaw(
        "BenchmarkService", "Ping", request_body,
        [ws, req_start](const rpc::RpcResponse &resp) {
            auto req_end = std::chrono::steady_clock::now();
            int64_t latency_us = std::chrono::duration_cast<std::chrono::microseconds>(req_end - req_start).count();

            RecordResponse(ws->result, resp.code(), latency_us, ws->latency_data);

            ws->result->pending_requests.fetch_sub(1);

            // 响应到达即无条件释放一个在途槽位（fetch_sub 返回的是减之前的值）
            int64_t after = ws->in_flight.fetch_sub(1) - 1;

            // 闭环控制：仍在时长内且连接正常则补发，维持固定在途并发数
            bool within_time = std::chrono::steady_clock::now() < ws->end_time;
            if (!ws->sending_done.load() && within_time && ws->client->IsConnected()) {
                SendOneRequest(ws);  // 补发：占回刚释放的槽位，in_flight 净变化 0
            } else {
                ws->sending_done.store(true);
                // 最后一个在途槽位排空时唤醒 worker 线程
                if (after == 0) {
                    std::lock_guard<std::mutex> lock(ws->mtx);
                    ws->cv.notify_all();
                }
            }
        },
        ws->timeout_ms);
}

void RunConcurrencyWorker(int thread_id, std::vector<rpc::RpcClient *> &clients, int total_concurrency, int threads,
                          int duration_ms, int timeout_ms, BenchmarkResult *result, WorkerLatencyData *latency_data) {
    int concurrency_per_thread = ConcurrencyPerThread(total_concurrency, threads, thread_id);
    rpc::RpcClient *client = clients[thread_id % clients.size()];

    ConcurrencyWorkerState ws;
    ws.client = client;
    ws.timeout_ms = timeout_ms;
    ws.result = result;
    ws.latency_data = latency_data;
    ws.end_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);

    // 并发模式下请求总数未知，预分配一个粗略容量，超出会自动扩容。
    // push_back 的 reallocate 不影响已记录延迟（req_end 在 push 前 capture），只会延迟下一次补发。
    latency_data->latency_samples.reserve(static_cast<size_t>(concurrency_per_thread) * 8);

    // 填满初始在途槽位，之后由回调自动补发维持并发度
    for (int i = 0; i < concurrency_per_thread; ++i) {
        SendOneRequest(&ws);
    }

    // 等待所有在途请求排空（时长到期后停止补发，剩余在途自然完成）
    {
        std::unique_lock<std::mutex> lock(ws.mtx);
        ws.cv.wait(lock, [&ws] { return ws.in_flight.load() == 0; });
    }
}

int main(int argc, char *argv[]) {
    std::string server_addr = "127.0.0.1:8080";
    TestMode test_mode = TestMode::QPS;
    int threads = 4;
    int connections = 1;
    int qps = 1000;
    int concurrency = 100;
    int duration = 30;
    int timeout_ms = 1000;
    int warmup = 5;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-server" && i + 1 < argc) {
            server_addr = argv[++i];
        } else if (arg == "-mode" && i + 1 < argc) {
            std::string m = argv[++i];
            test_mode = (m == "concurrency" || m == "conc") ? TestMode::CONCURRENCY : TestMode::QPS;
        } else if (arg == "-threads" && i + 1 < argc) {
            threads = ParseInt(argv[++i], 4);
        } else if (arg == "-connections" && i + 1 < argc) {
            connections = ParseInt(argv[++i], 1);
        } else if (arg == "-qps" && i + 1 < argc) {
            qps = ParseInt(argv[++i], 1000);
        } else if (arg == "-concurrency" && i + 1 < argc) {
            concurrency = ParseInt(argv[++i], 100);
        } else if (arg == "-duration" && i + 1 < argc) {
            duration = ParseInt(argv[++i], 30);
        } else if (arg == "-timeout" && i + 1 < argc) {
            timeout_ms = ParseInt(argv[++i], 1000);
        } else if (arg == "-warmup" && i + 1 < argc) {
            warmup = ParseInt(argv[++i], 5);
        } else if (arg == "-help") {
            PrintUsage(argv[0]);
            return 0;
        }
    }

    size_t colon_pos = server_addr.find(':');
    std::string ip = "127.0.0.1";
    uint16_t port = 8080;
    if (colon_pos != std::string::npos) {
        ip = server_addr.substr(0, colon_pos);
        port = static_cast<uint16_t>(std::stoi(server_addr.substr(colon_pos + 1)));
    }

    // 确保每个线程有独立的连接，避免线程安全问题
    if (connections < threads) {
        std::cout << "Warning: connections (" << connections << ") < threads (" << threads
                  << "), adjusting connections to " << threads << "\n";
        connections = threads;
    }

    // 并发模式下每个线程至少需要一个在途槽位
    if (test_mode == TestMode::CONCURRENCY && concurrency < threads) {
        std::cout << "Warning: concurrency (" << concurrency << ") < threads (" << threads
                  << "), adjusting concurrency to " << threads << "\n";
        concurrency = threads;
    }

    std::cout << "============================================\n";
    std::cout << "LightRPC Benchmark Client\n";
    std::cout << "============================================\n";
    std::cout << "Server: " << ip << ":" << port << "\n";
    std::cout << "Mode: " << (test_mode == TestMode::CONCURRENCY ? "concurrency" : "qps") << "\n";
    std::cout << "Threads: " << threads << "\n";
    std::cout << "Connections: " << connections << "\n";
    if (test_mode == TestMode::CONCURRENCY) {
        std::cout << "Target Concurrency: " << concurrency << "\n";
    } else {
        std::cout << "Target QPS: " << qps << "\n";
    }
    std::cout << "Duration: " << duration << "s\n";
    std::cout << "Timeout: " << timeout_ms << "ms\n";
    std::cout << "Warmup: " << warmup << "s\n";
    std::cout << "============================================\n\n";

    std::vector<std::unique_ptr<rpc::RpcClient>> client_instances;
    std::vector<rpc::RpcClient *> client_ptrs;

    for (int i = 0; i < connections; ++i) {
        auto client = std::make_unique<rpc::RpcClient>(ip, port);
        client->Start();
        client_instances.push_back(std::move(client));
        client_ptrs.push_back(client_instances.back().get());
    }

    for (int i = 0; i < connections; ++i) {
        int timeout = 500;
        while (!client_ptrs[i]->IsConnected() && timeout > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            timeout--;
        }

        if (!client_ptrs[i]->IsConnected()) {
            std::cerr << "Failed to connect to server on connection " << i << "!" << std::endl;
            for (auto &c : client_instances) {
                c->Stop();
            }
            return 1;
        }
    }

    std::cout << "Connected to server with " << connections << " connections!\n";

    // 真实的预热阶段：发送预热请求
    std::cout << "Warming up for " << warmup << " seconds...\n";
    {
        std::vector<std::thread> warmup_threads;
        std::atomic<int64_t> warmup_pending{0};
        std::mutex warmup_mutex;
        std::condition_variable warmup_cv;

        int warmup_qps = qps / 10;  // 预热QPS为目标QPS的10%
        if (warmup_qps < 10) warmup_qps = 10;

        for (int i = 0; i < threads; ++i) {
            warmup_threads.emplace_back([i, &client_ptrs, warmup_qps, threads, warmup_ms = warmup * 1000, timeout_ms,
                                         &warmup_pending, &warmup_cv]() {
                int qps_per_thread = QpsPerThread(warmup_qps, threads, i);
                int64_t requests_per_thread = static_cast<int64_t>(qps_per_thread) * warmup_ms / 1000;
                if (requests_per_thread <= 0) requests_per_thread = 10;

                int conn_idx = i % client_ptrs.size();
                rpc::RpcClient *client = client_ptrs[conn_idx];
                int64_t interval_us =
                    requests_per_thread > 0 ? static_cast<int64_t>(warmup_ms) * 1000LL / requests_per_thread : 0;
                auto start_time = std::chrono::steady_clock::now();

                for (int64_t j = 0; j < requests_per_thread; ++j) {
                    benchmark::EmptyRequest req;
                    std::string request_body;
                    req.SerializeToString(&request_body);

                    warmup_pending.fetch_add(1);
                    client->CallRaw(
                        "BenchmarkService", "Ping", request_body,
                        [&warmup_pending, &warmup_cv](const rpc::RpcResponse &resp) {
                            (void)resp;
                            if (warmup_pending.fetch_sub(1) == 1) {
                                warmup_cv.notify_all();
                            }
                        },
                        timeout_ms);

                    if (interval_us > 0) {
                        auto target = start_time + std::chrono::microseconds((j + 1) * interval_us);
                        ThrottleUntil(target);
                    }
                }
            });
        }

        for (auto &t : warmup_threads) {
            if (t.joinable()) {
                t.join();
            }
        }

        // 等待所有预热请求完成
        {
            std::unique_lock<std::mutex> lock(warmup_mutex);
            warmup_cv.wait(lock, [&warmup_pending] { return warmup_pending.load() == 0; });
        }
    }

    // 添加短暂等待，确保预热请求完全处理完毕
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "Warmup completed!\n";

    BenchmarkResult result;
    std::vector<std::thread> worker_threads;
    std::vector<WorkerLatencyData> latency_data(threads);

    std::cout << "Starting benchmark...\n";

    auto test_start_time = std::chrono::steady_clock::now();

    if (test_mode == TestMode::CONCURRENCY) {
        for (int i = 0; i < threads; ++i) {
            worker_threads.emplace_back(RunConcurrencyWorker, i, std::ref(client_ptrs), concurrency, threads,
                                        duration * 1000, timeout_ms, &result, &latency_data[i]);
        }
    } else {
        for (int i = 0; i < threads; ++i) {
            worker_threads.emplace_back(RunBenchmarkWorker, i, std::ref(client_ptrs), qps, threads, duration * 1000,
                                        timeout_ms, &result, &latency_data[i]);
        }
    }

    for (auto &t : worker_threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    // 并发模式下 worker 线程会阻塞直到所有在途请求排空，join 完成即所有响应已收回；
    // QPS 模式下 worker 只负责发送，join 完成后还需等待剩余在途回包。
    auto test_end_time = std::chrono::steady_clock::now();

    if (test_mode == TestMode::QPS) {
        std::cout << "Waiting for responses...\n";
        std::unique_lock<std::mutex> lock(result.finish_mutex);
        result.finish_cv.wait(lock, [&result] { return result.pending_requests.load() == 0; });
    }

    for (auto &client : client_instances) {
        client->Stop();
    }

    // 合并所有线程的延迟样本
    std::vector<int64_t> all_latency_samples;
    for (auto &data : latency_data) {
        all_latency_samples.insert(all_latency_samples.end(), data.latency_samples.begin(), data.latency_samples.end());
    }
    std::sort(all_latency_samples.begin(), all_latency_samples.end());

    auto sent_req = result.sent_requests.load();
    auto total_req = result.total_requests.load();
    auto success_req = result.success_requests.load();
    auto failed_req = result.failed_requests.load();
    auto total_latency_us = result.total_latency_us.load();

    // 错误分类统计
    auto timeout_errors = result.timeout_errors.load();
    auto conn_errors = result.connection_errors.load();
    auto network_errors = result.network_errors.load();
    auto server_errors = result.server_errors.load();
    auto other_errors = result.other_errors.load();

    double avg_latency_ms = 0;
    double p50_latency_ms = 0;
    double p90_latency_ms = 0;
    double p99_latency_ms = 0;

    if (total_req > 0) {
        avg_latency_ms = static_cast<double>(total_latency_us) / total_req / 1000.0;

        if (!all_latency_samples.empty()) {
            // nearest-rank 百分位：index = ceil(p/100 * size) - 1
            auto percentile = [&all_latency_samples](double p) -> double {
                size_t size = all_latency_samples.size();
                size_t idx = static_cast<size_t>(std::ceil(p / 100.0 * size));
                if (idx == 0) idx = 1;
                return static_cast<double>(all_latency_samples[idx - 1]) / 1000.0;
            };
            p50_latency_ms = percentile(50);
            p90_latency_ms = percentile(90);
            p99_latency_ms = percentile(99);
        }
    }

    // QPS 模式：actual_qps 为发送速率（基于发送完成时间，不含等待响应）。
    // 并发模式：sent == total（worker 阻塞至所有在途排空），actual_qps 即吞吐量，
    //           estimated_concurrency 按 Little's Law 应接近目标并发度，可作闭环正确性校验。
    auto actual_duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(test_end_time - test_start_time).count();
    double actual_qps = actual_duration_ms > 0 ? static_cast<double>(sent_req) * 1000.0 / actual_duration_ms : 0;
    double success_rate = total_req > 0 ? static_cast<double>(success_req) / total_req * 100 : 0;
    // 估算在途并发数 = 吞吐 QPS × 平均延迟
    double estimated_concurrency = actual_qps * avg_latency_ms / 1000.0;

    std::cout << "\n============================================\n";
    std::cout << "Benchmark Results\n";
    std::cout << "============================================\n";
    std::cout << std::fixed << std::setprecision(2);
    if (test_mode == TestMode::CONCURRENCY) {
        std::cout << "Target Concurrency: " << concurrency << "\n";
    }
    std::cout << "Sent Requests: " << sent_req << "\n";
    std::cout << "Total Responses: " << total_req << "\n";
    std::cout << "Success: " << success_req << "\n";
    std::cout << "Failed: " << failed_req << "\n";
    if (failed_req > 0) {
        std::cout << "  - Timeout: " << timeout_errors << "\n";
        std::cout << "  - Connection: " << conn_errors << "\n";
        std::cout << "  - Network: " << network_errors << "\n";
        std::cout << "  - Server: " << server_errors << "\n";
        std::cout << "  - Other: " << other_errors << "\n";
    }
    std::cout << "Success Rate: " << success_rate << "%\n";
    std::cout << "Actual QPS: " << actual_qps << "\n";
    std::cout << "Actual Duration: " << actual_duration_ms / 1000.0 << "s\n";
    std::cout << "Estimated Concurrency: " << estimated_concurrency << "\n";
    std::cout << "Average Latency: " << avg_latency_ms << " ms\n";
    std::cout << "P50 Latency: " << p50_latency_ms << " ms\n";
    std::cout << "P90 Latency: " << p90_latency_ms << " ms\n";
    std::cout << "P99 Latency: " << p99_latency_ms << " ms\n";
    std::cout << "(latency stats include all responses: success + failed)\n";
    std::cout << "============================================\n";

    return 0;
}
