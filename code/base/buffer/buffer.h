#pragma once

#include <arpa/inet.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace rpc {

class Buffer {
public:
    static constexpr size_t INITIAL_SIZE = 4096;

    explicit Buffer(size_t initialSize = INITIAL_SIZE);
    ~Buffer() = default;

    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;
    Buffer(Buffer &&other) noexcept;
    Buffer &operator=(Buffer &&other) noexcept;

    size_t ReadableBytes() const noexcept { return writePos_ - readPos_; }
    size_t WritableBytes() const noexcept { return buffer_.size() - writePos_; }
    size_t PrependableBytes() const noexcept { return readPos_; }

    const std::byte *Peek() const noexcept { return begin() + readPos_; }
    std::byte *Peek() noexcept { return begin() + readPos_; }

    void Retrieve(size_t len) noexcept;
    void RetrieveAll() noexcept;

    const std::byte *WriteBegin() const noexcept { return begin() + writePos_; }
    std::byte *WriteBegin() noexcept { return begin() + writePos_; }

    void HasWritten(size_t len) noexcept { writePos_ += len; }
    void EnsureWritable(size_t len);

    // 二进制写入接口
    void Append(const std::byte *data, size_t len);
    void Append(const std::string &str);
    void Append(const char *data, size_t len);

    std::string ReadString(size_t n);
    std::string_view PeekStringView() const noexcept;
    std::string RetrieveAllToStr();

    uint32_t ReadInt32();
    uint32_t PeekInt32() const;

    ssize_t ReadFd(int fd, int *savedErrno);
    ssize_t WriteFd(int fd, int *savedErrno);

    void swap(Buffer &other) noexcept;

private:
    std::byte *begin() noexcept { return buffer_.data(); }
    const std::byte *begin() const noexcept { return buffer_.data(); }

    void makeSpace(size_t needed);

    std::vector<std::byte> buffer_;
    size_t readPos_ = 0;
    size_t writePos_ = 0;
};

}  // namespace rpc
