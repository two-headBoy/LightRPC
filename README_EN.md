# LightRPC

A lightweight RPC framework based on C++17. It adopts the Reactor pattern, using epoll-driven asynchronous networking, Protobuf for business messages, and a thread pool to isolate business computation, providing compile-time strongly-typed service registration and invocation interfaces.

## Features

### Network & Concurrency
- **Reactor Asynchronous Network**: Single EventLoop + epoll; cross-thread tasks are woken up via `eventfd`. All network events are processed serially in the IO thread with no lock contention.
- **Layered EventLoop Management**: The lower-level `TcpServer` / `TcpClient` do **not** hold an EventLoop. It is injected from the upper layer (`RpcServer` / `RpcClient`) which also manages its lifetime, making components reusable.
- **IO / Business Decoupling**: Network IO runs on the EventLoop thread, while business processing is handed off to an independent thread pool (with backpressure + max queue length), preventing slow business calls from blocking the network event loop.
- **Connection-Level Thread Safety**: Each `RpcClient` is bound to one IO thread; in multi-connection scenarios, the application layer can create multiple instances and share them.

### RPC & Protocol
- **Compile-Time Strongly-Typed RPC**: Based on SFINAE `is_proto_message` type trait, performing compile-time Protobuf request/response type validation with automatic serialization and deserialization.
- **Custom Frame Protocol**: 4-byte big-endian length prefix + Protobuf binary body. The Codec layer only does byte-stream encoding/decoding and is unaware of business content, naturally solving TCP sticky-packet / half-packet issues.
- **Unified Error Code System**: 0 for success / 1xxx for client errors / 2xxx for server errors. Business and framework share the same enum.
- **Request Timeout Control**: Time-wheel timer automatically cleans up pending callbacks and notifies the caller on timeout, avoiding memory leaks and hanging requests.
- **Disconnect Awareness**: When a connection drops, all pending requests are actively called back rather than silently cleared.

### Engineering & Tooling
- **High-Performance Asynchronous Logging**: Async queue + background flush thread, supporting rolling by day / by line count.
- **Dual-Mode Benchmark Tool**: Built-in `benchmark_client` supporting **QPS mode (open-loop)** and **concurrency mode (closed-loop)**, outputting P50 / P90 / P99 latency percentiles and error classification stats.
- **Comprehensive Unit Tests**: Covering core modules including Buffer, Log, ThreadPool, EventLoop, Channel, Epoller, Timer, RpcClient, RpcServer, RpcService.

## Dependencies

- C++17 compiler
- CMake ≥ 3.10
- Protobuf
- Linux (depends on epoll / eventfd)

## Quick Start

```bash
mkdir build && cd build
cmake .. && make -j$(nproc)

# Run examples
./bin/examples/server_example   # Terminal 1
./bin/examples/client_example   # Terminal 2

# Run unit tests
./bin/tests/rpc_client_test
./bin/tests/rpc_server_test
./bin/tests/rpc_service_test
./bin/tests/event_loop_test
./bin/tests/channel_test
./bin/tests/epoller_test
./bin/tests/timer_test

# Run performance benchmark
./bin/tests/benchmark/benchmark_server           # Terminal 1

# QPS mode (open-loop benchmark): 4 threads, 10000 QPS, 30 seconds
./bin/tests/benchmark/benchmark_client -mode qps -threads 4 -connections 4 -qps 10000 -duration 30

# Concurrency mode (closed-loop benchmark): 4 threads, 200 in-flight, 30 seconds
./bin/tests/benchmark/benchmark_client -mode concurrency -threads 4 -connections 4 -concurrency 200 -duration 30
```

### Server

```cpp
#include "rpc/rpc_server.h"
#include "proto/user.pb.h"

int main() {
    rpc::RpcServer server("127.0.0.1", 8080, 4, 1024);

    server.Register<user::GetUserRequest, user::GetUserResponse>(
        "UserService", "GetUser",
        [](const user::GetUserRequest& req, user::GetUserResponse& resp) {
            resp.set_user_id(req.user_id());
            resp.set_name("John");
            resp.set_email("john@example.com");
        });

    server.Start();

    // The main thread can handle other tasks, or wait for a signal
    // ...

    server.Stop();
}
```

### Client

```cpp
#include "rpc/rpc_client.h"
#include "proto/user.pb.h"

int main() {
    rpc::RpcClient client("127.0.0.1", 8080);
    client.Start();

    user::GetUserRequest req;
    req.set_user_id(1);
    client.Call<user::GetUserRequest, user::GetUserResponse>(
        "UserService", "GetUser", req,
        [](const user::GetUserResponse& resp) {
            // handle response
        });

    // The main thread can handle other tasks
    // ...

    client.Stop();
}
```

## Directory Structure

```
LightRPC/
├── code/                  # Framework library
│   ├── base/              # Buffer / Log / ThreadPool / TimeWheel / ErrorCode
│   ├── network/           # EventLoop / Channel / Epoll / TCP
│   ├── protocol/          # Codec / proto_traits / RPC envelope
│   └── rpc/               # RpcServer / RpcClient / RpcService
├── examples/              # Application examples (including business .proto)
├── tests/                 # Unit tests
│   └── benchmark/         # Performance benchmark tool (benchmark_server / benchmark_client)
├── docs/                  # Design documents
├── .clang-format          # Clang formatting configuration
├── CMakeLists.txt
└── README.md
```

## Protocol

Frame format: `4-byte length (network byte order) + Protobuf data`

RPC envelope messages:

```protobuf
message RpcRequest  { string service_name; string method_name; string request_id; bytes body; }
message RpcResponse { string request_id; int32 code; string msg; bytes body; }
```

`body` carries the business message byte stream; the framework only encodes/decodes the envelope, and business types are automatically handled by the `Register` / `Call` templates.

Error code specification: see [docs/error_code.md](docs/error_code.md).

## API Reference

### RpcServer

| Method | Description |
|--------|-------------|
| `RpcServer(ip, port, threadNum, maxQueueSize)` | Constructor |
| `Start()` | Start server (automatically starts the event loop thread) |
| `Stop()` | Stop server (waits for the event loop thread to exit) |
| `Register<ReqT, RspT>(service, method, handler)` | Register a strongly-typed service method |
| `RegisterRaw(service, method, callback)` | Register a raw callback method |

### RpcClient

| Method | Description |
|--------|-------------|
| `RpcClient(ip, port)` | Constructor |
| `Start()` | Start client (automatically starts the event loop thread and connects) |
| `Stop()` | Stop client (disconnects and waits for the event loop thread to exit) |
| `Call<ReqT, RspT>(service, method, req, callback, timeout)` | Call a strongly-typed RPC |
| `CallRaw(service, method, body, callback, timeout)` | Call a raw RPC |
| `IsConnected()` | Check connection status |

## Error Codes

| Range | Meaning |
|-------|---------|
| 0 | Success |
| 1001 - 1999 | Client-side errors |
| 2001 - 2999 | Server-side errors |

See [docs/error_code.md](docs/error_code.md) for detailed error code definitions.

## Benchmark Tool

`tests/benchmark/` provides `benchmark_server` and `benchmark_client`, supporting two test modes:

### Two Modes

| Mode | Command | Control Target | Use Case |
|------|---------|----------------|----------|
| QPS mode (open-loop) | `-mode qps -qps N` | Fixed send rate (independent of response speed) | Simulate external traffic arrival rate, measure latency under a given load |
| Concurrency mode (closed-loop) | `-mode concurrency -concurrency N` | Fixed in-flight request count (refill immediately on response) | Measure saturated throughput, peak QPS, concurrency capacity |

### Common Parameters

| Parameter | Description | Default |
|-----------|-------------|---------|
| `-server <ip:port>` | Server address | 127.0.0.1:8080 |
| `-mode <qps\|concurrency>` | Test mode | qps |
| `-threads <n>` | Client thread count | 4 |
| `-connections <n>` | TCP connection count (auto ≥ threads) | 1 |
| `-qps <n>` | Target QPS in QPS mode | 1000 |
| `-concurrency <n>` | In-flight request count in concurrency mode | 100 |
| `-duration <s>` | Formal test duration (seconds) | 30 |
| `-warmup <s>` | Warmup duration (seconds) | 5 |
| `-timeout <ms>` | Per-request timeout | 1000 |

### Output Metrics

Each benchmark outputs:
- **Throughput**: actual QPS, sent/received request count, success rate
- **Latency**: average, P50, P90, P99 (including both success and failure)
- **Error classification**: timeout / connection dropped / network error / server error / other
- **Concurrency estimate**: in-flight concurrency estimated via Little's Law, useful for verifying closed-loop correctness

## Performance Data

### Test Environment

| Item | Configuration |
|------|---------------|
| CPU / Memory | Single-machine loopback |
| Framework config | 4 IO threads / 4 business threads / empty business handler |
| Test load | EmptyRequest / EmptyResponse (zero payload) |
| Test duration | 20s formal + 5s warmup |

### Core Metrics

| Test item | Mode | Target | Actual QPS | Success rate | P99 latency | Avg latency |
|-----------|------|--------|------------|--------------|-------------|-------------|
| Medium-load latency | QPS | 30,000 QPS | ~30,000 | ~100% | ~0.9 ms | ~0.17 ms |
| Peak throughput | Concurrency | 200 in-flight | ~51,500 | ~99.93% | ~6.1 ms | ~3.9 ms |

> The above data are reference values under a local loopback environment. Actual performance varies with hardware, payload size, and business logic complexity.
> You can run `benchmark_client` yourself to obtain accurate data for your current environment.

## Code Formatting

The project uses `.clang-format` for code formatting:

```bash
# Format a single file
clang-format -i code/rpc/rpc_server.cc

# Format all files
find code tests -name "*.cc" -o -name "*.h" | xargs clang-format -i
```

## License

MIT
