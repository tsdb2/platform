#include "io/cord.h"

#include <string_view>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "io/buffer.h"
#include "io/buffer_testing.h"

namespace {

using ::tsdb2::io::Buffer;
using ::tsdb2::io::Cord;
using ::tsdb2::testing::io::BufferAsString;

TEST(CordTest, Empty) { EXPECT_EQ(Cord().size(), 0); }

TEST(CordTest, OnePiece) {
  std::string_view constexpr kData = "abcde";
  Buffer buffer{kData.data(), kData.size()};
  Cord cord{std::move(buffer)};
  EXPECT_EQ(cord.size(), kData.size());
  EXPECT_EQ(cord.at(0), 'a');
  EXPECT_EQ(cord.at(1), 'b');
  EXPECT_EQ(cord.at(2), 'c');
  EXPECT_EQ(cord.at(3), 'd');
  EXPECT_EQ(cord.at(4), 'e');
  EXPECT_THAT(std::move(cord).Flatten(), BufferAsString(kData));
}

TEST(CordTest, OnePieceWithSpareCapacity) {
  std::string_view constexpr kData = "abcde";
  Buffer buffer{kData.size() * 2};
  buffer.MemCpy(kData.data(), kData.size());
  Cord cord{std::move(buffer)};
  EXPECT_EQ(cord.size(), kData.size());
  EXPECT_EQ(cord.at(0), 'a');
  EXPECT_EQ(cord.at(1), 'b');
  EXPECT_EQ(cord.at(2), 'c');
  EXPECT_EQ(cord.at(3), 'd');
  EXPECT_EQ(cord.at(4), 'e');
  EXPECT_THAT(std::move(cord).Flatten(), BufferAsString(kData));
}

TEST(CordTest, TwoPieces) {
  std::string_view constexpr kData = "abcde";
  Buffer buffer1{kData.data(), kData.size()};
  Buffer buffer2{kData.data(), kData.size()};
  Cord cord{std::move(buffer1), std::move(buffer2)};
  EXPECT_EQ(cord.size(), kData.size() * 2);
  EXPECT_EQ(cord.at(0), 'a');
  EXPECT_EQ(cord.at(1), 'b');
  EXPECT_EQ(cord.at(2), 'c');
  EXPECT_EQ(cord.at(3), 'd');
  EXPECT_EQ(cord.at(4), 'e');
  EXPECT_EQ(cord.at(5), 'a');
  EXPECT_EQ(cord.at(6), 'b');
  EXPECT_EQ(cord.at(7), 'c');
  EXPECT_EQ(cord.at(8), 'd');
  EXPECT_EQ(cord.at(9), 'e');
  EXPECT_THAT(std::move(cord).Flatten(), BufferAsString("abcdeabcde"));
}

TEST(CordTest, TwoPiecesWithSpareCapacities) {
  std::string_view constexpr kData = "abcde";
  Buffer buffer1{kData.size() * 3};
  buffer1.MemCpy(kData.data(), kData.size());
  Buffer buffer2{kData.size() * 2};
  buffer2.MemCpy(kData.data(), kData.size());
  Cord cord{std::move(buffer1), std::move(buffer2)};
  EXPECT_EQ(cord.size(), kData.size() * 2);
  EXPECT_EQ(cord.at(0), 'a');
  EXPECT_EQ(cord.at(1), 'b');
  EXPECT_EQ(cord.at(2), 'c');
  EXPECT_EQ(cord.at(3), 'd');
  EXPECT_EQ(cord.at(4), 'e');
  EXPECT_EQ(cord.at(5), 'a');
  EXPECT_EQ(cord.at(6), 'b');
  EXPECT_EQ(cord.at(7), 'c');
  EXPECT_EQ(cord.at(8), 'd');
  EXPECT_EQ(cord.at(9), 'e');
  EXPECT_THAT(std::move(cord).Flatten(), BufferAsString("abcdeabcde"));
}

TEST(CordTest, ThreePieces) {
  std::string_view constexpr kData1 = "abcde";
  Buffer buffer1{kData1.data(), kData1.size()};
  std::string_view constexpr kData2 = "def";
  Buffer buffer2{kData2.data(), kData2.size()};
  std::string_view constexpr kData3 = "ghij";
  Buffer buffer3{kData3.data(), kData3.size()};
  Cord cord{std::move(buffer1), std::move(buffer2), std::move(buffer3)};
  EXPECT_EQ(cord.size(), kData1.size() + kData2.size() + kData3.size());
  EXPECT_EQ(cord.at(0), 'a');
  EXPECT_EQ(cord.at(1), 'b');
  EXPECT_EQ(cord.at(2), 'c');
  EXPECT_EQ(cord.at(3), 'd');
  EXPECT_EQ(cord.at(4), 'e');
  EXPECT_EQ(cord.at(5), 'd');
  EXPECT_EQ(cord.at(6), 'e');
  EXPECT_EQ(cord.at(7), 'f');
  EXPECT_EQ(cord.at(8), 'g');
  EXPECT_EQ(cord.at(9), 'h');
  EXPECT_EQ(cord.at(10), 'i');
  EXPECT_EQ(cord.at(11), 'j');
  EXPECT_THAT(std::move(cord).Flatten(), BufferAsString("abcdedefghij"));
}

TEST(CordTest, ThreePiecesWithSpareCapacities) {
  std::string_view constexpr kData1 = "abcde";
  Buffer buffer1{6};
  buffer1.MemCpy(kData1.data(), kData1.size());
  std::string_view constexpr kData2 = "def";
  Buffer buffer2{6};
  buffer2.MemCpy(kData2.data(), kData2.size());
  std::string_view constexpr kData3 = "ghij";
  Buffer buffer3{6};
  buffer3.MemCpy(kData3.data(), kData3.size());
  Cord cord{std::move(buffer1), std::move(buffer2), std::move(buffer3)};
  EXPECT_EQ(cord.size(), kData1.size() + kData2.size() + kData3.size());
  EXPECT_EQ(cord.at(0), 'a');
  EXPECT_EQ(cord.at(1), 'b');
  EXPECT_EQ(cord.at(2), 'c');
  EXPECT_EQ(cord.at(3), 'd');
  EXPECT_EQ(cord.at(4), 'e');
  EXPECT_EQ(cord.at(5), 'd');
  EXPECT_EQ(cord.at(6), 'e');
  EXPECT_EQ(cord.at(7), 'f');
  EXPECT_EQ(cord.at(8), 'g');
  EXPECT_EQ(cord.at(9), 'h');
  EXPECT_EQ(cord.at(10), 'i');
  EXPECT_EQ(cord.at(11), 'j');
  EXPECT_THAT(std::move(cord).Flatten(), BufferAsString("abcdedefghij"));
}

TEST(CordTest, AppendFirstBuffer) {
  std::string_view constexpr kData = "abcde";
  Buffer buffer{kData.data(), kData.size()};
  Cord cord;
  ASSERT_EQ(cord.size(), 0);
  cord.Append(std::move(buffer));
  EXPECT_EQ(cord.size(), kData.size());
  EXPECT_EQ(cord.at(0), 'a');
  EXPECT_EQ(cord.at(1), 'b');
  EXPECT_EQ(cord.at(2), 'c');
  EXPECT_EQ(cord.at(3), 'd');
  EXPECT_EQ(cord.at(4), 'e');
  EXPECT_THAT(std::move(cord).Flatten(), BufferAsString(kData));
}

TEST(CordTest, AppendBuffer) {
  std::string_view constexpr kData1 = "abcde";
  Buffer buffer1{6};
  buffer1.MemCpy(kData1.data(), kData1.size());
  std::string_view constexpr kData2 = "def";
  Buffer buffer2{6};
  buffer2.MemCpy(kData2.data(), kData2.size());
  Cord cord{std::move(buffer1), std::move(buffer2)};
  ASSERT_EQ(cord.size(), kData1.size() + kData2.size());
  std::string_view constexpr kData3 = "ghij";
  Buffer buffer3{6};
  buffer3.MemCpy(kData3.data(), kData3.size());
  cord.Append(std::move(buffer3));
  EXPECT_EQ(cord.size(), kData1.size() + kData2.size() + kData3.size());
  EXPECT_EQ(cord.at(0), 'a');
  EXPECT_EQ(cord.at(1), 'b');
  EXPECT_EQ(cord.at(2), 'c');
  EXPECT_EQ(cord.at(3), 'd');
  EXPECT_EQ(cord.at(4), 'e');
  EXPECT_EQ(cord.at(5), 'd');
  EXPECT_EQ(cord.at(6), 'e');
  EXPECT_EQ(cord.at(7), 'f');
  EXPECT_EQ(cord.at(8), 'g');
  EXPECT_EQ(cord.at(9), 'h');
  EXPECT_EQ(cord.at(10), 'i');
  EXPECT_EQ(cord.at(11), 'j');
  EXPECT_THAT(std::move(cord).Flatten(), BufferAsString("abcdedefghij"));
}

TEST(CordTest, AppendCord) {
  std::string_view constexpr kData1 = "abcde";
  Buffer buffer1{6};
  buffer1.MemCpy(kData1.data(), kData1.size());
  std::string_view constexpr kData2 = "def";
  Buffer buffer2{6};
  buffer2.MemCpy(kData2.data(), kData2.size());
  Cord cord1{std::move(buffer1), std::move(buffer2)};
  ASSERT_EQ(cord1.size(), kData1.size() + kData2.size());
  std::string_view constexpr kData3 = "ghij";
  Buffer buffer3{6};
  buffer3.MemCpy(kData3.data(), kData3.size());
  std::string_view constexpr kData4 = "klm";
  Buffer buffer4{6};
  buffer4.MemCpy(kData4.data(), kData4.size());
  Cord cord2{std::move(buffer3), std::move(buffer4)};
  ASSERT_EQ(cord2.size(), kData3.size() + kData4.size());
  cord1.Append(std::move(cord2));
  EXPECT_EQ(cord1.size(), kData1.size() + kData2.size() + kData3.size() + kData4.size());
  EXPECT_EQ(cord1.at(0), 'a');
  EXPECT_EQ(cord1.at(1), 'b');
  EXPECT_EQ(cord1.at(2), 'c');
  EXPECT_EQ(cord1.at(3), 'd');
  EXPECT_EQ(cord1.at(4), 'e');
  EXPECT_EQ(cord1.at(5), 'd');
  EXPECT_EQ(cord1.at(6), 'e');
  EXPECT_EQ(cord1.at(7), 'f');
  EXPECT_EQ(cord1.at(8), 'g');
  EXPECT_EQ(cord1.at(9), 'h');
  EXPECT_EQ(cord1.at(10), 'i');
  EXPECT_EQ(cord1.at(11), 'j');
  EXPECT_EQ(cord1.at(12), 'k');
  EXPECT_EQ(cord1.at(13), 'l');
  EXPECT_EQ(cord1.at(14), 'm');
  EXPECT_THAT(std::move(cord1).Flatten(), BufferAsString("abcdedefghijklm"));
}

TEST(CordTest, MoveConstruct) {
  std::string_view constexpr kData = "abcde";
  Buffer buffer{kData.data(), kData.size()};
  Cord cord1{std::move(buffer)};
  Cord cord2{std::move(cord1)};
  EXPECT_EQ(cord2.size(), kData.size());
  EXPECT_EQ(cord2.at(0), 'a');
  EXPECT_EQ(cord2.at(1), 'b');
  EXPECT_EQ(cord2.at(2), 'c');
  EXPECT_EQ(cord2.at(3), 'd');
  EXPECT_EQ(cord2.at(4), 'e');
  EXPECT_THAT(std::move(cord2).Flatten(), BufferAsString(kData));
}

TEST(CordTest, MoveAssign) {
  std::string_view constexpr kData = "abcde";
  Buffer buffer{kData.data(), kData.size()};
  Cord cord1{std::move(buffer)};
  Cord cord2;
  cord2 = std::move(cord1);
  EXPECT_EQ(cord2.size(), kData.size());
  EXPECT_EQ(cord2.at(0), 'a');
  EXPECT_EQ(cord2.at(1), 'b');
  EXPECT_EQ(cord2.at(2), 'c');
  EXPECT_EQ(cord2.at(3), 'd');
  EXPECT_EQ(cord2.at(4), 'e');
  EXPECT_THAT(std::move(cord2).Flatten(), BufferAsString(kData));
}

TEST(CordTest, Swap) {
  std::string_view constexpr kData1 = "abcd";
  Buffer buffer1{kData1.data(), kData1.size()};
  Cord cord1{std::move(buffer1)};
  std::string_view constexpr kData2 = "fghijk";
  Buffer buffer2{kData2.data(), kData2.size()};
  Cord cord2{std::move(buffer2)};
  cord1.swap(cord2);
  EXPECT_EQ(cord1.size(), kData2.size());
  EXPECT_EQ(cord1.at(0), 'f');
  EXPECT_EQ(cord1.at(1), 'g');
  EXPECT_EQ(cord1.at(2), 'h');
  EXPECT_EQ(cord1.at(3), 'i');
  EXPECT_EQ(cord1.at(4), 'j');
  EXPECT_EQ(cord1.at(5), 'k');
  EXPECT_THAT(std::move(cord1).Flatten(), BufferAsString(kData2));
  EXPECT_EQ(cord2.size(), kData1.size());
  EXPECT_EQ(cord2.at(0), 'a');
  EXPECT_EQ(cord2.at(1), 'b');
  EXPECT_EQ(cord2.at(2), 'c');
  EXPECT_EQ(cord2.at(3), 'd');
  EXPECT_THAT(std::move(cord2).Flatten(), BufferAsString(kData1));
}

TEST(CordTest, AdlSwap) {
  std::string_view constexpr kData1 = "abcd";
  Buffer buffer1{kData1.data(), kData1.size()};
  Cord cord1{std::move(buffer1)};
  std::string_view constexpr kData2 = "fghijk";
  Buffer buffer2{kData2.data(), kData2.size()};
  Cord cord2{std::move(buffer2)};
  swap(cord1, cord2);
  EXPECT_EQ(cord1.size(), kData2.size());
  EXPECT_EQ(cord1.at(0), 'f');
  EXPECT_EQ(cord1.at(1), 'g');
  EXPECT_EQ(cord1.at(2), 'h');
  EXPECT_EQ(cord1.at(3), 'i');
  EXPECT_EQ(cord1.at(4), 'j');
  EXPECT_EQ(cord1.at(5), 'k');
  EXPECT_THAT(std::move(cord1).Flatten(), BufferAsString(kData2));
  EXPECT_EQ(cord2.size(), kData1.size());
  EXPECT_EQ(cord2.at(0), 'a');
  EXPECT_EQ(cord2.at(1), 'b');
  EXPECT_EQ(cord2.at(2), 'c');
  EXPECT_EQ(cord2.at(3), 'd');
  EXPECT_THAT(std::move(cord2).Flatten(), BufferAsString(kData1));
}

}  // namespace
