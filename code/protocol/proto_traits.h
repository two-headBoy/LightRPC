#pragma once

#include <type_traits>
#include <google/protobuf/message.h>

namespace rpc {

template <typename T>
struct is_proto_message : std::is_base_of<::google::protobuf::Message, T> {};

template <typename T>
constexpr bool is_proto_message_v = is_proto_message<T>::value;

}  // namespace rpc
