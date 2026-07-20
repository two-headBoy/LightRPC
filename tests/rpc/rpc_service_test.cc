#include <gtest/gtest.h>

#include <string>

#include "protocol/rpc.pb.h"
#include "rpc/rpc_service.h"

namespace rpc {

class RpcServiceTest : public ::testing::Test {
protected:
    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(RpcServiceTest, CreateAndDestroy) {
    RpcService service;
    EXPECT_TRUE(true);
}

TEST_F(RpcServiceTest, RegisterMethod) {
    RpcService service;

    service.RegisterMethod("TestService", "TestMethod", [](const RpcRequest &, RpcResponse &resp) {
        resp.set_code(0);
        resp.set_msg("success");
    });

    EXPECT_EQ(service.ServiceCount(), 1);
    EXPECT_TRUE(service.HasService("TestService", "TestMethod"));
}

TEST_F(RpcServiceTest, RegisterMultipleMethods) {
    RpcService service;

    service.RegisterMethod("ServiceA", "Method1", [](const RpcRequest &, RpcResponse &resp) { resp.set_code(0); });
    service.RegisterMethod("ServiceA", "Method2", [](const RpcRequest &, RpcResponse &resp) { resp.set_code(0); });
    service.RegisterMethod("ServiceB", "Method1", [](const RpcRequest &, RpcResponse &resp) { resp.set_code(0); });

    EXPECT_EQ(service.ServiceCount(), 3);
    EXPECT_TRUE(service.HasService("ServiceA", "Method1"));
    EXPECT_TRUE(service.HasService("ServiceA", "Method2"));
    EXPECT_TRUE(service.HasService("ServiceB", "Method1"));
}

TEST_F(RpcServiceTest, InvokeRegisteredMethod) {
    RpcService service;

    service.RegisterMethod("TestService", "TestMethod", [](const RpcRequest &, RpcResponse &resp) {
        resp.set_code(0);
        resp.set_msg("success");
        resp.set_body("response_data");
    });

    RpcRequest req;
    req.set_service_name("TestService");
    req.set_method_name("TestMethod");
    req.set_request_id("test_req_id");

    RpcResponse resp;
    bool result = service.Invoke(req, resp);

    EXPECT_TRUE(result);
    EXPECT_EQ(resp.code(), 0);
    EXPECT_EQ(resp.msg(), "success");
    EXPECT_EQ(resp.body(), "response_data");
}

TEST_F(RpcServiceTest, InvokeUnregisteredMethod) {
    RpcService service;

    RpcRequest req;
    req.set_service_name("UnknownService");
    req.set_method_name("UnknownMethod");
    req.set_request_id("test_req_id");

    RpcResponse resp;
    bool result = service.Invoke(req, resp);

    EXPECT_FALSE(result);
}

TEST_F(RpcServiceTest, HasServiceReturnsFalseForUnknown) {
    RpcService service;

    EXPECT_FALSE(service.HasService("UnknownService", "TestMethod"));
    EXPECT_FALSE(service.HasService("TestService", "UnknownMethod"));
}

TEST_F(RpcServiceTest, OverrideExistingMethod) {
    RpcService service;

    service.RegisterMethod("TestService", "TestMethod",
                           [](const RpcRequest &, RpcResponse &resp) { resp.set_code(1); });

    service.RegisterMethod("TestService", "TestMethod",
                           [](const RpcRequest &, RpcResponse &resp) { resp.set_code(2); });

    EXPECT_EQ(service.ServiceCount(), 1);

    RpcRequest req;
    req.set_service_name("TestService");
    req.set_method_name("TestMethod");
    req.set_request_id("test_req_id");

    RpcResponse resp;
    service.Invoke(req, resp);

    EXPECT_EQ(resp.code(), 2);
}

TEST_F(RpcServiceTest, ServiceCountIsZeroInitially) {
    RpcService service;
    EXPECT_EQ(service.ServiceCount(), 0);
}

}  // namespace rpc

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}