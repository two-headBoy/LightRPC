#pragma once
#include <string>
#include <type_traits>

namespace rpc {

template <typename T, typename = void>
struct is_proto_message : std::false_type {};

template <typename T>
struct is_proto_message<T,
                        std::void_t<decltype(std::declval<T>().SerializeToString(std::declval<std::string *>())),
                                    decltype(std::declval<T>().ParseFromString(std::declval<const std::string &>()))>>
    : std::true_type {};

template <typename T>
constexpr bool is_proto_message_v = is_proto_message<T>::value;

}  // namespace rpc
