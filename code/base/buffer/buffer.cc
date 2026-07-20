#include "buffer.h"

#include <sys/uio.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <utility>

namespace rpc {

Buffer::Buffer(size_t initialSize) : buffer_(initialSize) { assert(initialSize > 0); }

Buffer::Buffer(Buffer &&other) noexcept
    : buffer_(std::move(other.buffer_)), readPos_(std::exchange(other.readPos_, 0)),
      writePos_(std::exchange(other.writePos_, 0)) {}

Buffer &Buffer::operator=(Buffer &&other) noexcept {
    if (this != &other) {
        buffer_ = std::move(other.buffer_);
        readPos_ = std::exchange(other.readPos_, 0);
        writePos_ = std::exchange(other.writePos_, 0);
    }
    return *this;
}

void Buffer::Retrieve(size_t len) noexcept {
    assert(len <= ReadableBytes());
    if (len < ReadableBytes()) {
        readPos_ += len;
    } else {
        RetrieveAll();
    }
}

void Buffer::RetrieveAll() noexcept {
    readPos_ = 0;
    writePos_ = 0;
}

void Buffer::EnsureWritable(size_t len) {
    if (WritableBytes() < len) {
        makeSpace(len);
    }
    assert(WritableBytes() >= len);
}

void Buffer::Append(const std::byte *data, size_t len) {
    EnsureWritable(len);
    std::copy(data, data + len, WriteBegin());
    HasWritten(len);
}

void Buffer::Append(const std::string &str) { Append(reinterpret_cast<const std::byte *>(str.data()), str.size()); }

void Buffer::Append(const char *data, size_t len) { Append(reinterpret_cast<const std::byte *>(data), len); }

std::string Buffer::ReadString(size_t n) {
    assert(n <= ReadableBytes());
    std::string result(reinterpret_cast<const char *>(Peek()), n);
    Retrieve(n);
    return result;
}

std::string_view Buffer::PeekStringView() const noexcept {
    return std::string_view(reinterpret_cast<const char *>(Peek()), ReadableBytes());
}

std::string Buffer::RetrieveAllToStr() {
    std::string result(reinterpret_cast<const char *>(Peek()), ReadableBytes());
    RetrieveAll();
    return result;
}

uint32_t Buffer::ReadInt32() {
    assert(ReadableBytes() >= 4);
    uint32_t result = PeekInt32();
    Retrieve(4);
    return result;
}

uint32_t Buffer::PeekInt32() const {
    assert(ReadableBytes() >= 4);
    uint32_t be32;
    ::memcpy(&be32, Peek(), sizeof be32);
    return ntohl(be32);
}

ssize_t Buffer::ReadFd(int fd, int *savedErrno) {
    char extrabuf[65536];
    struct iovec vec[2];
    const size_t writable = WritableBytes();
    vec[0].iov_base = WriteBegin();  // std::byte* -> void* (allowed)
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const ssize_t n = readv(fd, vec, 2);
    if (n < 0) {
        *savedErrno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        writePos_ += n;
    } else {
        writePos_ = buffer_.size();
        Append(reinterpret_cast<const std::byte *>(extrabuf), static_cast<size_t>(n - writable));
    }
    return n;
}

ssize_t Buffer::WriteFd(int fd, int *savedErrno) {
    const size_t readable = ReadableBytes();
    const ssize_t n = write(fd, Peek(), readable);  // std::byte* -> const void*
    if (n < 0) {
        *savedErrno = errno;
    }
    return n;
}

void Buffer::swap(Buffer &other) noexcept {
    buffer_.swap(other.buffer_);
    std::swap(readPos_, other.readPos_);
    std::swap(writePos_, other.writePos_);
}

void Buffer::makeSpace(size_t needed) {
    if (WritableBytes() + PrependableBytes() < needed) {
        size_t newSize = std::max(writePos_ + needed, needed + INITIAL_SIZE);
        buffer_.resize(newSize);
    } else {
        size_t readable = ReadableBytes();
        std::copy(begin() + readPos_, begin() + writePos_, begin());
        readPos_ = 0;
        writePos_ = readable;
        assert(readable == ReadableBytes());
    }
}

}  // namespace rpc
