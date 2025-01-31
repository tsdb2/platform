#include "proto/wire.h"

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::tsdb2::proto::Decoder;

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

// TODO

}  // namespace
