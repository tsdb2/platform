#include "proto/wire.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

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
  return static_cast<int32_t>((value >> 1) ^ (value << 31));
}

absl::StatusOr<int64_t> Decoder::DecodeSInt64() {
  DEFINE_CONST_OR_RETURN(value, DecodeUInt64());
  return static_cast<int64_t>((value >> 1) ^ (value << 63));
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

absl::StatusOr<int32_t> Decoder::DecodeFixedInt32() {
  if (data_.size() < 4) {
    return IntegerDecodingError("reached end of input");
  }
  uint32_t value = *reinterpret_cast<uint32_t const*>(data_.data());
  data_.remove_prefix(4);
#ifdef ABSL_IS_BIG_ENDIAN
  value = ByteSwap32(value);
#endif  // ABSL_IS_BIG_ENDIAN
  return static_cast<int32_t>(value);
}

absl::StatusOr<uint32_t> Decoder::DecodeFixedUInt32() {
  if (data_.size() < 4) {
    return IntegerDecodingError("reached end of input");
  }
  uint32_t value = *reinterpret_cast<uint32_t const*>(data_.data());
  data_.remove_prefix(4);
#ifdef ABSL_IS_BIG_ENDIAN
  value = ByteSwap32(value);
#endif  // ABSL_IS_BIG_ENDIAN
  return value;
}

absl::StatusOr<int64_t> Decoder::DecodeFixedInt64() {
  if (data_.size() < 8) {
    return IntegerDecodingError("reached end of input");
  }
  uint64_t value = *reinterpret_cast<uint64_t const*>(data_.data());
  data_.remove_prefix(8);
#ifdef ABSL_IS_BIG_ENDIAN
  value = ByteSwap64(value);
#endif  // ABSL_IS_BIG_ENDIAN
  return static_cast<int64_t>(value);
}

absl::StatusOr<uint64_t> Decoder::DecodeFixedUInt64() {
  if (data_.size() < 8) {
    return IntegerDecodingError("reached end of input");
  }
  uint64_t value = *reinterpret_cast<uint64_t const*>(data_.data());
  data_.remove_prefix(8);
#ifdef ABSL_IS_BIG_ENDIAN
  value = ByteSwap64(value);
#endif  // ABSL_IS_BIG_ENDIAN
  return value;
}

absl::Status Decoder::IntegerDecodingError(std::string_view const message) {
  return absl::InvalidArgumentError(absl::StrCat("integer decoding error: ", message));
}

absl::StatusOr<uint64_t> Decoder::DecodeIntegerInternal(size_t const max_bits) {
  uint64_t value = 0;
  uint8_t byte;
  uint64_t m = 0;
  do {
    if (data_.empty()) {
      return IntegerDecodingError("reached end of input");
    }
    byte = data_.front();
    data_.remove_prefix(1);
    value += (byte & 0x7F) << m;
    m += 7;
    if (m > max_bits) {
      return IntegerDecodingError("exceeds 64 bits");
    }
  } while ((byte & 0x80) != 0);
  return value;
}

}  // namespace proto
}  // namespace tsdb2
