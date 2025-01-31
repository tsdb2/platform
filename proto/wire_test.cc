#include "proto/wire.h"

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::DoubleNear;
using ::testing::ElementsAre;
using ::testing::FloatNear;
using ::tsdb2::proto::Decoder;

TEST(DecoderTest, InitialState) {
  Decoder decoder{{0x82, 0x24, 0x83, 0x92, 0x01}};
  EXPECT_FALSE(decoder.at_end());
  EXPECT_EQ(decoder.remaining(), 5);
}

TEST(DecoderTest, DecodeSome) {
  Decoder decoder{{0x82, 0x24, 0x83, 0x92, 0x01}};
  ASSERT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(4610));
  EXPECT_FALSE(decoder.at_end());
  EXPECT_EQ(decoder.remaining(), 3);
}

TEST(DecoderTest, DecodeAll) {
  Decoder decoder{{0x82, 0x24, 0x83, 0x92, 0x01}};
  ASSERT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(4610));
  ASSERT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(18691));
  EXPECT_TRUE(decoder.at_end());
  EXPECT_EQ(decoder.remaining(), 0);
}

TEST(DecoderTest, DecodeSingleByteInteger) {
  Decoder decoder{{0x42}};
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(66));
}

TEST(DecoderTest, EmptyInteger) {
  Decoder decoder{{}};
  EXPECT_THAT(decoder.DecodeVarInt(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, IntegerDecodingError1) {
  Decoder decoder{{0x82}};
  EXPECT_THAT(decoder.DecodeVarInt(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, IntegerDecodingError2) {
  Decoder decoder{{0x82, 0x83}};
  EXPECT_THAT(decoder.DecodeVarInt(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeTwoByteInteger) {
  Decoder decoder{{0x82, 0x24}};
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(4610));
}

TEST(DecoderTest, DecodeThreeByteInteger) {
  Decoder decoder{{0x83, 0x92, 0x01}};
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(18691));
}

TEST(DecoderTest, DecodeMaxInteger) {
  Decoder decoder{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}};
  EXPECT_THAT(decoder.DecodeUInt64(), IsOkAndHolds(0xFFFFFFFFFFFFFFFF));
}

TEST(DecoderTest, IntegerOverflow) {
  Decoder decoder{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02}};
  EXPECT_THAT(decoder.DecodeUInt64(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeNegativeInteger1) {
  Decoder decoder{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}};
  EXPECT_THAT(decoder.DecodeInt64(), IsOkAndHolds(-1));
}

TEST(DecoderTest, DecodeNegativeInteger2) {
  Decoder decoder{{0xD6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}};
  EXPECT_THAT(decoder.DecodeInt64(), IsOkAndHolds(-42));
}

TEST(DecoderTest, DecodeMaxUInt32) {
  Decoder decoder{{0xFF, 0xFF, 0xFF, 0xFF, 0x0F}};
  EXPECT_THAT(decoder.DecodeUInt32(), IsOkAndHolds(0xFFFFFFFF));
}

TEST(DecoderTest, UInt32Overflow) {
  Decoder decoder{{0xFF, 0xFF, 0xFF, 0xFF, 0x10}};
  EXPECT_THAT(decoder.DecodeUInt32(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeMaxInt32) {
  Decoder decoder{{0xFF, 0xFF, 0xFF, 0xFF, 0x0F}};
  EXPECT_THAT(decoder.DecodeInt32(), IsOkAndHolds(-1));
}

TEST(DecoderTest, Int32Overflow) {
  Decoder decoder{{0xFF, 0xFF, 0xFF, 0xFF, 0x10}};
  EXPECT_THAT(decoder.DecodeInt32(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeSingleBytePositiveEvenSInt64) {
  Decoder decoder{{0x54}};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(42));
}

TEST(DecoderTest, DecodeSingleBytePositiveOddSInt64) {
  Decoder decoder{{0x56}};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(43));
}

TEST(DecoderTest, DecodeSingleByteNegativeEvenSInt64) {
  Decoder decoder{{0x55}};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(-42));
}

TEST(DecoderTest, DecodeSingleByteNegativeOddSInt64) {
  Decoder decoder{{0x57}};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(-43));
}

TEST(DecoderTest, DecodeTwoBytePositiveEvenSInt64) {
  Decoder decoder{{0x84, 0x48}};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(4610));
}

TEST(DecoderTest, DecodeTwoBytePositiveOddSInt64) {
  Decoder decoder{{0x86, 0x48}};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(4611));
}

TEST(DecoderTest, DecodeTwoByteNegativeEvenSInt64) {
  Decoder decoder{{0x85, 0x48}};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(-4610));
}

TEST(DecoderTest, DecodeTwoByteNegativeOddSInt64) {
  Decoder decoder{{0x87, 0x48}};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(-4611));
}

TEST(DecoderTest, DecodeMaxPositiveEvenSInt64) {
  Decoder decoder{{0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(0x7FFFFFFFFFFFFFFELL));
}

TEST(DecoderTest, DecodeMaxNegativeEvenSInt64) {
  Decoder decoder{{0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(-0x7FFFFFFFFFFFFFFELL));
}

TEST(DecoderTest, DecodeMaxPositiveOddSInt64) {
  Decoder decoder{{0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(0x7FFFFFFFFFFFFFFFLL));
}

TEST(DecoderTest, DecodeMaxNegativeOddSInt64) {
  Decoder decoder{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}};
  EXPECT_THAT(decoder.DecodeSInt64(), IsOkAndHolds(-0x7FFFFFFFFFFFFFFFLL));
}

TEST(DecoderTest, SInt64Overflow) {
  Decoder decoder{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02}};
  EXPECT_THAT(decoder.DecodeSInt64(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeSingleBytePositiveEvenSInt32) {
  Decoder decoder{{0x54}};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(42));
}

TEST(DecoderTest, DecodeSingleBytePositiveOddSInt32) {
  Decoder decoder{{0x56}};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(43));
}

TEST(DecoderTest, DecodeSingleByteNegativeEvenSInt32) {
  Decoder decoder{{0x55}};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(-42));
}

TEST(DecoderTest, DecodeSingleByteNegativeOddSInt32) {
  Decoder decoder{{0x57}};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(-43));
}

TEST(DecoderTest, DecodeTwoBytePositiveEvenSInt32) {
  Decoder decoder{{0x84, 0x48}};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(4610));
}

TEST(DecoderTest, DecodeTwoBytePositiveOddSInt32) {
  Decoder decoder{{0x86, 0x48}};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(4611));
}

TEST(DecoderTest, DecodeTwoByteNegativeEvenSInt32) {
  Decoder decoder{{0x85, 0x48}};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(-4610));
}

TEST(DecoderTest, DecodeTwoByteNegativeOddSInt32) {
  Decoder decoder{{0x87, 0x48}};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(-4611));
}

TEST(DecoderTest, DecodeMaxPositiveEvenSInt32) {
  Decoder decoder{{0xFC, 0xFF, 0xFF, 0xFF, 0x0F}};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(0x7FFFFFFE));
}

TEST(DecoderTest, DecodeMaxNegativeEvenSInt32) {
  Decoder decoder{{0xFD, 0xFF, 0xFF, 0xFF, 0x0F}};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(-0x7FFFFFFE));
}

TEST(DecoderTest, DecodeMaxPositiveOddSInt32) {
  Decoder decoder{{0xFE, 0xFF, 0xFF, 0xFF, 0x0F}};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(0x7FFFFFFF));
}

TEST(DecoderTest, DecodeMaxNegativeOddSInt32) {
  Decoder decoder{{0xFF, 0xFF, 0xFF, 0xFF, 0x0F}};
  EXPECT_THAT(decoder.DecodeSInt32(), IsOkAndHolds(-0x7FFFFFFF));
}

TEST(DecoderTest, SInt32Overflow) {
  Decoder decoder{{0xFF, 0xFF, 0xFF, 0xFF, 0x10}};
  EXPECT_THAT(decoder.DecodeSInt32(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeFixedInt32) {
  Decoder decoder{{0x12, 0x34, 0x56, 0x78}};
  EXPECT_THAT(decoder.DecodeFixedInt32(), IsOkAndHolds(0x78563412));
}

TEST(DecoderTest, DecodeNegativeFixedInt32) {
  Decoder decoder{{0x12, 0x34, 0x56, 0x87}};
  EXPECT_THAT(decoder.DecodeFixedInt32(), IsOkAndHolds(-2024393710));
}

TEST(DecoderTest, DecodeFixedInt64) {
  Decoder decoder{{0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56}};
  EXPECT_THAT(decoder.DecodeFixedInt64(), IsOkAndHolds(0x5634129078563412));
}

TEST(DecoderTest, DecodeNegativeFixedInt64) {
  Decoder decoder{{0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0xD6}};
  EXPECT_THAT(decoder.DecodeFixedInt64(), IsOkAndHolds(-3011761839100513262));
}

TEST(DecoderTest, DecodeBools) {
  Decoder decoder{{0x00, 0x01}};
  EXPECT_THAT(decoder.DecodeBool(), IsOkAndHolds(false));
  EXPECT_THAT(decoder.DecodeBool(), IsOkAndHolds(true));
}

TEST(DecoderTest, DecodeFloat) {
  Decoder decoder{{0xD0, 0x0F, 0x49, 0x40}};
  EXPECT_THAT(decoder.DecodeFloat(), IsOkAndHolds(FloatNear(3.14159f, 0.0001)));
}

TEST(DecoderTest, DecodeDouble) {
  Decoder decoder{{0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40}};
  EXPECT_THAT(decoder.DecodeDouble(), IsOkAndHolds(DoubleNear(3.14159, 0.0001)));
}

TEST(DecoderTest, DecodeEmptyString) {
  Decoder decoder{{0x00}};
  EXPECT_THAT(decoder.DecodeString(), IsOkAndHolds(""));
}

TEST(DecoderTest, DecodeString) {
  Decoder decoder{{0x05, 'l', 'o', 'r', 'e', 'm'}};
  EXPECT_THAT(decoder.DecodeString(), IsOkAndHolds("lorem"));
}

TEST(DecoderTest, StringDecodingError) {
  Decoder decoder{{0x08, 'l', 'o', 'r', 'e', 'm'}};
  EXPECT_THAT(decoder.DecodeString(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedInt32s) {
  Decoder decoder{{0x00}};
  EXPECT_THAT(decoder.DecodePackedInt32s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedInt32) {
  Decoder decoder{{0x03, 0x83, 0x92, 0x01}};
  EXPECT_THAT(decoder.DecodePackedInt32s(), IsOkAndHolds(ElementsAre(18691)));
}

TEST(DecoderTest, DecodeTwoPackedInt32s) {
  Decoder decoder{{0x05, 0x82, 0x24, 0x83, 0x92, 0x01}};
  EXPECT_THAT(decoder.DecodePackedInt32s(), IsOkAndHolds(ElementsAre(4610, 18691)));
}

TEST(DecoderTest, WrongPackedInt32Size) {
  Decoder decoder{{0x05, 0x82, 0x24, 0x83}};
  EXPECT_THAT(decoder.DecodePackedInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedInt64s) {
  Decoder decoder{{0x00}};
  EXPECT_THAT(decoder.DecodePackedInt64s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedInt64) {
  Decoder decoder{{0x03, 0x83, 0x92, 0x01}};
  EXPECT_THAT(decoder.DecodePackedInt64s(), IsOkAndHolds(ElementsAre(18691)));
}

TEST(DecoderTest, DecodeTwoPackedInt64s) {
  Decoder decoder{{0x05, 0x82, 0x24, 0x83, 0x92, 0x01}};
  EXPECT_THAT(decoder.DecodePackedInt64s(), IsOkAndHolds(ElementsAre(4610, 18691)));
}

TEST(DecoderTest, WrongPackedInt64Size) {
  Decoder decoder{{0x05, 0x82, 0x24, 0x83}};
  EXPECT_THAT(decoder.DecodePackedInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedUInt32s) {
  Decoder decoder{{0x00}};
  EXPECT_THAT(decoder.DecodePackedUInt32s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedUInt32) {
  Decoder decoder{{0x03, 0x83, 0x92, 0x01}};
  EXPECT_THAT(decoder.DecodePackedUInt32s(), IsOkAndHolds(ElementsAre(18691)));
}

TEST(DecoderTest, DecodeTwoPackedUInt32s) {
  Decoder decoder{{0x05, 0x82, 0x24, 0x83, 0x92, 0x01}};
  EXPECT_THAT(decoder.DecodePackedUInt32s(), IsOkAndHolds(ElementsAre(4610, 18691)));
}

TEST(DecoderTest, WrongPackedUInt32Size) {
  Decoder decoder{{0x05, 0x82, 0x24, 0x83}};
  EXPECT_THAT(decoder.DecodePackedUInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedUInt64s) {
  Decoder decoder{{0x00}};
  EXPECT_THAT(decoder.DecodePackedUInt64s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedUInt64) {
  Decoder decoder{{0x03, 0x83, 0x92, 0x01}};
  EXPECT_THAT(decoder.DecodePackedUInt64s(), IsOkAndHolds(ElementsAre(18691)));
}

TEST(DecoderTest, DecodeTwoPackedUInt64s) {
  Decoder decoder{{0x05, 0x82, 0x24, 0x83, 0x92, 0x01}};
  EXPECT_THAT(decoder.DecodePackedUInt64s(), IsOkAndHolds(ElementsAre(4610, 18691)));
}

TEST(DecoderTest, WrongPackedUInt64Size) {
  Decoder decoder{{0x05, 0x82, 0x24, 0x83}};
  EXPECT_THAT(decoder.DecodePackedUInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

// TODO

}  // namespace
