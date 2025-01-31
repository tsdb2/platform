#ifndef __TSDB2_PROTO_DECODER_H__
#define __TSDB2_PROTO_DECODER_H__

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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

  size_t remaining() const { return data_.size(); }
  bool at_end() const { return data_.empty(); }

  absl::StatusOr<FieldTag> DecodeTag();

  absl::StatusOr<uint64_t> DecodeVarInt() { return DecodeInteger<uint64_t>(); }
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

  absl::StatusOr<bool> DecodeBool();
  absl::StatusOr<float> DecodeFloat();
  absl::StatusOr<double> DecodeDouble();
  absl::StatusOr<std::string> DecodeString();

  absl::StatusOr<absl::Span<uint8_t const>> GetChildSpan();

  absl::StatusOr<std::vector<uint64_t>> DecodePackedVarInts() {
    return DecodePackedIntegers<uint64_t>();
  }

  absl::StatusOr<std::vector<int32_t>> DecodePackedInt32s() {
    return DecodePackedIntegers<int32_t>();
  }

  absl::StatusOr<std::vector<int64_t>> DecodePackedInt64s() {
    return DecodePackedIntegers<int64_t>();
  }

  absl::StatusOr<std::vector<uint32_t>> DecodePackedUInt32s() {
    return DecodePackedIntegers<uint32_t>();
  }

  absl::StatusOr<std::vector<uint64_t>> DecodePackedUInt64s() {
    return DecodePackedIntegers<uint64_t>();
  }

  absl::StatusOr<std::vector<int32_t>> DecodePackedSInt32s();
  absl::StatusOr<std::vector<int64_t>> DecodePackedSInt64s();
  absl::StatusOr<std::vector<int32_t>> DecodePackedFixedInt32s();
  absl::StatusOr<std::vector<int64_t>> DecodePackedFixedInt64s();
  absl::StatusOr<std::vector<uint32_t>> DecodePackedFixedUInt32s();
  absl::StatusOr<std::vector<uint64_t>> DecodePackedFixedUInt64s();
  absl::StatusOr<std::vector<bool>> DecodePackedBools();
  absl::StatusOr<std::vector<float>> DecodePackedFloats();
  absl::StatusOr<std::vector<double>> DecodePackedDoubles();

  absl::Status SkipRecord(WireType wire_type);

 private:
  static absl::Status EndOfInputError();

  absl::StatusOr<uint64_t> DecodeIntegerInternal(size_t max_bits);

  template <typename Integer,
            std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
  absl::StatusOr<Integer> DecodeInteger() {
    DEFINE_CONST_OR_RETURN(value, DecodeIntegerInternal(sizeof(Integer) * 8));
    return static_cast<Integer>(value);
  }

  absl::StatusOr<Decoder> DecodeChildSpan();
  absl::StatusOr<Decoder> DecodeChildSpan(size_t record_size);

  template <typename Integer,
            std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
  absl::StatusOr<std::vector<Integer>> DecodePackedIntegers() {
    DEFINE_VAR_OR_RETURN(child, DecodeChildSpan());
    std::vector<Integer> values;
    while (!child.at_end()) {
      DEFINE_CONST_OR_RETURN(value, child.DecodeIntegerInternal(sizeof(Integer) * 8));
      values.push_back(value);
    }
    return std::move(values);
  }

  absl::Span<uint8_t const> data_;
};

}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_DECODER_H__
