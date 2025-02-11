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
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "common/utilities.h"
#include "io/buffer.h"
#include "io/cord.h"

namespace tsdb2 {
namespace proto {

namespace {

inline size_t constexpr kMaxVarIntLength = 10;

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

absl::StatusOr<FieldTag> Decoder::DecodeTag() {
  DEFINE_CONST_OR_RETURN(tag, DecodeInteger<uint64_t>());
  return FieldTag{
      .field_number = tag >> 3,
      .wire_type = static_cast<WireType>(tag & 7),
  };
}

absl::StatusOr<int32_t> Decoder::DecodeSInt32() {
  DEFINE_CONST_OR_RETURN(value, DecodeUInt32());
  return (value >> 1) ^ -(value & 1);
}

absl::StatusOr<int64_t> Decoder::DecodeSInt64() {
  DEFINE_CONST_OR_RETURN(value, DecodeUInt64());
  return (value >> 1) ^ -(value & 1);
}

absl::StatusOr<int32_t> Decoder::DecodeFixedInt32(WireType const wire_type) {
  if (wire_type != WireType::kInt32) {
    return absl::InvalidArgumentError("invalid wire type for sfixed32");
  }
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

absl::StatusOr<uint32_t> Decoder::DecodeFixedUInt32(WireType const wire_type) {
  if (wire_type != WireType::kInt32) {
    return absl::InvalidArgumentError("invalid wire type for fixed32");
  }
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

absl::StatusOr<int64_t> Decoder::DecodeFixedInt64(WireType const wire_type) {
  if (wire_type != WireType::kInt64) {
    return absl::InvalidArgumentError("invalid wire type for sfixed64");
  }
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

absl::StatusOr<uint64_t> Decoder::DecodeFixedUInt64(WireType const wire_type) {
  if (wire_type != WireType::kInt64) {
    return absl::InvalidArgumentError("invalid wire type for fixed64");
  }
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

absl::StatusOr<bool> Decoder::DecodeBool(WireType const wire_type) {
  if (wire_type != WireType::kVarInt) {
    return absl::InvalidArgumentError("invalid wire type for bool");
  }
  DEFINE_CONST_OR_RETURN(value, DecodeInteger<uint8_t>());
  return value != 0;
}

absl::StatusOr<float> Decoder::DecodeFloat(WireType const wire_type) {
  DEFINE_VAR_OR_RETURN(value, DecodeFixedUInt32(wire_type));
  return *reinterpret_cast<float const *>(&value);
}

absl::StatusOr<double> Decoder::DecodeDouble(WireType const wire_type) {
  DEFINE_VAR_OR_RETURN(value, DecodeFixedUInt64(wire_type));
  return *reinterpret_cast<double const *>(&value);
}

absl::StatusOr<std::string> Decoder::DecodeString(WireType const wire_type) {
  if (wire_type != WireType::kLength) {
    return absl::InvalidArgumentError("invalid wire type for string");
  }
  DEFINE_CONST_OR_RETURN(length, DecodeInteger<size_t>());
  if (data_.size() < length) {
    return EndOfInputError();
  }
  std::string value{reinterpret_cast<char const *>(data_.data()), length};
  data_.remove_prefix(length);
  return std::move(value);
}

absl::StatusOr<absl::Span<uint8_t const>> Decoder::GetChildSpan() {
  DEFINE_CONST_OR_RETURN(length, DecodeInteger<size_t>());
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
    DEFINE_CONST_OR_RETURN(value, child.DecodeFixedInt32(WireType::kInt32));
    values.push_back(value);
  }
  return std::move(values);
}

absl::StatusOr<std::vector<int64_t>> Decoder::DecodePackedFixedInt64s() {
  DEFINE_VAR_OR_RETURN(child, DecodeChildSpan(/*record_size=*/8));
  std::vector<int64_t> values;
  while (!child.at_end()) {
    DEFINE_CONST_OR_RETURN(value, child.DecodeFixedInt64(WireType::kInt64));
    values.push_back(value);
  }
  return std::move(values);
}

absl::StatusOr<std::vector<uint32_t>> Decoder::DecodePackedFixedUInt32s() {
  DEFINE_VAR_OR_RETURN(child, DecodeChildSpan(/*record_size=*/4));
  std::vector<uint32_t> values;
  while (!child.at_end()) {
    DEFINE_CONST_OR_RETURN(value, child.DecodeFixedUInt32(WireType::kInt32));
    values.push_back(value);
  }
  return std::move(values);
}

absl::StatusOr<std::vector<uint64_t>> Decoder::DecodePackedFixedUInt64s() {
  DEFINE_VAR_OR_RETURN(child, DecodeChildSpan(/*record_size=*/8));
  std::vector<uint64_t> values;
  while (!child.at_end()) {
    DEFINE_CONST_OR_RETURN(value, child.DecodeFixedUInt64(WireType::kInt64));
    values.push_back(value);
  }
  return std::move(values);
}

absl::StatusOr<std::vector<bool>> Decoder::DecodePackedBools() {
  DEFINE_VAR_OR_RETURN(child, DecodeChildSpan());
  std::vector<bool> values;
  while (!child.at_end()) {
    DEFINE_CONST_OR_RETURN(value, child.DecodeBool(WireType::kVarInt));
    values.push_back(value);
  }
  return std::move(values);
}

absl::StatusOr<std::vector<float>> Decoder::DecodePackedFloats() {
  DEFINE_VAR_OR_RETURN(child, DecodeChildSpan(/*record_size=*/4));
  std::vector<float> values;
  while (!child.at_end()) {
    DEFINE_CONST_OR_RETURN(value, child.DecodeFloat(WireType::kInt32));
    values.push_back(value);
  }
  return std::move(values);
}

absl::StatusOr<std::vector<double>> Decoder::DecodePackedDoubles() {
  DEFINE_VAR_OR_RETURN(child, DecodeChildSpan(/*record_size=*/8));
  std::vector<double> values;
  while (!child.at_end()) {
    DEFINE_CONST_OR_RETURN(value, child.DecodeDouble(WireType::kInt64));
    values.push_back(value);
  }
  return std::move(values);
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
      DEFINE_CONST_OR_RETURN(length, DecodeInteger<size_t>());
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

absl::StatusOr<uint64_t> Decoder::DecodeIntegerInternal(size_t const max_bits) {
  uint64_t value = 0;
  uint8_t byte;
  size_t m = 0;
  do {
    if (data_.empty()) {
      return EndOfInputError();
    }
    byte = data_.front();
    if (m + 7 > max_bits) {
      uint8_t const mask = (1 << (max_bits - m)) - 1;
      if ((byte & 0x7F) > mask) {
        return absl::InvalidArgumentError(
            absl::StrCat("decoding error: integer value exceeds ", max_bits, " bits"));
      }
    }
    data_.remove_prefix(1);
    value += (uint64_t{byte} & 0x7FULL) << m;
    m += 7;
  } while ((byte & 0x80) != 0);
  return value;
}

absl::StatusOr<Decoder> Decoder::DecodeChildSpan() {
  DEFINE_CONST_OR_RETURN(length, DecodeInteger<size_t>());
  if (data_.size() < length) {
    return EndOfInputError();
  }
  auto const subspan = data_.subspan(0, length);
  data_.remove_prefix(length);
  return Decoder(subspan);
}

absl::StatusOr<Decoder> Decoder::DecodeChildSpan(size_t const record_size) {
  DEFINE_CONST_OR_RETURN(length, DecodeInteger<size_t>());
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
  EncodeIntegerInternal((tag.field_number << 3) | static_cast<uint64_t>(tag.wire_type));
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

void Encoder::EncodeBool(bool const value) { EncodeIntegerInternal(static_cast<uint8_t>(!!value)); }

void Encoder::EncodeFloat(float const value) {
  EncodeFixedUInt32(*reinterpret_cast<uint32_t const *>(&value));
}

void Encoder::EncodeDouble(double const value) {
  EncodeFixedUInt64(*reinterpret_cast<uint64_t const *>(&value));
}

void Encoder::EncodeString(std::string_view const value) {
  size_t const length = value.size();
  EncodeIntegerInternal(length);
  tsdb2::io::Buffer buffer{length};
  buffer.MemCpy(value.data(), length);
  cord_.Append(std::move(buffer));
}

void Encoder::EncodeSubMessage(Encoder &&child_encoder) {
  EncodeIntegerInternal(child_encoder.size());
  cord_.Append(std::move(child_encoder.cord_));
}

void Encoder::EncodePackedSInt32s(absl::Span<int32_t const> const values) {
  Encoder child;
  for (int32_t const value : values) {
    child.EncodeSInt32(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodePackedSInt64s(absl::Span<int64_t const> const values) {
  Encoder child;
  for (int64_t const value : values) {
    child.EncodeSInt64(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodePackedFixedInt32s(absl::Span<int32_t const> const values) {
  Encoder child;
  for (int32_t const value : values) {
    child.EncodeFixedInt32(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodePackedFixedUInt32s(absl::Span<uint32_t const> const values) {
  Encoder child;
  for (uint32_t const value : values) {
    child.EncodeFixedUInt32(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodePackedFixedInt64s(absl::Span<int64_t const> const values) {
  Encoder child;
  for (int64_t const value : values) {
    child.EncodeFixedInt64(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodePackedFixedUInt64s(absl::Span<uint64_t const> const values) {
  Encoder child;
  for (uint64_t const value : values) {
    child.EncodeFixedUInt64(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodePackedBools(absl::Span<bool const> const values) {
  Encoder child;
  for (bool const value : values) {
    child.EncodeBool(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodePackedFloats(absl::Span<float const> const values) {
  Encoder child;
  for (float const value : values) {
    child.EncodeFloat(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodePackedDoubles(absl::Span<double const> const values) {
  Encoder child;
  for (double const value : values) {
    child.EncodeDouble(value);
  }
  EncodeSubMessage(std::move(child));
}

void Encoder::EncodeIntegerInternal(uint64_t value) {
  tsdb2::io::Buffer buffer{kMaxVarIntLength};
  while (value > 0x7F) {
    buffer.Append<uint8_t>(0x80 | static_cast<uint8_t>(value & 0x7F));
    value >>= 7;
  }
  buffer.Append<uint8_t>(value & 0x7F);
  cord_.Append(std::move(buffer));
}

}  // namespace proto
}  // namespace tsdb2
