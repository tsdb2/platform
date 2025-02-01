#include "proto/wire.h"

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::DoubleNear;
using ::testing::ElementsAre;
using ::testing::FloatNear;
using ::tsdb2::proto::Decoder;
using ::tsdb2::proto::WireType;

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
  Decoder decoder{{0x03, 0x82, 0x24, 0x83, 0x92, 0x01}};
  EXPECT_THAT(decoder.DecodePackedInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedInt32) {
  Decoder decoder1{{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F}};
  ASSERT_THAT(decoder1.DecodePackedInt32s(), IsOkAndHolds(ElementsAre(-1)));
  Decoder decoder2{{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x10}};
  EXPECT_THAT(decoder2.DecodePackedInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
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
  Decoder decoder{{0x03, 0x82, 0x24, 0x83, 0x92, 0x01}};
  EXPECT_THAT(decoder.DecodePackedInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedInt64) {
  Decoder decoder1{{0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}};
  ASSERT_THAT(decoder1.DecodePackedInt64s(), IsOkAndHolds(ElementsAre(-1)));
  Decoder decoder2{{0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02}};
  EXPECT_THAT(decoder2.DecodePackedInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
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
  Decoder decoder{{0x03, 0x82, 0x24, 0x83, 0x92, 0x01}};
  EXPECT_THAT(decoder.DecodePackedUInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedUInt32) {
  Decoder decoder1{{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F}};
  ASSERT_THAT(decoder1.DecodePackedUInt32s(), IsOkAndHolds(ElementsAre(0xFFFFFFFF)));
  Decoder decoder2{{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x10}};
  EXPECT_THAT(decoder2.DecodePackedUInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
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
  Decoder decoder{{0x03, 0x82, 0x24, 0x83, 0x92, 0x01}};
  EXPECT_THAT(decoder.DecodePackedUInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedUInt64) {
  Decoder decoder1{{0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}};
  ASSERT_THAT(decoder1.DecodePackedUInt64s(), IsOkAndHolds(ElementsAre(0xFFFFFFFFFFFFFFFF)));
  Decoder decoder2{{0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02}};
  EXPECT_THAT(decoder2.DecodePackedUInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedSInt32s) {
  Decoder decoder{{0x00}};
  EXPECT_THAT(decoder.DecodePackedSInt32s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedSInt32) {
  Decoder decoder{{0x01, 0x54}};
  EXPECT_THAT(decoder.DecodePackedSInt32s(), IsOkAndHolds(ElementsAre(42)));
}

TEST(DecoderTest, DecodeTwoPackedSInt32s) {
  Decoder decoder{{0x03, 0x84, 0x48, 0x55}};
  EXPECT_THAT(decoder.DecodePackedSInt32s(), IsOkAndHolds(ElementsAre(4610, -42)));
}

TEST(DecoderTest, WrongPackedSInt32Size) {
  Decoder decoder{{0x01, 0x84, 0x48}};
  EXPECT_THAT(decoder.DecodePackedSInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedSInt32) {
  Decoder decoder1{{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F}};
  ASSERT_THAT(decoder1.DecodePackedSInt32s(), IsOkAndHolds(ElementsAre(-0x7FFFFFFF)));
  Decoder decoder2{{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x10}};
  EXPECT_THAT(decoder2.DecodePackedSInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedSInt64s) {
  Decoder decoder{{0x00}};
  EXPECT_THAT(decoder.DecodePackedSInt64s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedSInt64) {
  Decoder decoder{{0x01, 0x54}};
  EXPECT_THAT(decoder.DecodePackedSInt64s(), IsOkAndHolds(ElementsAre(42)));
}

TEST(DecoderTest, DecodeTwoPackedSInt64s) {
  Decoder decoder{{0x03, 0x84, 0x48, 0x55}};
  EXPECT_THAT(decoder.DecodePackedSInt64s(), IsOkAndHolds(ElementsAre(4610, -42)));
}

TEST(DecoderTest, WrongPackedSInt64Size) {
  Decoder decoder{{0x01, 0x84, 0x48}};
  EXPECT_THAT(decoder.DecodePackedSInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedSInt64) {
  Decoder decoder1{{0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}};
  ASSERT_THAT(decoder1.DecodePackedSInt64s(), IsOkAndHolds(ElementsAre(-0x7FFFFFFFFFFFFFFFLL)));
  Decoder decoder2{{0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02}};
  EXPECT_THAT(decoder2.DecodePackedSInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedFixedInt32s) {
  Decoder decoder{{0x00}};
  EXPECT_THAT(decoder.DecodePackedFixedInt32s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedFixedInt32) {
  Decoder decoder{{0x04, 0x2A, 0x00, 0x00, 0x00}};
  EXPECT_THAT(decoder.DecodePackedFixedInt32s(), IsOkAndHolds(ElementsAre(42)));
}

TEST(DecoderTest, DecodeTwoPackedFixedInt32s) {
  Decoder decoder{{0x08, 0x84, 0x48, 0x00, 0x00, 0xD6, 0xFF, 0xFF, 0xFF}};
  EXPECT_THAT(decoder.DecodePackedFixedInt32s(), IsOkAndHolds(ElementsAre(18564, -42)));
}

TEST(DecoderTest, WrongPackedFixedInt32Size) {
  Decoder decoder{{0x02, 0x84, 0x48}};
  EXPECT_THAT(decoder.DecodePackedFixedInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedFixedInt64s) {
  Decoder decoder{{0x00}};
  EXPECT_THAT(decoder.DecodePackedFixedInt64s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedFixedInt64) {
  Decoder decoder{{0x08, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
  EXPECT_THAT(decoder.DecodePackedFixedInt64s(), IsOkAndHolds(ElementsAre(42)));
}

TEST(DecoderTest, DecodeTwoPackedFixedInt64s) {
  Decoder decoder{{0x10, 0x84, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD6, 0xFF, 0xFF, 0xFF,
                   0xFF, 0xFF, 0xFF, 0xFF}};
  EXPECT_THAT(decoder.DecodePackedFixedInt64s(), IsOkAndHolds(ElementsAre(18564, -42)));
}

TEST(DecoderTest, WrongPackedFixedInt64Size) {
  Decoder decoder{{0x03, 0x84, 0x48, 0x00}};
  EXPECT_THAT(decoder.DecodePackedFixedInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedFixedUInt32s) {
  Decoder decoder{{0x00}};
  EXPECT_THAT(decoder.DecodePackedFixedUInt32s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedFixedUInt32) {
  Decoder decoder{{0x04, 0x2A, 0x00, 0x00, 0x00}};
  EXPECT_THAT(decoder.DecodePackedFixedUInt32s(), IsOkAndHolds(ElementsAre(42)));
}

TEST(DecoderTest, DecodeTwoPackedFixedUInt32s) {
  Decoder decoder{{0x08, 0x84, 0x48, 0x00, 0x00, 0xD6, 0xFF, 0xFF, 0xFF}};
  EXPECT_THAT(decoder.DecodePackedFixedUInt32s(), IsOkAndHolds(ElementsAre(18564, 4294967254)));
}

TEST(DecoderTest, WrongPackedFixedUInt32Size) {
  Decoder decoder{{0x02, 0x84, 0x48}};
  EXPECT_THAT(decoder.DecodePackedFixedUInt32s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedFixedUInt64s) {
  Decoder decoder{{0x00}};
  EXPECT_THAT(decoder.DecodePackedFixedUInt64s(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedFixedUInt64) {
  Decoder decoder{{0x08, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
  EXPECT_THAT(decoder.DecodePackedFixedUInt64s(), IsOkAndHolds(ElementsAre(42)));
}

TEST(DecoderTest, DecodeTwoPackedFixedUInt64s) {
  Decoder decoder{{0x10, 0x84, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD6, 0xFF, 0xFF, 0xFF,
                   0xFF, 0xFF, 0xFF, 0xFF}};
  EXPECT_THAT(decoder.DecodePackedFixedUInt64s(),
              IsOkAndHolds(ElementsAre(18564, 18446744073709551574ull)));
}

TEST(DecoderTest, WrongPackedFixedUInt64Size) {
  Decoder decoder{{0x03, 0x84, 0x48, 0x00}};
  EXPECT_THAT(decoder.DecodePackedFixedUInt64s(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedBools) {
  Decoder decoder{{0x00}};
  EXPECT_THAT(decoder.DecodePackedBools(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedBool) {
  Decoder decoder{{0x01, 0x00}};
  EXPECT_THAT(decoder.DecodePackedBools(), IsOkAndHolds(ElementsAre(false)));
}

TEST(DecoderTest, DecodeTwoPackedBools) {
  Decoder decoder{{0x02, 0x01, 0x00}};
  EXPECT_THAT(decoder.DecodePackedBools(), IsOkAndHolds(ElementsAre(true, false)));
}

TEST(DecoderTest, DecodeEmptyPackedFloats) {
  Decoder decoder{{0x00}};
  EXPECT_THAT(decoder.DecodePackedFloats(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedFloat) {
  Decoder decoder{{0x04, 0xD0, 0x0F, 0x49, 0x40}};
  EXPECT_THAT(decoder.DecodePackedFloats(), IsOkAndHolds(ElementsAre(FloatNear(3.14159f, 0.0001))));
}

TEST(DecoderTest, DecodeTwoPackedFloats) {
  Decoder decoder{{0x08, 0x4D, 0xF8, 0x2D, 0x40, 0xD0, 0x0F, 0x49, 0x40}};
  EXPECT_THAT(decoder.DecodePackedFloats(),
              IsOkAndHolds(ElementsAre(FloatNear(2.71828f, 0.0001), FloatNear(3.14159f, 0.0001))));
}

TEST(DecoderTest, WrongPackedFloatSize) {
  Decoder decoder{{0x06, 0xD0, 0x0F, 0x49, 0x40, 0x00, 0x00}};
  EXPECT_THAT(decoder.DecodePackedFloats(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedDouble) {
  Decoder decoder{{0x00}};
  EXPECT_THAT(decoder.DecodePackedDoubles(), IsOkAndHolds(ElementsAre()));
}

TEST(DecoderTest, DecodeOnePackedDouble) {
  Decoder decoder{{0x08, 0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40}};
  EXPECT_THAT(decoder.DecodePackedDoubles(),
              IsOkAndHolds(ElementsAre(DoubleNear(3.14159, 0.0001))));
}

TEST(DecoderTest, DecodeTwoPackedDoubles) {
  Decoder decoder{{0x10, 0x90, 0xF7, 0xAA, 0x95, 0x09, 0xBF, 0x05, 0x40, 0x6E, 0x86, 0x1B, 0xF0,
                   0xF9, 0x21, 0x09, 0x40}};
  EXPECT_THAT(decoder.DecodePackedDoubles(),
              IsOkAndHolds(ElementsAre(DoubleNear(2.71828, 0.0001), DoubleNear(3.14159, 0.0001))));
}

TEST(DecoderTest, WrongPackedDoubleSize) {
  Decoder decoder{{0x0C, 0x90, 0xF7, 0xAA, 0x95, 0x09, 0xBF, 0x05, 0x40, 0x6E, 0x86, 0x1B, 0xF0,
                   0xF9, 0x21, 0x09, 0x40}};
  EXPECT_THAT(decoder.DecodePackedDoubles(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, SkipEmptyBuffer) {
  Decoder decoder{{}};
  EXPECT_THAT(decoder.SkipRecord(WireType::kVarInt), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, SkipSingleByteInteger) {
  Decoder decoder{{0x48, 0x2A}};
  EXPECT_OK(decoder.SkipRecord(WireType::kVarInt));
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(42));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, SkipTwoByteInteger) {
  Decoder decoder{{0x84, 0x48, 0x2A}};
  EXPECT_OK(decoder.SkipRecord(WireType::kVarInt));
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(42));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, SkipInt64) {
  Decoder decoder{{0x84, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A}};
  EXPECT_OK(decoder.SkipRecord(WireType::kInt64));
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(42));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, SkipEmptySubMessage) {
  Decoder decoder{{0x00, 0x2A}};
  EXPECT_OK(decoder.SkipRecord(WireType::kLength));
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(42));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, SkipOneByteSubMessage) {
  Decoder decoder{{0x01, 0x56, 0x2A}};
  EXPECT_OK(decoder.SkipRecord(WireType::kLength));
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(42));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, SkipTwoByteSubMessage) {
  Decoder decoder{{0x02, 0x12, 0x34, 0x2A}};
  EXPECT_OK(decoder.SkipRecord(WireType::kLength));
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(42));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, SkipInt32) {
  Decoder decoder{{0x84, 0x48, 0x00, 0x00, 0x2A}};
  EXPECT_OK(decoder.SkipRecord(WireType::kInt32));
  EXPECT_THAT(decoder.DecodeVarInt(), IsOkAndHolds(42));
  EXPECT_TRUE(decoder.at_end());
}

}  // namespace
