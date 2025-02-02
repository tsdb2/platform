#include "proto/wire.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/config.h"  // IWYU pragma: keep
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "common/utilities.h"

namespace tsdb2 {
namespace proto {

absl::StatusOr<FieldTag> Decoder::DecodeTag() {
  DEFINE_CONST_OR_RETURN(tag, DecodeInteger<uint64_t>());
  return FieldTag{
      .field_number = tag >> 3,
      .wire_type = static_cast<WireType>(tag & 7),
  };
}

absl::StatusOr<int32_t> Decoder::DecodeSInt32() {
  DEFINE_CONST_OR_RETURN(value, DecodeUInt32());
  if ((value & 1) != 0) {
    return static_cast<int32_t>(-(value >> 1));
  } else {
    return static_cast<int32_t>(value >> 1);
  }
}

absl::StatusOr<int64_t> Decoder::DecodeSInt64() {
  DEFINE_CONST_OR_RETURN(value, DecodeUInt64());
  if ((value & 1) != 0) {
    return static_cast<int64_t>(-(value >> 1));
  } else {
    return static_cast<int64_t>(value >> 1);
  }
}

namespace {

constexpr uint16_t ByteSwap(uint16_t const value) { return (value >> 8) | (value << 8); }

constexpr uint32_t ByteSwap(uint32_t const value) {
  return ((value >> 24) & 0x000000FF) | ((value >> 8) & 0x0000FF00) | ((value << 8) & 0x00FF0000) |
         ((value << 24) & 0xFF000000);
}

constexpr uint64_t ByteSwap(uint64_t const value) {
  return ((value >> 56) & 0x00000000000000FF) | ((value >> 40) & 0x000000000000FF00) |
         ((value >> 24) & 0x0000000000FF0000) | ((value >> 8) & 0x00000000FF000000) |
         ((value << 8) & 0x000000FF00000000) | ((value << 24) & 0x0000FF0000000000) |
         ((value << 40) & 0x00FF000000000000) | ((value << 56) & 0xFF00000000000000);
}

}  // namespace

absl::StatusOr<int32_t> Decoder::DecodeFixedInt32(WireType const wire_type) {
  if (wire_type != WireType::kInt32) {
    return absl::InvalidArgumentError("invalid wire type for sfixed32");
  }
  if (data_.size() < 4) {
    return EndOfInputError();
  }
  uint32_t value = *reinterpret_cast<uint32_t const*>(data_.data());
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
  uint32_t value = *reinterpret_cast<uint32_t const*>(data_.data());
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
  uint64_t value = *reinterpret_cast<uint64_t const*>(data_.data());
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
  uint64_t value = *reinterpret_cast<uint64_t const*>(data_.data());
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
  return static_cast<bool>(value);
}

absl::StatusOr<float> Decoder::DecodeFloat(WireType const wire_type) {
  DEFINE_VAR_OR_RETURN(value, DecodeFixedUInt32(wire_type));
#ifdef ABSL_IS_BIG_ENDIAN
  value = ByteSwap32(value);
#endif  // ABSL_IS_BIG_ENDIAN
  return *reinterpret_cast<float const*>(&value);
}

absl::StatusOr<double> Decoder::DecodeDouble(WireType const wire_type) {
  DEFINE_VAR_OR_RETURN(value, DecodeFixedUInt64(wire_type));
#ifdef ABSL_IS_BIG_ENDIAN
  value = ByteSwap64(value);
#endif  // ABSL_IS_BIG_ENDIAN
  return *reinterpret_cast<double const*>(&value);
}

absl::StatusOr<std::string> Decoder::DecodeString(WireType const wire_type) {
  if (wire_type != WireType::kLength) {
    return absl::InvalidArgumentError("invalid wire type for string");
  }
  DEFINE_CONST_OR_RETURN(length, DecodeInteger<size_t>());
  if (data_.size() < length) {
    return EndOfInputError();
  }
  std::string value{reinterpret_cast<char const*>(data_.data()), length};
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

}  // namespace proto
}  // namespace tsdb2
