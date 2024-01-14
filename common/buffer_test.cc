#include "common/buffer.h"

#include <arpa/inet.h>

#include <cstddef>
#include <cstdint>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::_;
using ::tsdb2::common::Buffer;

TEST(BufferTest, Empty) {
  Buffer buffer;
  EXPECT_EQ(buffer.capacity(), 0);
  EXPECT_EQ(buffer.size(), 0);
  EXPECT_EQ(buffer.get(), nullptr);
  EXPECT_EQ(buffer.as_byte_array(), nullptr);
  EXPECT_FALSE(buffer.is_full());
}

TEST(BufferTest, Preallocated) {
  Buffer buffer{42};
  EXPECT_EQ(buffer.capacity(), 42);
  EXPECT_EQ(buffer.size(), 0);
  EXPECT_NE(buffer.get(), nullptr);
  EXPECT_EQ(buffer.get(), buffer.as_byte_array());
  EXPECT_FALSE(buffer.is_full());
}

TEST(BufferTest, TakeOwnership) {
  size_t constexpr size = 10;
  auto const data = new uint8_t[size];
  data[0] = 12;
  data[1] = 34;
  Buffer buffer{data, size, 2};
  EXPECT_EQ(buffer.capacity(), size);
  EXPECT_EQ(buffer.size(), 2);
  EXPECT_EQ(buffer.get(), data);
  EXPECT_EQ(buffer.get(), buffer.as_byte_array());
  EXPECT_FALSE(buffer.is_full());
}

TEST(BufferTest, TakeOwernshipWithDefaultLength) {
  size_t constexpr size = 10;
  auto const data = new uint8_t[size];
  data[0] = 12;
  data[1] = 34;
  Buffer buffer{data, size};
  EXPECT_EQ(buffer.capacity(), size);
  EXPECT_EQ(buffer.size(), 0);
  EXPECT_EQ(buffer.get(), data);
  EXPECT_EQ(buffer.get(), buffer.as_byte_array());
  EXPECT_FALSE(buffer.is_full());
}

TEST(BufferTest, ConstByteAt) {
  size_t constexpr size = 10;
  auto const data = new uint8_t[size];
  data[0] = 12;
  data[1] = 34;
  Buffer const buffer{data, size, 2};
  EXPECT_EQ(buffer.at<uint8_t>(0), 12);
  EXPECT_EQ(buffer.at<uint8_t>(1), 34);
}

TEST(BufferTest, ConstShortAt) {
  size_t constexpr size = 10;
  auto const data = new uint8_t[size];
  data[0] = 12;
  data[1] = 34;
  data[2] = 56;
  Buffer const buffer{data, size, 3};
  EXPECT_EQ(ntohs(buffer.at<uint16_t>(0)), 3106);
  EXPECT_EQ(ntohs(buffer.at<uint16_t>(1)), 8760);
}

TEST(BufferTest, ByteAt) {
  size_t constexpr size = 10;
  auto const data = new uint8_t[size];
  data[0] = 12;
  data[1] = 34;
  Buffer buffer{data, size, 2};
  buffer.at<uint8_t>(1) = 56;
  EXPECT_EQ(buffer.at<uint8_t>(1), 56);
}

TEST(BufferTest, ShortAt) {
  size_t constexpr size = 10;
  auto const data = new uint8_t[size];
  data[0] = 12;
  data[1] = 34;
  data[2] = 56;
  Buffer buffer{data, size, 3};
  buffer.at<uint16_t>(1) = htons(3106);
  EXPECT_EQ(ntohs(buffer.at<uint16_t>(1)), 3106);
}

TEST(BufferTest, MoveConstruct) {
  size_t constexpr size = 10;
  auto const data = new uint8_t[size];
  data[0] = 12;
  data[1] = 34;
  data[2] = 56;
  Buffer b1{data, size, 3};
  ASSERT_EQ(b1.capacity(), size);
  ASSERT_EQ(b1.size(), 3);
  ASSERT_EQ(b1.get(), data);
  {
    Buffer b2{std::move(b1)};
    EXPECT_EQ(b1.capacity(), 0);
    EXPECT_EQ(b1.size(), 0);
    EXPECT_EQ(b1.get(), nullptr);
    EXPECT_EQ(b2.capacity(), size);
    EXPECT_EQ(b2.size(), 3);
    EXPECT_EQ(b2.get(), data);
  }
  EXPECT_EQ(b1.capacity(), 0);
  EXPECT_EQ(b1.size(), 0);
  EXPECT_EQ(b1.get(), nullptr);
}

TEST(BufferTest, MoveAssign) {
  size_t constexpr size = 10;
  auto const data = new uint8_t[size];
  data[0] = 12;
  data[1] = 34;
  data[2] = 56;
  Buffer b1;
  ASSERT_EQ(b1.capacity(), 0);
  ASSERT_EQ(b1.size(), 0);
  ASSERT_EQ(b1.get(), nullptr);
  {
    Buffer b2{data, size, 3};
    b1 = std::move(b2);
    EXPECT_EQ(b1.capacity(), size);
    EXPECT_EQ(b1.size(), 3);
    EXPECT_EQ(b1.get(), data);
    EXPECT_EQ(b2.capacity(), 0);
    EXPECT_EQ(b2.size(), 0);
    EXPECT_EQ(b2.get(), nullptr);
  }
  EXPECT_EQ(b1.capacity(), size);
  EXPECT_EQ(b1.size(), 3);
  EXPECT_EQ(b1.get(), data);
  EXPECT_EQ(data[0], 12);
  EXPECT_EQ(data[1], 34);
  EXPECT_EQ(data[2], 56);
}

TEST(BufferTest, AppendInt) {
  size_t constexpr capacity = 256;
  size_t constexpr offset = 10;
  auto const data = new uint8_t[capacity];
  Buffer b{data, capacity, offset};
  b.Append<int>(123);
  EXPECT_EQ(b.capacity(), capacity);
  EXPECT_EQ(b.size(), offset + sizeof(int));
  EXPECT_EQ(b.get(), data);
  EXPECT_EQ(b.at<int>(offset), 123);
}

TEST(BufferTest, AppendLongLong) {
  size_t constexpr capacity = 256;
  size_t constexpr offset = 10;
  auto const data = new uint8_t[capacity];
  Buffer b{data, capacity, offset};
  b.Append<long long>(456);
  EXPECT_EQ(b.capacity(), capacity);
  EXPECT_EQ(b.size(), offset + sizeof(long long));
  EXPECT_EQ(b.get(), data);
  EXPECT_EQ(b.at<long long>(offset), 456);
}

TEST(BufferTest, AppendBuffer) {
  Buffer b1{sizeof(uintptr_t) * 2};
  b1.Append<uintptr_t>(123456789);
  ASSERT_EQ(b1.size(), sizeof(uintptr_t));
  {
    Buffer b2{sizeof(uintptr_t)};
    b2.Append<uintptr_t>(987654321);
    b1.Append(b2);
    EXPECT_EQ(b1.size(), sizeof(uintptr_t) * 2);
    EXPECT_EQ(b1.at<uintptr_t>(0), 123456789);
    EXPECT_EQ(b1.at<uintptr_t>(sizeof(uintptr_t)), 987654321);
    EXPECT_EQ(b2.size(), sizeof(uintptr_t));
    EXPECT_EQ(b2.at<uintptr_t>(0), 987654321);
  }
  EXPECT_EQ(b1.size(), sizeof(uintptr_t) * 2);
  EXPECT_EQ(b1.at<uintptr_t>(0), 123456789);
  EXPECT_EQ(b1.at<uintptr_t>(sizeof(uintptr_t)), 987654321);
}

TEST(BufferDeathTest, WordAppendOverflow) {
  Buffer buffer{10};
  buffer.Append<uint64_t>(123);
  EXPECT_DEATH(buffer.Append<uint64_t>(456), _);
}

TEST(BufferDeathTest, BufferAppendOverflow) {
  Buffer b1{10};
  b1.Append<uint64_t>(12);
  Buffer b2{10};
  b2.Append<uint64_t>(34);
  EXPECT_DEATH(b1.Append(b2), _);
}

TEST(BufferTest, NotFull) {
  Buffer buffer{10};
  buffer.Append<uint32_t>(42);
  ASSERT_EQ(buffer.capacity(), 10);
  ASSERT_EQ(buffer.size(), 4);
  EXPECT_FALSE(buffer.is_full());
}

TEST(BufferTest, Full) {
  Buffer buffer{10};
  buffer.Append<uint64_t>(42);
  buffer.Append<uint16_t>(42);
  ASSERT_EQ(buffer.capacity(), 10);
  ASSERT_EQ(buffer.size(), 10);
  EXPECT_TRUE(buffer.is_full());
}

TEST(BufferTest, Advance) {
  Buffer buffer{10};
  ASSERT_EQ(buffer.capacity(), 10);
  ASSERT_EQ(buffer.size(), 0);
  buffer.Advance(3);
  EXPECT_EQ(buffer.size(), 3);
  buffer.Advance(4);
  EXPECT_EQ(buffer.size(), 7);
}

TEST(BufferTest, AdvanceOverflow) {
  Buffer buffer{10};
  EXPECT_DEATH(buffer.Advance(30), _);
}

TEST(BufferTest, Release) {
  size_t constexpr size = 10;
  auto const data = new uint8_t[size];
  data[0] = 12;
  data[1] = 34;
  data[2] = 56;
  {
    Buffer buffer{data, size, 3};
    ASSERT_EQ(buffer.capacity(), size);
    ASSERT_EQ(buffer.size(), 3);
    ASSERT_EQ(buffer.get(), data);
    EXPECT_EQ(buffer.Release(), data);
    EXPECT_EQ(buffer.capacity(), 0);
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_EQ(buffer.get(), nullptr);
  }
  EXPECT_EQ(data[0], 12);
  EXPECT_EQ(data[1], 34);
  EXPECT_EQ(data[2], 56);
  delete[] data;
}

}  // namespace
