cmake_minimum_required(VERSION 3.10)

# LightRPC 示例程序
#
# 架构约定：examples 属于「应用层」。
# 业务相关的 .proto 文件（如 user.proto、math.proto）及其预编译产物
# .pb.cc/.pb.h 必须放在本目录下，不得放入 code/protocol/，
# 以保证框架库 lightrpc 不依赖任何具体业务 schema。
#
# 重新生成 .pb.h/.pb.cc（手动预编译，不入 CMake）：
#   protoc -I proto --cpp_out=proto proto/user.proto proto/math.proto

# 业务 protobuf 消息库：包含应用层 .proto 的预编译产物
add_library(example_protos STATIC
    proto/user.pb.cc
    proto/math.pb.cc)
target_include_directories(example_protos PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(example_protos PUBLIC
    ${Protobuf_LIBRARIES})

add_executable(server_example server_example.cc)
target_link_libraries(server_example lightrpc example_protos)
set_target_properties(server_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples
)

add_executable(client_example client_example.cc)
target_link_libraries(client_example lightrpc example_protos)
set_target_properties(client_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples
)

message(STATUS "Examples will be built in: ${CMAKE_BINARY_DIR}/bin/examples/")
