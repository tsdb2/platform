#ifndef __TSDB2_PROTO_DECODER_H__
#define __TSDB2_PROTO_DECODER_H__

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/utilities.h"
#include "io/buffer.h"
#include "io/cord.h"

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
  auto tie() const { return std::tie(field_number, wire_type); }

  friend bool operator==(FieldTag const &lhs, FieldTag const &rhs) {
    return lhs.tie() == rhs.tie();
  }

  friend bool operator!=(FieldTag const &lhs, FieldTag const &rhs) {
    return lhs.tie() != rhs.tie();
  }

  friend bool operator<(FieldTag const &lhs, FieldTag const &rhs) { return lhs.tie() < rhs.tie(); }

  friend bool operator<=(FieldTag const &lhs, FieldTag const &rhs) {
    return lhs.tie() <= rhs.tie();
  }

  friend bool operator>(FieldTag const &lhs, FieldTag const &rhs) { return lhs.tie() > rhs.tie(); }

  friend bool operator>=(FieldTag const &lhs, FieldTag const &rhs) {
    return lhs.tie() >= rhs.tie();
  }

  uint64_t field_number;
  WireType wire_type;
};

class Decoder {
 public:
  explicit Decoder(absl::Span<uint8_t const> const data) : data_(data) {}

  ~Decoder() = default;

  Decoder(Decoder const &) = delete;
  Decoder &operator=(Decoder const &) = delete;

  Decoder(Decoder &&) noexcept = default;
  Decoder &operator=(Decoder &&) noexcept = default;

  size_t remaining() const { return data_.size(); }
  bool at_end() const { return data_.empty(); }

  absl::StatusOr<FieldTag> DecodeTag();

  template <typename Integer,
            std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
  absl::StatusOr<Integer> DecodeInteger() {
    DEFINE_CONST_OR_RETURN(value, DecodeIntegerInternal(sizeof(Integer) * 8));
    return static_cast<Integer>(value);
  }

  absl::StatusOr<uint64_t> DecodeVarInt() { return DecodeInteger<uint64_t>(); }
  absl::StatusOr<int32_t> DecodeInt32() { return DecodeInteger<int32_t>(); }
  absl::StatusOr<int64_t> DecodeInt64() { return DecodeInteger<int64_t>(); }
  absl::StatusOr<uint32_t> DecodeUInt32() { return DecodeInteger<uint32_t>(); }
  absl::StatusOr<uint64_t> DecodeUInt64() { return DecodeInteger<uint64_t>(); }

  absl::StatusOr<int32_t> DecodeSInt32();
  absl::StatusOr<int64_t> DecodeSInt64();

  absl::StatusOr<int32_t> DecodeFixedInt32(WireType wire_type);
  absl::StatusOr<uint32_t> DecodeFixedUInt32(WireType wire_type);
  absl::StatusOr<int64_t> DecodeFixedInt64(WireType wire_type);
  absl::StatusOr<uint64_t> DecodeFixedUInt64(WireType wire_type);

  absl::StatusOr<bool> DecodeBool(WireType wire_type);
  absl::StatusOr<float> DecodeFloat(WireType wire_type);
  absl::StatusOr<double> DecodeDouble(WireType wire_type);
  absl::StatusOr<std::string> DecodeString(WireType wire_type);

  absl::StatusOr<absl::Span<uint8_t const>> GetChildSpan();

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

 public:  // TODO: make private
  absl::StatusOr<Decoder> DecodeChildSpan();
  absl::StatusOr<Decoder> DecodeChildSpan(size_t record_size);

 private:
  absl::Span<uint8_t const> data_;
};

class Encoder {
 public:
  explicit Encoder() = default;
  ~Encoder() = default;

  Encoder(Encoder const &) = delete;
  Encoder &operator=(Encoder const &) = delete;

  Encoder(Encoder &&) noexcept = default;
  Encoder &operator=(Encoder &&) noexcept = default;

  [[nodiscard]] bool empty() const { return cord_.empty(); }
  size_t size() const { return cord_.size(); }

  void EncodeTag(FieldTag const &tag);

  void EncodeVarInt(uint64_t const value) { return EncodeIntegerInternal(value); }
  void EncodeInt32(int32_t const value) { return EncodeIntegerInternal(value); }
  void EncodeUInt32(uint32_t const value) { return EncodeIntegerInternal(value); }
  void EncodeInt64(int64_t const value) { return EncodeIntegerInternal(value); }
  void EncodeUInt64(uint64_t const value) { return EncodeIntegerInternal(value); }

  void EncodeSInt32(int32_t value);
  void EncodeSInt64(int64_t value);

  void EncodeFixedInt32(int32_t value);
  void EncodeFixedUInt32(uint32_t value);
  void EncodeFixedInt64(int64_t value);
  void EncodeFixedUInt64(uint64_t value);

  void EncodeBool(bool value);
  void EncodeFloat(float value);
  void EncodeDouble(double value);
  void EncodeString(std::string_view value);

  void EncodeSubMessage(Encoder &&child_encoder);

  tsdb2::io::Cord Finish() && { return std::move(cord_); }
  tsdb2::io::Buffer Flatten() && { return std::move(cord_).Flatten(); }

 private:
  void EncodeIntegerInternal(uint64_t value);

  tsdb2::io::Cord cord_;
};

}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_DECODER_H__
