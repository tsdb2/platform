#include "proto/wire_format.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "common/testing.h"
#include "common/utilities.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "io/buffer.h"
#include "io/buffer_testing.h"
#include "proto/duration.pb.sync.h"
#include "proto/timestamp.pb.sync.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::tsdb2::proto::Decoder;
using ::tsdb2::proto::Encoder;
using ::tsdb2::proto::FieldTag;
using ::tsdb2::proto::WireType;
using ::tsdb2::testing::io::BufferAsBytes;

enum class TestEnum : int8_t {
  kCyan = -30,
  kMagenta = -20,
  kYellow = -10,
  kRed = 10,
  kGreen = 20,
  kBlue = 30,
};

template <typename Float>
auto Near(Float value);

template <>
auto Near<float>(float const value) {
  return ::testing::FloatNear(value, 0.0001);
}

template <>
auto Near<double>(double const value) {
  return ::testing::DoubleNear(value, 0.0001);
}

tsdb2::io::Buffer EncodeGoogleProtobufTimestamp(size_t const field_number,
                                                ::google::protobuf::Timestamp const& timestamp) {
  Encoder encoder;
  encoder.EncodeSubMessageField(field_number, ::google::protobuf::Timestamp::Encode(timestamp));
  return std::move(encoder).Flatten();
}

absl::StatusOr<::google::protobuf::Timestamp> DecodeGoogleProtobufTimestamp(
    size_t const field_number, absl::Span<uint8_t const> data) {
  Decoder decoder{data};
  DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
  if (!maybe_tag.has_value()) {
    return absl::FailedPreconditionError("invalid Google API protobuf encoding");
  }
  auto const tag = maybe_tag.value();
  if (tag.field_number != field_number) {
    return absl::FailedPreconditionError(
        absl::StrCat("invalid field number: ", tag.field_number, " != ", field_number));
  }
  DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));
  return ::google::protobuf::Timestamp::Decode(child_span);
}

tsdb2::io::Buffer EncodeGoogleProtobufDuration(size_t const field_number,
                                               ::google::protobuf::Duration const& duration) {
  Encoder encoder;
  encoder.EncodeSubMessageField(field_number, ::google::protobuf::Duration::Encode(duration));
  return std::move(encoder).Flatten();
}

absl::StatusOr<::google::protobuf::Duration> DecodeGoogleProtobufDuration(
    size_t const field_number, absl::Span<uint8_t const> data) {
  Decoder decoder{data};
  DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
  if (!maybe_tag.has_value()) {
    return absl::FailedPreconditionError("invalid Google API protobuf encoding");
  }
  auto const tag = maybe_tag.value();
  if (tag.field_number != field_number) {
    return absl::FailedPreconditionError(
        absl::StrCat("invalid field number: ", tag.field_number, " != ", field_number));
  }
  DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(WireType::kLength));
  return ::google::protobuf::Duration::Decode(child_span);
}

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

TEST(DecoderTest, DecodeTagSkippingGroup) {
  std::vector<uint8_t> const data{0x43, 0x08, 0x2A, 0x12, 0x03, 'F', 'O', 'O', 0x44, 0x20};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 4, .wire_type = WireType::kVarInt}));
}

TEST(DecoderTest, DecodeTagSkippingTwoGroups) {
  std::vector<uint8_t> const data{
      0x43, 0x08, 0x2A, 0x12, 0x03, 'F',  'O',  'O',  0x44, 0x43,
      0x12, 0x03, 'B',  'A',  'R',  0x08, 0x2B, 0x44, 0x20,
  };
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 4, .wire_type = WireType::kVarInt}));
}

TEST(DecoderTest, SkipGroupAndFinish) {
  std::vector<uint8_t> const data{0x43, 0x08, 0x2A, 0x12, 0x03, 'F', 'O', 'O', 0x44};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeTag(), IsOkAndHolds(std::nullopt));
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

TEST(DecoderTest, DecodeEnum) {
  std::vector<uint8_t> const data{0x0A};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeEnumField<TestEnum>(WireType::kVarInt), IsOkAndHolds(TestEnum::kRed));
}

TEST(DecoderTest, DecodeNegativeEnum) {
  std::vector<uint8_t> const data{0xEC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeEnumField<TestEnum>(WireType::kVarInt),
              IsOkAndHolds(TestEnum::kMagenta));
}

TEST(DecoderTest, EnumOverflow) {
  std::vector<uint8_t> const data{0x80, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeEnumField<TestEnum>(WireType::kVarInt),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(DecoderTest, EnumUnderflow) {
  std::vector<uint8_t> const data{0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeEnumField<TestEnum>(WireType::kVarInt),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(DecoderTest, WrongWireTypeForEnum) {
  std::vector<uint8_t> const data{0x0A};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeEnumField<TestEnum>(WireType::kInt32),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeMaxInteger) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeUInt64Field(WireType::kVarInt), IsOkAndHolds(0xFFFFFFFFFFFFFFFF));
}

TEST(DecoderTest, IntegerOverflow) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeUInt64Field(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongWireTypeForInteger) {
  std::vector<uint8_t> const data{0x42};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeUInt64Field(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeNegativeInteger1) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeInt64Field(WireType::kVarInt), IsOkAndHolds(-1));
}

TEST(DecoderTest, DecodeNegativeInteger2) {
  std::vector<uint8_t> const data{0xD6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeInt64Field(WireType::kVarInt), IsOkAndHolds(-42));
}

TEST(DecoderTest, WrongWireTypeForSignedInteger) {
  std::vector<uint8_t> const data{0x42};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeInt64Field(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeMaxUInt32) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeUInt32Field(WireType::kVarInt), IsOkAndHolds(0xFFFFFFFF));
}

TEST(DecoderTest, UInt32Overflow) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0x10};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeUInt32Field(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongWireTypeForUInt32) {
  std::vector<uint8_t> const data{0x42};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeUInt32Field(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeMaxInt32) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeInt32Field(WireType::kVarInt), IsOkAndHolds(-1));
}

TEST(DecoderTest, DecodeNegativeInt32WithLargeEncoding1) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeInt32Field(WireType::kVarInt), IsOkAndHolds(-1));
}

TEST(DecoderTest, DecodeNegativeInt32WithLargeEncoding2) {
  std::vector<uint8_t> const data{0xD6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeInt32Field(WireType::kVarInt), IsOkAndHolds(-42));
}

TEST(DecoderTest, Int32Overflow) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0x10};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeInt32Field(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongWireTypeForInt32) {
  std::vector<uint8_t> const data{0x42};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeInt32Field(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeSingleBytePositiveEvenSInt64) {
  std::vector<uint8_t> const data{0x54};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64Field(WireType::kVarInt), IsOkAndHolds(42));
}

TEST(DecoderTest, DecodeSingleBytePositiveOddSInt64) {
  std::vector<uint8_t> const data{0x56};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64Field(WireType::kVarInt), IsOkAndHolds(43));
}

TEST(DecoderTest, DecodeSingleByteNegativeEvenSInt64) {
  std::vector<uint8_t> const data{0x53};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64Field(WireType::kVarInt), IsOkAndHolds(-42));
}

TEST(DecoderTest, DecodeSingleByteNegativeOddSInt64) {
  std::vector<uint8_t> const data{0x55};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64Field(WireType::kVarInt), IsOkAndHolds(-43));
}

TEST(DecoderTest, DecodeTwoBytePositiveEvenSInt64) {
  std::vector<uint8_t> const data{0x84, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64Field(WireType::kVarInt), IsOkAndHolds(4610));
}

TEST(DecoderTest, DecodeTwoBytePositiveOddSInt64) {
  std::vector<uint8_t> const data{0x86, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64Field(WireType::kVarInt), IsOkAndHolds(4611));
}

TEST(DecoderTest, DecodeTwoByteNegativeEvenSInt64) {
  std::vector<uint8_t> const data{0x83, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64Field(WireType::kVarInt), IsOkAndHolds(-4610));
}

TEST(DecoderTest, DecodeTwoByteNegativeOddSInt64) {
  std::vector<uint8_t> const data{0x85, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64Field(WireType::kVarInt), IsOkAndHolds(-4611));
}

TEST(DecoderTest, DecodeMaxPositiveEvenSInt64) {
  std::vector<uint8_t> const data{0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64Field(WireType::kVarInt), IsOkAndHolds(0x7FFFFFFFFFFFFFFELL));
}

TEST(DecoderTest, DecodeMaxNegativeOddSInt64) {
  std::vector<uint8_t> const data{0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64Field(WireType::kVarInt), IsOkAndHolds(-0x7FFFFFFFFFFFFFFFLL));
}

TEST(DecoderTest, DecodeMaxPositiveOddSInt64) {
  std::vector<uint8_t> const data{0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64Field(WireType::kVarInt), IsOkAndHolds(0x7FFFFFFFFFFFFFFFLL));
}

TEST(DecoderTest, DecodeMaxNegativeEvenSInt64) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64Field(WireType::kVarInt), IsOkAndHolds(-0x8000000000000000LL));
}

TEST(DecoderTest, SInt64Overflow) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64Field(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongWireTypeForSInt64) {
  std::vector<uint8_t> const data{0x42};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt64Field(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeSingleBytePositiveEvenSInt32) {
  std::vector<uint8_t> const data{0x54};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32Field(WireType::kVarInt), IsOkAndHolds(42));
}

TEST(DecoderTest, DecodeSingleBytePositiveOddSInt32) {
  std::vector<uint8_t> const data{0x56};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32Field(WireType::kVarInt), IsOkAndHolds(43));
}

TEST(DecoderTest, DecodeSingleByteNegativeEvenSInt32) {
  std::vector<uint8_t> const data{0x53};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32Field(WireType::kVarInt), IsOkAndHolds(-42));
}

TEST(DecoderTest, DecodeSingleByteNegativeOddSInt32) {
  std::vector<uint8_t> const data{0x55};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32Field(WireType::kVarInt), IsOkAndHolds(-43));
}

TEST(DecoderTest, DecodeTwoBytePositiveEvenSInt32) {
  std::vector<uint8_t> const data{0x84, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32Field(WireType::kVarInt), IsOkAndHolds(4610));
}

TEST(DecoderTest, DecodeTwoBytePositiveOddSInt32) {
  std::vector<uint8_t> const data{0x86, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32Field(WireType::kVarInt), IsOkAndHolds(4611));
}

TEST(DecoderTest, DecodeTwoByteNegativeEvenSInt32) {
  std::vector<uint8_t> const data{0x83, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32Field(WireType::kVarInt), IsOkAndHolds(-4610));
}

TEST(DecoderTest, DecodeTwoByteNegativeOddSInt32) {
  std::vector<uint8_t> const data{0x85, 0x48};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32Field(WireType::kVarInt), IsOkAndHolds(-4611));
}

TEST(DecoderTest, DecodeMaxPositiveEvenSInt32) {
  std::vector<uint8_t> const data{0xFC, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32Field(WireType::kVarInt), IsOkAndHolds(0x7FFFFFFE));
}

TEST(DecoderTest, DecodeMaxNegativeEvenSInt32) {
  std::vector<uint8_t> const data{0xFD, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32Field(WireType::kVarInt), IsOkAndHolds(-0x7FFFFFFF));
}

TEST(DecoderTest, DecodeMaxPositiveOddSInt32) {
  std::vector<uint8_t> const data{0xFE, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32Field(WireType::kVarInt), IsOkAndHolds(0x7FFFFFFF));
}

TEST(DecoderTest, DecodeMaxNegativeOddSInt32) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32Field(WireType::kVarInt), IsOkAndHolds(-0x80000000));
}

TEST(DecoderTest, SInt32Overflow) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0x10};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32Field(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongWireTypeForSInt32) {
  std::vector<uint8_t> const data{0x42};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeSInt32Field(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeFixedInt32) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedInt32Field(WireType::kInt32), IsOkAndHolds(0x78563412));
}

TEST(DecoderTest, DecodeNegativeFixedInt32) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x87};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedInt32Field(WireType::kInt32), IsOkAndHolds(-2024393710));
}

TEST(DecoderTest, WrongWireTypeForFixedInt32) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedInt32Field(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedInt32Field(WireType::kInt64),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedInt32Field(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedInt32Field(WireType::kDeprecatedStartGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedInt32Field(WireType::kDeprecatedEndGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeFixedUInt32) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedUInt32Field(WireType::kInt32), IsOkAndHolds(0x78563412));
}

TEST(DecoderTest, WrongWireTypeForFixedUInt32) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedUInt32Field(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedUInt32Field(WireType::kInt64),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedUInt32Field(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedUInt32Field(WireType::kDeprecatedStartGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedUInt32Field(WireType::kDeprecatedEndGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeFixedInt64) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedInt64Field(WireType::kInt64), IsOkAndHolds(0x5634129078563412));
}

TEST(DecoderTest, DecodeNegativeFixedInt64) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0xD6};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedInt64Field(WireType::kInt64), IsOkAndHolds(-3011761839100513262));
}

TEST(DecoderTest, WrongWireTypeForFixedInt64) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedInt64Field(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedInt64Field(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedInt64Field(WireType::kDeprecatedStartGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedInt64Field(WireType::kDeprecatedEndGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedInt64Field(WireType::kInt32),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeFixedUInt64) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedUInt64Field(WireType::kInt64), IsOkAndHolds(0x5634129078563412));
}

TEST(DecoderTest, WrongWireTypeForFixedUInt64) {
  std::vector<uint8_t> const data{0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFixedUInt64Field(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedUInt64Field(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedUInt64Field(WireType::kDeprecatedStartGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedUInt64Field(WireType::kDeprecatedEndGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFixedUInt64Field(WireType::kInt32),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeBools) {
  std::vector<uint8_t> const data{0x00, 0x01};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeBoolField(WireType::kVarInt), IsOkAndHolds(false));
  EXPECT_THAT(decoder.DecodeBoolField(WireType::kVarInt), IsOkAndHolds(true));
}

TEST(DecoderTest, WrongWireTypeForBool) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeBoolField(WireType::kInt64),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeBoolField(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeBoolField(WireType::kDeprecatedStartGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeBoolField(WireType::kDeprecatedEndGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeBoolField(WireType::kInt32),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeFloat) {
  std::vector<uint8_t> const data{0xD0, 0x0F, 0x49, 0x40};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFloatField(WireType::kInt32), IsOkAndHolds(Near(3.14159f)));
}

TEST(DecoderTest, WrongWireTypeForFloat) {
  std::vector<uint8_t> const data{0xD0, 0x0F, 0x49, 0x40};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeFloatField(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFloatField(WireType::kInt64),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFloatField(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFloatField(WireType::kDeprecatedStartGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeFloatField(WireType::kDeprecatedEndGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeDouble) {
  std::vector<uint8_t> const data{0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeDoubleField(WireType::kInt64), IsOkAndHolds(Near(3.14159)));
}

TEST(DecoderTest, WrongWireTypeForDouble) {
  std::vector<uint8_t> const data{0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeDoubleField(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeDoubleField(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeDoubleField(WireType::kDeprecatedStartGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeDoubleField(WireType::kDeprecatedEndGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeDoubleField(WireType::kInt32),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyString) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeStringField(WireType::kLength), IsOkAndHolds(""));
}

TEST(DecoderTest, DecodeString) {
  std::vector<uint8_t> const data{0x05, 'l', 'o', 'r', 'e', 'm'};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeStringField(WireType::kLength), IsOkAndHolds("lorem"));
}

TEST(DecoderTest, StringDecodingError) {
  std::vector<uint8_t> const data{0x08, 'l', 'o', 'r', 'e', 'm'};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeStringField(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongWireTypeForString) {
  std::vector<uint8_t> const data{0x05, 'l', 'o', 'r', 'e', 'm'};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeStringField(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeStringField(WireType::kInt64),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeStringField(WireType::kDeprecatedStartGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeStringField(WireType::kDeprecatedEndGroup),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(decoder.DecodeStringField(WireType::kInt32),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyBytes) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeBytesField(WireType::kLength), IsOkAndHolds(IsEmpty()));
}

TEST(DecoderTest, DecodeBytes) {
  std::vector<uint8_t> const data{0x03, 12, 34, 56};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeBytesField(WireType::kLength), IsOkAndHolds(ElementsAre(12, 34, 56)));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, MoreDataAfterBytes) {
  std::vector<uint8_t> const data{0x02, 12, 34, 56, 78};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeBytesField(WireType::kLength), IsOkAndHolds(ElementsAre(12, 34)));
  EXPECT_FALSE(decoder.at_end());
}

TEST(DecoderTest, BytesOverflow) {
  std::vector<uint8_t> const data{0x03, 12, 34};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeBytesField(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, IncorrectWireTypeForBytes) {
  std::vector<uint8_t> const data{0x02, 12, 34};
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeBytesField(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeTime) {
  auto const data = EncodeGoogleProtobufTimestamp(42, {.seconds = 123, .nanos = 456});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeTimeField(WireType::kLength),
              IsOkAndHolds(absl::UnixEpoch() + absl::Seconds(123) + absl::Nanoseconds(456)));
}

TEST(DecoderTest, DecodeZeroTime) {
  auto const data = EncodeGoogleProtobufTimestamp(42, {.seconds = 0, .nanos = 0});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeTimeField(WireType::kLength), IsOkAndHolds(absl::UnixEpoch()));
}

TEST(DecoderTest, DecodeEmptyTime) {
  auto const data =
      EncodeGoogleProtobufTimestamp(42, {.seconds = std::nullopt, .nanos = std::nullopt});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeTimeField(WireType::kLength), IsOkAndHolds(absl::UnixEpoch()));
}

TEST(DecoderTest, DecodeTimeWithoutSeconds) {
  auto const data = EncodeGoogleProtobufTimestamp(42, {.seconds = std::nullopt, .nanos = 123});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeTimeField(WireType::kLength),
              IsOkAndHolds(absl::UnixEpoch() + absl::Nanoseconds(123)));
}

TEST(DecoderTest, DecodeTimeWithoutNanoseconds) {
  auto const data = EncodeGoogleProtobufTimestamp(42, {.seconds = 456, .nanos = std::nullopt});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeTimeField(WireType::kLength),
              IsOkAndHolds(absl::UnixEpoch() + absl::Seconds(456)));
}

TEST(DecoderTest, DecodeTimeWithNegativeSeconds) {
  auto const data = EncodeGoogleProtobufTimestamp(42, {.seconds = -123, .nanos = 456});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeTimeField(WireType::kLength),
              IsOkAndHolds(absl::UnixEpoch() - absl::Seconds(123) + absl::Nanoseconds(456)));
}

TEST(DecoderTest, DecodeTimeWithNegativeNanoseconds) {
  auto const data = EncodeGoogleProtobufTimestamp(42, {.seconds = 123, .nanos = -456});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeTimeField(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, InvalidTimeNanoseconds) {
  auto const data = EncodeGoogleProtobufTimestamp(42, {.seconds = 123, .nanos = 1000000000});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeTimeField(WireType::kLength), StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(DecoderTest, DecodeDuration) {
  auto const data = EncodeGoogleProtobufDuration(42, {.seconds = 123, .nanos = 456});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeDurationField(WireType::kLength),
              IsOkAndHolds(absl::Seconds(123) + absl::Nanoseconds(456)));
}

TEST(DecoderTest, DecodeZeroDuration) {
  auto const data = EncodeGoogleProtobufDuration(42, {.seconds = 0, .nanos = 0});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeDurationField(WireType::kLength), IsOkAndHolds(absl::ZeroDuration()));
}

TEST(DecoderTest, DecodeEmptyDuration) {
  auto const data =
      EncodeGoogleProtobufDuration(42, {.seconds = std::nullopt, .nanos = std::nullopt});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeDurationField(WireType::kLength), IsOkAndHolds(absl::ZeroDuration()));
}

TEST(DecoderTest, DecodeDurationWithoutSeconds) {
  auto const data = EncodeGoogleProtobufDuration(42, {.seconds = std::nullopt, .nanos = 123});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeDurationField(WireType::kLength), IsOkAndHolds(absl::Nanoseconds(123)));
}

TEST(DecoderTest, DecodeDurationWithoutNanoseconds) {
  auto const data = EncodeGoogleProtobufDuration(42, {.seconds = 456, .nanos = std::nullopt});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeDurationField(WireType::kLength), IsOkAndHolds(absl::Seconds(456)));
}

TEST(DecoderTest, DurationSignConflict1) {
  auto const data = EncodeGoogleProtobufDuration(42, {.seconds = 123, .nanos = -456});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeDurationField(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DurationSignConflict2) {
  auto const data = EncodeGoogleProtobufDuration(42, {.seconds = -123, .nanos = 456});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeDurationField(WireType::kLength),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeNegativeDuration) {
  auto const data = EncodeGoogleProtobufDuration(42, {.seconds = -123, .nanos = -456});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeDurationField(WireType::kLength),
              IsOkAndHolds(-(absl::Seconds(123) + absl::Nanoseconds(456))));
}

TEST(DecoderTest, InvalidNegativeDurationNanos) {
  auto const data = EncodeGoogleProtobufDuration(42, {.seconds = -123, .nanos = -1000000000});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeDurationField(WireType::kLength),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(DecoderTest, InvalidDurationNanos) {
  auto const data = EncodeGoogleProtobufDuration(42, {.seconds = 123, .nanos = 1000000000});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeDurationField(WireType::kLength),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(DecoderTest, InvalidNegativeDurationSeconds) {
  auto const data = EncodeGoogleProtobufDuration(42, {.seconds = -315576000001, .nanos = -456});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeDurationField(WireType::kLength),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(DecoderTest, InvalidDurationSeconds) {
  auto const data = EncodeGoogleProtobufDuration(42, {.seconds = 315576000001, .nanos = 456});
  Decoder decoder{data.span()};
  ASSERT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 42, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeDurationField(WireType::kLength),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(DecoderTest, GetSingleByteChildSpan) {
  std::vector<uint8_t> const data{0x01, 0x42};
  Decoder decoder{data};
  EXPECT_THAT(decoder.GetChildSpan(WireType::kLength), IsOkAndHolds(ElementsAre(0x42)));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, GetTwoByteChildSpan) {
  std::vector<uint8_t> const data{0x02, 0x12, 0x34};
  Decoder decoder{data};
  EXPECT_THAT(decoder.GetChildSpan(WireType::kLength), IsOkAndHolds(ElementsAre(0x12, 0x34)));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, MoreDataAfterChildSpan) {
  std::vector<uint8_t> const data{0x01, 0x12, 0x34};
  Decoder decoder{data};
  EXPECT_THAT(decoder.GetChildSpan(WireType::kLength), IsOkAndHolds(ElementsAre(0x12)));
  EXPECT_FALSE(decoder.at_end());
}

TEST(DecoderTest, ChildSpanTooLong) {
  std::vector<uint8_t> const data{0x03, 0x12, 0x34};
  Decoder decoder{data};
  EXPECT_THAT(decoder.GetChildSpan(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongWireTypeForChildSpan) {
  std::vector<uint8_t> const data{0x01, 0x42};
  Decoder decoder{data};
  EXPECT_THAT(decoder.GetChildSpan(WireType::kVarInt),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_FALSE(decoder.at_end());
}

TEST(DecoderTest, DecodeEmptyPackedInt32s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedInt32s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34));
}

TEST(DecoderTest, DecodeRepeatedInt32) {
  std::vector<uint8_t> const data{0x83, 0x92, 0x01};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedInt32s(WireType::kVarInt, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 18691));
}

TEST(DecoderTest, DecodeOnePackedInt32) {
  std::vector<uint8_t> const data{0x03, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedInt32s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 18691));
}

TEST(DecoderTest, DecodeTwoPackedInt32s) {
  std::vector<uint8_t> const data{0x05, 0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedInt32s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 4610, 18691));
}

TEST(DecoderTest, WrongWireTypeForRepeatedInt32) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedInt32s(WireType::kInt32, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongPackedInt32Size) {
  std::vector<uint8_t> const data{0x03, 0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedInt32s(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedInt32) {
  std::vector<int32_t> values{12, 34};
  std::vector<uint8_t> const data1{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder1{data1};
  ASSERT_OK(decoder1.DecodeRepeatedInt32s(WireType::kLength, &values));
  ASSERT_THAT(values, ElementsAre(12, 34, -1));
  std::vector<uint8_t> const data2{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x10};
  Decoder decoder2{data2};
  EXPECT_THAT(decoder2.DecodeRepeatedInt32s(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedInt64s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedInt64s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34));
}

TEST(DecoderTest, DecodeRepeatedInt64) {
  std::vector<uint8_t> const data{0x83, 0x92, 0x01};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedInt64s(WireType::kVarInt, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 18691));
}

TEST(DecoderTest, DecodeOnePackedInt64) {
  std::vector<uint8_t> const data{0x03, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedInt64s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 18691));
}

TEST(DecoderTest, DecodeTwoPackedInt64s) {
  std::vector<uint8_t> const data{0x05, 0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedInt64s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 4610, 18691));
}

TEST(DecoderTest, WrongWireTypeForRepeatedInt64) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedInt64s(WireType::kInt64, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongPackedInt64Size) {
  std::vector<uint8_t> const data{0x03, 0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedInt64s(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedInt64) {
  std::vector<int64_t> values{12, 34};
  std::vector<uint8_t> const data1{
      0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01,
  };
  Decoder decoder1{data1};
  ASSERT_OK(decoder1.DecodeRepeatedInt64s(WireType::kLength, &values));
  ASSERT_THAT(values, ElementsAre(12, 34, -1));
  std::vector<uint8_t> const data2{
      0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02,
  };
  Decoder decoder2{data2};
  EXPECT_THAT(decoder2.DecodeRepeatedInt64s(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedUInt32s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  std::vector<uint32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedUInt32s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34));
}

TEST(DecoderTest, DecodeRepeatedUInt32) {
  std::vector<uint8_t> const data{0x83, 0x92, 0x01};
  Decoder decoder{data};
  std::vector<uint32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedUInt32s(WireType::kVarInt, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 18691));
}

TEST(DecoderTest, DecodeOnePackedUInt32) {
  std::vector<uint8_t> const data{0x03, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  std::vector<uint32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedUInt32s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 18691));
}

TEST(DecoderTest, DecodeTwoPackedUInt32s) {
  std::vector<uint8_t> const data{0x05, 0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  std::vector<uint32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedUInt32s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 4610, 18691));
}

TEST(DecoderTest, WrongWireTypeForRepeatedUInt32) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder{data};
  std::vector<uint32_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedUInt32s(WireType::kInt32, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongPackedUInt32Size) {
  std::vector<uint8_t> const data{0x03, 0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  std::vector<uint32_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedUInt32s(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedUInt32) {
  std::vector<uint32_t> values{12, 34};
  std::vector<uint8_t> const data1{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder1{data1};
  ASSERT_OK(decoder1.DecodeRepeatedUInt32s(WireType::kLength, &values));
  ASSERT_THAT(values, ElementsAre(12, 34, 0xFFFFFFFF));
  std::vector<uint8_t> const data2{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x10};
  Decoder decoder2{data2};
  EXPECT_THAT(decoder2.DecodeRepeatedUInt32s(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedUInt64s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  std::vector<uint64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedUInt64s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34));
}

TEST(DecoderTest, DecodeRepeatedUInt64) {
  std::vector<uint8_t> const data{0x83, 0x92, 0x01};
  Decoder decoder{data};
  std::vector<uint64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedUInt64s(WireType::kVarInt, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 18691));
}

TEST(DecoderTest, DecodeOnePackedUInt64) {
  std::vector<uint8_t> const data{0x03, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  std::vector<uint64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedUInt64s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 18691));
}

TEST(DecoderTest, DecodeTwoPackedUInt64s) {
  std::vector<uint8_t> const data{0x05, 0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  std::vector<uint64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedUInt64s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 4610, 18691));
}

TEST(DecoderTest, WrongWireTypeForRepeatedUInt64) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  std::vector<uint64_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedUInt64s(WireType::kInt64, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongPackedUInt64Size) {
  std::vector<uint8_t> const data{0x03, 0x82, 0x24, 0x83, 0x92, 0x01};
  Decoder decoder{data};
  std::vector<uint64_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedUInt64s(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedUInt64) {
  std::vector<uint64_t> values{12, 34};
  std::vector<uint8_t> const data1{
      0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01,
  };
  Decoder decoder1{data1};
  ASSERT_OK(decoder1.DecodeRepeatedUInt64s(WireType::kLength, &values));
  ASSERT_THAT(values, ElementsAre(12, 34, 0xFFFFFFFFFFFFFFFF));
  std::vector<uint8_t> const data2{
      0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02,
  };
  Decoder decoder2{data2};
  EXPECT_THAT(decoder2.DecodeRepeatedUInt64s(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedSInt32s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedSInt32s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34));
}

TEST(DecoderTest, DecodeRepeatedSInt32) {
  std::vector<uint8_t> const data{0x84, 0x48};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedSInt32s(WireType::kVarInt, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 4610));
}

TEST(DecoderTest, DecodeOnePackedSInt32) {
  std::vector<uint8_t> const data{0x01, 0x54};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedSInt32s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 42));
}

TEST(DecoderTest, DecodeTwoPackedSInt32s) {
  std::vector<uint8_t> const data{0x03, 0x84, 0x48, 0x53};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedSInt32s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 4610, -42));
}

TEST(DecoderTest, WrongWireTypeForRepeatedSInt32) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedSInt32s(WireType::kInt32, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongPackedSInt32Size) {
  std::vector<uint8_t> const data{0x01, 0x84, 0x48};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedSInt32s(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedSInt32) {
  std::vector<int32_t> values{12, 34};
  std::vector<uint8_t> const data1{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
  Decoder decoder1{data1};
  ASSERT_OK(decoder1.DecodeRepeatedSInt32s(WireType::kLength, &values));
  ASSERT_THAT(values, ElementsAre(12, 34, -0x80000000));
  std::vector<uint8_t> const data2{0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0x10};
  Decoder decoder2{data2};
  EXPECT_THAT(decoder2.DecodeRepeatedSInt32s(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedSInt64s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedSInt64s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34));
}

TEST(DecoderTest, DecodeRepeatedSInt64) {
  std::vector<uint8_t> const data{0x84, 0x48};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedSInt64s(WireType::kVarInt, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 4610));
}

TEST(DecoderTest, DecodeOnePackedSInt64) {
  std::vector<uint8_t> const data{0x01, 0x54};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedSInt64s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 42));
}

TEST(DecoderTest, DecodeTwoPackedSInt64s) {
  std::vector<uint8_t> const data{0x03, 0x84, 0x48, 0x53};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedSInt64s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 4610, -42));
}

TEST(DecoderTest, WrongWireTypeForRepeatedSInt64) {
  std::vector<uint8_t> const data{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedSInt64s(WireType::kInt64, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongPackedSInt64Size) {
  std::vector<uint8_t> const data{0x01, 0x84, 0x48};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedSInt64s(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, OverflowingPackedSInt64) {
  std::vector<int64_t> values{12, 34};
  std::vector<uint8_t> const data1{
      0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01,
  };
  Decoder decoder1{data1};
  ASSERT_OK(decoder1.DecodeRepeatedSInt64s(WireType::kLength, &values));
  ASSERT_THAT(values, ElementsAre(12, 34, -0x8000000000000000LL));
  std::vector<uint8_t> const data2{
      0x0A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02,
  };
  Decoder decoder2{data2};
  EXPECT_THAT(decoder2.DecodeRepeatedSInt64s(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedFixedInt32s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFixedInt32s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34));
}

TEST(DecoderTest, DecodeRepeatedFixedInt32) {
  std::vector<uint8_t> const data{0x2A, 0x00, 0x00, 0x00};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFixedInt32s(WireType::kInt32, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 42));
}

TEST(DecoderTest, DecodeOnePackedFixedInt32) {
  std::vector<uint8_t> const data{0x04, 0x2A, 0x00, 0x00, 0x00};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFixedInt32s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 42));
}

TEST(DecoderTest, DecodeTwoPackedFixedInt32s) {
  std::vector<uint8_t> const data{0x08, 0x84, 0x48, 0x00, 0x00, 0xD6, 0xFF, 0xFF, 0xFF};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFixedInt32s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 18564, -42));
}

TEST(DecoderTest, WrongWireTypeForRepeatedFixedInt32) {
  std::vector<uint8_t> const data{0x01, 0x23, 0x45, 0x67};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedFixedInt32s(WireType::kVarInt, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongPackedFixedInt32Size) {
  std::vector<uint8_t> const data{0x02, 0x84, 0x48};
  Decoder decoder{data};
  std::vector<int32_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedFixedInt32s(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedFixedInt64s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFixedInt64s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34));
}

TEST(DecoderTest, DecodeRepeatedFixedInt64) {
  std::vector<uint8_t> const data{0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFixedInt64s(WireType::kInt64, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 42));
}

TEST(DecoderTest, DecodeOnePackedFixedInt64) {
  std::vector<uint8_t> const data{0x08, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFixedInt64s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 42));
}

TEST(DecoderTest, DecodeTwoPackedFixedInt64s) {
  std::vector<uint8_t> const data{
      0x10, 0x84, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xD6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  };
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFixedInt64s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 18564, -42));
}

TEST(DecoderTest, WrongWireTypeForRepeatedFixedInt64) {
  std::vector<uint8_t> const data{0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedFixedInt64s(WireType::kVarInt, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongPackedFixedInt64Size) {
  std::vector<uint8_t> const data{0x03, 0x84, 0x48, 0x00};
  Decoder decoder{data};
  std::vector<int64_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedFixedInt64s(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedFixedUInt32s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  std::vector<uint32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFixedUInt32s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34));
}

TEST(DecoderTest, DecodeRepeatedFixedUInt32) {
  std::vector<uint8_t> const data{0x2A, 0x00, 0x00, 0x00};
  Decoder decoder{data};
  std::vector<uint32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFixedUInt32s(WireType::kInt32, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 42));
}

TEST(DecoderTest, DecodeOnePackedFixedUInt32) {
  std::vector<uint8_t> const data{0x04, 0x2A, 0x00, 0x00, 0x00};
  Decoder decoder{data};
  std::vector<uint32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFixedUInt32s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 42));
}

TEST(DecoderTest, DecodeTwoPackedFixedUInt32s) {
  std::vector<uint8_t> const data{0x08, 0x84, 0x48, 0x00, 0x00, 0xD6, 0xFF, 0xFF, 0xFF};
  Decoder decoder{data};
  std::vector<uint32_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFixedUInt32s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 18564, 4294967254));
}

TEST(DecoderTest, WrongWireTypeForRepeatedFixedUInt32) {
  std::vector<uint8_t> const data{0x01, 0x23, 0x45, 0x67};
  Decoder decoder{data};
  std::vector<uint32_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedFixedUInt32s(WireType::kVarInt, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongPackedFixedUInt32Size) {
  std::vector<uint8_t> const data{0x02, 0x84, 0x48};
  Decoder decoder{data};
  std::vector<uint32_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedFixedUInt32s(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedFixedUInt64s) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  std::vector<uint64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFixedUInt64s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34));
}

TEST(DecoderTest, DecodeRepeatedFixedUInt64) {
  std::vector<uint8_t> const data{0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  Decoder decoder{data};
  std::vector<uint64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFixedUInt64s(WireType::kInt64, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 42));
}

TEST(DecoderTest, DecodeOnePackedFixedUInt64) {
  std::vector<uint8_t> const data{0x08, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  Decoder decoder{data};
  std::vector<uint64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFixedUInt64s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 42));
}

TEST(DecoderTest, DecodeTwoPackedFixedUInt64s) {
  std::vector<uint8_t> const data{
      0x10, 0x84, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xD6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  };
  Decoder decoder{data};
  std::vector<uint64_t> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFixedUInt64s(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(12, 34, 18564, 18446744073709551574ull));
}

TEST(DecoderTest, WrongWireTypeForRepeatedFixedUInt64) {
  std::vector<uint8_t> const data{0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45};
  Decoder decoder{data};
  std::vector<uint64_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedFixedUInt64s(WireType::kVarInt, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongPackedFixedUInt64Size) {
  std::vector<uint8_t> const data{0x03, 0x84, 0x48, 0x00};
  Decoder decoder{data};
  std::vector<uint64_t> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedFixedUInt64s(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedEnums) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  std::vector<TestEnum> values{TestEnum::kRed, TestEnum::kGreen};
  EXPECT_OK(decoder.DecodeRepeatedEnums(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(TestEnum::kRed, TestEnum::kGreen));
}

TEST(DecoderTest, DecodeRepeatedEnum) {
  std::vector<uint8_t> const data{0x1E};
  Decoder decoder{data};
  std::vector<TestEnum> values{TestEnum::kRed, TestEnum::kGreen};
  EXPECT_OK(decoder.DecodeRepeatedEnums(WireType::kVarInt, &values));
  EXPECT_THAT(values, ElementsAre(TestEnum::kRed, TestEnum::kGreen, TestEnum::kBlue));
}

TEST(DecoderTest, DecodeNegativeRepeatedEnum) {
  std::vector<uint8_t> const data{0xEC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  std::vector<TestEnum> values{TestEnum::kRed, TestEnum::kGreen};
  EXPECT_OK(decoder.DecodeRepeatedEnums(WireType::kVarInt, &values));
  EXPECT_THAT(values, ElementsAre(TestEnum::kRed, TestEnum::kGreen, TestEnum::kMagenta));
}

TEST(DecoderTest, DecodeOnePackedEnum) {
  std::vector<uint8_t> const data{0x01, 0x1E};
  Decoder decoder{data};
  std::vector<TestEnum> values{TestEnum::kRed, TestEnum::kGreen};
  EXPECT_OK(decoder.DecodeRepeatedEnums(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(TestEnum::kRed, TestEnum::kGreen, TestEnum::kBlue));
}

TEST(DecoderTest, DecodeOneNegativePackedEnum) {
  std::vector<uint8_t> const data{0x0A, 0xEC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  std::vector<TestEnum> values{TestEnum::kRed, TestEnum::kGreen};
  EXPECT_OK(decoder.DecodeRepeatedEnums(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(TestEnum::kRed, TestEnum::kGreen, TestEnum::kMagenta));
}

TEST(DecoderTest, DecodeTwoNegativePackedEnums) {
  std::vector<uint8_t> const data{
      0x0B, 0xEC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x1E,
  };
  Decoder decoder{data};
  std::vector<TestEnum> values{TestEnum::kRed, TestEnum::kGreen};
  EXPECT_OK(decoder.DecodeRepeatedEnums(WireType::kLength, &values));
  EXPECT_THAT(values,
              ElementsAre(TestEnum::kRed, TestEnum::kGreen, TestEnum::kMagenta, TestEnum::kBlue));
}

TEST(DecoderTest, WrongWireTypeForRepeatedEnum) {
  std::vector<uint8_t> const data{0x01, 0x23, 0x45, 0x67};
  Decoder decoder{data};
  std::vector<TestEnum> values{TestEnum::kRed, TestEnum::kGreen};
  EXPECT_THAT(decoder.DecodeRepeatedEnums(WireType::kInt32, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, PackedEnumOverflow) {
  std::vector<uint8_t> const data{0x02, 0x80, 0x01};
  Decoder decoder{data};
  std::vector<TestEnum> values{TestEnum::kRed, TestEnum::kGreen};
  EXPECT_THAT(decoder.DecodeRepeatedEnums(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(DecoderTest, PackedEnumUnderflow) {
  std::vector<uint8_t> const data{0x0A, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
  Decoder decoder{data};
  std::vector<TestEnum> values{TestEnum::kRed, TestEnum::kGreen};
  EXPECT_THAT(decoder.DecodeRepeatedEnums(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(DecoderTest, DecodeEmptyPackedBools) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  std::vector<bool> values{true, false};
  EXPECT_OK(decoder.DecodeRepeatedBools(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(true, false));
}

TEST(DecoderTest, DecodeRepeatedBool) {
  std::vector<uint8_t> const data{0x01};
  Decoder decoder{data};
  std::vector<bool> values{true, false};
  EXPECT_OK(decoder.DecodeRepeatedBools(WireType::kVarInt, &values));
  EXPECT_THAT(values, ElementsAre(true, false, true));
}

TEST(DecoderTest, DecodeOnePackedBool) {
  std::vector<uint8_t> const data{0x01, 0x00};
  Decoder decoder{data};
  std::vector<bool> values{true, false};
  EXPECT_OK(decoder.DecodeRepeatedBools(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(true, false, false));
}

TEST(DecoderTest, DecodeTwoPackedBools) {
  std::vector<uint8_t> const data{0x02, 0x01, 0x00};
  Decoder decoder{data};
  std::vector<bool> values{true, false};
  EXPECT_OK(decoder.DecodeRepeatedBools(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(true, false, true, false));
}

TEST(DecoderTest, WrongWireTypeForRepeatedBool) {
  std::vector<uint8_t> const data{0x01, 0x23, 0x45, 0x67};
  Decoder decoder{data};
  std::vector<bool> values{true, false};
  EXPECT_THAT(decoder.DecodeRepeatedBools(WireType::kInt32, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedFloats) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  std::vector<float> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFloats(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(Near(12.0f), Near(34.0f)));
}

TEST(DecoderTest, DecodeRepeatedFloat) {
  std::vector<uint8_t> const data{0xD0, 0x0F, 0x49, 0x40};
  Decoder decoder{data};
  std::vector<float> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFloats(WireType::kInt32, &values));
  EXPECT_THAT(values, ElementsAre(Near(12.0f), Near(34.0f), Near(3.14159f)));
}

TEST(DecoderTest, DecodeOnePackedFloat) {
  std::vector<uint8_t> const data{0x04, 0xD0, 0x0F, 0x49, 0x40};
  Decoder decoder{data};
  std::vector<float> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFloats(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(Near(12.0f), Near(34.0f), Near(3.14159f)));
}

TEST(DecoderTest, DecodeTwoPackedFloats) {
  std::vector<uint8_t> const data{0x08, 0x4D, 0xF8, 0x2D, 0x40, 0xD0, 0x0F, 0x49, 0x40};
  Decoder decoder{data};
  std::vector<float> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedFloats(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(Near(12.0f), Near(34.0f), Near(2.71828f), Near(3.14159f)));
}

TEST(DecoderTest, WrongWireTypeForRepeatedFloat) {
  std::vector<uint8_t> const data{0x01, 0x23, 0x45, 0x67};
  Decoder decoder{data};
  std::vector<float> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedFloats(WireType::kVarInt, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongPackedFloatSize) {
  std::vector<uint8_t> const data{0x06, 0xD0, 0x0F, 0x49, 0x40, 0x00, 0x00};
  Decoder decoder{data};
  std::vector<float> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedFloats(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, DecodeEmptyPackedDouble) {
  std::vector<uint8_t> const data{0x00};
  Decoder decoder{data};
  std::vector<double> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedDoubles(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(Near(12.0), Near(34.0)));
}

TEST(DecoderTest, DecodeRepeatedDouble) {
  std::vector<uint8_t> const data{0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40};
  Decoder decoder{data};
  std::vector<double> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedDoubles(WireType::kInt64, &values));
  EXPECT_THAT(values, ElementsAre(Near(12.0), Near(34.0), Near(3.14159)));
}

TEST(DecoderTest, DecodeOnePackedDouble) {
  std::vector<uint8_t> const data{0x08, 0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40};
  Decoder decoder{data};
  std::vector<double> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedDoubles(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(Near(12.0), Near(34.0), Near(3.14159)));
}

TEST(DecoderTest, DecodeTwoPackedDoubles) {
  std::vector<uint8_t> const data{
      0x10, 0x90, 0xF7, 0xAA, 0x95, 0x09, 0xBF, 0x05, 0x40,
      0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40,
  };
  Decoder decoder{data};
  std::vector<double> values{12, 34};
  EXPECT_OK(decoder.DecodeRepeatedDoubles(WireType::kLength, &values));
  EXPECT_THAT(values, ElementsAre(Near(12.0), Near(34.0), Near(2.71828), Near(3.14159)));
}

TEST(DecoderTest, WrongWireTypeForRepeatedDouble) {
  std::vector<uint8_t> const data{0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45};
  Decoder decoder{data};
  std::vector<double> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedDoubles(WireType::kVarInt, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(DecoderTest, WrongPackedDoubleSize) {
  std::vector<uint8_t> const data{
      0x0C, 0x90, 0xF7, 0xAA, 0x95, 0x09, 0xBF, 0x05, 0x40,
      0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40,
  };
  Decoder decoder{data};
  std::vector<double> values{12, 34};
  EXPECT_THAT(decoder.DecodeRepeatedDoubles(WireType::kLength, &values),
              StatusIs(absl::StatusCode::kInvalidArgument));
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
  EXPECT_THAT(decoder.DecodeUInt64Field(WireType::kVarInt), IsOkAndHolds(123456));
  EXPECT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 2, .wire_type = WireType::kLength}));
  EXPECT_THAT(decoder.DecodeStringField(WireType::kLength), IsOkAndHolds("sator arepo"));
  EXPECT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 3, .wire_type = WireType::kInt64}));
  EXPECT_THAT(decoder.DecodeDoubleField(WireType::kInt64), IsOkAndHolds(Near(2.71828)));
  EXPECT_TRUE(decoder.at_end());
}

TEST(DecoderTest, AddRecordToExtensionData) {
  std::vector<uint8_t> const data{
      0x08, 0xC0, 0xC4, 0x07, 0x12, 0x0B, 's',  'a',  't',  'o',  'r',  ' ',  'a',
      'r',  'e',  'p',  'o',  0x19, 0x90, 0xF7, 0xAA, 0x95, 0x09, 0xBF, 0x05, 0x40,
  };
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 1, .wire_type = WireType::kVarInt}));
  EXPECT_THAT(decoder.DecodeUInt64Field(WireType::kVarInt), IsOkAndHolds(123456));
  auto const status_or_maybe_tag = decoder.DecodeTag();
  EXPECT_THAT(status_or_maybe_tag, IsOkAndHolds(Optional<FieldTag>(FieldTag{
                                       .field_number = 2, .wire_type = WireType::kLength})));
  EXPECT_OK(decoder.AddRecordToExtensionData(status_or_maybe_tag.value().value()));
  EXPECT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 3, .wire_type = WireType::kInt64}));
  EXPECT_THAT(decoder.DecodeDoubleField(WireType::kInt64), IsOkAndHolds(Near(2.71828)));
  EXPECT_TRUE(decoder.at_end());
  EXPECT_THAT(std::move(decoder).GetExtensionData(),
              BufferAsBytes(
                  ElementsAre(0x12, 0x0B, 's', 'a', 't', 'o', 'r', ' ', 'a', 'r', 'e', 'p', 'o')));
}

TEST(DecoderTest, AddRecordsToExtensionData) {
  std::vector<uint8_t> const data{
      0x08, 0xC0, 0xC4, 0x07, 0x20, 0x2A, 0x19, 0x90, 0xF7, 0xAA, 0x95, 0x09, 0xBF, 0x05, 0x40,
      0x12, 0x0B, 't',  'e',  'n',  'e',  't',  ' ',  'o',  'p',  'e',  'r',  'a',  0x28, 0x01,
  };
  Decoder decoder{data};
  EXPECT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 1, .wire_type = WireType::kVarInt}));
  EXPECT_THAT(decoder.DecodeUInt64Field(WireType::kVarInt), IsOkAndHolds(123456));
  auto status_or_maybe_tag = decoder.DecodeTag();
  EXPECT_THAT(status_or_maybe_tag, IsOkAndHolds(Optional<FieldTag>(FieldTag{
                                       .field_number = 4, .wire_type = WireType::kVarInt})));
  EXPECT_OK(decoder.AddRecordToExtensionData(status_or_maybe_tag.value().value()));
  EXPECT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 3, .wire_type = WireType::kInt64}));
  EXPECT_THAT(decoder.DecodeDoubleField(WireType::kInt64), IsOkAndHolds(Near(2.71828)));
  status_or_maybe_tag = decoder.DecodeTag();
  EXPECT_THAT(status_or_maybe_tag, IsOkAndHolds(Optional<FieldTag>(FieldTag{
                                       .field_number = 2, .wire_type = WireType::kLength})));
  EXPECT_OK(decoder.AddRecordToExtensionData(status_or_maybe_tag.value().value()));
  EXPECT_THAT(decoder.DecodeTag(),
              IsOkAndHolds(FieldTag{.field_number = 5, .wire_type = WireType::kVarInt}));
  EXPECT_THAT(decoder.DecodeBoolField(WireType::kVarInt), IsOkAndHolds(true));
  EXPECT_TRUE(decoder.at_end());
  EXPECT_THAT(std::move(decoder).GetExtensionData(),
              BufferAsBytes(ElementsAre(0x20, 0x2A, 0x12, 0x0B, 't', 'e', 'n', 'e', 't', ' ', 'o',
                                        'p', 'e', 'r', 'a')));
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

TEST_F(EncoderTest, EncodeInt32Field) {
  encoder_.EncodeInt32Field(42, 123);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x7B)));
}

TEST_F(EncoderTest, EncodeNegativeInt32Field) {
  encoder_.EncodeInt32Field(42, -123);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD0, 0x02, 0x85, 0xFF, 0xFF, 0xFF, 0x0F)));
}

TEST_F(EncoderTest, EncodeUInt32Field) {
  encoder_.EncodeUInt32Field(42, 123);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x7B)));
}

TEST_F(EncoderTest, EncodeBigUInt32Field) {
  encoder_.EncodeUInt32Field(42, 0xFFFFFFFF);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD0, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F)));
}

TEST_F(EncoderTest, EncodeInt64Field) {
  encoder_.EncodeInt64Field(42, 123);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x7B)));
}

TEST_F(EncoderTest, EncodeNegativeInt64Field) {
  encoder_.EncodeInt64Field(42, -123);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD0, 0x02, 0x85, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                        0xFF, 0x01)));
}

TEST_F(EncoderTest, EncodeUInt64Field) {
  encoder_.EncodeUInt64Field(42, 123);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x7B)));
}

TEST_F(EncoderTest, EncodeBigUInt64Field) {
  encoder_.EncodeUInt64Field(42, 0xFFFFFFFFFFFFFFFFULL);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD0, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                        0xFF, 0x01)));
}

TEST_F(EncoderTest, EncodeSingleBytePositiveEvenSInt32) {
  encoder_.EncodeSInt32Field(42, 42);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x54)));
}

TEST_F(EncoderTest, EncodeSingleBytePositiveOddSInt32) {
  encoder_.EncodeSInt32Field(42, 43);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x56)));
}

TEST_F(EncoderTest, EncodeSingleByteNegativeEvenSInt32) {
  encoder_.EncodeSInt32Field(42, -42);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x53)));
}

TEST_F(EncoderTest, EncodeSingleByteNegativeOddSInt32) {
  encoder_.EncodeSInt32Field(42, -43);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x55)));
}

TEST_F(EncoderTest, EncodeTwoBytePositiveEvenSInt32) {
  encoder_.EncodeSInt32Field(42, 4610);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x84, 0x48)));
}

TEST_F(EncoderTest, EncodeTwoBytePositiveOddSInt32) {
  encoder_.EncodeSInt32Field(42, 4611);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x86, 0x48)));
}

TEST_F(EncoderTest, EncodeTwoByteNegativeEvenSInt32) {
  encoder_.EncodeSInt32Field(42, -4610);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x83, 0x48)));
}

TEST_F(EncoderTest, EncodeTwoByteNegativeOddSInt32) {
  encoder_.EncodeSInt32Field(42, -4611);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x85, 0x48)));
}

TEST_F(EncoderTest, EncodeMaxPositiveEvenSInt32) {
  encoder_.EncodeSInt32Field(42, 0x7FFFFFFE);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD0, 0x02, 0xFC, 0xFF, 0xFF, 0xFF, 0x0F)));
}

TEST_F(EncoderTest, EncodeMaxNegativeEvenSInt32) {
  encoder_.EncodeSInt32Field(42, -0x7FFFFFFF);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD0, 0x02, 0xFD, 0xFF, 0xFF, 0xFF, 0x0F)));
}

TEST_F(EncoderTest, EncodeMaxPositiveOddSInt32) {
  encoder_.EncodeSInt32Field(42, 0x7FFFFFFF);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD0, 0x02, 0xFE, 0xFF, 0xFF, 0xFF, 0x0F)));
}

TEST_F(EncoderTest, EncodeMaxNegativeOddSInt32) {
  encoder_.EncodeSInt32Field(42, -0x80000000);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD0, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F)));
}

TEST_F(EncoderTest, EncodeSingleBytePositiveEvenSInt64) {
  encoder_.EncodeSInt64Field(42, 42);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x54)));
}

TEST_F(EncoderTest, EncodeSingleBytePositiveOddSInt64) {
  encoder_.EncodeSInt64Field(42, 43);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x56)));
}

TEST_F(EncoderTest, EncodeSingleByteNegativeEvenSInt64) {
  encoder_.EncodeSInt64Field(42, -42);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x53)));
}

TEST_F(EncoderTest, EncodeSingleByteNegativeOddSInt64) {
  encoder_.EncodeSInt64Field(42, -43);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x55)));
}

TEST_F(EncoderTest, EncodeTwoBytePositiveEvenSInt64) {
  encoder_.EncodeSInt64Field(42, 4610);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x84, 0x48)));
}

TEST_F(EncoderTest, EncodeTwoBytePositiveOddSInt64) {
  encoder_.EncodeSInt64Field(42, 4611);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x86, 0x48)));
}

TEST_F(EncoderTest, EncodeTwoByteNegativeEvenSInt64) {
  encoder_.EncodeSInt64Field(42, -4610);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x83, 0x48)));
}

TEST_F(EncoderTest, EncodeTwoByteNegativeOddSInt64) {
  encoder_.EncodeSInt64Field(42, -4611);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x85, 0x48)));
}

TEST_F(EncoderTest, EncodeMaxPositiveEvenSInt64) {
  encoder_.EncodeSInt64Field(42, 0x7FFFFFFFFFFFFFFELL);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD0, 0x02, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                        0xFF, 0x01)));
}

TEST_F(EncoderTest, EncodeMaxNegativeOddSInt64) {
  encoder_.EncodeSInt64Field(42, -0x7FFFFFFFFFFFFFFFLL);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD0, 0x02, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                        0xFF, 0x01)));
}

TEST_F(EncoderTest, EncodeMaxPositiveOddSInt64) {
  encoder_.EncodeSInt64Field(42, 0x7FFFFFFFFFFFFFFFLL);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD0, 0x02, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                        0xFF, 0x01)));
}

TEST_F(EncoderTest, EncodeMaxNegativeEvenSInt64) {
  encoder_.EncodeSInt64Field(42, -0x8000000000000000LL);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD0, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                        0xFF, 0x01)));
}

TEST_F(EncoderTest, EncodeFixedInt32) {
  encoder_.EncodeFixedInt32Field(42, 4610);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD5, 0x02, 0x02, 0x12, 0x00, 0x00)));
}

TEST_F(EncoderTest, EncodeNegativeFixedInt32) {
  encoder_.EncodeFixedInt32Field(42, -4610);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD5, 0x02, 0xFE, 0xED, 0xFF, 0xFF)));
}

TEST_F(EncoderTest, EncodeFixedInt64) {
  encoder_.EncodeFixedInt64Field(42, 4610);
  EXPECT_THAT(
      std::move(encoder_).Flatten(),
      BufferAsBytes(ElementsAre(0xD1, 0x02, 0x02, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)));
}

TEST_F(EncoderTest, EncodeNegativeFixedInt64) {
  encoder_.EncodeFixedInt64Field(42, -4610);
  EXPECT_THAT(
      std::move(encoder_).Flatten(),
      BufferAsBytes(ElementsAre(0xD1, 0x02, 0xFE, 0xED, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF)));
}

TEST_F(EncoderTest, EncodeTrue) {
  encoder_.EncodeBoolField(42, true);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x01)));
}

TEST_F(EncoderTest, EncodeFalse) {
  encoder_.EncodeBoolField(42, false);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x00)));
}

TEST_F(EncoderTest, EncodeFloat) {
  encoder_.EncodeFloatField(42, 3.14159f);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD5, 0x02, 0xD0, 0x0F, 0x49, 0x40)));
}

TEST_F(EncoderTest, EncodeDouble) {
  encoder_.EncodeDoubleField(42, 3.14159);
  EXPECT_THAT(
      std::move(encoder_).Flatten(),
      BufferAsBytes(ElementsAre(0xD1, 0x02, 0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40)));
}

TEST_F(EncoderTest, EncodeEmptyString) {
  encoder_.EncodeStringField(42, "");
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x00)));
}

TEST_F(EncoderTest, EncodeString) {
  encoder_.EncodeStringField(42, "lorem");
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x05, 'l', 'o', 'r', 'e', 'm')));
}

TEST_F(EncoderTest, EncodeBytes) {
  encoder_.EncodeBytesField(42, {0x12, 0x34, 0x56, 0x78});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x04, 0x12, 0x34, 0x56, 0x78)));
}

TEST_F(EncoderTest, EncodeTime) {
  encoder_.EncodeTimeField(42, absl::UnixEpoch() + absl::Seconds(12) + absl::Nanoseconds(34));
  auto const buffer = std::move(encoder_).Flatten();
  EXPECT_THAT(DecodeGoogleProtobufTimestamp(42, buffer.span()),
              IsOkAndHolds(::google::protobuf::Timestamp{.seconds = 12, .nanos = 34}));
}

TEST_F(EncoderTest, EncodeZeroTime) {
  encoder_.EncodeTimeField(42, absl::UnixEpoch());
  auto const buffer = std::move(encoder_).Flatten();
  EXPECT_THAT(DecodeGoogleProtobufTimestamp(42, buffer.span()),
              IsOkAndHolds(::google::protobuf::Timestamp{.seconds = 0, .nanos = 0}));
}

TEST_F(EncoderTest, EncodeNegativeTimeWithoutFraction) {
  encoder_.EncodeTimeField(42, absl::UnixEpoch() - absl::Seconds(10));
  auto const buffer = std::move(encoder_).Flatten();
  EXPECT_THAT(DecodeGoogleProtobufTimestamp(42, buffer.span()),
              IsOkAndHolds(::google::protobuf::Timestamp{.seconds = -10, .nanos = 0}));
}

TEST_F(EncoderTest, EncodeNegativeTimeWithFraction1) {
  encoder_.EncodeTimeField(42,
                           absl::UnixEpoch() - absl::Seconds(10) + absl::Nanoseconds(200000000));
  auto const buffer = std::move(encoder_).Flatten();
  EXPECT_THAT(DecodeGoogleProtobufTimestamp(42, buffer.span()),
              IsOkAndHolds(::google::protobuf::Timestamp{.seconds = -10, .nanos = 200000000}));
}

TEST_F(EncoderTest, EncodeNegativeTimeWithFraction2) {
  encoder_.EncodeTimeField(42,
                           absl::UnixEpoch() - absl::Seconds(10) - absl::Nanoseconds(200000000));
  auto const buffer = std::move(encoder_).Flatten();
  EXPECT_THAT(DecodeGoogleProtobufTimestamp(42, buffer.span()),
              IsOkAndHolds(::google::protobuf::Timestamp{.seconds = -11, .nanos = 800000000}));
}

TEST_F(EncoderTest, EncodeDuration) {
  encoder_.EncodeDurationField(42, absl::Seconds(12) + absl::Nanoseconds(34));
  auto const buffer = std::move(encoder_).Flatten();
  EXPECT_THAT(DecodeGoogleProtobufDuration(42, buffer.span()),
              IsOkAndHolds(::google::protobuf::Duration{.seconds = 12, .nanos = 34}));
}

TEST_F(EncoderTest, EncodeZeroDuration) {
  encoder_.EncodeDurationField(42, absl::ZeroDuration());
  auto const buffer = std::move(encoder_).Flatten();
  EXPECT_THAT(DecodeGoogleProtobufDuration(42, buffer.span()),
              IsOkAndHolds(::google::protobuf::Duration{.seconds = 0, .nanos = 0}));
}

TEST_F(EncoderTest, EncodeNegativeDuration) {
  encoder_.EncodeDurationField(42, -(absl::Seconds(12) + absl::Nanoseconds(34)));
  auto const buffer = std::move(encoder_).Flatten();
  EXPECT_THAT(DecodeGoogleProtobufDuration(42, buffer.span()),
              IsOkAndHolds(::google::protobuf::Duration{.seconds = -12, .nanos = -34}));
}

TEST_F(EncoderTest, EncodeEnum) {
  encoder_.EncodeEnumField(42, TestEnum::kGreen);
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD0, 0x02, 0x14)));
}

TEST_F(EncoderTest, EncodeEmptyPackedInt32s) {
  encoder_.EncodePackedInt32s(42, {});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedInt32) {
  encoder_.EncodePackedInt32s(42, {18691});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x03, 0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeTwoPackedInt32s) {
  encoder_.EncodePackedInt32s(42, {4610, 18691});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x05, 0x82, 0x24, 0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeEmptyPackedInt64s) {
  encoder_.EncodePackedInt64s(42, {});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedInt64) {
  encoder_.EncodePackedInt64s(42, {18691});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x03, 0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeTwoPackedInt64s) {
  encoder_.EncodePackedInt64s(42, {4610, 18691});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x05, 0x82, 0x24, 0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeEmptyPackedUInt32s) {
  encoder_.EncodePackedUInt32s(42, {});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedUInt32) {
  encoder_.EncodePackedUInt32s(42, {18691});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x03, 0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeTwoPackedUInt32s) {
  encoder_.EncodePackedUInt32s(42, {4610, 18691});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x05, 0x82, 0x24, 0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeEmptyPackedUInt64s) {
  encoder_.EncodePackedUInt64s(42, {});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedUInt64) {
  encoder_.EncodePackedUInt64s(42, {18691});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x03, 0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeTwoPackedUInt64s) {
  encoder_.EncodePackedUInt64s(42, {4610, 18691});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x05, 0x82, 0x24, 0x83, 0x92, 0x01)));
}

TEST_F(EncoderTest, EncodeEmptyPackedSInt32s) {
  encoder_.EncodePackedSInt32s(42, {});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedSInt32) {
  encoder_.EncodePackedSInt32s(42, {42});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x01, 0x54)));
}

TEST_F(EncoderTest, EncodeTwoPackedSInt32s) {
  encoder_.EncodePackedSInt32s(42, {4610, -42});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x03, 0x84, 0x48, 0x53)));
}

TEST_F(EncoderTest, EncodeEmptyPackedSInt64s) {
  encoder_.EncodePackedSInt64s(42, {});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedSInt64) {
  encoder_.EncodePackedSInt64s(42, {42});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x01, 0x54)));
}

TEST_F(EncoderTest, EncodeTwoPackedSInt64s) {
  encoder_.EncodePackedSInt64s(42, {4610, -42});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x03, 0x84, 0x48, 0x53)));
}

TEST_F(EncoderTest, EncodeEmptyPackedFixedInt32s) {
  encoder_.EncodePackedFixedInt32s(42, {});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedFixedInt32) {
  encoder_.EncodePackedFixedInt32s(42, {42});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x04, 0x2A, 0x00, 0x00, 0x00)));
}

TEST_F(EncoderTest, EncodeTwoPackedFixedInt32s) {
  encoder_.EncodePackedFixedInt32s(42, {18564, -42});
  EXPECT_THAT(
      std::move(encoder_).Flatten(),
      BufferAsBytes(ElementsAre(0xD2, 0x02, 0x08, 0x84, 0x48, 0x00, 0x00, 0xD6, 0xFF, 0xFF, 0xFF)));
}

TEST_F(EncoderTest, EncodeEmptyPackedFixedInt64s) {
  encoder_.EncodePackedFixedInt64s(42, {});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedFixedInt64) {
  encoder_.EncodePackedFixedInt64s(42, {42});
  EXPECT_THAT(
      std::move(encoder_).Flatten(),
      BufferAsBytes(ElementsAre(0xD2, 0x02, 0x08, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)));
}

TEST_F(EncoderTest, EncodeTwoPackedFixedInt64s) {
  encoder_.EncodePackedFixedInt64s(42, {18564, -42});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x10, 0x84, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0xD6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF)));
}

TEST_F(EncoderTest, EncodeEmptyPackedFixedUInt32s) {
  encoder_.EncodePackedFixedUInt32s(42, {});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedFixedUInt32) {
  encoder_.EncodePackedFixedUInt32s(42, {42});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x04, 0x2A, 0x00, 0x00, 0x00)));
}

TEST_F(EncoderTest, EncodeTwoPackedFixedUInt32s) {
  encoder_.EncodePackedFixedUInt32s(42, {18564, 42});
  EXPECT_THAT(
      std::move(encoder_).Flatten(),
      BufferAsBytes(ElementsAre(0xD2, 0x02, 0x08, 0x84, 0x48, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00)));
}

TEST_F(EncoderTest, EncodeEmptyPackedFixedUInt64s) {
  encoder_.EncodePackedFixedUInt64s(42, {});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedFixedUInt64) {
  encoder_.EncodePackedFixedUInt64s(42, {42});
  EXPECT_THAT(
      std::move(encoder_).Flatten(),
      BufferAsBytes(ElementsAre(0xD2, 0x02, 0x08, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)));
}

TEST_F(EncoderTest, EncodeTwoPackedFixedUInt64s) {
  encoder_.EncodePackedFixedUInt64s(42, {18564, 42});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x10, 0x84, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)));
}

TEST_F(EncoderTest, EncodeEmptyPackedBools) {
  encoder_.EncodePackedBools(42, {});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedBool) {
  encoder_.EncodePackedBools(42, {false});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x01, 0x00)));
}

TEST_F(EncoderTest, EncodeTwoPackedBools) {
  encoder_.EncodePackedBools(42, {true, false});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x02, 0x01, 0x00)));
}

TEST_F(EncoderTest, EncodeEmptyPackedFloats) {
  encoder_.EncodePackedFloats(42, {});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedFloat) {
  encoder_.EncodePackedFloats(42, {3.14159f});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x04, 0xD0, 0x0F, 0x49, 0x40)));
}

TEST_F(EncoderTest, EncodeTwoPackedFloats) {
  encoder_.EncodePackedFloats(42, {2.71828f, 3.14159f});
  EXPECT_THAT(
      std::move(encoder_).Flatten(),
      BufferAsBytes(ElementsAre(0xD2, 0x02, 0x08, 0x4D, 0xF8, 0x2D, 0x40, 0xD0, 0x0F, 0x49, 0x40)));
}

TEST_F(EncoderTest, EncodeEmptyPackedDouble) {
  encoder_.EncodePackedDoubles(42, {});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedDouble) {
  encoder_.EncodePackedDoubles(42, {3.14159});
  EXPECT_THAT(
      std::move(encoder_).Flatten(),
      BufferAsBytes(ElementsAre(0xD2, 0x02, 0x08, 0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40)));
}

TEST_F(EncoderTest, EncodeTwoPackedDoubles) {
  encoder_.EncodePackedDoubles(42, {2.71828, 3.14159});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x10, 0x90, 0xF7, 0xAA, 0x95, 0x09, 0xBF, 0x05,
                                        0x40, 0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40)));
}

TEST_F(EncoderTest, EncodeEmptyPackedEnums) {
  encoder_.EncodePackedEnums<TestEnum>(42, {});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x00)));
}

TEST_F(EncoderTest, EncodeOnePackedEnums) {
  encoder_.EncodePackedEnums<TestEnum>(42, {TestEnum::kBlue});
  EXPECT_THAT(std::move(encoder_).Flatten(), BufferAsBytes(ElementsAre(0xD2, 0x02, 0x01, 0x1E)));
}

TEST_F(EncoderTest, EncodeTwoPackedEnums) {
  encoder_.EncodePackedEnums<TestEnum>(42, {TestEnum::kRed, TestEnum::kBlue});
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0xD2, 0x02, 0x02, 0x0A, 0x1E)));
}

TEST_F(EncoderTest, EncodeFields) {
  encoder_.EncodeUInt64Field(1, 123456);
  encoder_.EncodeStringField(2, "sator arepo");
  encoder_.EncodeDoubleField(3, 2.71828);
  EXPECT_THAT(std::move(encoder_).Flatten(),
              BufferAsBytes(ElementsAre(0x08, 0xC0, 0xC4, 0x07, 0x12, 0x0B, 's', 'a', 't', 'o', 'r',
                                        ' ', 'a', 'r', 'e', 'p', 'o', 0x19, 0x90, 0xF7, 0xAA, 0x95,
                                        0x09, 0xBF, 0x05, 0x40)));
}

}  // namespace
