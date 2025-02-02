#ifndef __TSDB2_PROTO_OBJECT_H__
#define __TSDB2_PROTO_OBJECT_H__

#include <cstddef>
#include <cstdint>
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
struct FieldDecoder<FieldImpl<std::vector<Type>, Name, tag>> {
  absl::Status operator()(Decoder *const decoder, WireType const wire_type,
                          std::vector<Type> *const value) const {
    if (wire_type != WireType::kLength) {
      Type decoded{};
      RETURN_IF_ERROR((FieldDecoder<FieldImpl<Type, Name, tag>>()(decoder, wire_type, &decoded)));
      value->emplace_back(std::move(decoded));
      return absl::OkStatus();
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
  }
};

}  // namespace internal

template <typename Type, char const name[], size_t tag>
using Field = internal::FieldImpl<Type, tsdb2::common::TypeStringT<name>, tag>;

struct Initialize {};
inline Initialize constexpr kInitialize{};

template <typename... Fields>
class Object;

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
      return internal::FieldDecoder<internal::FieldImpl<Type, Name, tag>>()(decoder, field_tag,
                                                                            &value_);
    }
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
