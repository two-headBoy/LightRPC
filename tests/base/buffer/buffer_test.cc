#include <gtest/gtest.h>

#include "base/buffer/buffer.h"

namespace rpc {

TEST(BufferTest, InitialState) {
    Buffer buffer;

    EXPECT_EQ(buffer.ReadableBytes(), 0);
    EXPECT_EQ(buffer.WritableBytes(), Buffer::INITIAL_SIZE);
    EXPECT_EQ(buffer.PrependableBytes(), 0);
}

TEST(BufferTest, AppendAndRead) {
    Buffer buffer;

    const std::string test_str = "hello, world!";
    buffer.Append(test_str);

    EXPECT_EQ(buffer.ReadableBytes(), test_str.size());
    EXPECT_EQ(buffer.WritableBytes(), Buffer::INITIAL_SIZE - test_str.size());

    std::string result = buffer.ReadString(test_str.size());
    EXPECT_EQ(result, test_str);
    EXPECT_EQ(buffer.ReadableBytes(), 0);
}

TEST(BufferTest, AppendMultiple) {
    Buffer buffer;

    buffer.Append("hello");
    buffer.Append(" ");
    buffer.Append("world");

    EXPECT_EQ(buffer.ReadableBytes(), 11);

    std::string result = buffer.ReadString(5);
    EXPECT_EQ(result, "hello");

    result = buffer.ReadString(11 - 5);
    EXPECT_EQ(result, " world");
}

TEST(BufferTest, Retrieve) {
    Buffer buffer;

    buffer.Append("hello world");

    buffer.Retrieve(5);
    EXPECT_EQ(buffer.ReadableBytes(), 6);

    std::string result = buffer.ReadString(6);
    EXPECT_EQ(result, " world");
}

TEST(BufferTest, RetrieveAll) {
    Buffer buffer;

    buffer.Append("test data");
    EXPECT_EQ(buffer.ReadableBytes(), 9);

    buffer.RetrieveAll();
    EXPECT_EQ(buffer.ReadableBytes(), 0);
    EXPECT_EQ(buffer.WritableBytes(), Buffer::INITIAL_SIZE);
}

TEST(BufferTest, Peek) {
    Buffer buffer;

    buffer.Append("hello");

    EXPECT_EQ(buffer.PeekStringView(), "hello");
    EXPECT_EQ(buffer.ReadableBytes(), 5);
}

TEST(BufferTest, RetrieveAllToStr) {
    Buffer buffer;

    buffer.Append("test retrieve all");

    std::string result = buffer.RetrieveAllToStr();
    EXPECT_EQ(result, "test retrieve all");
    EXPECT_EQ(buffer.ReadableBytes(), 0);
}

TEST(BufferTest, GrowBuffer) {
    Buffer buffer(10);

    std::string large_str(20, 'a');
    buffer.Append(large_str);

    EXPECT_EQ(buffer.ReadableBytes(), 20);

    std::string result = buffer.ReadString(20);
    EXPECT_EQ(result, large_str);
}

TEST(BufferTest, MoveSemantics) {
    Buffer buffer1;
    buffer1.Append("test");

    Buffer buffer2(std::move(buffer1));
    EXPECT_EQ(buffer2.ReadableBytes(), 4);
    EXPECT_EQ(buffer1.ReadableBytes(), 0);

    Buffer buffer3;
    buffer3 = std::move(buffer2);
    EXPECT_EQ(buffer3.ReadableBytes(), 4);
    EXPECT_EQ(buffer2.ReadableBytes(), 0);
}

TEST(BufferTest, ReadInt32) {
    Buffer buffer;

    uint32_t value = 0x12345678;
    uint32_t be_value = htonl(value);
    buffer.Append(reinterpret_cast<const std::byte *>(&be_value), sizeof(be_value));

    EXPECT_EQ(buffer.ReadInt32(), value);
    EXPECT_EQ(buffer.ReadableBytes(), 0);
}

TEST(BufferTest, PeekInt32) {
    Buffer buffer;

    uint32_t value = 0x12345678;
    uint32_t be_value = htonl(value);
    buffer.Append(reinterpret_cast<const std::byte *>(&be_value), sizeof(be_value));

    EXPECT_EQ(buffer.PeekInt32(), value);
    EXPECT_EQ(buffer.ReadableBytes(), 4);
}

}  // namespace rpc

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}