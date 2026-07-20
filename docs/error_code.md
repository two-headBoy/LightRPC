# LightRPC 错误码规范

## 概述

LightRPC 采用统一的错误码体系，所有 RPC 调用的状态通过 `RpcResponse.code` 字段返回。`0` 表示成功，非零表示失败。

错误码按来源分段：

| 区间 | 含义 |
|------|------|
| 0 | 成功 |
| 1001 - 1999 | 客户端侧错误 |
| 2001 - 2999 | 服务端侧错误 |

## 错误码定义

### 成功

| 错误码 | 枚举值 | 说明 |
|--------|--------|------|
| 0 | `RpcErrorCode::OK` | 调用成功 |

### 客户端错误（1xxx）

| 错误码 | 枚举值 | 说明 | 触发场景 |
|--------|--------|------|----------|
| 1001 | `CLIENT_REQUEST_TIMEOUT` | 请求超时 | 在 `timeout_ms` 内未收到服务端响应 |
| 1002 | `CLIENT_CONNECTION_DISCONNECTED` | 连接断开 | TCP 连接在等待响应期间断开 |
| 1003 | `CLIENT_NETWORK_ERROR` | 网络 IO 错误 | 底层 socket 发生 IO 错误 |
| 1004 | `CLIENT_NOT_RUNNING` | 客户端未运行 | 客户端已停止或未连接时发起调用 |
| 1005 | `CLIENT_REQUEST_SERIALIZE_FAILED` | 请求序列化失败 | `req.SerializeToString()` 失败 |
| 1006 | `CLIENT_ENCODE_FAILED` | 报文编码失败 | `Codec::encode()` 编码 RPC 报文失败 |
| 1007 | `CLIENT_RESPONSE_PARSE_FAILED` | 响应解析失败 | 响应 body 无法解析为目标 protobuf 类型 |

### 服务端错误（2xxx）

| 错误码 | 枚举值 | 说明 | 触发场景 |
|--------|--------|------|----------|
| 2001 | `SERVER_SERVICE_NOT_FOUND` | 服务/方法不存在 | 请求的 `service_name.method_name` 未注册 |
| 2002 | `SERVER_INTERNAL_ERROR` | 服务端内部错误 | 业务 handler 抛出未捕获异常 |
| 2003 | `SERVER_BUSY` | 服务端繁忙 | 线程池任务队列已满，拒绝新请求 |
| 2004 | `SERVER_REQUEST_PARSE_FAILED` | 请求体解析失败 | 请求 body 无法解析为业务 handler 的 ReqT 类型 |
| 2005 | `SERVER_RESPONSE_SERIALIZE_FAILED` | 响应体序列化失败 | 业务 handler 的 RspT 无法序列化为字符串 |

## 头文件

错误码定义位于 [rpc_error_code.h](../code/base/error_code/rpc_error_code.h)：

```cpp
#include "base/error_code/rpc_error_code.h"
```

提供辅助函数将错误码转为字符串：

```cpp
std::string RpcErrorCodeToString(RpcErrorCode code);
```

## 使用示例

### 客户端判断调用结果

```cpp
client.Call<GetUserRequest, GetUserResponse>(
    "UserService", "GetUser", req,
    [](const GetUserResponse& resp) {
        // 注意：Call 模板回调仅传业务 RspT，
        // 若需要访问 code/msg，请使用 CallRaw 接口
        // ...
    });
```

### 使用 CallRaw 获取完整状态

```cpp
client.CallRaw("UserService", "GetUser", body,
    [](const RpcResponse& resp) {
        if (resp.code() == static_cast<int32_t>(RpcErrorCode::OK)) {
            GetUserResponse user_resp;
            user_resp.ParseFromString(resp.body());
            // ...
        } else {
            std::cerr << "RPC failed: code=" << resp.code()
                      << ", msg=" << resp.msg() << std::endl;
        }
    }, 5000);
```

### 服务端业务错误

当前 `Register<>` 模板中，业务 handler 只能通过 `RspT` 字段传递业务含义的结果。
若需要返回业务层面的错误码，建议在业务 proto 中定义自己的状态字段，例如：

```protobuf
message GetUserResponse {
  int32  code    = 1;  // 业务错误码
  string msg     = 2;  // 业务错误信息
  int64  user_id = 3;
  string name    = 4;
}
```

## 设计原则

1. **分段清晰**：1xxx 客户端、2xxx 服务端，从错误码数值即可判断问题来源
2. **零值成功**：`code == 0` 表示成功，与大多数 RPC 框架习惯一致
3. **信息充足**：每个错误码都伴随人类可读的 `msg` 字段，便于排查
4. **可扩展**：预留充足号段，未来新增错误码不会打乱现有体系
