cmake_minimum_required(VERSION 3.10)
project(LightRPC VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)

message(STATUS "Protobuf version: ${Protobuf_VERSION}")

include_directories(
    ${CMAKE_SOURCE_DIR}/code
    ${Protobuf_INCLUDE_DIRS}
)

set(LIGHTRPC_SOURCES
    code/base/buffer/buffer.cc
    code/base/timer/timer.cc
    code/base/log/log.cc

    code/network/core/event_loop.cc
    code/network/core/channel.cc

    code/network/epoll/epoller.cc

    code/network/tcp/tcp_connection.cc
    code/network/tcp/tcp_server.cc
    code/network/tcp/tcp_client.cc

    code/protocol/rpc.pb.cc

    code/rpc/rpc_server.cc
    code/rpc/rpc_client.cc
    code/rpc/rpc_service.cc
)

add_library(lightrpc STATIC ${LIGHTRPC_SOURCES})

target_link_libraries(lightrpc
    PUBLIC
    ${Protobuf_LIBRARIES}
    Threads::Threads
)

target_include_directories(lightrpc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/code>
    $<INSTALL_INTERFACE:include>
)

option(BUILD_EXAMPLES "Build example programs" ON)

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

install(TARGETS lightrpc
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY code/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

message(STATUS "")
message(STATUS "=== LightRPC Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build Examples: ${BUILD_EXAMPLES}")
message(STATUS "")
message(STATUS "Output directories:")
message(STATUS "  Library: ${CMAKE_BINARY_DIR}/lib/")
message(STATUS "  Binary:  ${CMAKE_BINARY_DIR}/bin/")
message(STATUS "")