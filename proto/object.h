#ifndef __TSDB2_PROTO_OBJECT_H__
#define __TSDB2_PROTO_OBJECT_H__

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

#include "absl/base/attributes.h"
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
  static absl::StatusOr<Object<>> Decode(absl::Span<uint8_t const> const buffer) {
    Decoder decoder{buffer};
    while (!decoder.at_end()) {
      DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());
      RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
    }
    return Object<>();
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
  // NOLINTEND(readability-convert-member-functions-to-static)
};

template <typename Type, typename Name, size_t tag, typename... OtherFields>
class Object<internal::FieldImpl<Type, Name, tag>, OtherFields...> : public Object<OtherFields...> {
 public:
  template <typename FieldName, typename Dummy = void>
  struct FieldTypeImpl;

  template <char const field_name[]>
  using FieldType = typename FieldTypeImpl<tsdb2::common::TypeStringT<field_name>>::Type;

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

 private:
  Type value_;
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
