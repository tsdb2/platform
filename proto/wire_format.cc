#include "proto/wire_format.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/config.h"  // IWYU pragma: keep
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "common/utilities.h"
#include "io/buffer.h"
#include "io/cord.h"
#include "proto/duration.pb.sync.h"
#include "proto/time_util.h"
#include "proto/timestamp.pb.sync.h"

namespace tsdb2 {
namespace proto {

namespace {

#ifdef ABSL_IS_BIG_ENDIAN
constexpr uint32_t ByteSwap32(uint32_t const value) {
  return ((value >> 24) & 0x000000FF) | ((value >> 8) & 0x0000FF00) | ((value << 8) & 0x00FF0000) |
         ((value << 24) & 0xFF000000);
}

constexpr uint64_t ByteSwap64(uint64_t const value) {
  return ((value >> 56) & 0x00000000000000FF) | ((value >> 40) & 0x000000000000FF00) |
         ((value >> 24) & 0x0000000000FF0000) | ((value >> 8) & 0x00000000FF000000) |
         ((value << 8) & 0x000000FF00000000) | ((value << 24) & 0x0000FF0000000000) |
         ((value << 40) & 0x00FF000000000000) | ((value << 56) & 0xFF00000000000000);
}
#endif  // ABSL_IS_BIG_ENDIAN

}  // namespace

absl::StatusOr<uint64_t> Decoder::DecodeVarInt() {
  uint64_t value = 0;
  uint8_t byte;
  size_t m = 0;
  do {
    if (data_.empty()) {
      return EndOfInputError();
    }
    byte = data_.front();
    if (m + 7 > kMaxVarIntBits) {
      uint8_t const mask = (1 << (kMaxVarIntBits - m)) - 1;
      if ((byte & 0x7F) > mask) {
        return absl::InvalidArgumentError("decoding error: integer value exceeds 64 bits");
      }
    }
    data_.remove_prefix(1);
    value += (uint64_t{byte} & 0x7FULL) << m;
    m += 7;
  } while ((byte & 0x80) != 0);
  return value;
}

absl::StatusOr<FieldTag> Decoder::DecodeTag() {
  DEFINE_CONST_OR_RETURN(tag, DecodeUInt64());
  return FieldTag{
      .field_number = tag >> 3,
      .wire_type = static_cast<WireType>(tag & 7),
  };
}

absl::StatusOr<int32_t> Decoder::DecodeInt32Field(WireType const wire_type) {
  if (wire_type != WireType::kVarInt) {
    return absl::InvalidArgumentError("invalid wire type for int32");
  }
  return DecodeInt32();
}

absl::StatusOr<int64_t> Decoder::DecodeInt64Field(WireType const wire_type) {
  if (wire_type != WireType::kVarInt) {
    return absl::InvalidArgumentError("invalid wire type for int64");
  }
  return DecodeInt64();
}

absl::StatusOr<uint32_t> Decoder::DecodeUInt32Field(WireType const wire_type) {
  if (wire_type != WireType::kVarInt) {
    return absl::InvalidArgumentError("invalid wire type for uint32");
  }
  return DecodeUInt32();
}

absl::StatusOr<uint64_t> Decoder::DecodeUInt64Field(WireType const wire_type) {
  if (wire_type != WireType::kVarInt) {
    return absl::InvalidArgumentError("invalid wire type for uint64");
  }
  return DecodeUInt64();
}

absl::StatusOr<int32_t> Decoder::DecodeSInt32Field(WireType const wire_type) {
  if (wire_type != WireType::kVarInt) {
    return absl::InvalidArgumentError("invalid wire type for sint32");
  }
  return DecodeSInt32();
}

absl::StatusOr<int64_t> Decoder::DecodeSInt64Field(WireType const wire_type) {
  if (wire_type != WireType::kVarInt) {
    return absl::InvalidArgumentError("invalid wire type for sint64");
  }
  return DecodeSInt64();
}

absl::StatusOr<int32_t> Decoder::DecodeFixedInt32Field(WireType const wire_type) {
  if (wire_type != WireType::kInt32) {
    return absl::InvalidArgumentError("invalid wire type for sfixed32");
  }
  return DecodeFixedInt32();
}

absl::StatusOr<uint32_t> Decoder::DecodeFixedUInt32Field(WireType const wire_type) {
  if (wire_type != WireType::kInt32) {
    return absl::InvalidArgumentError("invalid wire type for fixed32");
  }
  return DecodeFixedUInt32();
}

absl::StatusOr<int64_t> Decoder::DecodeFixedInt64Field(WireType const wire_type) {
  if (wire_type != WireType::kInt64) {
    return absl::InvalidArgumentError("invalid wire type for sfixed64");
  }
  return DecodeFixedInt64();
}

absl::StatusOr<uint64_t> Decoder::DecodeFixedUInt64Field(WireType const wire_type) {
  if (wire_type != WireType::kInt64) {
    return absl::InvalidArgumentError("invalid wire type for fixed64");
  }
  return DecodeFixedUInt64();
}

absl::StatusOr<bool> Decoder::DecodeBoolField(WireType const wire_type) {
  if (wire_type != WireType::kVarInt) {
    return absl::InvalidArgumentError("invalid wire type for bool");
  }
  DEFINE_CONST_OR_RETURN(value, DecodeVarInt());
  return value != 0;
}

absl::StatusOr<float> Decoder::DecodeFloatField(WireType const wire_type) {
  DEFINE_VAR_OR_RETURN(value, DecodeFixedUInt32Field(wire_type));
  return *reinterpret_cast<float const *>(&value);
}

absl::StatusOr<double> Decoder::DecodeDoubleField(WireType const wire_type) {
  DEFINE_VAR_OR_RETURN(value, DecodeFixedUInt64Field(wire_type));
  return *reinterpret_cast<double const *>(&value);
}

absl::StatusOr<std::string> Decoder::DecodeStringField(WireType const wire_type) {
  if (wire_type != WireType::kLength) {
    return absl::InvalidArgumentError("invalid wire type for string");
  }
  DEFINE_CONST_OR_RETURN(length, DecodeUInt64());
  if (data_.size() < length) {
    return EndOfInputError();
  }
  std::string value{reinterpret_cast<char const *>(data_.data()), length};
  data_.remove_prefix(length);
  return std::move(value);
}

absl::StatusOr<std::vector<uint8_t>> Decoder::DecodeBytesField(WireType const wire_type) {
  if (wire_type != WireType::kLength) {
    return absl::InvalidArgumentError("invalid wire type for bytes");
  }
  DEFINE_CONST_OR_RETURN(length, DecodeUInt64());
  if (data_.size() < length) {
    return EndOfInputError();
  }
  std::vector<uint8_t> value{data_.begin(), data_.begin() + length};
  data_.remove_prefix(length);
  return std::move(value);
}

absl::StatusOr<absl::Time> Decoder::DecodeTimeField(WireType const wire_type) {
  DEFINE_CONST_OR_RETURN(child_span, GetChildSpan(wire_type));
  DEFINE_CONST_OR_RETURN(proto, ::google::protobuf::Timestamp::Decode(child_span));
  return DecodeGoogleApiProto(proto);
}

absl::StatusOr<absl::Duration> Decoder::DecodeDurationField(WireType const wire_type) {
  DEFINE_CONST_OR_RETURN(child_span, GetChildSpan(wire_type));
  DEFINE_CONST_OR_RETURN(proto, ::google::protobuf::Duration::Decode(child_span));
  return DecodeGoogleApiProto(proto);
}

absl::StatusOr<absl::Span<uint8_t const>> Decoder::GetChildSpan(WireType const wire_type) {
  if (wire_type != WireType::kLength) {
    return absl::InvalidArgumentError("invalid wire type for submessage");
  }
  DEFINE_CONST_OR_RETURN(length, DecodeUInt64());
  if (data_.size() < length) {
    return EndOfInputError();
  }
  auto const subspan = data_.subspan(0, length);
  data_.remove_prefix(length);
  return subspan;
}

absl::StatusOr<std::vector<int32_t>> Decoder::DecodePackedSInt32s() {
  DEFINE_VAR_OR_RETURN(child, DecodeChildSpan());
  std::vector<int32_t> values;
  while (!child.at_end()) {
    DEFINE_CONST_OR_RETURN(value, child.DecodeSInt32());
    values.push_back(value);
  }
  return std::move(values);
}

absl::StatusOr<std::vector<int64_t>> Decoder::DecodePackedSInt64s() {
  DEFINE_VAR_OR_RETURN(child, DecodeChildSpan());
  std::vector<int64_t> values;
  while (!child.at_end()) {
    DEFINE_CONST_OR_RETURN(value, child.DecodeSInt64());
    values.push_back(value);
  }
  return std::move(values);
}

absl::StatusOr<std::vector<int32_t>> Decoder::DecodePackedFixedInt32s() {
  DEFINE_VAR_OR_RETURN(child, DecodeChildSpan(/*record_size=*/4));
  std::vector<int32_t> values;
  while (!child.at_end()) {
    DEFINE_CONST_OR_RETURN(value, child.DecodeFixedInt32());
    values.push_back(value);
  }
  return std::move(values);
}

absl::StatusOr<std::vector<int64_t>> Decoder::DecodePackedFixedInt64s() {
  DEFINE_VAR_OR_RETURN(child, DecodeChildSpan(/*record_size=*/8));
  std::vector<int64_t> values;
  while (!child.at_end()) {
    DEFINE_CONST_OR_RETURN(value, child.DecodeFixedInt64());
    values.push_back(value);
  }
  return std::move(values);
}

absl::StatusOr<std::vector<uint32_t>> Decoder::DecodePackedFixedUInt32s() {
  DEFINE_VAR_OR_RETURN(child, DecodeChildSpan(/*record_size=*/4));
  std::vector<uint32_t> values;
  while (!child.at_end()) {
    DEFINE_CONST_OR_RETURN(value, child.DecodeFixedUInt32());
    values.push_back(value);
  }
  return std::move(values);
}

absl::StatusOr<std::vector<uint64_t>> Decoder::DecodePackedFixedUInt64s() {
  DEFINE_VAR_OR_RETURN(child, DecodeChildSpan(/*record_size=*/8));
  std::vector<uint64_t> values;
  while (!child.at_end()) {
    DEFINE_CONST_OR_RETURN(value, child.DecodeFixedUInt64());
    values.push_back(value);
  }
  return std::move(values);
}

absl::StatusOr<std::vector<bool>> Decoder::DecodePackedBools() {
  DEFINE_VAR_OR_RETURN(child, DecodeChildSpan());
  std::vector<bool> values;
  while (!child.at_end()) {
    DEFINE_CONST_OR_RETURN(value, child.DecodeBool());
    values.push_back(value);
  }
  return std::move(values);
}

absl::StatusOr<std::vector<float>> Decoder::DecodePackedFloats() {
  DEFINE_VAR_OR_RETURN(child, DecodeChildSpan(/*record_size=*/4));
  std::vector<float> values;
  while (!child.at_end()) {
    DEFINE_CONST_OR_RETURN(value, child.DecodeFloat());
    values.push_back(value);
  }
  return std::move(values);
}

absl::StatusOr<std::vector<double>> Decoder::DecodePackedDoubles() {
  DEFINE_VAR_OR_RETURN(child, DecodeChildSpan(/*record_size=*/8));
  std::vector<double> values;
  while (!child.at_end()) {
    DEFINE_CONST_OR_RETURN(value, child.DecodeDouble());
    values.push_back(value);
  }
  return std::move(values);
}

absl::Status Decoder::DecodeRepeatedSInt32s(WireType const wire_type,
                                            std::vector<int32_t> *const values) {
  switch (wire_type) {
    case WireType::kVarInt: {
      DEFINE_CONST_OR_RETURN(value, DecodeSInt32());
      values->emplace_back(value);
    } break;
    case WireType::kLength: {
      DEFINE_VAR_OR_RETURN(decoded, DecodePackedSInt32s());
      if (values->empty()) {
        *values = std::move(decoded);
      } else {
        values->insert(values->end(), decoded.begin(), decoded.end());
      }
    } break;
    default:
      return absl::InvalidArgumentError("invalid wire type for repeated integer field");
  }
  return absl::OkStatus();
}

absl::Status Decoder::DecodeRepeatedSInt64s(WireType const wire_type,
                                            std::vector<int64_t> *const values) {
  switch (wire_type) {
    case WireType::kVarInt: {
      DEFINE_CONST_OR_RETURN(value, DecodeSInt64());
      values->emplace_back(value);
    } break;
    case WireType::kLength: {
      DEFINE_VAR_OR_RETURN(decoded, DecodePackedSInt64s());
      if (values->empty()) {
        *values = std::move(decoded);
      } else {
        values->insert(values->end(), decoded.begin(), decoded.end());
      }
    } break;
    default:
      return absl::InvalidArgumentError("invalid wire type for repeated integer field");
  }
  return absl::OkStatus();
}

absl::Status Decoder::DecodeRepeatedFixedInt32s(WireType const wire_type,
                                                std::vector<int32_t> *const values) {
  switch (wire_type) {
    case WireType::kInt32: {
      DEFINE_CONST_OR_RETURN(value, DecodeFixedInt32());
      values->emplace_back(value);
    } break;
    case WireType::kLength: {
      DEFINE_VAR_OR_RETURN(decoded, DecodePackedFixedInt32s());
      if (values->empty()) {
        *values = std::move(decoded);
      } else {
        values->insert(values->end(), decoded.begin(), decoded.end());
      }
    } break;
    default:
      return absl::InvalidArgumentError("invalid wire type for repeated sfixed32 field");
  }
  return absl::OkStatus();
}

absl::Status Decoder::DecodeRepeatedFixedInt64s(WireType const wire_type,
                                                std::vector<int64_t> *const values) {
  switch (wire_type) {
    case WireType::kInt64: {
      DEFINE_CONST_OR_RETURN(value, DecodeFixedInt64());
      values->emplace_back(value);
    } break;
    case WireType::kLength: {
      DEFINE_VAR_OR_RETURN(decoded, DecodePackedFixedInt64s());
      if (values->empty()) {
        *values = std::move(decoded);
      } else {
        values->insert(values->end(), decoded.begin(), decoded.end());
      }
    } break;
    default:
      return absl::InvalidArgumentError("invalid wire type for repeated sfixed64 field");
  }
  return absl::OkStatus();
}

absl::Status Decoder::DecodeRepeatedFixedUInt32s(WireType const wire_type,
                                                 std::vector<uint32_t> *const values) {
  switch (wire_type) {
    case WireType::kInt32: {
      DEFINE_CONST_OR_RETURN(value, DecodeFixedUInt32());
      values->emplace_back(value);
    } break;
    case WireType::kLength: {
      DEFINE_VAR_OR_RETURN(decoded, DecodePackedFixedUInt32s());
      if (values->empty()) {
        *values = std::move(decoded);
      } else {
        values->insert(values->end(), decoded.begin(), decoded.end());
      }
    } break;
    default:
      return absl::InvalidArgumentError("invalid wire type for repeated fixed32 field");
  }
  return absl::OkStatus();
}

absl::Status Decoder::DecodeRepeatedFixedUInt64s(WireType const wire_type,
                                                 std::vector<uint64_t> *const values) {
  switch (wire_type) {
    case WireType::kInt64: {
      DEFINE_CONST_OR_RETURN(value, DecodeFixedUInt64());
      values->emplace_back(value);
    } break;
    case WireType::kLength: {
      DEFINE_VAR_OR_RETURN(decoded, DecodePackedFixedUInt64s());
      if (values->empty()) {
        *values = std::move(decoded);
      } else {
        values->insert(values->end(), decoded.begin(), decoded.end());
      }
    } break;
    default:
      return absl::InvalidArgumentError("invalid wire type for repeated fixed64 field");
  }
  return absl::OkStatus();
}

absl::Status Decoder::DecodeRepeatedBools(WireType const wire_type,
                                          std::vector<bool> *const values) {
  switch (wire_type) {
    case WireType::kVarInt: {
      DEFINE_CONST_OR_RETURN(value, DecodeBool());
      values->emplace_back(value);
    } break;
    case WireType::kLength: {
      DEFINE_VAR_OR_RETURN(decoded, DecodePackedBools());
      if (values->empty()) {
        *values = std::move(decoded);
      } else {
        values->insert(values->end(), decoded.begin(), decoded.end());
      }
    } break;
    default:
      return absl::InvalidArgumentError("invalid wire type for repeated bool field");
  }
  return absl::OkStatus();
}

absl::Status Decoder::DecodeRepeatedFloats(WireType const wire_type,
                                           std::vector<float> *const values) {
  switch (wire_type) {
    case WireType::kInt32: {
      DEFINE_CONST_OR_RETURN(value, DecodeFloat());
      values->emplace_back(value);
    } break;
    case WireType::kLength: {
      DEFINE_VAR_OR_RETURN(decoded, DecodePackedFloats());
      if (values->empty()) {
        *values = std::move(decoded);
      } else {
        values->insert(values->end(), decoded.begin(), decoded.end());
      }
    } break;
    default:
      return absl::InvalidArgumentError("invalid wire type for repeated float field");
  }
  return absl::OkStatus();
}

absl::Status Decoder::DecodeRepeatedDoubles(WireType const wire_type,
                                            std::vector<double> *const values) {
  switch (wire_type) {
    case WireType::kInt64: {
      DEFINE_CONST_OR_RETURN(value, DecodeDouble());
      values->emplace_back(value);
    } break;
    case WireType::kLength: {
      DEFINE_VAR_OR_RETURN(decoded, DecodePackedDoubles());
      if (values->empty()) {
        *values = std::move(decoded);
      } else {
        values->insert(values->end(), decoded.begin(), decoded.end());
      }
    } break;
    default:
      return absl::InvalidArgumentError("invalid wire type for repeated double field");
  }
  return absl::OkStatus();
}

absl::Status Decoder::SkipRecord(WireType const wire_type) {
  if (data_.empty()) {
    return EndOfInputError();
  }
  switch (wire_type) {
    case WireType::kVarInt: {
      size_t offset = 0;
      while ((data_[offset] & 0x80) != 0) {
        ++offset;
        if (offset >= data_.size()) {
          return EndOfInputError();
        }
      }
      data_.remove_prefix(offset + 1);
    } break;
    case WireType::kInt64:
      if (data_.size() < 8) {
        return EndOfInputError();
      } else {
        data_.remove_prefix(8);
      }
      break;
    case WireType::kLength: {
      DEFINE_CONST_OR_RETURN(length, DecodeUInt64());
      if (data_.size() < length) {
        return EndOfInputError();
      }
      data_.remove_prefix(length);
    } break;
    case WireType::kInt32:
      if (data_.size() < 4) {
        return EndOfInputError();
      } else {
        data_.remove_prefix(4);
      }
      break;
    default:
      return absl::InvalidArgumentError("unrecognized wire type");
  }
  return absl::OkStatus();
}

absl::Status Decoder::EndOfInputError() {
  return absl::InvalidArgumentError("decoding error: reached end of input");
}

absl::StatusOr<int32_t> Decoder::DecodeSInt32() {
  DEFINE_CONST_OR_RETURN(value, DecodeUInt32());
  return (value >> 1) ^ -(value & 1);
}

absl::StatusOr<int64_t> Decoder::DecodeSInt64() {
  DEFINE_CONST_OR_RETURN(value, DecodeUInt64());
  return (value >> 1) ^ -(value & 1);
}

absl::StatusOr<int32_t> Decoder::DecodeFixedInt32() {
  if (data_.size() < 4) {
    return EndOfInputError();
  }
  uint32_t value = *reinterpret_cast<uint32_t const *>(data_.data());
  data_.remove_prefix(4);
#ifdef ABSL_IS_BIG_ENDIAN
  value = ByteSwap32(value);
#endif  // ABSL_IS_BIG_ENDIAN
  return static_cast<int32_t>(value);
}

absl::StatusOr<uint32_t> Decoder::DecodeFixedUInt32() {
  if (data_.size() < 4) {
    return EndOfInputError();
  }
  uint32_t value = *reinterpret_cast<uint32_t const *>(data_.data());
  data_.remove_prefix(4);
#ifdef ABSL_IS_BIG_ENDIAN
  value = ByteSwap32(value);
#endif  // ABSL_IS_BIG_ENDIAN
  return value;
}

absl::StatusOr<int64_t> Decoder::DecodeFixedInt64() {
  if (data_.size() < 8) {
    return EndOfInputError();
  }
  uint64_t value = *reinterpret_cast<uint64_t const *>(data_.data());
  data_.remove_prefix(8);
#ifdef ABSL_IS_BIG_ENDIAN
  value = ByteSwap64(value);
#endif  // ABSL_IS_BIG_ENDIAN
  return static_cast<int64_t>(value);
}

absl::StatusOr<uint64_t> Decoder::DecodeFixedUInt64() {
  if (data_.size() < 8) {
    return EndOfInputError();
  }
  uint64_t value = *reinterpret_cast<uint64_t const *>(data_.data());
  data_.remove_prefix(8);
#ifdef ABSL_IS_BIG_ENDIAN
  value = ByteSwap64(value);
#endif  // ABSL_IS_BIG_ENDIAN
  return value;
}

absl::StatusOr<bool> Decoder::DecodeBool() {
  DEFINE_CONST_OR_RETURN(value, DecodeVarInt());
  return value != 0;
}

absl::StatusOr<float> Decoder::DecodeFloat() {
  DEFINE_VAR_OR_RETURN(value, DecodeFixedUInt32());
  return *reinterpret_cast<float const *>(&value);
}

absl::StatusOr<double> Decoder::DecodeDouble() {
  DEFINE_VAR_OR_RETURN(value, DecodeFixedUInt64());
  return *reinterpret_cast<double const *>(&value);
}

absl::StatusOr<Decoder> Decoder::DecodeChildSpan() {
  DEFINE_CONST_OR_RETURN(length, DecodeUInt64());
  if (data_.size() < length) {
    return EndOfInputError();
  }
  auto const subspan = data_.subspan(0, length);
  data_.remove_prefix(length);
  return Decoder(subspan);
}

absl::StatusOr<Decoder> Decoder::DecodeChildSpan(size_t const record_size) {
  DEFINE_CONST_OR_RETURN(length, DecodeUInt64());
  if ((length % record_size) != 0) {
    return absl::InvalidArgumentError("invalid packed array size");
  }
  if (data_.size() < length) {
    return EndOfInputError();
  }
  auto const subspan = data_.subspan(0, length);
  data_.remove_prefix(length);
  return Decoder(subspan);
}

void Encoder::EncodeTag(FieldTag const &tag) {
  EncodeIntegerInternal<uint64_t>((tag.field_number << 3) | static_cast<uint64_t>(tag.wire_type));
}

void Encoder::EncodeInt32Field(size_t const number, int32_t const value) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kVarInt});
  EncodeInt32(value);
}

void Encoder::EncodeUInt32Field(size_t const number, uint32_t const value) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kVarInt});
  EncodeUInt32(value);
}

void Encoder::EncodeInt64Field(size_t const number, int64_t const value) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kVarInt});
  EncodeInt64(value);
}

void Encoder::EncodeUInt64Field(size_t const number, uint64_t const value) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kVarInt});
  EncodeUInt64(value);
}

void Encoder::EncodeSInt32Field(size_t const number, int32_t const value) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kVarInt});
  EncodeSInt32(value);
}

void Encoder::EncodeSInt64Field(size_t const number, int64_t const value) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kVarInt});
  EncodeSInt64(value);
}

void Encoder::EncodeFixedInt32Field(size_t const number, int32_t const value) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kInt32});
  EncodeFixedInt32(value);
}

void Encoder::EncodeFixedUInt32Field(size_t const number, uint32_t const value) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kInt32});
  EncodeFixedUInt32(value);
}

void Encoder::EncodeFixedInt64Field(size_t const number, int64_t const value) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kInt64});
  EncodeFixedInt64(value);
}

void Encoder::EncodeFixedUInt64Field(size_t const number, uint64_t const value) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kInt64});
  EncodeFixedUInt64(value);
}

void Encoder::EncodeBoolField(size_t const number, bool const value) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kVarInt});
  EncodeBool(value);
}

void Encoder::EncodeFloatField(size_t const number, float const value) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kInt32});
  EncodeFloat(value);
}

void Encoder::EncodeDoubleField(size_t const number, double const value) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kInt64});
  EncodeDouble(value);
}

void Encoder::EncodeStringField(size_t const number, std::string_view const value) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kLength});
  size_t const length = value.size();
  EncodeIntegerInternal(length);
  tsdb2::io::Buffer buffer{length};
  buffer.MemCpy(value.data(), length);
  cord_.Append(std::move(buffer));
}

void Encoder::EncodeBytesField(size_t const number, absl::Span<uint8_t const> const value) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kLength});
  size_t const length = value.size();
  EncodeIntegerInternal(length);
  tsdb2::io::Buffer buffer{length};
  buffer.MemCpy(value.data(), length);
  cord_.Append(std::move(buffer));
}

void Encoder::EncodeTimeField(size_t const number, absl::Time const time) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kLength});
  EncodeSubMessage(::google::protobuf::Timestamp::Encode(EncodeGoogleApiProto(time)));
}

void Encoder::EncodeDurationField(size_t const number, absl::Duration const duration) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kLength});
  EncodeSubMessage(::google::protobuf::Duration::Encode(EncodeGoogleApiProto(duration)));
}

void Encoder::EncodeSubMessageField(size_t const number, Encoder &&child_encoder) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kLength});
  EncodeSubMessage(std::move(child_encoder));
}

void Encoder::EncodeSubMessageField(size_t const number, tsdb2::io::Cord cord) {
  EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kLength});
  EncodeSubMessage(std::move(cord));
}

void Encoder::EncodePackedSInt32s(size_t const field_number,
                                  absl::Span<int32_t const> const values) {
  EncodeTag(FieldTag{.field_number = field_number, .wire_type = WireType::kLength});
  Encoder child;
  for (int32_t const value : values) {
    child.EncodeSInt32(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodePackedSInt64s(size_t const field_number,
                                  absl::Span<int64_t const> const values) {
  EncodeTag(FieldTag{.field_number = field_number, .wire_type = WireType::kLength});
  Encoder child;
  for (int64_t const value : values) {
    child.EncodeSInt64(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodePackedFixedInt32s(size_t const field_number,
                                      absl::Span<int32_t const> const values) {
  EncodeTag(FieldTag{.field_number = field_number, .wire_type = WireType::kLength});
  Encoder child;
  for (int32_t const value : values) {
    child.EncodeFixedInt32(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodePackedFixedUInt32s(size_t const field_number,
                                       absl::Span<uint32_t const> const values) {
  EncodeTag(FieldTag{.field_number = field_number, .wire_type = WireType::kLength});
  Encoder child;
  for (uint32_t const value : values) {
    child.EncodeFixedUInt32(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodePackedFixedInt64s(size_t const field_number,
                                      absl::Span<int64_t const> const values) {
  EncodeTag(FieldTag{.field_number = field_number, .wire_type = WireType::kLength});
  Encoder child;
  for (int64_t const value : values) {
    child.EncodeFixedInt64(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodePackedFixedUInt64s(size_t const field_number,
                                       absl::Span<uint64_t const> const values) {
  EncodeTag(FieldTag{.field_number = field_number, .wire_type = WireType::kLength});
  Encoder child;
  for (uint64_t const value : values) {
    child.EncodeFixedUInt64(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodePackedBools(size_t const field_number, absl::Span<bool const> const values) {
  EncodeTag(FieldTag{.field_number = field_number, .wire_type = WireType::kLength});
  Encoder child;
  for (bool const value : values) {
    child.EncodeBool(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodePackedFloats(size_t const field_number, absl::Span<float const> const values) {
  EncodeTag(FieldTag{.field_number = field_number, .wire_type = WireType::kLength});
  Encoder child;
  for (float const value : values) {
    child.EncodeFloat(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodePackedDoubles(size_t const field_number,
                                  absl::Span<double const> const values) {
  EncodeTag(FieldTag{.field_number = field_number, .wire_type = WireType::kLength});
  Encoder child;
  for (double const value : values) {
    child.EncodeDouble(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodeSInt32(int32_t const value) { EncodeUInt32((value << 1) ^ (value >> 31)); }

void Encoder::EncodeSInt64(int64_t const value) { EncodeUInt64((value << 1) ^ (value >> 63)); }

void Encoder::EncodeFixedInt32(int32_t value) {
#ifdef ABSL_IS_BIG_ENDIAN
  value = ByteSwap32(value);
#endif  // ABSL_IS_BIG_ENDIAN
  tsdb2::io::Buffer buffer{4};
  buffer.as<int32_t>() = value;
  buffer.Advance(4);
  cord_.Append(std::move(buffer));
}

void Encoder::EncodeFixedUInt32(uint32_t value) {
#ifdef ABSL_IS_BIG_ENDIAN
  value = ByteSwap32(value);
#endif  // ABSL_IS_BIG_ENDIAN
  tsdb2::io::Buffer buffer{4};
  buffer.as<uint32_t>() = value;
  buffer.Advance(4);
  cord_.Append(std::move(buffer));
}

void Encoder::EncodeFixedInt64(int64_t value) {
#ifdef ABSL_IS_BIG_ENDIAN
  value = ByteSwap64(value);
#endif  // ABSL_IS_BIG_ENDIAN
  tsdb2::io::Buffer buffer{8};
  buffer.as<int64_t>() = value;
  buffer.Advance(8);
  cord_.Append(std::move(buffer));
}

void Encoder::EncodeFixedUInt64(uint64_t value) {
#ifdef ABSL_IS_BIG_ENDIAN
  value = ByteSwap64(value);
#endif  // ABSL_IS_BIG_ENDIAN
  tsdb2::io::Buffer buffer{8};
  buffer.as<uint64_t>() = value;
  buffer.Advance(8);
  cord_.Append(std::move(buffer));
}

void Encoder::EncodeSubMessage(Encoder &&child_encoder) {
  EncodeIntegerInternal(child_encoder.size());
  cord_.Append(std::move(child_encoder.cord_));
}

void Encoder::EncodeSubMessage(tsdb2::io::Cord cord) {
  EncodeIntegerInternal(cord.size());
  cord_.Append(std::move(cord));
}

}  // namespace proto
}  // namespace tsdb2
