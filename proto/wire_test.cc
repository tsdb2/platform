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

// TODO

}  // namespace
