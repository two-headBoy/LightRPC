#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "protocol/rpc.pb.h"

namespace rpc {

class RpcService {
public:
    using ServiceCallback = std::function<void(const RpcRequest &, RpcResponse &)>;

    RpcService() = default;
    ~RpcService() = default;

    RpcService(const RpcService &) = delete;
    RpcService &operator=(const RpcService &) = delete;

    void RegisterMethod(const std::string &service_name, const std::string &method_name, ServiceCallback callback);

    bool Invoke(const RpcRequest &request, RpcResponse &response);

    bool HasService(const std::string &service_name, const std::string &method_name) const;

    size_t ServiceCount() const { return methods_.size(); }

private:
    static std::string MakeMethodKey(const std::string &service_name, const std::string &method_name);

    std::unordered_map<std::string, ServiceCallback> methods_;

    mutable std::mutex mtx_;
};

}  // namespace rpc
