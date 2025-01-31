#ifndef __TSDB2_PROTO_DECODER_H__
#define __TSDB2_PROTO_DECODER_H__

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/utilities.h"

namespace tsdb2 {
namespace proto {

enum class WireType : uint8_t {
  kVarInt = 0,
  kInt64 = 1,
  kLength = 2,
  kDeprecatedStartGroup = 3,
  kDeprecatedEndGroup = 4,
  kInt32 = 5,
};

struct FieldTag {
  uint64_t field_number;
  WireType wire_type;
};

class Decoder {
 public:
  explicit Decoder(absl::Span<uint8_t const> const data) : data_(data) {}

  ~Decoder() = default;

  Decoder(Decoder const &) = default;
  Decoder &operator=(Decoder const &) = default;
  Decoder(Decoder &&) noexcept = default;
  Decoder &operator=(Decoder &&) noexcept = default;

  template <typename Integer,
            std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
  absl::StatusOr<Integer> DecodeInteger() {
    return DecodeIntegerInternal(sizeof(Integer) * 8);
  }

  absl::StatusOr<FieldTag> DecodeTag();

  absl::StatusOr<int32_t> DecodeInt32() { return DecodeInteger<int32_t>(); }
  absl::StatusOr<int64_t> DecodeInt64() { return DecodeInteger<int64_t>(); }
  absl::StatusOr<uint32_t> DecodeUInt32() { return DecodeInteger<uint32_t>(); }
  absl::StatusOr<uint64_t> DecodeUInt64() { return DecodeInteger<uint64_t>(); }

  absl::StatusOr<int32_t> DecodeSInt32();
  absl::StatusOr<int64_t> DecodeSInt64();

  absl::StatusOr<int32_t> DecodeFixedInt32();
  absl::StatusOr<uint32_t> DecodeFixedUInt32();
  absl::StatusOr<int64_t> DecodeFixedInt64();
  absl::StatusOr<uint64_t> DecodeFixedUInt64();

  // TODO

 private:
  static absl::Status IntegerDecodingError(std::string_view message);

  absl::StatusOr<uint64_t> DecodeIntegerInternal(size_t max_bits);

  absl::Span<uint8_t const> data_;
};

}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_DECODER_H__
