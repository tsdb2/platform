#include "proto/proto.h"

#include <string_view>
#include <utility>

#include "absl/hash/hash.h"
#include "common/fingerprint.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "io/buffer.h"
#include "io/buffer_testing.h"

namespace {

using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::tsdb2::io::Buffer;
using ::tsdb2::proto::ExtensionData;
using ::tsdb2::testing::io::BufferAsString;

TEST(ExtensionDataTest, DefaultConstruction) {
  ExtensionData const ed;
  EXPECT_TRUE(ed.empty());
  EXPECT_EQ(ed.size(), 0);
  EXPECT_EQ(ed.capacity(), 0);
  EXPECT_THAT(ed.span(), IsEmpty());
}

TEST(ExtensionDataTest, ConstructFromEmptyBuffer) {
  Buffer buffer;
  ExtensionData const ed{std::move(buffer)};
  EXPECT_TRUE(ed.empty());
  EXPECT_EQ(ed.size(), 0);
  EXPECT_EQ(ed.capacity(), 0);
  EXPECT_THAT(ed.span(), IsEmpty());
}

TEST(ExtensionDataTest, ConstructFromBuffer) {
  std::string_view constexpr kData = "SATOR AREPO";
  Buffer buffer{/*capacity=*/20};
  buffer.MemCpy(kData.data(), kData.size());
  ExtensionData const ed{std::move(buffer)};
  EXPECT_FALSE(ed.empty());
  EXPECT_EQ(ed.size(), 11);
  EXPECT_EQ(ed.capacity(), 20);
  EXPECT_THAT(ed.span(), ElementsAreArray(kData));
}

TEST(ExtensionDataTest, CopyConstruct) {
  std::string_view constexpr kData = "SATOR AREPO";
  Buffer buffer{/*capacity=*/20};
  buffer.MemCpy(kData.data(), kData.size());
  ExtensionData const ed1{std::move(buffer)};
  ExtensionData const ed2{ed1};  // NOLINT
  EXPECT_FALSE(ed2.empty());
  EXPECT_EQ(ed2.size(), 11);
  EXPECT_EQ(ed2.capacity(), 11);
  EXPECT_THAT(ed2.span(), ElementsAreArray(kData));
}

TEST(ExtensionDataTest, CopyConstructAndClearSource) {
  std::string_view constexpr kData = "SATOR AREPO";
  Buffer buffer{/*capacity=*/20};
  buffer.MemCpy(kData.data(), kData.size());
  ExtensionData ed1{std::move(buffer)};
  ExtensionData const ed2{ed1};  // NOLINT
  ed1.Clear();
  EXPECT_FALSE(ed2.empty());
  EXPECT_EQ(ed2.size(), 11);
  EXPECT_EQ(ed2.capacity(), 11);
  EXPECT_THAT(ed2.span(), ElementsAreArray(kData));
}

TEST(ExtensionDataTest, CopyAssign) {
  std::string_view constexpr kData = "SATOR AREPO";
  Buffer buffer{/*capacity=*/20};
  buffer.MemCpy(kData.data(), kData.size());
  ExtensionData const ed1{std::move(buffer)};
  ExtensionData ed2;
  ed2 = ed1;
  EXPECT_FALSE(ed2.empty());
  EXPECT_EQ(ed2.size(), 11);
  EXPECT_EQ(ed2.capacity(), 11);
  EXPECT_THAT(ed2.span(), ElementsAreArray(kData));
}

TEST(ExtensionDataTest, CopyAssignAndClearSource) {
  std::string_view constexpr kData = "SATOR AREPO";
  Buffer buffer{/*capacity=*/20};
  buffer.MemCpy(kData.data(), kData.size());
  ExtensionData ed1{std::move(buffer)};
  ExtensionData ed2;
  ed2 = ed1;
  ed1.Clear();
  EXPECT_FALSE(ed2.empty());
  EXPECT_EQ(ed2.size(), 11);
  EXPECT_EQ(ed2.capacity(), 11);
  EXPECT_THAT(ed2.span(), ElementsAreArray(kData));
}

TEST(ExtensionDataTest, SelfCopy) {
  std::string_view constexpr kData = "SATOR AREPO";
  Buffer buffer{/*capacity=*/20};
  buffer.MemCpy(kData.data(), kData.size());
  ExtensionData ed{std::move(buffer)};
  ed = ed;
  EXPECT_FALSE(ed.empty());
  EXPECT_EQ(ed.size(), 11);
  EXPECT_EQ(ed.capacity(), 20);
  EXPECT_THAT(ed.span(), ElementsAreArray(kData));
}

TEST(ExtensionDataTest, MoveConstruct) {
  std::string_view constexpr kData = "SATOR AREPO";
  Buffer buffer{/*capacity=*/20};
  buffer.MemCpy(kData.data(), kData.size());
  ExtensionData ed1{std::move(buffer)};
  ExtensionData const ed2{std::move(ed1)};
  EXPECT_FALSE(ed2.empty());
  EXPECT_EQ(ed2.size(), 11);
  EXPECT_EQ(ed2.capacity(), 20);
  EXPECT_THAT(ed2.span(), ElementsAreArray(kData));
}

TEST(ExtensionDataTest, MoveAssign) {
  std::string_view constexpr kData = "SATOR AREPO";
  Buffer buffer{/*capacity=*/20};
  buffer.MemCpy(kData.data(), kData.size());
  ExtensionData ed1{std::move(buffer)};
  ExtensionData ed2;
  ed2 = std::move(ed1);
  EXPECT_FALSE(ed2.empty());
  EXPECT_EQ(ed2.size(), 11);
  EXPECT_EQ(ed2.capacity(), 20);
  EXPECT_THAT(ed2.span(), ElementsAreArray(kData));
}

TEST(ExtensionDataTest, SelfMove) {
  std::string_view constexpr kData = "SATOR AREPO";
  Buffer buffer{/*capacity=*/20};
  buffer.MemCpy(kData.data(), kData.size());
  ExtensionData ed{std::move(buffer)};
  ed = std::move(ed);
  EXPECT_FALSE(ed.empty());
  EXPECT_EQ(ed.size(), 11);
  EXPECT_EQ(ed.capacity(), 20);
  EXPECT_THAT(ed.span(), ElementsAreArray(kData));
}

TEST(ExtensionDataTest, Swap) {
  std::string_view constexpr kData1 = "SATOR AREPO";
  Buffer buffer1{/*capacity=*/20};
  buffer1.MemCpy(kData1.data(), kData1.size());
  ExtensionData ed1{std::move(buffer1)};
  std::string_view constexpr kData2 = "TENET";
  Buffer buffer2{/*capacity=*/8};
  buffer2.MemCpy(kData2.data(), kData2.size());
  ExtensionData ed2{std::move(buffer2)};
  ed1.swap(ed2);
  EXPECT_FALSE(ed1.empty());
  EXPECT_EQ(ed1.size(), 5);
  EXPECT_EQ(ed1.capacity(), 8);
  EXPECT_THAT(ed1.span(), ElementsAreArray(kData2));
  EXPECT_FALSE(ed2.empty());
  EXPECT_EQ(ed2.size(), 11);
  EXPECT_EQ(ed2.capacity(), 20);
  EXPECT_THAT(ed2.span(), ElementsAreArray(kData1));
}

TEST(ExtensionDataTest, AdlSwap) {
  std::string_view constexpr kData1 = "SATOR AREPO";
  Buffer buffer1{/*capacity=*/20};
  buffer1.MemCpy(kData1.data(), kData1.size());
  ExtensionData ed1{std::move(buffer1)};
  std::string_view constexpr kData2 = "TENET";
  Buffer buffer2{/*capacity=*/8};
  buffer2.MemCpy(kData2.data(), kData2.size());
  ExtensionData ed2{std::move(buffer2)};
  using std::swap;
  swap(ed1, ed2);
  EXPECT_FALSE(ed1.empty());
  EXPECT_EQ(ed1.size(), 5);
  EXPECT_EQ(ed1.capacity(), 8);
  EXPECT_THAT(ed1.span(), ElementsAreArray(kData2));
  EXPECT_FALSE(ed2.empty());
  EXPECT_EQ(ed2.size(), 11);
  EXPECT_EQ(ed2.capacity(), 20);
  EXPECT_THAT(ed2.span(), ElementsAreArray(kData1));
}

TEST(ExtensionDataTest, SelfSwap) {
  std::string_view constexpr kData = "SATOR AREPO";
  Buffer buffer{/*capacity=*/20};
  buffer.MemCpy(kData.data(), kData.size());
  ExtensionData ed{std::move(buffer)};
  ed.swap(ed);
  EXPECT_FALSE(ed.empty());
  EXPECT_EQ(ed.size(), 11);
  EXPECT_EQ(ed.capacity(), 20);
  EXPECT_THAT(ed.span(), ElementsAreArray(kData));
}

TEST(ExtensionDataTest, CompareEmptyWithEmpty) {
  ExtensionData const ed1;
  ExtensionData const ed2;
  EXPECT_TRUE(ed1 == ed2);
  EXPECT_TRUE(ed2 == ed1);
  EXPECT_FALSE(ed1 != ed2);
  EXPECT_FALSE(ed2 != ed1);
  EXPECT_FALSE(ed1 < ed2);
  EXPECT_FALSE(ed2 < ed1);
  EXPECT_TRUE(ed1 <= ed2);
  EXPECT_TRUE(ed2 <= ed1);
  EXPECT_FALSE(ed1 > ed2);
  EXPECT_FALSE(ed2 > ed1);
  EXPECT_TRUE(ed1 >= ed2);
  EXPECT_TRUE(ed2 >= ed1);
  EXPECT_EQ(absl::HashOf(ed1), absl::HashOf(ed2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(ed1), tsdb2::common::FingerprintOf(ed2));
}

TEST(ExtensionDataTest, CompareWithEmpty) {
  std::string_view constexpr kData = "SATOR AREPO";
  Buffer buffer{/*capacity=*/20};
  buffer.MemCpy(kData.data(), kData.size());
  ExtensionData const ed1{std::move(buffer)};
  ExtensionData const ed2;
  EXPECT_FALSE(ed1 == ed2);
  EXPECT_FALSE(ed2 == ed1);
  EXPECT_TRUE(ed1 != ed2);
  EXPECT_TRUE(ed2 != ed1);
  EXPECT_FALSE(ed1 < ed2);
  EXPECT_TRUE(ed2 < ed1);
  EXPECT_FALSE(ed1 <= ed2);
  EXPECT_TRUE(ed2 <= ed1);
  EXPECT_TRUE(ed1 > ed2);
  EXPECT_FALSE(ed2 > ed1);
  EXPECT_TRUE(ed1 >= ed2);
  EXPECT_FALSE(ed2 >= ed1);
  EXPECT_NE(absl::HashOf(ed1), absl::HashOf(ed2));
  EXPECT_NE(tsdb2::common::FingerprintOf(ed1), tsdb2::common::FingerprintOf(ed2));
}

TEST(ExtensionDataTest, CompareDifferent) {
  std::string_view constexpr kData1 = "SATOR AREPO";
  Buffer buffer1{/*capacity=*/20};
  buffer1.MemCpy(kData1.data(), kData1.size());
  ExtensionData const ed1{std::move(buffer1)};
  std::string_view constexpr kData2 = "TENET";
  Buffer buffer2{/*capacity=*/8};
  buffer2.MemCpy(kData2.data(), kData2.size());
  ExtensionData const ed2{std::move(buffer2)};
  EXPECT_FALSE(ed1 == ed2);
  EXPECT_FALSE(ed2 == ed1);
  EXPECT_TRUE(ed1 != ed2);
  EXPECT_TRUE(ed2 != ed1);
  EXPECT_FALSE(ed1 < ed2);
  EXPECT_TRUE(ed2 < ed1);
  EXPECT_FALSE(ed1 <= ed2);
  EXPECT_TRUE(ed2 <= ed1);
  EXPECT_TRUE(ed1 > ed2);
  EXPECT_FALSE(ed2 > ed1);
  EXPECT_TRUE(ed1 >= ed2);
  EXPECT_FALSE(ed2 >= ed1);
  EXPECT_NE(absl::HashOf(ed1), absl::HashOf(ed2));
  EXPECT_NE(tsdb2::common::FingerprintOf(ed1), tsdb2::common::FingerprintOf(ed2));
}

TEST(ExtensionDataTest, CompareEqualWithDifferentCapacities) {
  std::string_view constexpr kData = "SATOR AREPO";
  Buffer buffer1{/*capacity=*/15};
  buffer1.MemCpy(kData.data(), kData.size());
  ExtensionData const ed1{std::move(buffer1)};
  Buffer buffer2{/*capacity=*/20};
  buffer2.MemCpy(kData.data(), kData.size());
  ExtensionData const ed2{std::move(buffer2)};
  EXPECT_TRUE(ed1 == ed2);
  EXPECT_TRUE(ed2 == ed1);
  EXPECT_FALSE(ed1 != ed2);
  EXPECT_FALSE(ed2 != ed1);
  EXPECT_FALSE(ed1 < ed2);
  EXPECT_FALSE(ed2 < ed1);
  EXPECT_TRUE(ed1 <= ed2);
  EXPECT_TRUE(ed2 <= ed1);
  EXPECT_FALSE(ed1 > ed2);
  EXPECT_FALSE(ed2 > ed1);
  EXPECT_TRUE(ed1 >= ed2);
  EXPECT_TRUE(ed2 >= ed1);
  EXPECT_EQ(absl::HashOf(ed1), absl::HashOf(ed2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(ed1), tsdb2::common::FingerprintOf(ed2));
}

TEST(ExtensionDataTest, CompareIdentical) {
  std::string_view constexpr kData = "SATOR AREPO";
  Buffer buffer1{kData.data(), kData.size()};
  ExtensionData const ed1{std::move(buffer1)};
  Buffer buffer2{kData.data(), kData.size()};
  ExtensionData const ed2{std::move(buffer2)};
  EXPECT_TRUE(ed1 == ed2);
  EXPECT_TRUE(ed2 == ed1);
  EXPECT_FALSE(ed1 != ed2);
  EXPECT_FALSE(ed2 != ed1);
  EXPECT_FALSE(ed1 < ed2);
  EXPECT_FALSE(ed2 < ed1);
  EXPECT_TRUE(ed1 <= ed2);
  EXPECT_TRUE(ed2 <= ed1);
  EXPECT_FALSE(ed1 > ed2);
  EXPECT_FALSE(ed2 > ed1);
  EXPECT_TRUE(ed1 >= ed2);
  EXPECT_TRUE(ed2 >= ed1);
  EXPECT_EQ(absl::HashOf(ed1), absl::HashOf(ed2));
  EXPECT_EQ(tsdb2::common::FingerprintOf(ed1), tsdb2::common::FingerprintOf(ed2));
}

TEST(ExtensionDataTest, Clear) {
  std::string_view constexpr kData = "SATOR AREPO";
  Buffer buffer{/*capacity=*/20};
  buffer.MemCpy(kData.data(), kData.size());
  ExtensionData ed{std::move(buffer)};
  ed.Clear();
  EXPECT_TRUE(ed.empty());
  EXPECT_EQ(ed.size(), 0);
  EXPECT_EQ(ed.capacity(), 0);
  EXPECT_THAT(ed.span(), IsEmpty());
}

TEST(ExtensionDataTest, Release) {
  std::string_view constexpr kData = "SATOR AREPO";
  Buffer buffer{/*capacity=*/20};
  buffer.MemCpy(kData.data(), kData.size());
  ExtensionData ed{std::move(buffer)};
  buffer = ed.Release();
  EXPECT_TRUE(ed.empty());
  EXPECT_EQ(ed.size(), 0);
  EXPECT_EQ(ed.capacity(), 0);
  EXPECT_THAT(ed.span(), IsEmpty());
  EXPECT_THAT(buffer, BufferAsString(kData));
}

}  // namespace
