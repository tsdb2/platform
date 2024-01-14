#include "common/buffer.h"

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
  auto const data = new uint8_t[10];
  data[0] = 12;
  data[1] = 34;
  Buffer buffer{data, 10, 2};
  EXPECT_EQ(buffer.capacity(), 10);
  EXPECT_EQ(buffer.size(), 2);
  EXPECT_EQ(buffer.get(), data);
  EXPECT_EQ(buffer.get(), buffer.as_byte_array());
}

// TODO

}  // namespace
