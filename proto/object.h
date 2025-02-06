#ifndef __TSDB2_PROTO_OBJECT_H__
#define __TSDB2_PROTO_OBJECT_H__

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/type_string.h"
#include "common/utilities.h"
#include "io/cord.h"
#include "proto/wire.h"

namespace tsdb2 {
namespace proto {

namespace internal {

template <typename Type, typename Name, size_t tag>
struct FieldImpl;

template <typename Type, char... ch, size_t tag>
struct FieldImpl<Type, tsdb2::common::TypeStringMatcher<ch...>, tag> {
  using Name = tsdb2::common::TypeStringMatcher<ch...>;
  static std::string_view constexpr name = Name::value;
};

template <typename Type, typename Enable = void>
struct WireTypeFor;

template <typename Integral>
struct WireTypeFor<Integral, std::enable_if_t<std::is_integral_v<Integral>>> {
  static WireType constexpr value = WireType::kVarInt;
};

template <typename Enum>
struct WireTypeFor<Enum, std::enable_if_t<std::is_enum_v<Enum>>> {
  static WireType constexpr value = WireType::kVarInt;
};

template <>
struct WireTypeFor<std::string> {
  static WireType constexpr value = WireType::kLength;
};

template <>
struct WireTypeFor<float> {
  static WireType constexpr value = WireType::kInt32;
};

template <>
struct WireTypeFor<double> {
  static WireType constexpr value = WireType::kInt64;
};

template <typename Type>
inline WireType constexpr WireTypeForV = WireTypeFor<Type>::value;

template <typename Type>
inline bool constexpr kPackedEncodingAllowed = std::is_arithmetic_v<Type> || std::is_enum_v<Type>;

template <typename Field, typename Enable = void>
struct FieldDecoder;

template <typename Name, size_t tag>
struct FieldDecoder<FieldImpl<bool, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          bool *const value) const {
    DEFINE_CONST_OR_RETURN(decoded, decoder->DecodeBool(wire_type));
    *value = decoded;
    return absl::OkStatus();
  }
};

template <typename Name, size_t tag>
struct FieldDecoder<FieldImpl<int8_t, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          int8_t *const value) const {
    if (wire_type == WireType::kVarInt) {
      return absl::InvalidArgumentError("invalid wire type for int8_t");
    }
    DEFINE_CONST_OR_RETURN(decoded, decoder->DecodeInteger<int8_t>());
    *value = decoded;
    return absl::OkStatus();
  }
};

template <typename Name, size_t tag>
struct FieldDecoder<FieldImpl<uint8_t, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          uint8_t *const value) const {
    if (wire_type == WireType::kVarInt) {
      return absl::InvalidArgumentError("invalid wire type for uint8_t");
    }
    DEFINE_CONST_OR_RETURN(decoded, decoder->DecodeInteger<uint8_t>());
    *value = decoded;
    return absl::OkStatus();
  }
};

template <typename Name, size_t tag>
struct FieldDecoder<FieldImpl<int16_t, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          int16_t *const value) const {
    if (wire_type == WireType::kVarInt) {
      return absl::InvalidArgumentError("invalid wire type for int16_t");
    }
    DEFINE_CONST_OR_RETURN(decoded, decoder->DecodeInteger<int16_t>());
    *value = decoded;
    return absl::OkStatus();
  }
};

template <typename Name, size_t tag>
struct FieldDecoder<FieldImpl<uint16_t, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          uint16_t *const value) const {
    if (wire_type == WireType::kVarInt) {
      return absl::InvalidArgumentError("invalid wire type for uint16_t");
    }
    DEFINE_CONST_OR_RETURN(decoded, decoder->DecodeInteger<uint16_t>());
    *value = decoded;
    return absl::OkStatus();
  }
};

template <typename Name, size_t tag>
struct FieldDecoder<FieldImpl<int32_t, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          int32_t *const value) const {
    switch (wire_type) {
      case WireType::kVarInt: {
        DEFINE_CONST_OR_RETURN(decoded, decoder->DecodeInteger<int32_t>());
        *value = decoded;
        return absl::OkStatus();
      }
      case WireType::kInt32: {
        DEFINE_CONST_OR_RETURN(decoded, decoder->DecodeFixedInt32(WireType::kInt32));
        *value = decoded;
        return absl::OkStatus();
      }
      default:
        return absl::InvalidArgumentError("invalid wire type for int32_t");
    }
  }
};

template <typename Name, size_t tag>
struct FieldDecoder<FieldImpl<uint32_t, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          uint32_t *const value) const {
    switch (wire_type) {
      case WireType::kVarInt: {
        DEFINE_CONST_OR_RETURN(decoded, decoder->DecodeInteger<uint32_t>());
        *value = decoded;
        return absl::OkStatus();
      }
      case WireType::kInt32: {
        DEFINE_CONST_OR_RETURN(decoded, decoder->DecodeFixedUInt32(WireType::kInt32));
        *value = decoded;
        return absl::OkStatus();
      }
      default:
        return absl::InvalidArgumentError("invalid wire type for uint32_t");
    }
  }
};

template <typename Name, size_t tag>
struct FieldDecoder<FieldImpl<int64_t, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          int64_t *const value) const {
    switch (wire_type) {
      case WireType::kVarInt: {
        DEFINE_CONST_OR_RETURN(decoded, decoder->DecodeInteger<int64_t>());
        *value = decoded;
        return absl::OkStatus();
      }
      case WireType::kInt64: {
        DEFINE_CONST_OR_RETURN(decoded, decoder->DecodeFixedInt32(WireType::kInt64));
        *value = decoded;
        return absl::OkStatus();
      }
      default:
        return absl::InvalidArgumentError("invalid wire type for int64_t");
    }
  }
};

template <typename Name, size_t tag>
struct FieldDecoder<FieldImpl<uint64_t, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          uint64_t *const value) const {
    switch (wire_type) {
      case WireType::kVarInt: {
        DEFINE_CONST_OR_RETURN(decoded, decoder->DecodeInteger<uint64_t>());
        *value = decoded;
        return absl::OkStatus();
      }
      case WireType::kInt64: {
        DEFINE_CONST_OR_RETURN(decoded, decoder->DecodeFixedUInt64(WireType::kInt64));
        *value = decoded;
        return absl::OkStatus();
      }
      default:
        return absl::InvalidArgumentError("invalid wire type for uint64_t");
    }
  }
};

template <typename Name, size_t tag>
struct FieldDecoder<FieldImpl<float, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          float *const value) const {
    DEFINE_CONST_OR_RETURN(decoded, decoder->DecodeFloat(wire_type));
    *value = decoded;
    return absl::OkStatus();
  }
};

template <typename Name, size_t tag>
struct FieldDecoder<FieldImpl<double, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          double *const value) const {
    DEFINE_CONST_OR_RETURN(decoded, decoder->DecodeDouble(wire_type));
    *value = decoded;
    return absl::OkStatus();
  }
};

template <typename Name, size_t tag>
struct FieldDecoder<FieldImpl<std::string, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          std::string *const value) const {
    DEFINE_VAR_OR_RETURN(decoded, decoder->DecodeString(wire_type));
    *value = std::move(decoded);
    return absl::OkStatus();
  }
};

template <typename Type, typename Name, size_t tag>
struct FieldDecoder<FieldImpl<std::optional<Type>, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          std::optional<Type> *const value) const {
    Type decoded{};
    RETURN_IF_ERROR((FieldDecoder<FieldImpl<Type, Name, tag>>()(decoder, wire_type, &decoded)));
    *value = std::move(decoded);
    return absl::OkStatus();
  }
};

template <typename Type, typename Name, size_t tag>
struct FieldDecoder<FieldImpl<std::unique_ptr<Type>, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          std::unique_ptr<Type> *const value) const {
    auto decoded = std::make_unique<Type>();
    return FieldDecoder<FieldImpl<Type, Name, tag>>()(decoder, wire_type, decoded.get());
  }
};

template <typename Type, typename Name, size_t tag>
struct FieldDecoder<FieldImpl<std::shared_ptr<Type>, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          std::shared_ptr<Type> *const value) const {
    auto decoded = std::make_shared<Type>();
    return FieldDecoder<FieldImpl<Type, Name, tag>>()(decoder, wire_type, decoded.get());
  }
};

template <typename Type, typename Name, size_t tag>
struct FieldDecoder<FieldImpl<std::vector<Type>, Name, tag>,
                    std::enable_if_t<kPackedEncodingAllowed<Type>>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          std::vector<Type> *const value) const {
    if (wire_type != WireType::kLength) {
      Type decoded{};
      RETURN_IF_ERROR((FieldDecoder<FieldImpl<Type, Name, tag>>()(decoder, wire_type, &decoded)));
      value->emplace_back(std::move(decoded));
    } else {
      DEFINE_VAR_OR_RETURN(child_decoder, decoder->DecodeChildSpan());
      WireType const wire_type = WireTypeForV<Type>;
      while (!child_decoder.at_end()) {
        Type decoded{};
        RETURN_IF_ERROR(
            (FieldDecoder<FieldImpl<Type, Name, tag>>()(&child_decoder, wire_type, &decoded)));
        value->emplace_back(std::move(decoded));
      }
    }
    return absl::OkStatus();
  }
};

template <typename Type, typename Name, size_t tag>
struct FieldDecoder<FieldImpl<std::vector<Type>, Name, tag>,
                    std::enable_if_t<!kPackedEncodingAllowed<Type>>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          std::vector<Type> *const value) const {
    Type decoded{};
    RETURN_IF_ERROR((FieldDecoder<FieldImpl<Type, Name, tag>>()(decoder, wire_type, &decoded)));
    value->emplace_back(std::move(decoded));
    return absl::OkStatus();
  }
};

template <typename Type>
struct MergeField {
  void operator()(Type &lhs, Type &&rhs) const { lhs = std::move(rhs); }
};

template <typename Type>
struct MergeField<std::optional<Type>> {
  void operator()(std::optional<Type> &lhs, std::optional<Type> &&rhs) const {
    if (rhs.has_value()) {
      lhs = std::move(rhs);
    }
  }
};

template <typename Type>
struct MergeField<std::unique_ptr<Type>> {
  void operator()(std::unique_ptr<Type> &lhs, std::unique_ptr<Type> &&rhs) const {
    if (rhs) {
      lhs = std::move(rhs);
    }
  }
};

template <typename Type>
struct MergeField<std::shared_ptr<Type>> {
  void operator()(std::shared_ptr<Type> &lhs, std::shared_ptr<Type> &&rhs) const {
    if (rhs) {
      lhs = std::move(rhs);
    }
  }
};

template <typename Type>
struct MergeField<std::vector<Type>> {
  void operator()(std::vector<Type> &lhs, std::vector<Type> &&rhs) const {
    if (rhs.has_value()) {
      lhs.insert(lhs.end(), std::make_move_iterator(rhs.begin()),
                 std::make_move_iterator(rhs.end()));
    }
  }
};

template <typename Type, typename Enable = void>
struct ValueEncoder;

template <typename Integer>
struct ValueEncoder<Integer, std::enable_if_t<std::is_integral_v<Integer>>> {
  void operator()(Encoder *const encoder, Integer const value) const {
    encoder->EncodeVarInt(value);
  }
};

template <typename Enum>
struct ValueEncoder<Enum, std::enable_if_t<std::is_enum_v<Enum>>> {
  void operator()(Encoder *const encoder, Enum const value) const {
    encoder->EncodeVarInt(tsdb2::util::to_underlying(value));
  }
};

template <>
struct ValueEncoder<float> {
  void operator()(Encoder *const encoder, float const value) const { encoder->EncodeFloat(value); }
};

template <>
struct ValueEncoder<double> {
  void operator()(Encoder *const encoder, double const value) const {
    encoder->EncodeDouble(value);
  }
};

template <>
struct ValueEncoder<std::string> {
  void operator()(Encoder *const encoder, std::string_view const value) const {
    encoder->EncodeString(value);
  }
};

template <typename Type, size_t tag, typename Enable = void>
struct FieldEncoder;

template <typename Integer, size_t tag>
struct FieldEncoder<Integer, tag, std::enable_if_t<std::is_integral_v<Integer>>> {
  void operator()(Encoder *const encoder, Integer const value) const {
    encoder->EncodeTag(FieldTag{.field_number = tag, .wire_type = WireType::kVarInt});
    ValueEncoder<Integer>{}(encoder, value);
  }
};

template <typename Enum, size_t tag>
struct FieldEncoder<Enum, tag, std::enable_if_t<std::is_enum_v<Enum>>> {
  void operator()(Encoder *const encoder, Enum const value) const {
    encoder->EncodeTag(FieldTag{.field_number = tag, .wire_type = WireType::kVarInt});
    ValueEncoder<Enum>{}(encoder, value);
  }
};

template <size_t tag>
struct FieldEncoder<float, tag> {
  void operator()(Encoder *const encoder, float const value) const {
    encoder->EncodeTag(FieldTag{.field_number = tag, .wire_type = WireType::kInt32});
    ValueEncoder<float>{}(encoder, value);
  }
};

template <size_t tag>
struct FieldEncoder<double, tag> {
  void operator()(Encoder *const encoder, double const value) const {
    encoder->EncodeTag(FieldTag{.field_number = tag, .wire_type = WireType::kInt64});
    ValueEncoder<double>{}(encoder, value);
  }
};

template <size_t tag>
struct FieldEncoder<std::string, tag> {
  void operator()(Encoder *const encoder, std::string_view const value) const {
    encoder->EncodeTag(FieldTag{.field_number = tag, .wire_type = WireType::kLength});
    ValueEncoder<std::string>{}(encoder, value);
  }
};

template <typename Type, size_t tag>
struct FieldEncoder<std::optional<Type>, tag> {
  void operator()(Encoder *const encoder, std::optional<Type> const &value) const {
    if (value.has_value()) {
      FieldEncoder<Type, tag>{}(encoder, value.value());
    }
  }
};

template <typename Type, size_t tag>
struct FieldEncoder<std::unique_ptr<Type>, tag> {
  void operator()(Encoder *const encoder, std::unique_ptr<Type> const &value) const {
    if (value) {
      FieldEncoder<Type, tag>{}(encoder, *value);
    }
  }
};

template <typename Type, size_t tag>
struct FieldEncoder<std::shared_ptr<Type>, tag> {
  void operator()(Encoder *const encoder, std::shared_ptr<Type> const &value) const {
    if (value) {
      FieldEncoder<Type, tag>{}(encoder, *value);
    }
  }
};

template <typename Type, size_t tag>
struct FieldEncoder<std::vector<Type>, tag, std::enable_if_t<kPackedEncodingAllowed<Type>>> {
  void operator()(Encoder *const encoder, std::vector<Type> const &value) const {
    encoder->EncodeTag(FieldTag{.field_number = tag, .wire_type = WireType::kLength});
    Encoder child_encoder;
    for (Type const &element : value) {
      ValueEncoder<Type>{}(&child_encoder, element);
    }
    encoder->EncodeSubMessage(std::move(child_encoder));
  }
};

template <typename Type, size_t tag>
struct FieldEncoder<std::vector<Type>, tag, std::enable_if_t<!kPackedEncodingAllowed<Type>>> {
  void operator()(Encoder *const encoder, std::vector<Type> const &value) const {
    for (auto const &element : value) {
      FieldEncoder<Type, tag>{}(encoder, element);
    }
  }
};

}  // namespace internal

template <typename Type, char const name[], size_t tag>
using Field = internal::FieldImpl<Type, tsdb2::common::TypeStringT<name>, tag>;

struct Initialize {};
inline Initialize constexpr kInitialize{};

template <typename... Fields>
class Object;

namespace internal {

template <typename... Fields, typename Name, size_t tag>
struct FieldDecoder<FieldImpl<Object<Fields...>, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          Object<Fields...> *const value) const {
    if (wire_type != WireType::kLength) {
      return absl::InvalidArgumentError("invalid wire type for submessage");
    }
    DEFINE_CONST_OR_RETURN(child_span, decoder->GetChildSpan());
    DEFINE_VAR_OR_RETURN(decoded, Object<Fields...>::Decode(child_span));
    value->Merge(std::move(decoded));
    return absl::OkStatus();
  }
};

template <typename... Fields>
struct ValueEncoder<Object<Fields...>> {
  void operator()(Encoder *const encoder, Object<Fields...> const &value) const {
    Encoder child_encoder;
    value.EncodeInternal(&child_encoder);
    encoder->EncodeSubMessage(std::move(child_encoder));
  }
};

template <typename... Fields, size_t tag>
struct FieldEncoder<Object<Fields...>, tag> {
  void operator()(Encoder *const encoder, Object<Fields...> const &value) const {
    encoder->EncodeTag(FieldTag{.field_number = tag, .wire_type = WireType::kLength});
    ValueEncoder<Object<Fields...>>{}(encoder, value);
  }
};

}  // namespace internal

template <>
class Object<> {
 public:
  static absl::StatusOr<Object> Decode(absl::Span<uint8_t const> const buffer) {
    Decoder decoder{buffer};
    while (!decoder.at_end()) {
      DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
      RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
    }
    return Object();
  }

  void Merge(Object &&other) {}

  tsdb2::io::Cord Encode() const {  // NOLINT
    return Encoder{}.Finish();
  }

  explicit Object() = default;
  explicit Object(Initialize /*initialize*/) {}
  ~Object() = default;

  Object(Object const &) = default;
  Object &operator=(Object const &) = default;
  Object(Object &&) noexcept = default;
  Object &operator=(Object &&) noexcept = default;

  friend bool operator==(Object const &lhs, Object const &rhs) {
    return lhs.CompareEqualInternal(rhs);
  }

  friend bool operator!=(Object const &lhs, Object const &rhs) {
    return !lhs.CompareEqualInternal(rhs);
  }

  friend bool operator<(Object const &lhs, Object const &rhs) {
    return lhs.CompareLessInternal(rhs);
  }

  friend bool operator<=(Object const &lhs, Object const &rhs) {
    return !rhs.CompareLessInternal(lhs);
  }

  friend bool operator>(Object const &lhs, Object const &rhs) {
    return rhs.CompareLessInternal(lhs);
  }

  friend bool operator>=(Object const &lhs, Object const &rhs) {
    return !lhs.CompareLessInternal(rhs);
  }

  template <typename H>
  friend H AbslHashValue(H h, Object const &value) {
    return H::combine(std::move(h));
  }

 protected:
  // NOLINTBEGIN(readability-convert-member-functions-to-static)

  ABSL_ATTRIBUTE_ALWAYS_INLINE bool CompareEqualInternal(Object const &other) const { return true; }
  ABSL_ATTRIBUTE_ALWAYS_INLINE bool CompareLessInternal(Object const &other) const { return false; }

  absl::Status ReadField(Decoder *const decoder, FieldTag const &field_tag) {
    return decoder->SkipRecord(field_tag.wire_type);
  }

  void EncodeInternal(Encoder *const encoder) const {}

  // NOLINTEND(readability-convert-member-functions-to-static)
};

template <typename Type, typename Name, size_t tag, typename... OtherFields>
class Object<internal::FieldImpl<Type, Name, tag>, OtherFields...> : public Object<OtherFields...> {
 public:
  template <typename FieldName, typename Dummy = void>
  struct FieldTypeImpl;

  template <char const field_name[]>
  using FieldType = typename FieldTypeImpl<tsdb2::common::TypeStringT<field_name>>::Type;

  static absl::StatusOr<Object> Decode(absl::Span<uint8_t const> const buffer) {
    Decoder decoder{buffer};
    Object object;
    while (!decoder.at_end()) {
      DEFINE_CONST_OR_RETURN(field_tag, decoder.DecodeTag());
      RETURN_IF_ERROR(object.ReadField(&decoder, field_tag));
    }
    return std::move(object);
  }

  void Merge(Object &&other) {
    internal::MergeField<Type>()(value_, std::move(other.value_));
    Object<OtherFields...>::Merge(std::move(other));
  }

  tsdb2::io::Cord Encode() const {
    Encoder encoder;
    EncodeInternal(&encoder);
    return std::move(encoder).Finish();
  }

  explicit Object() = default;

  template <typename FirstArg, typename... OtherArgs>
  explicit Object(Initialize /*initialize*/, FirstArg &&value, OtherArgs &&...args)
      : Object<OtherFields...>(kInitialize, std::forward<OtherArgs>(args)...),
        value_(std::forward<FirstArg>(value)) {}

  ~Object() = default;

  Object(Object const &) = default;
  Object &operator=(Object const &) = default;

  Object(Object &&) noexcept = default;
  Object &operator=(Object &&) noexcept = default;

  friend bool operator==(Object const &lhs, Object const &rhs) {
    return lhs.CompareEqualInternal(rhs);
  }

  friend bool operator!=(Object const &lhs, Object const &rhs) {
    return !lhs.CompareEqualInternal(rhs);
  }

  friend bool operator<(Object const &lhs, Object const &rhs) {
    return lhs.CompareLessInternal(rhs);
  }

  friend bool operator<=(Object const &lhs, Object const &rhs) {
    return !rhs.CompareLessInternal(lhs);
  }

  friend bool operator>(Object const &lhs, Object const &rhs) {
    return rhs.CompareLessInternal(lhs);
  }

  friend bool operator>=(Object const &lhs, Object const &rhs) {
    return !lhs.CompareLessInternal(rhs);
  }

  template <typename H>
  friend H AbslHashValue(H h, Object const &value) {
    return H::combine(std::move(h), value.value_,
                      ConstGetter<typename OtherFields::Name>(value)()...);
  }

  template <char const field_name[]>
  auto &get() & noexcept {
    return Getter<tsdb2::common::TypeStringT<field_name>>(*this)();
  }

  template <char const field_name[]>
  auto &&get() && noexcept {
    return std::move(Getter<tsdb2::common::TypeStringT<field_name>>(*this)());
  }

  template <char const field_name[]>
  auto const &get() const & noexcept {
    return ConstGetter<tsdb2::common::TypeStringT<field_name>>(*this)();
  }

  template <char const field_name[]>
  auto const &cget() const & noexcept {
    return ConstGetter<tsdb2::common::TypeStringT<field_name>>(*this)();
  }

 protected:
  template <typename FieldType, typename Enable>
  friend struct internal::ValueEncoder;

  template <typename FieldName, typename Dummy = void>
  struct Getter;

  template <typename FieldName, typename Dummy = void>
  struct ConstGetter;

  ABSL_ATTRIBUTE_ALWAYS_INLINE bool CompareEqualInternal(Object const &other) const {
    return value_ == other.value_ && Object<OtherFields...>::CompareEqualInternal(other);
  }

  ABSL_ATTRIBUTE_ALWAYS_INLINE bool CompareLessInternal(Object const &other) const {
    return value_ < other.value_ ||
           (!(other.value_ < value_) && Object<OtherFields...>::CompareLessInternal(other));
  }

  absl::Status ReadField(Decoder *const decoder, FieldTag const &field_tag) {
    if (field_tag.field_number != tag) {
      return Object<OtherFields...>::ReadField(decoder, field_tag);
    } else {
      return internal::FieldDecoder<internal::FieldImpl<Type, Name, tag>>{}(
          decoder, field_tag.wire_type, &value_);
    }
  }

  void EncodeInternal(Encoder *const encoder) const {
    internal::FieldEncoder<Type, tag>{}(encoder, value_);
    Object<OtherFields...>::EncodeInternal(encoder);
  }

 private:
  Type value_{};
};

template <typename Type2, typename Name, size_t tag, typename... OtherFields>
template <typename Dummy>
struct Object<internal::FieldImpl<Type2, Name, tag>, OtherFields...>::FieldTypeImpl<Name, Dummy> {
  using Type = Type2;
};

template <typename Type, typename Name, size_t tag, typename... OtherFields>
template <typename Dummy, char... field_name>
struct Object<internal::FieldImpl<Type, Name, tag>,
              OtherFields...>::FieldTypeImpl<tsdb2::common::TypeStringMatcher<field_name...>, Dummy>
    : public Object<OtherFields...>::template FieldTypeImpl<
          tsdb2::common::TypeStringMatcher<field_name...>> {};

template <typename Type, typename Name, size_t tag, typename... OtherFields>
template <typename Dummy>
struct Object<internal::FieldImpl<Type, Name, tag>, OtherFields...>::Getter<Name, Dummy> {
  explicit ABSL_ATTRIBUTE_ALWAYS_INLINE Getter(Object &obj) : value(obj.value_) {}
  ABSL_ATTRIBUTE_ALWAYS_INLINE Type &operator()() const noexcept { return value; }
  Type &value;
};

template <typename Type, typename Name, size_t tag, typename... OtherFields>
template <typename Dummy, char... field_name>
struct Object<internal::FieldImpl<Type, Name, tag>,
              OtherFields...>::Getter<tsdb2::common::TypeStringMatcher<field_name...>, Dummy>
    : public Object<OtherFields...>::template Getter<
          tsdb2::common::TypeStringMatcher<field_name...>> {
  explicit ABSL_ATTRIBUTE_ALWAYS_INLINE Getter(Object &obj)
      : Object<OtherFields...>::template Getter<tsdb2::common::TypeStringMatcher<field_name...>>(
            obj) {}
};

template <typename Type, typename Name, size_t tag, typename... OtherFields>
template <typename Dummy>
struct Object<internal::FieldImpl<Type, Name, tag>, OtherFields...>::ConstGetter<Name, Dummy> {
  explicit ABSL_ATTRIBUTE_ALWAYS_INLINE ConstGetter(Object const &obj) : value(obj.value_) {}
  ABSL_ATTRIBUTE_ALWAYS_INLINE Type const &operator()() const noexcept { return value; }
  Type const &value;
};

template <typename Type, typename Name, size_t tag, typename... OtherFields>
template <typename Dummy, char... field_name>
struct Object<internal::FieldImpl<Type, Name, tag>,
              OtherFields...>::ConstGetter<tsdb2::common::TypeStringMatcher<field_name...>, Dummy>
    : public Object<OtherFields...>::template ConstGetter<
          tsdb2::common::TypeStringMatcher<field_name...>> {
  explicit ABSL_ATTRIBUTE_ALWAYS_INLINE ConstGetter(Object const &obj)
      : Object<OtherFields...>::template ConstGetter<
            tsdb2::common::TypeStringMatcher<field_name...>>(obj) {}
};

template <typename... Fields>
class ObjectDecoder;

}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_OBJECT_H__
