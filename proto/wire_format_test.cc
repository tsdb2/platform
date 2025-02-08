#include "proto/wire_format.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "io/buffer_testing.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::DoubleNear;
using ::testing::ElementsAre;
using ::testing::FloatNear;
using ::testing::IsEmpty;
using ::tsdb2::proto::Decoder;
using ::tsdb2::proto::Encoder;
using ::tsdb2::proto::FieldTag;
using ::tsdb2::proto::WireType;
using ::tsdb2::testing::io::BufferAsBytes;

TEST(DecoderTest, InitialState) {
  std::vector<uint8_t> const data{0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  EXPECT_FALSE(decoder.at_end());
  EXPECT_EQ(decoder.remaining(), 5);
}

TEST(DecoderTest, DecodeSome) {
  std::vector<uint8_t> const data{0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  ASSERT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(4610));
  EXPECT_FALSE(decoder.at_end());
  EXPECT_EQ(decoder.remaining(), 3);
}

TEST(DecoderTest, DecodeAll) {
  std::vector<uint8_t> const data{0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  ASSERT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(4610));
  ASSERT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(18691));
  EXPECT_TRUE(decoder.at_end());
  EXPECT_EQ(decoder.remaining(), 0);
}

TEST(DecoderTest, DecodeSingleByteTag) {
  EXPECT_THAT(Decoder{{0x10}}.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 2, .wire_type = WireType::kVarInt}));
  EXPECT_THAT(Decoder{{0x12}}.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 2, .wire_type = WireType::kLength}));
  EXPECT_THAT(Decoder{{0x18}}.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 3, .wire_type = WireType::kVarInt}));
  EXPECT_THAT(Decoder{{0x1D}}.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 3, .wire_type = WireType::kInt32}));
}

TEST(DecoderTest, DecodeTwoByteTag) {
  EXPECT_THAT((Decoder{{0x80, 0x7D}}.DecodeTag()),
              IsOkAndHolds(FieldTag{.field_number = 2000, .wire_type = WireType::kVarInt}));
  EXPECT_THAT((Decoder{{0x82, 0x7D}}.DecodeTag()),
              IsOkAndHolds(FieldTag{.field_number = 2000, .wire_type = WireType::kLength}));
  EXPECT_THAT((Decoder{{0x88, 0x7D}}.DecodeTag()),
              IsOkAndHolds(FieldTag{.field_number = 2001, .wire_type = WireType::kVarInt}));
  EXPECT_THAT((Decoder{{0x8D, 0x7D}}.DecodeTag()),
              IsOkAndHolds(FieldTag{.field_number = 2001, .wire_type = WireType::kInt32}));
}

TEST(DecoderTest, DecodeSingleByteInteger) {
  std::vector<uint8_t> const data{0x42};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(66));
}

TEST(DecoderTest, EmptyInteger) {
  std::vector<uint8_t> const data;
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeVarInt(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, IntegerDecodingError1) {
  std::vector<uint8_t> const data{0x82};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeVarInt(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, IntegerDecodingError2) {
  std::vector<uint8_t> const data{0x82, 0x83};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeVarInt(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeTwoByteInteger) {
  std::vector<uint8_t> const data{0x82, 0x24};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(4610));
}

TEST(DecoderTest, DecodeThreeByteInteger) {
  std::vector<uint8_t> const data{0x83, 0x92, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(18691));
}

TEST(DecoderTest, DecodeMaxInteger) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeUInt64(), IsOkAndHolds(0xFFFFFFFFFFFFFFFF));
}

TEST(DecoderTest, IntegerOverflow) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeUInt64(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeNegativeInteger1) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeInt64(), IsOkAndHolds(-1));
}

TEST(DecoderTest, DecodeNegativeInteger2) {
  std::vector<uint8_t> const data{0xD6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeInt64(), IsOkAndHolds(-42));
}

TEST(DecoderTest, DecodeMaxUInt32) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeUInt32(), IsOkAndHolds(0xFFFFFFFF));
}

TEST(DecoderTest, UInt32Overflow) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0x10};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeUInt32(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeMaxInt32) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeInt32(), IsOkAndHolds(-1));
}

TEST(DecoderTest, Int32Overflow) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0x10};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeInt32(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeSingleBytePositiveEvenSInt64) {
  std::vector<uint8_t> const data{0x54};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(42));
}

TEST(DecoderTest, DecodeSingleBytePositiveOddSInt64) {
  std::vector<uint8_t> const data{0x56};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(43));
}

TEST(DecoderTest, DecodeSingleByteNegativeEvenSInt64) {
  std::vector<uint8_t> const data{0x53};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(-42));
}

TEST(DecoderTest, DecodeSingleByteNegativeOddSInt64) {
  std::vector<uint8_t> const data{0x55};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(-43));
}

TEST(DecoderTest, DecodeTwoBytePositiveEvenSInt64) {
  std::vector<uint8_t> const data{0x84, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(4610));
}

TEST(DecoderTest, DecodeTwoBytePositiveOddSInt64) {
  std::vector<uint8_t> const data{0x86, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(4611));
}

TEST(DecoderTest, DecodeTwoByteNegativeEvenSInt64) {
  std::vector<uint8_t> const data{0x83, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(-4610));
}

TEST(DecoderTest, DecodeTwoByteNegativeOddSInt64) {
  std::vector<uint8_t> const data{0x85, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(-4611));
}

TEST(DecoderTest, DecodeMaxPositiveEvenSInt64) {
  std::vector<uint8_t> const data{0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(0x7FFFFFFFFFFFFFFELL));
}

TEST(DecoderTest, DecodeMaxNegativeOddSInt64) {
  std::vector<uint8_t> const data{0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(-0x7FFFFFFFFFFFFFFFLL));
}

TEST(DecoderTest, DecodeMaxPositiveOddSInt64) {
  std::vector<uint8_t> const data{0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(0x7FFFFFFFFFFFFFFFLL));
}

TEST(DecoderTest, DecodeMaxNegativeEvenSInt64) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(-0x8000000000000000LL));
}

TEST(DecoderTest, SInt64Overflow) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeSingleBytePositiveEvenSInt32) {
  std::vector<uint8_t> const data{0x54};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(42));
}

TEST(DecoderTest, DecodeSingleBytePositiveOddSInt32) {
  std::vector<uint8_t> const data{0x56};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(43));
}

TEST(DecoderTest, DecodeSingleByteNegativeEvenSInt32) {
  std::vector<uint8_t> const data{0x53};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(-42));
}

TEST(DecoderTest, DecodeSingleByteNegativeOddSInt32) {
  std::vector<uint8_t> const data{0x55};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(-43));
}

TEST(DecoderTest, DecodeTwoBytePositiveEvenSInt32) {
  std::vector<uint8_t> const data{0x84, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(4610));
}

TEST(DecoderTest, DecodeTwoBytePositiveOddSInt32) {
  std::vector<uint8_t> const data{0x86, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(4611));
}

TEST(DecoderTest, DecodeTwoByteNegativeEvenSInt32) {
  std::vector<uint8_t> const data{0x83, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(-4610));
}

TEST(DecoderTest, DecodeTwoByteNegativeOddSInt32) {
  std::vector<uint8_t> const data{0x85, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(-4611));
}

TEST(DecoderTest, DecodeMaxPositiveEvenSInt32) {
  std::vector<uint8_t> const data{0xFC, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(0x7FFFFFFE));
}

TEST(DecoderTest, DecodeMaxNegativeEvenSInt32) {
  std::vector<uint8_t> const data{0xFD, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(-0x7FFFFFFF));
}

TEST(DecoderTest, DecodeMaxPositiveOddSInt32) {
  std::vector<uint8_t> const data{0xFE, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(0x7FFFFFFF));
}

TEST(DecoderTest, DecodeMaxNegativeOddSInt32) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(-0x80000000));
}

TEST(DecoderTest, SInt32Overflow) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0x10};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeFixedInt32) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedInt32(WireType::kInt32), IsOkAndHolds(0x78563412));
}

TEST(DecoderTest, DecodeNegativeFixedInt32) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x87};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedInt32(WireType::kInt32), IsOkAndHolds(-2024393710));
}

TEST(DecoderTest, WrongWireTypeForFixedInt32) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedInt32(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedInt32(WireType::kInt64),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedInt32(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedInt32(WireType::kDeprecatedStartGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedInt32(WireType::kDeprecatedEndGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeFixedUInt32) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedUInt32(WireType::kInt32), IsOkAndHolds(0x78563412));
}

TEST(DecoderTest, WrongWireTypeForFixedUInt32) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedUInt32(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedUInt32(WireType::kInt64),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedUInt32(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedUInt32(WireType::kDeprecatedStartGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedUInt32(WireType::kDeprecatedEndGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeFixedInt64) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedInt64(WireType::kInt64), IsOkAndHolds(0x5634129078563412));
}

TEST(DecoderTest, DecodeNegativeFixedInt64) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0xD6};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedInt64(WireType::kInt64), IsOkAndHolds(-3011761839100513262));
}

TEST(DecoderTest, WrongWireTypeForFixedInt64) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedInt64(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedInt64(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedInt64(WireType::kDeprecatedStartGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedInt64(WireType::kDeprecatedEndGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedInt64(WireType::kInt32),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeFixedUInt64) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedUInt64(WireType::kInt64), IsOkAndHolds(0x5634129078563412));
}

TEST(DecoderTest, WrongWireTypeForFixedUInt64) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedUInt64(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedUInt64(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedUInt64(WireType::kDeprecatedStartGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedUInt64(WireType::kDeprecatedEndGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedUInt64(WireType::kInt32),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeBools) {
  std::vector<uint8_t> const data{0x00, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeBool(WireType::kVarInt), IsOkAndHolds(false));
  EXPECT_THAT(decoder.DecodeBool(WireType::kVarInt), IsOkAndHolds(true));
}

TEST(DecoderTest, WrongWireTypeForBool) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeBool(WireType::kInt64), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeBool(WireType::kLength), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeBool(WireType::kDeprecatedStartGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeBool(WireType::kDeprecatedEndGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeBool(WireType::kInt32), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeFloat) {
  std::vector<uint8_t> const data{0xD0, 0x0F, 0x49, 0x40};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFloat(WireType::kInt32), IsOkAndHolds(FloatNear(3.14159f, 0.0001)));
}

TEST(DecoderTest, WrongWireTypeForFloat) {
  std::vector<uint8_t> const data{0xD0, 0x0F, 0x49, 0x40};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFloat(WireType::kVarInt), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFloat(WireType::kInt64), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFloat(WireType::kLength), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFloat(WireType::kDeprecatedStartGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFloat(WireType::kDeprecatedEndGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeDouble) {
  std::vector<uint8_t> const data{0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeDouble(WireType::kInt64), IsOkAndHolds(DoubleNear(3.14159, 0.0001)));
}

TEST(DecoderTest, WrongWireTypeForDouble) {
  std::vector<uint8_t> const data{0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeDouble(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeDouble(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeDouble(WireType::kDeprecatedStartGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeDouble(WireType::kDeprecatedEndGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeDouble(WireType::kInt32), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyString) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeString(WireType::kLength), IsOkAndHolds(""));
}

TEST(DecoderTest, DecodeString) {
  std::vector<uint8_t> const data{0x05, 'l', 'o', 'r', 'e', 'm'};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeString(WireType::kLength), IsOkAndHolds("lorem"));
}

TEST(DecoderTest, StringDecodingError) {
  std::vector<uint8_t> const data{0x08, 'l', 'o', 'r', 'e', 'm'};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeString(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongWireTypeForString) {
  std::vector<uint8_t> const data{0x05, 'l', 'o', 'r', 'e', 'm'};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeString(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeString(WireType::kInt64), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeString(WireType::kDeprecatedStartGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeString(WireType::kDeprecatedEndGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeString(WireType::kInt32), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedInt32s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedInt32s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedInt32) {
  std::vector<uint8_t> const data{0x03, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedInt32s(), IsOkAndHolds(ElementsAre(18691)));
}

TEST(DecoderTest, DecodeTwoPackedInt32s) {
  std::vector<uint8_t> const data{0x05, 0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedInt32s(), IsOkAndHolds(ElementsAre(4610, 18691)));
}

TEST(DecoderTest, WrongPackedInt32Size) {
  std::vector<uint8_t> const data{0x03, 0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedInt32) {
  std::vector<uint8_t> const data1{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder1{data1};
  ASSERT_THAT(decoder1.DecodePackedInt32s(), IsOkAndHolds(ElementsAre(-1)));
  std::vector<uint8_t> const data2{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x10};
  Decoder decoder2{data2};
  EXPECT_THAT(decoder2.DecodePackedInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedInt64s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedInt64s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedInt64) {
  std::vector<uint8_t> const data{0x03, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedInt64s(), IsOkAndHolds(ElementsAre(18691)));
}

TEST(DecoderTest, DecodeTwoPackedInt64s) {
  std::vector<uint8_t> const data{0x05, 0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedInt64s(), IsOkAndHolds(ElementsAre(4610, 18691)));
}

TEST(DecoderTest, WrongPackedInt64Size) {
  std::vector<uint8_t> const data{0x03, 0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedInt64) {
  std::vector<uint8_t> const data1{
      0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01,
  };
  Decoder decoder1{data1};
  ASSERT_THAT(decoder1.DecodePackedInt64s(), IsOkAndHolds(ElementsAre(-1)));
  std::vector<uint8_t> const data2{
      0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02,
  };
  Decoder decoder2{data2};
  EXPECT_THAT(decoder2.DecodePackedInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedUInt32s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedUInt32s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedUInt32) {
  std::vector<uint8_t> const data{0x03, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedUInt32s(), IsOkAndHolds(ElementsAre(18691)));
}

TEST(DecoderTest, DecodeTwoPackedUInt32s) {
  std::vector<uint8_t> const data{0x05, 0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedUInt32s(), IsOkAndHolds(ElementsAre(4610, 18691)));
}

TEST(DecoderTest, WrongPackedUInt32Size) {
  std::vector<uint8_t> const data{0x03, 0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedUInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedUInt32) {
  std::vector<uint8_t> const data1{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder1{data1};
  ASSERT_THAT(decoder1.DecodePackedUInt32s(), IsOkAndHolds(ElementsAre(0xFFFFFFFF)));
  std::vector<uint8_t> const data2{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x10};
  Decoder decoder2{data2};
  EXPECT_THAT(decoder2.DecodePackedUInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedUInt64s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedUInt64s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedUInt64) {
  std::vector<uint8_t> const data{0x03, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedUInt64s(), IsOkAndHolds(ElementsAre(18691)));
}

TEST(DecoderTest, DecodeTwoPackedUInt64s) {
  std::vector<uint8_t> const data{0x05, 0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedUInt64s(), IsOkAndHolds(ElementsAre(4610, 18691)));
}

TEST(DecoderTest, WrongPackedUInt64Size) {
  std::vector<uint8_t> const data{0x03, 0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedUInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedUInt64) {
  std::vector<uint8_t> const data1{
      0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01,
  };
  Decoder decoder1{data1};
  ASSERT_THAT(decoder1.DecodePackedUInt64s(), IsOkAndHolds(ElementsAre(0xFFFFFFFFFFFFFFFF)));
  std::vector<uint8_t> const data2{
      0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02,
  };
  Decoder decoder2{data2};
  EXPECT_THAT(decoder2.DecodePackedUInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedSInt32s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedSInt32s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedSInt32) {
  std::vector<uint8_t> const data{0x01, 0x54};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedSInt32s(), IsOkAndHolds(ElementsAre(42)));
}

TEST(DecoderTest, DecodeTwoPackedSInt32s) {
  std::vector<uint8_t> const data{0x03, 0x84, 0x48, 0x53};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedSInt32s(), IsOkAndHolds(ElementsAre(4610, -42)));
}

TEST(DecoderTest, WrongPackedSInt32Size) {
  std::vector<uint8_t> const data{0x01, 0x84, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedSInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedSInt32) {
  std::vector<uint8_t> const data1{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder1{data1};
  ASSERT_THAT(decoder1.DecodePackedSInt32s(), IsOkAndHolds(ElementsAre(-0x80000000)));
  std::vector<uint8_t> const data2{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x10};
  Decoder decoder2{data2};
  EXPECT_THAT(decoder2.DecodePackedSInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedSInt64s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedSInt64s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedSInt64) {
  std::vector<uint8_t> const data{0x01, 0x54};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedSInt64s(), IsOkAndHolds(ElementsAre(42)));
}

TEST(DecoderTest, DecodeTwoPackedSInt64s) {
  std::vector<uint8_t> const data{0x03, 0x84, 0x48, 0x53};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedSInt64s(), IsOkAndHolds(ElementsAre(4610, -42)));
}

TEST(DecoderTest, WrongPackedSInt64Size) {
  std::vector<uint8_t> const data{0x01, 0x84, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedSInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedSInt64) {
  std::vector<uint8_t> const data1{
      0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01,
  };
  Decoder decoder1{data1};
  ASSERT_THAT(decoder1.DecodePackedSInt64s(), IsOkAndHolds(ElementsAre(-0x8000000000000000LL)));
  std::vector<uint8_t> const data2{
      0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02,
  };
  Decoder decoder2{data2};
  EXPECT_THAT(decoder2.DecodePackedSInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedFixedInt32s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFixedInt32s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedFixedInt32) {
  std::vector<uint8_t> const data{0x04, 0x2A, 0x00, 0x00, 0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFixedInt32s(), IsOkAndHolds(ElementsAre(42)));
}

TEST(DecoderTest, DecodeTwoPackedFixedInt32s) {
  std::vector<uint8_t> const data{0x08, 0x84, 0x48, 0x00, 0x00, 0xD6, 0xFF, 0xFF, 0xFF};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFixedInt32s(), IsOkAndHolds(ElementsAre(18564, -42)));
}

TEST(DecoderTest, WrongPackedFixedInt32Size) {
  std::vector<uint8_t> const data{0x02, 0x84, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFixedInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedFixedInt64s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFixedInt64s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedFixedInt64) {
  std::vector<uint8_t> const data{0x08, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFixedInt64s(), IsOkAndHolds(ElementsAre(42)));
}

TEST(DecoderTest, DecodeTwoPackedFixedInt64s) {
  std::vector<uint8_t> const data{
      0x10, 0x84, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xD6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  };
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFixedInt64s(), IsOkAndHolds(ElementsAre(18564, -42)));
}

TEST(DecoderTest, WrongPackedFixedInt64Size) {
  std::vector<uint8_t> const data{0x03, 0x84, 0x48, 0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFixedInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedFixedUInt32s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFixedUInt32s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedFixedUInt32) {
  std::vector<uint8_t> const data{0x04, 0x2A, 0x00, 0x00, 0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFixedUInt32s(), IsOkAndHolds(ElementsAre(42)));
}

TEST(DecoderTest, DecodeTwoPackedFixedUInt32s) {
  std::vector<uint8_t> const data{0x08, 0x84, 0x48, 0x00, 0x00, 0xD6, 0xFF, 0xFF, 0xFF};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFixedUInt32s(), IsOkAndHolds(ElementsAre(18564, 4294967254)));
}

TEST(DecoderTest, WrongPackedFixedUInt32Size) {
  std::vector<uint8_t> const data{0x02, 0x84, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFixedUInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedFixedUInt64s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFixedUInt64s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedFixedUInt64) {
  std::vector<uint8_t> const data{0x08, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFixedUInt64s(), IsOkAndHolds(ElementsAre(42)));
}

TEST(DecoderTest, DecodeTwoPackedFixedUInt64s) {
  std::vector<uint8_t> const data{
      0x10, 0x84, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xD6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  };
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFixedUInt64s(),
              IsOkAndHolds(ElementsAre(18564, 18446744073709551574ull)));
}

TEST(DecoderTest, WrongPackedFixedUInt64Size) {
  std::vector<uint8_t> const data{0x03, 0x84, 0x48, 0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFixedUInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedBools) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedBools(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedBool) {
  std::vector<uint8_t> const data{0x01, 0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedBools(), IsOkAndHolds(ElementsAre(false)));
}

TEST(DecoderTest, DecodeTwoPackedBools) {
  std::vector<uint8_t> const data{0x02, 0x01, 0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedBools(), IsOkAndHolds(ElementsAre(true, false)));
}

TEST(DecoderTest, DecodeEmptyPackedFloats) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFloats(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedFloat) {
  std::vector<uint8_t> const data{0x04, 0xD0, 0x0F, 0x49, 0x40};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFloats(), IsOkAndHolds(ElementsAre(FloatNear(3.14159f, 0.0001))));
}

TEST(DecoderTest, DecodeTwoPackedFloats) {
  std::vector<uint8_t> const data{0x08, 0x4D, 0xF8, 0x2D, 0x40, 0xD0, 0x0F, 0x49, 0x40};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFloats(),
              IsOkAndHolds(ElementsAre(FloatNear(2.71828f, 0.0001), FloatNear(3.14159f, 0.0001))));
}

TEST(DecoderTest, WrongPackedFloatSize) {
  std::vector<uint8_t> const data{0x06, 0xD0, 0x0F, 0x49, 0x40, 0x00, 0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedFloats(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedDouble) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedDoubles(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedDouble) {
  std::vector<uint8_t> const data{0x08, 0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedDoubles(),
              IsOkAndHolds(ElementsAre(DoubleNear(3.14159, 0.0001))));
}

TEST(DecoderTest, DecodeTwoPackedDoubles) {
  std::vector<uint8_t> const data{
      0x10, 0x90, 0xF7, 0xAA, 0x95, 0x09, 0xBF, 0x05, 0x40,
      0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40,
  };
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedDoubles(),
              IsOkAndHolds(ElementsAre(DoubleNear(2.71828, 0.0001), DoubleNear(3.14159, 0.0001))));
}

TEST(DecoderTest, WrongPackedDoubleSize) {
  std::vector<uint8_t> const data{
      0x0C, 0x90, 0xF7, 0xAA, 0x95, 0x09, 0xBF, 0x05, 0x40,
      0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40,
  };
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodePackedDoubles(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, SkipEmptyBuffer) {
  std::vector<uint8_t> const data;
  Decoder decoder{data};
  EXPECT_THAT(decoder.SkipRecord(WireType::kVarInt), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, SkipSingleByteInteger) {
  std::vector<uint8_t> const data{0x48, 0x2A};
  Decoder decoder{data};
  EXPECT_OK(decoder.SkipRecord(WireType::kVarInt));
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(42));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, SkipTwoByteInteger) {
  std::vector<uint8_t> const data{0x84, 0x48, 0x2A};
  Decoder decoder{data};
  EXPECT_OK(decoder.SkipRecord(WireType::kVarInt));
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(42));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, SkipInt64) {
  std::vector<uint8_t> const data{0x84, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A};
  Decoder decoder{data};
  EXPECT_OK(decoder.SkipRecord(WireType::kInt64));
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(42));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, SkipEmptySubMessage) {
  std::vector<uint8_t> const data{0x00, 0x2A};
  Decoder decoder{data};
  EXPECT_OK(decoder.SkipRecord(WireType::kLength));
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(42));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, SkipOneByteSubMessage) {
  std::vector<uint8_t> const data{0x01, 0x56, 0x2A};
  Decoder decoder{data};
  EXPECT_OK(decoder.SkipRecord(WireType::kLength));
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(42));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, SkipTwoByteSubMessage) {
  std::vector<uint8_t> const data{0x02, 0x12, 0x34, 0x2A};
  Decoder decoder{data};
  EXPECT_OK(decoder.SkipRecord(WireType::kLength));
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(42));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, SkipInt32) {
  std::vector<uint8_t> const data{0x84, 0x48, 0x00, 0x00, 0x2A};
  Decoder decoder{data};
  EXPECT_OK(decoder.SkipRecord(WireType::kInt32));
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(42));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, DecodeFields) {
  std::vector<uint8_t> const data{
      0x08, 0xC0, 0xC4, 0x07, 0x12, 0x0B, 's',  'a',  't',  'o',  'r',  ' ',  'a',
      'r',  'e',  'p',  'o',  0x19, 0x90, 0xF7, 0xAA, 0x95, 0x09, 0xBF, 0x05, 0x40,
  };
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 1, .wire_type = WireType::kVarInt}));
  EXPECT_THAT(decoder.DecodeUInt64(), IsOkAndHolds(123456));
  EXPECT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 2, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeString(WireType::kLength), IsOkAndHolds("sator arepo"));
  EXPECT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 3, .wire_type = WireType::kInt64}));
  EXPECT_THAT(decoder.DecodeDouble(WireType::kInt64), IsOkAndHolds(DoubleNear(2.71828, 0.0001)));
  EXPECT_TRUE(decoder.at_end());
}

class EncoderTest : public ::testing::Test {
 protected:
  Encoder encoder_;
};

TEST_F(EncoderTest, EncodeNothing) {
  EXPECT_TRUE(encoder_.empty());
  EXPECT_EQ(encoder_.size(), 0);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(IsEmpty()));
}

TEST_F(EncoderTest, EncodeSingleByteTag1) {
  encoder_.EncodeTag(FieldTag{.field_number = 2, .wire_type = WireType::kVarInt});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x10)));
}

TEST_F(EncoderTest, EncodeSingleByteTag2) {
  encoder_.EncodeTag(FieldTag{.field_number = 2, .wire_type = WireType::kLength});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x12)));
}

TEST_F(EncoderTest, EncodeSingleByteTag3) {
  encoder_.EncodeTag(FieldTag{.field_number = 3, .wire_type = WireType::kVarInt});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x18)));
}

TEST_F(EncoderTest, EncodeSingleByteTag4) {
  encoder_.EncodeTag(FieldTag{.field_number = 3, .wire_type = WireType::kInt32});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x1D)));
}

TEST_F(EncoderTest, EncodeTwoByteTag1) {
  encoder_.EncodeTag(FieldTag{.field_number = 2000, .wire_type = WireType::kVarInt});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x80, 0x7D)));
}

TEST_F(EncoderTest, EncodeTwoByteTag2) {
  encoder_.EncodeTag(FieldTag{.field_number = 2000, .wire_type = WireType::kLength});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x82, 0x7D)));
}

TEST_F(EncoderTest, EncodeTwoByteTag3) {
  encoder_.EncodeTag(FieldTag{.field_number = 2001, .wire_type = WireType::kVarInt});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x88, 0x7D)));
}

TEST_F(EncoderTest, EncodeTwoByteTag4) {
  encoder_.EncodeTag(FieldTag{.field_number = 2001, .wire_type = WireType::kInt32});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x8D, 0x7D)));
}

TEST_F(EncoderTest, EncodeZero) {
  encoder_.EncodeVarInt(0);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0)));
}

TEST_F(EncoderTest, EncodeSingleByteInteger) {
  encoder_.EncodeVarInt(42);
  EXPECT_FALSE(encoder_.empty());
  EXPECT_EQ(encoder_.size(), 1);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x2A)));
}

TEST_F(EncoderTest, EncodeTwoByteInteger) {
  encoder_.EncodeVarInt(4610);
  EXPECT_FALSE(encoder_.empty());
  EXPECT_EQ(encoder_.size(), 2);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x82, 0x24)));
}

TEST_F(EncoderTest, EncodeThreeByteInteger) {
  encoder_.EncodeVarInt(18691);
  EXPECT_FALSE(encoder_.empty());
  EXPECT_EQ(encoder_.size(), 3);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeInt32) {
  encoder_.EncodeInt32(123);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x7B)));
}

TEST_F(EncoderTest, EncodeNegativeInt32) {
  encoder_.EncodeInt32(-123);
  EXPECT_THAT(
      std::move(encoder_).Flatten(),
      BufferAsBytes(ElementsAre(0x85, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01)));
}

TEST_F(EncoderTest, EncodeUInt32) {
  encoder_.EncodeUInt32(123);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x7B)));
}

TEST_F(EncoderTest, EncodeBigUInt32) {
  encoder_.EncodeUInt32(0xFFFFFFFF);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xFF, 0xFF, 0xFF, 0xFF, 0x0F)));
}

TEST_F(EncoderTest, EncodeInt64) {
  encoder_.EncodeInt64(123);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x7B)));
}

TEST_F(EncoderTest, EncodeNegativeInt64) {
  encoder_.EncodeInt64(-123);
  EXPECT_THAT(
      std::move(encoder_).Flatten(),
      BufferAsBytes(ElementsAre(0x85, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01)));
}

TEST_F(EncoderTest, EncodeUInt64) {
  encoder_.EncodeUInt64(123);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x7B)));
}

TEST_F(EncoderTest, EncodeBigUInt64) {
  encoder_.EncodeUInt64(0xFFFFFFFFFFFFFFFFULL);
  EXPECT_THAT(
      std::move(encoder_).Flatten(),
      BufferAsBytes(ElementsAre(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01)));
}

TEST_F(EncoderTest, EncodeSingleBytePositiveEvenSInt32) {
  encoder_.EncodeSInt32(42);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x54)));
}

TEST_F(EncoderTest, EncodeSingleBytePositiveOddSInt32) {
  encoder_.EncodeSInt32(43);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x56)));
}

TEST_F(EncoderTest, EncodeSingleByteNegativeEvenSInt32) {
  encoder_.EncodeSInt32(-42);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x53)));
}

TEST_F(EncoderTest, EncodeSingleByteNegativeOddSInt32) {
  encoder_.EncodeSInt32(-43);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x55)));
}

TEST_F(EncoderTest, EncodeTwoBytePositiveEvenSInt32) {
  encoder_.EncodeSInt32(4610);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x84, 0x48)));
}

TEST_F(EncoderTest, EncodeTwoBytePositiveOddSInt32) {
  encoder_.EncodeSInt32(4611);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x86, 0x48)));
}

TEST_F(EncoderTest, EncodeTwoByteNegativeEvenSInt32) {
  encoder_.EncodeSInt32(-4610);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x83, 0x48)));
}

TEST_F(EncoderTest, EncodeTwoByteNegativeOddSInt32) {
  encoder_.EncodeSInt32(-4611);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x85, 0x48)));
}

TEST_F(EncoderTest, EncodeMaxPositiveEvenSInt32) {
  encoder_.EncodeSInt32(0x7FFFFFFE);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xFC, 0xFF, 0xFF, 0xFF, 0x0F)));
}

TEST_F(EncoderTest, EncodeMaxNegativeEvenSInt32) {
  encoder_.EncodeSInt32(-0x7FFFFFFF);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xFD, 0xFF, 0xFF, 0xFF, 0x0F)));
}

TEST_F(EncoderTest, EncodeMaxPositiveOddSInt32) {
  encoder_.EncodeSInt32(0x7FFFFFFF);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xFE, 0xFF, 0xFF, 0xFF, 0x0F)));
}

TEST_F(EncoderTest, EncodeMaxNegativeOddSInt32) {
  encoder_.EncodeSInt32(-0x80000000);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xFF, 0xFF, 0xFF, 0xFF, 0x0F)));
}

TEST_F(EncoderTest, EncodeSingleBytePositiveEvenSInt64) {
  encoder_.EncodeSInt64(42);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x54)));
}

TEST_F(EncoderTest, EncodeSingleBytePositiveOddSInt64) {
  encoder_.EncodeSInt64(43);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x56)));
}

TEST_F(EncoderTest, EncodeSingleByteNegativeEvenSInt64) {
  encoder_.EncodeSInt64(-42);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x53)));
}

TEST_F(EncoderTest, EncodeSingleByteNegativeOddSInt64) {
  encoder_.EncodeSInt64(-43);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x55)));
}

TEST_F(EncoderTest, EncodeTwoBytePositiveEvenSInt64) {
  encoder_.EncodeSInt64(4610);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x84, 0x48)));
}

TEST_F(EncoderTest, EncodeTwoBytePositiveOddSInt64) {
  encoder_.EncodeSInt64(4611);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x86, 0x48)));
}

TEST_F(EncoderTest, EncodeTwoByteNegativeEvenSInt64) {
  encoder_.EncodeSInt64(-4610);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x83, 0x48)));
}

TEST_F(EncoderTest, EncodeTwoByteNegativeOddSInt64) {
  encoder_.EncodeSInt64(-4611);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x85, 0x48)));
}

TEST_F(EncoderTest, EncodeMaxPositiveEvenSInt64) {
  encoder_.EncodeSInt64(0x7FFFFFFFFFFFFFFELL);
  EXPECT_THAT(
      std::move(encoder_).Flatten(),
      BufferAsBytes(ElementsAre(0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01)));
}

TEST_F(EncoderTest, EncodeMaxNegativeOddSInt64) {
  encoder_.EncodeSInt64(-0x7FFFFFFFFFFFFFFFLL);
  EXPECT_THAT(
      std::move(encoder_).Flatten(),
      BufferAsBytes(ElementsAre(0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01)));
}

TEST_F(EncoderTest, EncodeMaxPositiveOddSInt64) {
  encoder_.EncodeSInt64(0x7FFFFFFFFFFFFFFFLL);
  EXPECT_THAT(
      std::move(encoder_).Flatten(),
      BufferAsBytes(ElementsAre(0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01)));
}

TEST_F(EncoderTest, EncodeMaxNegativeEvenSInt64) {
  encoder_.EncodeSInt64(-0x8000000000000000LL);
  EXPECT_THAT(
      std::move(encoder_).Flatten(),
      BufferAsBytes(ElementsAre(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01)));
}

TEST_F(EncoderTest, EncodeFixedInt32) {
  encoder_.EncodeFixedInt32(4610);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x02, 0x12, 0x00, 0x00)));
}

TEST_F(EncoderTest, EncodeNegativeFixedInt32) {
  encoder_.EncodeFixedInt32(-4610);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xFE, 0xED, 0xFF, 0xFF)));
}

TEST_F(EncoderTest, EncodeFixedInt64) {
  encoder_.EncodeFixedInt64(4610);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x02, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)));
}

TEST_F(EncoderTest, EncodeNegativeFixedInt64) {
  encoder_.EncodeFixedInt64(-4610);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xFE, 0xED, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF)));
}

TEST_F(EncoderTest, EncodeTrue) {
  encoder_.EncodeBool(true);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x01)));
}

TEST_F(EncoderTest, EncodeFalse) {
  encoder_.EncodeBool(false);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x00)));
}

TEST_F(EncoderTest, EncodeFloat) {
  encoder_.EncodeFloat(3.14159f);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x0F, 0x49, 0x40)));
}

TEST_F(EncoderTest, EncodeDouble) {
  encoder_.EncodeDouble(3.14159);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40)));
}

TEST_F(EncoderTest, EncodeEmptyString) {
  encoder_.EncodeString("");
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x00)));
}

TEST_F(EncoderTest, EncodeString) {
  encoder_.EncodeString("lorem");
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x05, 'l', 'o', 'r', 'e', 'm')));
}

TEST_F(EncoderTest, EncodeEmptyPackedInt32s) {
  encoder_.EncodePackedInt32s({});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedInt32) {
  encoder_.EncodePackedInt32s({18691});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x03, 0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeTwoPackedInt32s) {
  encoder_.EncodePackedInt32s({4610, 18691});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x05, 0x82, 0x24, 0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeEmptyPackedInt64s) {
  encoder_.EncodePackedInt64s({});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedInt64) {
  encoder_.EncodePackedInt64s({18691});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x03, 0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeTwoPackedInt64s) {
  encoder_.EncodePackedInt64s({4610, 18691});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x05, 0x82, 0x24, 0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeEmptyPackedUInt32s) {
  encoder_.EncodePackedUInt32s({});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedUInt32) {
  encoder_.EncodePackedUInt32s({18691});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x03, 0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeTwoPackedUInt32s) {
  encoder_.EncodePackedUInt32s({4610, 18691});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x05, 0x82, 0x24, 0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeEmptyPackedUInt64s) {
  encoder_.EncodePackedUInt64s({});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedUInt64) {
  encoder_.EncodePackedUInt64s({18691});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x03, 0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeTwoPackedUInt64s) {
  encoder_.EncodePackedUInt64s({4610, 18691});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x05, 0x82, 0x24, 0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeEmptyPackedSInt32s) {
  encoder_.EncodePackedSInt32s({});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedSInt32) {
  encoder_.EncodePackedSInt32s({42});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x01, 0x54)));
}

TEST_F(EncoderTest, EncodeTwoPackedSInt32s) {
  encoder_.EncodePackedSInt32s({4610, -42});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x03, 0x84, 0x48, 0x53)));
}

TEST_F(EncoderTest, EncodeEmptyPackedSInt64s) {
  encoder_.EncodePackedSInt64s({});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedSInt64) {
  encoder_.EncodePackedSInt64s({42});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x01, 0x54)));
}

TEST_F(EncoderTest, EncodeTwoPackedSInt64s) {
  encoder_.EncodePackedSInt64s({4610, -42});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x03, 0x84, 0x48, 0x53)));
}

TEST_F(EncoderTest, EncodeEmptyPackedFixedInt32s) {
  encoder_.EncodePackedFixedInt32s({});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedFixedInt32) {
  encoder_.EncodePackedFixedInt32s({42});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x04, 0x2A, 0x00, 0x00, 0x00)));
}

TEST_F(EncoderTest, EncodeTwoPackedFixedInt32s) {
  encoder_.EncodePackedFixedInt32s({18564, -42});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x08, 0x84, 0x48, 0x00, 0x00, 0xD6, 0xFF, 0xFF, 0xFF)));
}

TEST_F(EncoderTest, EncodeEmptyPackedFixedInt64s) {
  encoder_.EncodePackedFixedInt64s({});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedFixedInt64) {
  encoder_.EncodePackedFixedInt64s({42});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x08, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)));
}

TEST_F(EncoderTest, EncodeTwoPackedFixedInt64s) {
  encoder_.EncodePackedFixedInt64s({18564, -42});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x10, 0x84, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD6,
                                        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF)));
}

TEST_F(EncoderTest, EncodeEmptyPackedFixedUInt32s) {
  encoder_.EncodePackedFixedUInt32s({});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedFixedUInt32) {
  encoder_.EncodePackedFixedUInt32s({42});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x04, 0x2A, 0x00, 0x00, 0x00)));
}

TEST_F(EncoderTest, EncodeTwoPackedFixedUInt32s) {
  encoder_.EncodePackedFixedUInt32s({18564, 42});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x08, 0x84, 0x48, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00)));
}

TEST_F(EncoderTest, EncodeEmptyPackedFixedUInt64s) {
  encoder_.EncodePackedFixedUInt64s({});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedFixedUInt64) {
  encoder_.EncodePackedFixedUInt64s({42});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x08, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)));
}

TEST_F(EncoderTest, EncodeTwoPackedFixedUInt64s) {
  encoder_.EncodePackedFixedUInt64s({18564, 42});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x10, 0x84, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A,
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)));
}

TEST_F(EncoderTest, EncodeEmptyPackedBools) {
  encoder_.EncodePackedBools({});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedBool) {
  encoder_.EncodePackedBools({false});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x01, 0x00)));
}

TEST_F(EncoderTest, EncodeTwoPackedBools) {
  encoder_.EncodePackedBools({true, false});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x02, 0x01, 0x00)));
}

TEST_F(EncoderTest, EncodeEmptyPackedFloats) {
  encoder_.EncodePackedFloats({});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedFloat) {
  encoder_.EncodePackedFloats({3.14159f});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x04, 0xD0, 0x0F, 0x49, 0x40)));
}

TEST_F(EncoderTest, EncodeTwoPackedFloats) {
  encoder_.EncodePackedFloats({2.71828f, 3.14159f});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x08, 0x4D, 0xF8, 0x2D, 0x40, 0xD0, 0x0F, 0x49, 0x40)));
}

TEST_F(EncoderTest, EncodeEmptyPackedDouble) {
  encoder_.EncodePackedDoubles({});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedDouble) {
  encoder_.EncodePackedDoubles({3.14159});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x08, 0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40)));
}

TEST_F(EncoderTest, EncodeTwoPackedDoubles) {
  encoder_.EncodePackedDoubles({2.71828, 3.14159});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x10, 0x90, 0xF7, 0xAA, 0x95, 0x09, 0xBF, 0x05, 0x40, 0x6E,
                                        0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40)));
}

TEST_F(EncoderTest, EncodeFields) {
  encoder_.EncodeTag(FieldTag{.field_number = 1, .wire_type = WireType::kVarInt});
  encoder_.EncodeUInt64(123456);
  encoder_.EncodeTag(FieldTag{.field_number = 2, .wire_type = WireType::kLength});
  encoder_.EncodeString("sator arepo");
  encoder_.EncodeTag(FieldTag{.field_number = 3, .wire_type = WireType::kInt64});
  encoder_.EncodeDouble(2.71828);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x08, 0xC0, 0xC4, 0x07, 0x12, 0x0B, 's', 'a', 't', 'o', 'r',
                                        ' ', 'a', 'r', 'e', 'p', 'o', 0x19, 0x90, 0xF7, 0xAA, 0x95,
                                        0x09, 0xBF, 0x05, 0x40)));
}

}  // namespace
