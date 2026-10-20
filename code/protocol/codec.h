#pragma once

#include <arpa/inet.h>
#include <cstdint>
#include <cstring>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace rpc {

class Codec {
public:
    // 单帧最大长度，超过视为恶意帧直接断连，避免 buffer 无限增长
    static constexpr uint32_t MAX_FRAME_SIZE = 64 * 1024 * 1024;

    enum class DecodeStatus { Ok, Incomplete, Corrupted };

    template <typename T>
    static std::optional<std::string> encode(const T &msg) {
        std::string data;
        if (!msg.SerializeToString(&data)) {
            return std::nullopt;
        }

        if (data.size() > UINT32_MAX) {
            return std::nullopt;
        }

        uint32_t len = htonl(static_cast<uint32_t>(data.size()));
        std::string res;
        res.append(reinterpret_cast<const char *>(&len), 4);
        res.append(data);
        return res;
    }

    // 解析一帧。out_msg 仅在 Ok 时有意义。
    // Ok: 帧完整且解析成功，consumed = 4 + len
    // Incomplete: 数据未收齐，consumed = 0，调用方应等待更多数据
    // Corrupted: 长度非法或 protobuf 解析失败，调用方应关闭连接
    template <typename T>
    static DecodeStatus decode(std::string_view buffer, T &out_msg, size_t &consumed) {
        consumed = 0;
        if (buffer.size() < 4) {
            return DecodeStatus::Incomplete;
        }

        uint32_t len;
        memcpy(&len, buffer.data(), 4);
        len = ntohl(len);

        if (len > MAX_FRAME_SIZE) {
            return DecodeStatus::Corrupted;
        }
        if (len > buffer.size() - 4) {
            return DecodeStatus::Incomplete;
        }

        consumed = 4 + len;
        std::string data(buffer.data() + 4, len);
        if (!out_msg.ParseFromString(data)) {
            return DecodeStatus::Corrupted;
        }
        return DecodeStatus::Ok;
    }
};

}  // namespace rpc