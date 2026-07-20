# LightRPC

A lightweight RPC framework based on C++17. Reactor pattern + epoll asynchronous network + Protobuf + thread pool, providing strongly-typed service registration and invocation interfaces.

## Features

- **Reactor Network Model**: Single-threaded EventLoop + epoll, cross-thread tasks are woken up via `eventfd`
- **Internal EventLoop Management**: `RpcServer` / `RpcClient` create and manage EventLoop internally, automatically run event loop thread on startup
- **Strongly-typed RPC Interface**: Based on SFINAE `is_proto_message` trait, compile-time Protobuf type checking, automatic serialization/deserialization
- **Length-prefix Frame Protocol**: 4-byte length (network byte order) + Protobuf data, solving sticky/unpacking issues
- **Asynchronous Logging**: Asynchronous queue + background write thread, rolling by day/lines
- **Request Timeout Mechanism**: Client supports custom request timeout, automatically cleans up pending requests on timeout

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

    // Main thread can handle other tasks or wait for signals
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

    // Main thread can handle other tasks
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

`body` carries the business message byte stream; the framework only encodes/decodes the envelope, and business types are automatically handled by `Register` / `Call` templates.

Error code specification see [docs/error_code.md](docs/error_code.md).

## API Reference

### RpcServer

| Method | Description |
|--------|-------------|
| `RpcServer(ip, port, threadNum, maxQueueSize)` | Constructor |
| `Start()` | Start server (automatically starts event loop thread) |
| `Stop()` | Stop server (waits for event loop thread to exit) |
| `Register<ReqT, RspT>(service, method, handler)` | Register strongly-typed service method |
| `RegisterRaw(service, method, callback)` | Register raw callback method |

### RpcClient

| Method | Description |
|--------|-------------|
| `RpcClient(ip, port)` | Constructor |
| `Start()` | Start client (automatically starts event loop thread and connects) |
| `Stop()` | Stop client (disconnects and waits for event loop thread to exit) |
| `Call<ReqT, RspT>(service, method, req, callback, timeout)` | Call strongly-typed RPC |
| `CallRaw(service, method, body, callback, timeout)` | Call raw RPC |
| `IsConnected()` | Check connection status |

## Error Codes

| Range | Meaning |
|-------|---------|
| 0 | Success |
| 1001 - 1999 | Client-side errors |
| 2001 - 2999 | Server-side errors |

See [docs/error_code.md](docs/error_code.md) for detailed error code definitions.

## Code Formatting

The project uses `.clang-format` for code formatting:

```bash
# Format single file
clang-format -i code/rpc/rpc_server.cc

# Format all files
find code tests -name "*.cc" -o -name "*.h" | xargs clang-format -i
```

## License

MIT
