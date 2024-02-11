#ifndef __TSDB2_COMMON_JSON_H__
#define __TSDB2_COMMON_JSON_H__

#include <string>
#include <string_view>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/strip.h"
#include "common/preprocessor.h"
#include "common/type_string.h"

namespace tsdb2 {
namespace common {
namespace json {

namespace internal {

template <typename Type, typename Name>
struct FieldImpl;

template <typename Type, char... ch>
struct FieldImpl<Type, TypeStringMatcher<ch...>> {
  using Name = TypeStringMatcher<ch...>;
  static std::string_view constexpr name = Name::value;
};

}  // namespace internal

template <typename Type, char const name[]>
using Field = internal::FieldImpl<Type, TypeStringT<name>>;

template <typename... Fields>
class Object;

template <>
class Object<> {};

template <typename Type, char... name, typename... OtherFields>
class Object<internal::FieldImpl<Type, TypeStringMatcher<name...>>, OtherFields...>
    : public Object<OtherFields...> {
 public:
  template <char const field_name[]>
  auto& get() {
    return Getter<TypeStringT<field_name>>(*this)();
  }

  template <char const field_name[]>
  auto const& get() const {
    return ConstGetter<TypeStringT<field_name>>(*this)();
  }

 protected:
  template <typename FieldName>
  struct Getter;

  template <>
  struct Getter<TypeStringMatcher<name...>> {
    explicit ABSL_ATTRIBUTE_ALWAYS_INLINE Getter(Object& obj) : value(obj.value_) {}
    ABSL_ATTRIBUTE_ALWAYS_INLINE Type& operator()() const { return value; }
    Type& value;
  };

  template <char... field_name>
  struct Getter<TypeStringMatcher<field_name...>>
      : public Object<OtherFields...>::template Getter<TypeStringMatcher<field_name...>> {
    explicit ABSL_ATTRIBUTE_ALWAYS_INLINE Getter(Object& obj)
        : Object<OtherFields...>::template Getter<TypeStringMatcher<field_name...>>(obj) {}
  };

  template <typename FieldName>
  struct ConstGetter;

  template <>
  struct ConstGetter<TypeStringMatcher<name...>> {
    explicit ABSL_ATTRIBUTE_ALWAYS_INLINE ConstGetter(Object const& obj) : value(obj.value_) {}
    ABSL_ATTRIBUTE_ALWAYS_INLINE Type const& operator()() const { return value; }
    Type const& value;
  };

  template <char... field_name>
  struct ConstGetter<TypeStringMatcher<field_name...>>
      : public Object<OtherFields...>::template ConstGetter<TypeStringMatcher<field_name...>> {
    explicit ABSL_ATTRIBUTE_ALWAYS_INLINE ConstGetter(Object const& obj)
        : Object<OtherFields...>::template ConstGetter<TypeStringMatcher<field_name...>>(obj) {}
  };

 private:
  Type value_;
};

#define _TSDB2_JSON_GET_FIELD_NAME(Type, name) name

#define _TSDB2_JSON_FIELD_NAME_SYMBOL(ObjectName, field_name) \
  TSDB2_PP_CAT(k, TSDB2_PP_CAT(ObjectName, TSDB2_PP_CAT(_, TSDB2_PP_CAT(field_name, _FieldName))))

#define _TSDB2_JSON_FIELD_NAME_IMPL(ObjectName, field_name)                       \
  static char constexpr _TSDB2_JSON_FIELD_NAME_SYMBOL(ObjectName, field_name)[] = \
      TSDB2_PP_STRINGIFY(field_name);

#define _TSDB2_JSON_FIELD_NAME(ObjectName, field) \
  _TSDB2_JSON_FIELD_NAME_IMPL(ObjectName, _TSDB2_JSON_GET_FIELD_NAME field)

#define _TSDB2_JSON_FIELD_NAMES(ObjectName, ...) \
  TSDB2_PP_FOR_EACH(_TSDB2_JSON_FIELD_NAME, ObjectName, __VA_ARGS__)

#define _TSDB2_JSON_FIELD_IMPL(Type, name) Type name;
#define _TSDB2_JSON_FIELD(_, field) _TSDB2_JSON_FIELD_IMPL field
#define _TSDB2_JSON_FIELDS(...) TSDB2_PP_FOR_EACH(_TSDB2_JSON_FIELD, _, __VA_ARGS__)

#define _TSDB2_JSON_OBJECT(Name, ...)                          \
  _TSDB2_JSON_FIELD_NAMES(Name, __VA_ARGS__)                   \
  struct Name {                                                \
    friend std::string Tsdb2JsonStringify(Name const& value) { \
      return absl::StrCat("{", /* TODO */ "}");                \
    }                                                          \
    _TSDB2_JSON_FIELDS(__VA_ARGS__)                            \
  };

#define JSON_OBJECT _TSDB2_JSON_OBJECT

namespace internal {

class Parser {
 public:
  explicit Parser(std::string_view const input) : input_(input) {}

  template <typename Value>
  absl::StatusOr<Value> Parse();

 private:
  absl::Status InvalidSyntaxError() const {
    // TODO: include row and column numbers in the error message.
    return absl::InvalidArgumentError("invalid JSON syntax");
  }

  void ConsumeWhitespace() {
    size_t offset = 0;
    while (offset < input_.size() && absl::ascii_isspace(input_[offset])) {
      ++offset;
    }
    input_.remove_prefix(offset);
  }

  template <typename Value>
  absl::StatusOr<Value> Read();

  std::string_view input_;
};

template <typename Value>
absl::StatusOr<Value> Parser::Parse() {
  auto status_or_value = Read<Value>();
  if (!status_or_value.ok()) {
    return std::move(status_or_value).status();
  }
  ConsumeWhitespace();
  if (!input_.empty()) {
    return InvalidSyntaxError();
  }
  return status_or_value;
}

template <>
absl::StatusOr<bool> Parser::Read<bool>() {
  ConsumeWhitespace();
  if (absl::ConsumePrefix(&input_, "true")) {
    if (!input_.empty() && absl::ascii_isalnum(input_[0])) {
      return InvalidSyntaxError();
    }
    return true;
  } else if (absl::ConsumePrefix(&input_, "false")) {
    if (!input_.empty() && absl::ascii_isalnum(input_[0])) {
      return InvalidSyntaxError();
    }
    return false;
  } else {
    return InvalidSyntaxError();
  }
}

}  // namespace internal

template <typename Value>
absl::StatusOr<Value> Parse(std::string_view const input) {
  return internal::Parser(input).Parse<Value>();
}

}  // namespace json
}  // namespace common

namespace json = common::json;

}  // namespace tsdb2

#endif  // __TSDB2_COMMON_JSON_H__
