# LightRPC

基于 C++17 的轻量级 RPC 框架。Reactor 模式 + epoll 异步网络 + Protobuf + 线程池，提供强类型的服务注册与调用接口。

## 特性

- **Reactor 网络模型**：单线程 EventLoop + epoll，跨线程任务通过 `eventfd` 唤醒
- **内部 EventLoop 管理**：`RpcServer` / `RpcClient` 内部创建和管理 EventLoop，启动时自动运行事件循环线程
- **强类型 RPC 接口**：基于 SFINAE 的 `is_proto_message` trait，编译期校验 Protobuf 类型，自动完成序列化/反序列化
- **长度前缀帧协议**：4 字节长度（网络字节序）+ Protobuf 数据，解决粘包/半包
- **异步日志**：异步队列 + 后台写线程，按天/按行数滚动
- **请求超时机制**：客户端支持自定义请求超时时间，超时自动清理待处理请求

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
