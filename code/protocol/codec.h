#pragma once

#include <arpa/inet.h>
#include <cstring>

#include <optional>
#include <string>
#include <string_view>

namespace rpc {

class Codec {
public:
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

    template <typename T>
    static bool decode(std::string_view buffer, T &out_msg) {
        if (buffer.size() < 4) {
            return false;
        }

        uint32_t len;
        memcpy(&len, buffer.data(), 4);
        len = ntohl(len);

        if (len > buffer.size() - 4) {
            return false;
        }

        std::string data(buffer.data() + 4, len);
        return out_msg.ParseFromString(data);
    }
};

}  // namespace rpc