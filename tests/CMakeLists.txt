cmake_minimum_required(VERSION 3.10)

find_package(GTest REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}/code
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(buffer_test base/buffer/buffer_test.cc)
target_link_libraries(buffer_test
    lightrpc
    GTest::GTest
    GTest::Main
)
set_target_properties(buffer_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

add_executable(log_test base/log/log_test.cc)
target_link_libraries(log_test
    lightrpc
    GTest::GTest
    GTest::Main
)
set_target_properties(log_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

add_executable(threadpool_test base/pool/threadpool_test.cc)
target_link_libraries(threadpool_test
    lightrpc
    GTest::GTest
    GTest::Main
)
set_target_properties(threadpool_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

add_executable(rpc_client_test rpc/rpc_client_test.cc)
target_link_libraries(rpc_client_test
    lightrpc
    GTest::GTest
    GTest::Main
)
set_target_properties(rpc_client_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

add_executable(epoller_test network/epoll/epoller_test.cc)
target_link_libraries(epoller_test
    lightrpc
    GTest::GTest
    GTest::Main
)
set_target_properties(epoller_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

add_executable(channel_test network/core/channel_test.cc)
target_link_libraries(channel_test
    lightrpc
    GTest::GTest
    GTest::Main
)
set_target_properties(channel_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

add_executable(event_loop_test network/core/event_loop_test.cc)
target_link_libraries(event_loop_test
    lightrpc
    GTest::GTest
    GTest::Main
)
set_target_properties(event_loop_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

add_executable(timer_test base/timer/timer_test.cc)
target_link_libraries(timer_test
    lightrpc
    GTest::GTest
    GTest::Main
)
set_target_properties(timer_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

add_executable(rpc_service_test rpc/rpc_service_test.cc)
target_link_libraries(rpc_service_test
    lightrpc
    GTest::GTest
    GTest::Main
)
set_target_properties(rpc_service_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

add_executable(rpc_server_test rpc/rpc_server_test.cc)
target_link_libraries(rpc_server_test
    lightrpc
    GTest::GTest
    GTest::Main
)
set_target_properties(rpc_server_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)

add_library(benchmark_protos STATIC
    benchmark/proto/benchmark.pb.cc
)
target_include_directories(benchmark_protos PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/benchmark
)
target_link_libraries(benchmark_protos PUBLIC
    ${Protobuf_LIBRARIES}
)

add_executable(benchmark_server benchmark/benchmark_server.cc)
target_link_libraries(benchmark_server
    lightrpc
    benchmark_protos
)
set_target_properties(benchmark_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests/benchmark
)

add_executable(benchmark_client benchmark/benchmark_client.cc)
target_link_libraries(benchmark_client
    lightrpc
    benchmark_protos
)
set_target_properties(benchmark_client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests/benchmark
)

message(STATUS "Tests will be built in: ${CMAKE_BINARY_DIR}/bin/tests/")