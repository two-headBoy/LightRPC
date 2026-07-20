#include "rpc/rpc_service.h"

#include "base/error_code/rpc_error_code.h"

namespace rpc {

void RpcService::RegisterMethod(const std::string &service_name, const std::string &method_name,
                                ServiceCallback callback) {
    std::string key = MakeMethodKey(service_name, method_name);
    std::lock_guard<std::mutex> locker(mtx_);
    methods_[key] = std::move(callback);
}

bool RpcService::Invoke(const RpcRequest &request, RpcResponse &response) {
    std::string key = MakeMethodKey(request.service_name(), request.method_name());
    ServiceCallback target_cb;

    {
        std::lock_guard<std::mutex> locker(mtx_);
        auto it = methods_.find(key);
        if (it == methods_.end()) {
            response.set_code(static_cast<int32_t>(RpcErrorCode::SERVER_SERVICE_NOT_FOUND));
            response.set_msg("Service not found: " + key);
            response.set_request_id(request.request_id());
            return false;
        }
        target_cb = it->second;
    }

    try {
        target_cb(request, response);
        response.set_request_id(request.request_id());
        return true;
    } catch (const std::exception &e) {
        response.set_code(static_cast<int32_t>(RpcErrorCode::SERVER_INTERNAL_ERROR));
        response.set_msg(e.what());
        response.set_request_id(request.request_id());
        return false;
    }
}

bool RpcService::HasService(const std::string &service_name, const std::string &method_name) const {
    std::string key = MakeMethodKey(service_name, method_name);
    std::lock_guard<std::mutex> lock(mtx_);
    return methods_.find(key) != methods_.end();
}

std::string RpcService::MakeMethodKey(const std::string &service_name, const std::string &method_name) {
    return service_name + "." + method_name;
}

}  // namespace rpc
