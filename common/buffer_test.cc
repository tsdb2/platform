#include "common/buffer.h"

#include <arpa/inet.h>

#include <cstdint>

#include "gtest/gtest.h"

namespace {

using ::tsdb2::common::Buffer;

TEST(BufferTest, Empty) {
  Buffer buffer;
  EXPECT_EQ(buffer.capacity(), 0);
  EXPECT_EQ(buffer.size(), 0);
  EXPECT_EQ(buffer.get(), nullptr);
  EXPECT_EQ(buffer.as_byte_array(), nullptr);
}

TEST(BufferTest, Preallocated) {
  Buffer buffer{42};
  EXPECT_EQ(buffer.capacity(), 42);
  EXPECT_EQ(buffer.size(), 0);
  EXPECT_NE(buffer.get(), nullptr);
  EXPECT_EQ(buffer.get(), buffer.as_byte_array());
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

// TODO

}  // namespace
