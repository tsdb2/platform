#ifndef __TSDB2_PROTO_WIRE_FORMAT_H__
#define __TSDB2_PROTO_WIRE_FORMAT_H__

#include <cstddef>
#include <cstdint>
#include <limits>
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

  template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
  absl::StatusOr<Enum> DecodeEnumField(WireType const wire_type) {
    if (wire_type != WireType::kVarInt) {
      return absl::InvalidArgumentError("invalid wire type for enum");
    }
    return DecodeEnum<Enum>();
  }

  absl::StatusOr<int32_t> DecodeInt32Field(WireType wire_type);
  absl::StatusOr<int64_t> DecodeInt64Field(WireType wire_type);
  absl::StatusOr<uint32_t> DecodeUInt32Field(WireType wire_type);
  absl::StatusOr<uint64_t> DecodeUInt64Field(WireType wire_type);

  absl::StatusOr<int32_t> DecodeSInt32Field(WireType wire_type);
  absl::StatusOr<int64_t> DecodeSInt64Field(WireType wire_type);

  absl::StatusOr<int32_t> DecodeFixedInt32Field(WireType wire_type);
  absl::StatusOr<uint32_t> DecodeFixedUInt32Field(WireType wire_type);
  absl::StatusOr<int64_t> DecodeFixedInt64Field(WireType wire_type);
  absl::StatusOr<uint64_t> DecodeFixedUInt64Field(WireType wire_type);

  absl::StatusOr<bool> DecodeBoolField(WireType wire_type);
  absl::StatusOr<float> DecodeFloatField(WireType wire_type);
  absl::StatusOr<double> DecodeDoubleField(WireType wire_type);
  absl::StatusOr<std::string> DecodeStringField(WireType wire_type);
  absl::StatusOr<std::vector<uint8_t>> DecodeBytesField(WireType wire_type);

  absl::StatusOr<absl::Span<uint8_t const>> GetChildSpan(WireType wire_type);

  template <typename Integer,
            std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
  absl::Status DecodeRepeatedIntegers(WireType const wire_type,
                                      std::vector<Integer> *const values) {
    switch (wire_type) {
      case WireType::kVarInt: {
        DEFINE_CONST_OR_RETURN(value, DecodeInteger<Integer>());
        values->emplace_back(value);
      } break;
      case WireType::kLength: {
        DEFINE_VAR_OR_RETURN(decoded, DecodePackedIntegers<Integer>());
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

  absl::Status DecodeRepeatedInt32s(WireType const wire_type, std::vector<int32_t> *const values) {
    return DecodeRepeatedIntegers(wire_type, values);
  }

  absl::Status DecodeRepeatedInt64s(WireType const wire_type, std::vector<int64_t> *const values) {
    return DecodeRepeatedIntegers(wire_type, values);
  }

  absl::Status DecodeRepeatedUInt32s(WireType const wire_type,
                                     std::vector<uint32_t> *const values) {
    return DecodeRepeatedIntegers(wire_type, values);
  }

  absl::Status DecodeRepeatedUInt64s(WireType const wire_type,
                                     std::vector<uint64_t> *const values) {
    return DecodeRepeatedIntegers(wire_type, values);
  }

  absl::Status DecodeRepeatedSInt32s(WireType wire_type, std::vector<int32_t> *values);
  absl::Status DecodeRepeatedSInt64s(WireType wire_type, std::vector<int64_t> *values);

  absl::Status DecodeRepeatedFixedInt32s(WireType wire_type, std::vector<int32_t> *values);
  absl::Status DecodeRepeatedFixedInt64s(WireType wire_type, std::vector<int64_t> *values);
  absl::Status DecodeRepeatedFixedUInt32s(WireType wire_type, std::vector<uint32_t> *values);
  absl::Status DecodeRepeatedFixedUInt64s(WireType wire_type, std::vector<uint64_t> *values);

  template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
  absl::Status DecodeRepeatedEnums(WireType const wire_type, std::vector<Enum> *const values) {
    switch (wire_type) {
      case WireType::kVarInt: {
        DEFINE_CONST_OR_RETURN(value, DecodeEnum<Enum>());
        values->emplace_back(value);
      } break;
      case WireType::kLength: {
        DEFINE_VAR_OR_RETURN(decoded, DecodePackedEnums<Enum>());
        values->reserve(values->size() + decoded.size());
        for (auto const value : decoded) {
          values->emplace_back(value);
        }
      } break;
      default:
        return absl::InvalidArgumentError("invalid wire type for repeated enum field");
    }
    return absl::OkStatus();
  }

  absl::Status DecodeRepeatedBools(WireType wire_type, std::vector<bool> *values);
  absl::Status DecodeRepeatedFloats(WireType wire_type, std::vector<float> *values);
  absl::Status DecodeRepeatedDoubles(WireType wire_type, std::vector<double> *values);

  absl::Status SkipRecord(WireType wire_type);

 private:
  static absl::Status EndOfInputError();

  absl::StatusOr<uint64_t> DecodeIntegerInternal(size_t max_bits);

  template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
  absl::StatusOr<Enum> DecodeEnum() {
    DEFINE_CONST_OR_RETURN(value, DecodeInteger<int64_t>());
    using Underlying = std::underlying_type_t<Enum>;
    if (value < std::numeric_limits<Underlying>::min()) {
      return absl::FailedPreconditionError(
          "the decoded enum value is lower than the minimum allowed");
    }
    if (value > std::numeric_limits<Underlying>::max()) {
      return absl::FailedPreconditionError(
          "the decoded enum value is greater than the maximum allowed");
    }
    return static_cast<Enum>(value);
  }

  absl::StatusOr<int32_t> DecodeSInt32();
  absl::StatusOr<int64_t> DecodeSInt64();

  absl::StatusOr<int32_t> DecodeFixedInt32();
  absl::StatusOr<uint32_t> DecodeFixedUInt32();
  absl::StatusOr<int64_t> DecodeFixedInt64();
  absl::StatusOr<uint64_t> DecodeFixedUInt64();

  absl::StatusOr<bool> DecodeBool();
  absl::StatusOr<float> DecodeFloat();
  absl::StatusOr<double> DecodeDouble();

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

  template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
  absl::StatusOr<std::vector<Enum>> DecodePackedEnums() {
    DEFINE_VAR_OR_RETURN(child, DecodeChildSpan());
    std::vector<Enum> values;
    while (!child.at_end()) {
      DEFINE_CONST_OR_RETURN(value, child.DecodeEnum<Enum>());
      values.push_back(value);
    }
    return std::move(values);
  }

  absl::StatusOr<std::vector<bool>> DecodePackedBools();
  absl::StatusOr<std::vector<float>> DecodePackedFloats();
  absl::StatusOr<std::vector<double>> DecodePackedDoubles();

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

  void EncodeInt32Field(size_t number, int32_t value);
  void EncodeUInt32Field(size_t number, uint32_t value);
  void EncodeInt64Field(size_t number, int64_t value);
  void EncodeUInt64Field(size_t number, uint64_t value);

  void EncodeSInt32Field(size_t number, int32_t value);
  void EncodeSInt64Field(size_t number, int64_t value);

  void EncodeFixedInt32Field(size_t number, int32_t value);
  void EncodeFixedUInt32Field(size_t number, uint32_t value);
  void EncodeFixedInt64Field(size_t number, int64_t value);
  void EncodeFixedUInt64Field(size_t number, uint64_t value);

  void EncodeBoolField(size_t number, bool value);
  void EncodeFloatField(size_t number, float value);
  void EncodeDoubleField(size_t number, double value);
  void EncodeStringField(size_t number, std::string_view value);
  void EncodeBytesField(size_t number, absl::Span<uint8_t const> value);

  template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
  void EncodeEnumField(size_t const number, Enum const value) {
    EncodeTag(FieldTag{.field_number = number, .wire_type = WireType::kVarInt});
    EncodeIntegerInternal(tsdb2::util::to_underlying(value));
  }

  void EncodeSubMessage(Encoder &&child_encoder);
  void EncodeSubMessage(tsdb2::io::Cord cord);

  void EncodePackedVarInts(size_t const field_number, absl::Span<uint64_t const> const values) {
    EncodePackedIntegers(field_number, values);
  }

  void EncodePackedInt32s(size_t const field_number, absl::Span<int32_t const> const values) {
    EncodePackedIntegers(field_number, values);
  }

  void EncodePackedUInt32s(size_t const field_number, absl::Span<uint32_t const> const values) {
    EncodePackedIntegers(field_number, values);
  }

  void EncodePackedInt64s(size_t const field_number, absl::Span<int64_t const> const values) {
    EncodePackedIntegers(field_number, values);
  }

  void EncodePackedUInt64s(size_t const field_number, absl::Span<uint64_t const> const values) {
    EncodePackedIntegers(field_number, values);
  }

  void EncodePackedSInt32s(size_t field_number, absl::Span<int32_t const> values);
  void EncodePackedSInt64s(size_t field_number, absl::Span<int64_t const> values);
  void EncodePackedFixedInt32s(size_t field_number, absl::Span<int32_t const> values);
  void EncodePackedFixedUInt32s(size_t field_number, absl::Span<uint32_t const> values);
  void EncodePackedFixedInt64s(size_t field_number, absl::Span<int64_t const> values);
  void EncodePackedFixedUInt64s(size_t field_number, absl::Span<uint64_t const> values);
  void EncodePackedBools(size_t field_number, absl::Span<bool const> values);
  void EncodePackedFloats(size_t field_number, absl::Span<float const> values);
  void EncodePackedDoubles(size_t field_number, absl::Span<double const> values);

  template <typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
  void EncodePackedEnums(size_t const field_number, absl::Span<Enum const> const values) {
    EncodeTag(FieldTag{.field_number = field_number, .wire_type = WireType::kLength});
    Encoder child;
    for (Enum const value : values) {
      child.EncodeIntegerInternal(tsdb2::util::to_underlying(value));
    }
    EncodeSubMessage(std::move(child));
  }

  tsdb2::io::Cord Finish() && { return std::move(cord_); }
  tsdb2::io::Buffer Flatten() && { return std::move(cord_).Flatten(); }

 private:
  static inline size_t constexpr kMaxVarIntLength = 10;

  template <typename Integer,
            std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
  void EncodeIntegerInternal(Integer const value) {
    std::make_unsigned_t<Integer> bits = value;
    tsdb2::io::Buffer buffer{kMaxVarIntLength};
    while (bits > 0x7F) {
      buffer.Append<uint8_t>(0x80 | static_cast<uint8_t>(bits & 0x7F));
      bits >>= 7;
    }
    buffer.Append<uint8_t>(bits & 0x7F);
    cord_.Append(std::move(buffer));
  }

  void EncodeInt32(int32_t const value) { EncodeIntegerInternal(value); }
  void EncodeUInt32(uint32_t const value) { EncodeIntegerInternal(value); }
  void EncodeInt64(int64_t const value) { EncodeIntegerInternal(value); }
  void EncodeUInt64(uint64_t const value) { EncodeIntegerInternal(value); }

  void EncodeSInt32(int32_t value);
  void EncodeSInt64(int64_t value);

  void EncodeFixedInt32(int32_t value);
  void EncodeFixedUInt32(uint32_t value);
  void EncodeFixedInt64(int64_t value);
  void EncodeFixedUInt64(uint64_t value);

  void EncodeBool(bool const value) { EncodeIntegerInternal(static_cast<uint8_t>(!!value)); }

  void EncodeFloat(float const value) {
    EncodeFixedUInt32(*reinterpret_cast<uint32_t const *>(&value));
  }

  void EncodeDouble(double const value) {
    EncodeFixedUInt64(*reinterpret_cast<uint64_t const *>(&value));
  }

  template <typename Integer,
            std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
  void EncodePackedIntegers(size_t const field_number, absl::Span<Integer const> values) {
    EncodeTag(FieldTag{.field_number = field_number, .wire_type = WireType::kLength});
    Encoder child;
    for (Integer const value : values) {
      child.EncodeIntegerInternal(value);
    }
    EncodeSubMessage(std::move(child));
  }

  tsdb2::io::Cord cord_;
};

}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_WIRE_FORMAT_H__
