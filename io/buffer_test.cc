#include "io/buffer.h"

#include <arpa/inet.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <tuple>

#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Property;
using ::tsdb2::io::Buffer;

using Span = absl::Span<uint8_t const>;

struct TestData {
  auto tie() const { return std::tie(field1, field2, field3, field4); }

  friend bool operator==(TestData const& lhs, TestData const& rhs) {
    return lhs.tie() == rhs.tie();
  }

  friend bool operator!=(TestData const& lhs, TestData const& rhs) {
    return lhs.tie() != rhs.tie();
  }

  uint64_t field1;
  uint32_t field2;
  uint16_t field3;
  uint8_t field4;
};

TEST(BufferTest, Empty) {
  Buffer buffer;
  EXPECT_EQ(buffer.capacity(), 0);
  EXPECT_EQ(buffer.size(), 0);
  EXPECT_TRUE(buffer.empty());
  EXPECT_EQ(buffer.get(), nullptr);
  EXPECT_EQ(buffer.as_byte_array(), nullptr);
  EXPECT_EQ(buffer.as_array<int32_t>(), nullptr);
  EXPECT_EQ(buffer.as_char_array(), nullptr);
  EXPECT_FALSE(buffer.is_full());
}

TEST(BufferTest, Preallocated) {
  Buffer buffer{42};
  EXPECT_EQ(buffer.capacity(), 42);
  EXPECT_EQ(buffer.size(), 0);
  EXPECT_TRUE(buffer.empty());
  EXPECT_NE(buffer.get(), nullptr);
  EXPECT_EQ(buffer.get(), buffer.as_byte_array());
  EXPECT_EQ(buffer.get(), buffer.as_array<int32_t>());
  EXPECT_EQ(buffer.get(), buffer.as_char_array());
  EXPECT_FALSE(buffer.is_full());
}

TEST(BufferTest, TakeOwnership) {
  size_t constexpr capacity = 10;
  size_t constexpr size = 2;
  auto const data = new uint8_t[capacity];
  data[0] = 12;
  data[1] = 34;
  Buffer buffer{data, capacity, size};
  EXPECT_EQ(buffer.capacity(), capacity);
  EXPECT_EQ(buffer.size(), 2);
  EXPECT_FALSE(buffer.empty());
  EXPECT_EQ(buffer.get(), data);
  EXPECT_EQ(buffer.get(), buffer.as_byte_array());
  EXPECT_EQ(buffer.get(), buffer.as_array<uint8_t>());
  EXPECT_EQ(buffer.get(), buffer.as_char_array());
  EXPECT_THAT(buffer.span(), AllOf(Property(&Span::data, data), Property(&Span::size, size)));
  EXPECT_FALSE(buffer.is_full());
}

TEST(BufferTest, CopyFromCaller) {
  std::string_view constexpr kData = "lorem ipsum dolor sit amet";
  Buffer buffer{kData.data(), kData.size()};
  EXPECT_EQ(buffer.capacity(), kData.size());
  EXPECT_EQ(buffer.size(), kData.size());
  EXPECT_FALSE(buffer.empty());
  EXPECT_NE(buffer.get(), kData.data());
  EXPECT_EQ(buffer.get(), buffer.as_byte_array());
  EXPECT_EQ(buffer.get(), buffer.as_array<char>());
  EXPECT_EQ(buffer.get(), buffer.as_char_array());
  EXPECT_THAT(buffer.span(), ElementsAreArray(kData));
  EXPECT_TRUE(buffer.is_full());
  EXPECT_TRUE(kData == std::string_view(buffer.as_char_array(), kData.size()));
  EXPECT_TRUE(kData == std::string_view(buffer.as_array<char>(), kData.size()));
}

TEST(BufferTest, Spans) {
  std::string_view constexpr kData = "lorem ipsum dolor sit amet";
  std::string_view constexpr kSubData1 = kData.substr(10);
  std::string_view constexpr kSubData2 = kData.substr(10, 11);
  Buffer buffer{kData.data(), kData.size()};
  EXPECT_THAT(buffer.span(), ElementsAreArray(kData.data(), kData.size()));
  EXPECT_THAT(buffer.span(10), ElementsAreArray(kSubData1.data(), kSubData1.size()));
  EXPECT_THAT(buffer.span(10, 11), ElementsAreArray(kSubData2.data(), kSubData2.size()));
}

TEST(BufferTest, TypedSpan) {
  TestData data[3] = {
      TestData{.field1 = 1, .field2 = 2, .field3 = 3, .field4 = 4},
      TestData{.field1 = 2, .field2 = 3, .field3 = 4, .field4 = 5},
      TestData{.field1 = 3, .field2 = 4, .field3 = 5, .field4 = 6},
  };
  Buffer buffer{sizeof(data)};
  buffer.MemCpy(&data, sizeof(data));
  EXPECT_THAT(buffer.span<TestData>(),
              ElementsAreArray({
                  TestData{.field1 = 1, .field2 = 2, .field3 = 3, .field4 = 4},
                  TestData{.field1 = 2, .field2 = 3, .field3 = 4, .field4 = 5},
                  TestData{.field1 = 3, .field2 = 4, .field3 = 5, .field4 = 6},
              }));
}

TEST(BufferTest, TypedSpansWithOffset) {
  std::string_view constexpr prefix = "12345";
  TestData data[3] = {
      TestData{.field1 = 1, .field2 = 2, .field3 = 3, .field4 = 4},
      TestData{.field1 = 2, .field2 = 3, .field3 = 4, .field4 = 5},
      TestData{.field1 = 3, .field2 = 4, .field3 = 5, .field4 = 6},
  };
  Buffer buffer{prefix.size() + sizeof(data)};
  buffer.MemCpy(prefix.data(), prefix.size());
  buffer.MemCpy(&data, sizeof(data));
  EXPECT_THAT(buffer.span<TestData>(5),
              ElementsAreArray({
                  TestData{.field1 = 1, .field2 = 2, .field3 = 3, .field4 = 4},
                  TestData{.field1 = 2, .field2 = 3, .field3 = 4, .field4 = 5},
                  TestData{.field1 = 3, .field2 = 4, .field3 = 5, .field4 = 6},
              }));
  EXPECT_THAT(buffer.span<TestData>(5, 3),
              ElementsAreArray({
                  TestData{.field1 = 1, .field2 = 2, .field3 = 3, .field4 = 4},
                  TestData{.field1 = 2, .field2 = 3, .field3 = 4, .field4 = 5},
                  TestData{.field1 = 3, .field2 = 4, .field3 = 5, .field4 = 6},
              }));
  EXPECT_THAT(buffer.span<TestData>(5, 2),
              ElementsAreArray({
                  TestData{.field1 = 1, .field2 = 2, .field3 = 3, .field4 = 4},
                  TestData{.field1 = 2, .field2 = 3, .field3 = 4, .field4 = 5},
              }));
  EXPECT_THAT(buffer.span<TestData>(5, 1),
              ElementsAreArray({
                  TestData{.field1 = 1, .field2 = 2, .field3 = 3, .field4 = 4},
              }));
  EXPECT_THAT(buffer.span<TestData>(5, 0), IsEmpty());
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

TEST(BufferTest, AsConstData) {
  TestData const data{
      .field1 = 0xFEDCBA9876543210,
      .field2 = 0x01234567,
      .field3 = 0x3210,
      .field4 = 0x12,
  };
  Buffer const buffer{&data, sizeof(data)};
  EXPECT_THAT(buffer.as<TestData>(),
              AllOf(Field(&TestData::field1, data.field1), Field(&TestData::field2, data.field2),
                    Field(&TestData::field3, data.field3), Field(&TestData::field4, data.field4)));
}

TEST(BufferTest, AsData) {
  TestData const data{
      .field1 = 0xFEDCBA9876543210,
      .field2 = 0x01234567,
      .field3 = 0x3210,
      .field4 = 0x12,
  };
  Buffer buffer{&data, sizeof(data)};
  EXPECT_THAT(buffer.as<TestData>(),
              AllOf(Field(&TestData::field1, data.field1), Field(&TestData::field2, data.field2),
                    Field(&TestData::field3, data.field3), Field(&TestData::field4, data.field4)));
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
  ASSERT_FALSE(b1.empty());
  ASSERT_EQ(b1.get(), data);
  {
    Buffer b2{std::move(b1)};
    EXPECT_EQ(b1.capacity(), 0);
    EXPECT_EQ(b1.size(), 0);
    EXPECT_TRUE(b1.empty());
    EXPECT_EQ(b1.get(), nullptr);
    EXPECT_EQ(b2.capacity(), size);
    EXPECT_EQ(b2.size(), 3);
    EXPECT_FALSE(b2.empty());
    EXPECT_EQ(b2.get(), data);
  }
  EXPECT_EQ(b1.capacity(), 0);
  EXPECT_EQ(b1.size(), 0);
  EXPECT_TRUE(b1.empty());
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
  ASSERT_TRUE(b1.empty());
  ASSERT_EQ(b1.get(), nullptr);
  {
    Buffer b2{data, size, 3};
    b1 = std::move(b2);
    EXPECT_EQ(b1.capacity(), size);
    EXPECT_EQ(b1.size(), 3);
    EXPECT_FALSE(b1.empty());
    EXPECT_EQ(b1.get(), data);
    EXPECT_EQ(b2.capacity(), 0);
    EXPECT_EQ(b2.size(), 0);
    EXPECT_TRUE(b2.empty());
    EXPECT_EQ(b2.get(), nullptr);
  }
  EXPECT_EQ(b1.capacity(), size);
  EXPECT_EQ(b1.size(), 3);
  EXPECT_FALSE(b1.empty());
  EXPECT_EQ(b1.get(), data);
  EXPECT_THAT(b1.span(), ElementsAre(12, 34, 56));
}

TEST(BufferTest, Swap) {
  size_t constexpr size1 = 10;
  auto const data1 = new uint8_t[size1];
  data1[0] = 12;
  data1[1] = 34;
  data1[2] = 56;
  size_t constexpr size2 = 5;
  auto const data2 = new uint8_t[size2];
  data2[0] = 56;
  data2[1] = 78;
  data2[2] = 90;
  data2[3] = 12;
  Buffer b1{data1, size1, 3};
  Buffer b2{data2, size2, 4};
  b1.swap(b2);
  EXPECT_EQ(b1.capacity(), size2);
  EXPECT_EQ(b1.size(), 4);
  EXPECT_FALSE(b1.empty());
  EXPECT_EQ(b1.get(), data2);
  EXPECT_THAT(b1.span(), ElementsAre(56, 78, 90, 12));
  EXPECT_EQ(b2.capacity(), size1);
  EXPECT_EQ(b2.size(), 3);
  EXPECT_FALSE(b2.empty());
  EXPECT_EQ(b2.get(), data1);
  EXPECT_THAT(b2.span(), ElementsAre(12, 34, 56));
}

TEST(BufferTest, AdlSwap) {
  size_t constexpr size1 = 10;
  auto const data1 = new uint8_t[size1];
  data1[0] = 12;
  data1[1] = 34;
  data1[2] = 56;
  size_t constexpr size2 = 5;
  auto const data2 = new uint8_t[size2];
  data2[0] = 56;
  data2[1] = 78;
  data2[2] = 90;
  data2[3] = 12;
  Buffer b1{data1, size1, 3};
  Buffer b2{data2, size2, 4};
  swap(b1, b2);
  EXPECT_EQ(b1.capacity(), size2);
  EXPECT_EQ(b1.size(), 4);
  EXPECT_FALSE(b1.empty());
  EXPECT_EQ(b1.get(), data2);
  EXPECT_THAT(b1.span(), ElementsAre(56, 78, 90, 12));
  EXPECT_EQ(b2.capacity(), size1);
  EXPECT_EQ(b2.size(), 3);
  EXPECT_FALSE(b2.empty());
  EXPECT_EQ(b2.get(), data1);
  EXPECT_THAT(b2.span(), ElementsAre(12, 34, 56));
}

TEST(BufferTest, AppendInt) {
  size_t constexpr capacity = 256;
  size_t constexpr offset = 10;
  auto const data = new uint8_t[capacity];
  Buffer b{data, capacity, offset};
  b.Append<int>(123);
  EXPECT_EQ(b.capacity(), capacity);
  EXPECT_EQ(b.size(), offset + sizeof(int));
  EXPECT_FALSE(b.empty());
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
  EXPECT_FALSE(b.empty());
  EXPECT_EQ(b.get(), data);
  EXPECT_EQ(b.at<long long>(offset), 456);
}

TEST(BufferTest, AppendBuffer) {
  Buffer b1{sizeof(uintptr_t) * 2};
  b1.Append<uintptr_t>(123456789);
  ASSERT_EQ(b1.size(), sizeof(uintptr_t));
  ASSERT_FALSE(b1.empty());
  {
    Buffer b2{sizeof(uintptr_t)};
    b2.Append<uintptr_t>(987654321);
    b1.Append(b2);
    EXPECT_EQ(b1.size(), sizeof(uintptr_t) * 2);
    EXPECT_FALSE(b1.empty());
    EXPECT_EQ(b1.at<uintptr_t>(0), 123456789);
    EXPECT_EQ(b1.at<uintptr_t>(sizeof(uintptr_t)), 987654321);
    EXPECT_EQ(b2.size(), sizeof(uintptr_t));
    EXPECT_FALSE(b2.empty());
    EXPECT_EQ(b2.at<uintptr_t>(0), 987654321);
  }
  EXPECT_EQ(b1.size(), sizeof(uintptr_t) * 2);
  EXPECT_FALSE(b1.empty());
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
  ASSERT_FALSE(buffer.empty());
  EXPECT_FALSE(buffer.is_full());
}

TEST(BufferTest, Full) {
  Buffer buffer{10};
  buffer.Append<uint64_t>(42);
  buffer.Append<uint16_t>(42);
  ASSERT_EQ(buffer.capacity(), 10);
  ASSERT_EQ(buffer.size(), 10);
  ASSERT_FALSE(buffer.empty());
  EXPECT_TRUE(buffer.is_full());
}

TEST(BufferTest, Advance) {
  Buffer buffer{10};
  ASSERT_EQ(buffer.capacity(), 10);
  ASSERT_EQ(buffer.size(), 0);
  ASSERT_TRUE(buffer.empty());
  buffer.Advance(3);
  EXPECT_EQ(buffer.size(), 3);
  EXPECT_FALSE(buffer.empty());
  buffer.Advance(4);
  EXPECT_EQ(buffer.size(), 7);
  EXPECT_FALSE(buffer.empty());
}

TEST(BufferTest, AdvanceOverflow) {
  Buffer buffer{10};
  EXPECT_DEATH(buffer.Advance(30), _);
}

TEST(BufferTest, MemCpy) {
  std::string_view constexpr kData = "HELLO";
  Buffer buffer{20};
  buffer.Append<uint32_t>(42);
  buffer.MemCpy(kData.data(), kData.size());
  EXPECT_EQ(buffer.capacity(), 20);
  EXPECT_EQ(buffer.size(), 4 + kData.size());
  EXPECT_FALSE(buffer.empty());
  EXPECT_EQ(buffer.at<uint32_t>(0), 42);
  EXPECT_EQ(std::strncmp(kData.data(), buffer.as_char_array() + 4, kData.size()), 0);
  EXPECT_EQ(std::strncmp(kData.data(), buffer.as_array<char>() + 4, kData.size()), 0);
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
    ASSERT_FALSE(buffer.empty());
    ASSERT_EQ(buffer.get(), data);
    EXPECT_EQ(buffer.Release(), data);
    EXPECT_EQ(buffer.capacity(), 0);
    EXPECT_EQ(buffer.size(), 0);
    ASSERT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.get(), nullptr);
  }
  EXPECT_EQ(data[0], 12);
  EXPECT_EQ(data[1], 34);
  EXPECT_EQ(data[2], 56);
  delete[] data;
}

}  // namespace
