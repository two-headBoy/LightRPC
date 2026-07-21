# LightRPC

基于 C++17 的轻量级 RPC 框架。采用 Reactor 模式，以 epoll 驱动异步网络、Protobuf 承载业务消息、线程池隔离业务计算，提供编译期强类型的服务注册与调用接口。

## 特性

### 网络与并发
- **Reactor 异步网络**：单 EventLoop + epoll；通过 eventfd 实现跨线程任务唤醒，所有网络事件在 IO 线程串行处理，无锁竞争
- **分层的 EventLoop 管理**：底层 TcpServer / TcpClient **不持有** EventLoop，由上层 RpcServer / RpcClient 注入并托管生命周期，组件可复用
- **IO 与业务解耦**：网络 IO 运行在 EventLoop 线程，业务处理交给独立线程池（带背压 + 最大队列长度），避免业务慢调用阻塞网络事件循环
- **连接级线程安全**：每个 `RpcClient` 绑定一个 IO 线程；多连接场景下应用层可创建多个实例共享使用

### RPC 与协议
- **编译期强类型 RPC**：基于 SFINAE 实现 `is_proto_message` 类型萃取，编译期校验 Protobuf 请求/响应类型，自动序列化与反序列化
- **自定义帧协议**：4 字节大端长度前缀 + Protobuf 二进制体，Codec 层只做字节流编解码，不感知业务内容，天然解决 TCP 粘包/半包
- **统一错误码体系**：0 成功 / 1xxx 客户端错 / 2xxx 服务端错，业务与框架共用同一份枚举
- **请求超时管控**：时间轮定时器，超时自动清理 pending 回调并通知调用方，避免内存泄漏与请求悬挂
- **断连感知**：连接断开时主动回调所有 pending 请求，而不是静默清空

### 工程与工具
- **高性能异步日志**：异步队列 + 后台落盘线程，支持按天 / 按行数滚动切割
- **双模式压测工具**：内置 benchmark_client，支持 **QPS 模式（开环）** 与 **并发模式（闭环）**，输出 P50 / P90 / P99 延迟分位与错误分类统计
- **完整单元测试**：覆盖 Buffer、Log、ThreadPool、EventLoop、Channel、Epoller、Timer、RpcClient、RpcServer、RpcService 等核心模块

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
./bin/tests/benchmark/benchmark_server           # 终端 1

# QPS 模式（开环压测）：4 线程，10000 QPS，持续 30 秒
./bin/tests/benchmark/benchmark_client -mode qps -threads 4 -connections 4 -qps 10000 -duration 30

# 并发模式（闭环压测）：4 线程，200 在途，持续 30 秒
./bin/tests/benchmark/benchmark_client -mode concurrency -threads 4 -connections 4 -concurrency 200 -duration 30
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

## 压测工具

`tests/benchmark/` 下提供 benchmark_server 和 benchmark_client，支持两种测试模式：

### 两种模式

| 模式 | 命令 | 控制目标 | 适用场景 |
|------|------|----------|----------|
| QPS 模式（开环） | `-mode qps -qps N` | 固定发送速率（与响应速度无关） | 模拟外部流量到达率，测系统在给定负载下的延迟 |
| 并发模式（闭环） | `-mode concurrency -concurrency N` | 固定在途请求数（响应回来立即补发） | 测系统饱和吞吐、峰值 QPS、并发能力 |

### 常用参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-server <ip:port>` | 服务端地址 | 127.0.0.1:8080 |
| `-mode <qps\|concurrency>` | 测试模式 | qps |
| `-threads <n>` | 客户端线程数 | 4 |
| `-connections <n>` | TCP 连接数（自动 ≥ threads） | 1 |
| `-qps <n>` | QPS 模式下的目标 QPS | 1000 |
| `-concurrency <n>` | 并发模式下的在途请求数 | 100 |
| `-duration <s>` | 正式测试时长（秒） | 30 |
| `-warmup <s>` | 预热时长（秒） | 5 |
| `-timeout <ms>` | 单次请求超时 | 1000 |

### 输出指标

每次压测输出：
- **吞吐**：实际 QPS、发送/接收请求数、成功率
- **延迟**：平均、P50、P90、P99（含成功与失败）
- **错误分类**：超时 / 连接断开 / 网络错误 / 服务端错误 / 其他
- **并发估算**：按 Little's Law 估算的在途并发数，可用于校验闭环正确性

## 性能数据

### 测试环境

| 项 | 配置 |
|----|------|
| CPU / 内存 | 单机回环 (loopback) |
| 框架配置 | 4 IO 线程 / 4 业务线程 / 空业务 handler |
| 测试负载 | EmptyRequest / EmptyResponse（零 payload） |
| 测试时长 | 20s 正式 + 5s 预热 |

### 核心指标

| 测试项 | 模式 | 目标 | 实际 QPS | 成功率 | P99 延迟 | 平均延迟 |
|--------|------|------|----------|--------|----------|----------|
| 中载延迟 | QPS | 30,000 QPS | ~30,000 | ~100% | ~0.9 ms | ~0.17 ms |
| 峰值吞吐 | Concurrency | 200 在途 | ~51,500 | ~99.93% | ~6.1 ms | ~3.9 ms |

> 以上数据为本地回环环境下的参考值，实际性能因硬件、payload 大小、业务逻辑复杂度而异。
> 可通过 `benchmark_client` 自行压测获得当前环境的准确数据。

## 代码规范

项目使用 `.clang-format` 进行代码格式化：

```bash
# 格式化单个文件
clang-format -i code/rpc/rpc_server.cc

# 格式化所有文件
find code tests -name "*.cc" -o -name "*.h" | xargs clang-format -i
```
