# LightRPC

基于 C++17 的轻量级 RPC 框架。Reactor 模式 + epoll 异步网络 + Protobuf + 线程池，提供强类型的服务注册与调用接口。

## 特性

## 特性
- **Reactor 异步网络**：当前实现为单 EventLoop + epoll；通过 eventfd 实现跨线程任务唤醒
- **内置事件循环管理**：RpcServer / RpcClient 内部托管 EventLoop，启动后自动运行事件循环线程
- **IO 与业务解耦**：网络IO逻辑运行在EventLoop，业务处理交由独立线程池，杜绝阻塞网络事件循环
- **编译期强类型 RPC**：基于 SFINAE 实现 `is_proto_message` 类型特征，编译期校验 Protobuf 请求/响应类型，自动序列化与反序列化
- **自定义帧协议**：4字节大端长度前缀 + Protobuf 二进制体，完整解决 TCP 粘包、半包问题
- **客户端长连接池**：支持多TCP长连接复用，可配置连接数量，实现多路请求并发传输
- **请求超时管控**：客户端支持自定义RPC超时，超时自动清理pending回调，避免内存泄漏
- **高性能异步日志**：日志异步队列 + 后台落盘线程，支持按天/按行数滚动切割
- **配套压测工具**：内置QPS限流压测客户端，支持自定义线程数、连接数、压测时长，输出完整延迟分位指标(P50/P90/P99)

## 依赖

- C++17 编译器
- CMake ≥ 3.10
- Protobuf
- Linux（依赖 epoll / eventfd）

## 快速开始

```bash
mkdir build && cd build
cmake .. && make -j$(nproc)

# 运行示例
./bin/examples/server_example   # 终端 1
./bin/examples/client_example   # 终端 2

# 运行单元测试
./bin/tests/rpc_client_test
./bin/tests/rpc_server_test
./bin/tests/rpc_service_test
./bin/tests/event_loop_test
./bin/tests/channel_test
./bin/tests/epoller_test
./bin/tests/timer_test

# 运行性能压测
./bin/tests/benchmark/benchmark_server   # 终端 1
./bin/tests/benchmark/benchmark_client   # 终端 2
# 高并发压测：8线程，5000 QPS，持续60秒
./bin/tests/benchmark/benchmark_client -threads 8 -qps 5000 -duration 60
```

### 服务端

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

    // 主线程可以处理其他事务，或者等待信号
    // ...

    server.Stop();
}
```

### 客户端

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

    // 主线程可以处理其他事务
    // ...

    client.Stop();
}
```

## 目录结构

```
LightRPC/
├── code/                  # 框架库
│   ├── base/              # Buffer / Log / ThreadPool / TimeWheel / ErrorCode
│   ├── network/           # EventLoop / Channel / Epoll / TCP
│   ├── protocol/          # Codec / proto_traits / RPC 信封
│   └── rpc/               # RpcServer / RpcClient / RpcService
├── examples/              # 应用层示例（含业务 .proto）
├── tests/                 # 单元测试
│   └── benchmark/         # 性能压测工具（benchmark_server / benchmark_client）
├── docs/                  # 设计文档
├── .clang-format          # Clang 格式化配置
├── CMakeLists.txt
└── README.md
```

## 协议

帧格式：`4 字节长度（网络字节序） + Protobuf 数据`

RPC 信封消息：

```protobuf
message RpcRequest  { string service_name; string method_name; string request_id; bytes body; }
message RpcResponse { string request_id; int32 code; string msg; bytes body; }
```

`body` 承载业务消息字节流；框架只编解码信封，业务类型由 `Register` / `Call` 模板自动处理。

错误码规范见 [docs/error_code.md](docs/error_code.md)。

## API 参考

### RpcServer

| 方法 | 说明 |
|------|------|
| `RpcServer(ip, port, threadNum, maxQueueSize)` | 构造函数 |
| `Start()` | 启动服务器（自动启动事件循环线程） |
| `Stop()` | 停止服务器（等待事件循环线程退出） |
| `Register<ReqT, RspT>(service, method, handler)` | 注册强类型服务方法 |
| `RegisterRaw(service, method, callback)` | 注册原始回调方法 |

### RpcClient

| 方法 | 说明 |
|------|------|
| `RpcClient(ip, port)` | 构造函数 |
| `Start()` | 启动客户端（自动启动事件循环线程并连接） |
| `Stop()` | 停止客户端（断开连接并等待事件循环线程退出） |
| `Call<ReqT, RspT>(service, method, req, callback, timeout)` | 调用强类型 RPC |
| `CallRaw(service, method, body, callback, timeout)` | 调用原始 RPC |
| `IsConnected()` | 检查连接状态 |

## 错误码

| 区间 | 含义 |
|------|------|
| 0 | 成功 |
| 1001 - 1999 | 客户端侧错误 |
| 2001 - 2999 | 服务端侧错误 |

详细错误码定义见 [docs/error_code.md](docs/error_code.md)。

## 编译规范

项目使用 `.clang-format` 进行代码格式化：

```bash
# 格式化单个文件
clang-format -i code/rpc/rpc_server.cc

# 格式化所有文件
find code tests -name "*.cc" -o -name "*.h" | xargs clang-format -i
```

## License

MIT
