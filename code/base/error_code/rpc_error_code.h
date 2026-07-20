#pragma once

#include <cstdint>
#include <string>

namespace rpc {

enum class RpcErrorCode : int32_t {
    OK = 0,

    // 1xxx - 客户端错误
    CLIENT_REQUEST_TIMEOUT = 1001,
    CLIENT_CONNECTION_DISCONNECTED = 1002,
    CLIENT_NETWORK_ERROR = 1003,
    CLIENT_NOT_RUNNING = 1004,
    CLIENT_REQUEST_SERIALIZE_FAILED = 1005,
    CLIENT_ENCODE_FAILED = 1006,
    CLIENT_RESPONSE_PARSE_FAILED = 1007,

    // 2xxx - 服务端错误
    SERVER_SERVICE_NOT_FOUND = 2001,
    SERVER_INTERNAL_ERROR = 2002,
    SERVER_BUSY = 2003,
    SERVER_REQUEST_PARSE_FAILED = 2004,
    SERVER_RESPONSE_SERIALIZE_FAILED = 2005,
};

inline std::string RpcErrorCodeToString(RpcErrorCode code) {
    switch (code) {
        case RpcErrorCode::OK:
            return "OK";
        case RpcErrorCode::CLIENT_REQUEST_TIMEOUT:
            return "CLIENT_REQUEST_TIMEOUT";
        case RpcErrorCode::CLIENT_CONNECTION_DISCONNECTED:
            return "CLIENT_CONNECTION_DISCONNECTED";
        case RpcErrorCode::CLIENT_NETWORK_ERROR:
            return "CLIENT_NETWORK_ERROR";
        case RpcErrorCode::CLIENT_NOT_RUNNING:
            return "CLIENT_NOT_RUNNING";
        case RpcErrorCode::CLIENT_REQUEST_SERIALIZE_FAILED:
            return "CLIENT_REQUEST_SERIALIZE_FAILED";
        case RpcErrorCode::CLIENT_ENCODE_FAILED:
            return "CLIENT_ENCODE_FAILED";
        case RpcErrorCode::CLIENT_RESPONSE_PARSE_FAILED:
            return "CLIENT_RESPONSE_PARSE_FAILED";
        case RpcErrorCode::SERVER_SERVICE_NOT_FOUND:
            return "SERVER_SERVICE_NOT_FOUND";
        case RpcErrorCode::SERVER_INTERNAL_ERROR:
            return "SERVER_INTERNAL_ERROR";
        case RpcErrorCode::SERVER_BUSY:
            return "SERVER_BUSY";
        case RpcErrorCode::SERVER_REQUEST_PARSE_FAILED:
            return "SERVER_REQUEST_PARSE_FAILED";
        case RpcErrorCode::SERVER_RESPONSE_SERIALIZE_FAILED:
            return "SERVER_RESPONSE_SERIALIZE_FAILED";
        default:
            return "UNKNOWN";
    }
}

}  // namespace rpc
