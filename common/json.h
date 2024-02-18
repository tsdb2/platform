#ifndef __TSDB2_COMMON_JSON_H__
#define __TSDB2_COMMON_JSON_H__

#include <array>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/strip.h"
#include "common/preprocessor.h"
#include "common/type_string.h"
#include "common/utilities.h"

std::string Tsdb2JsonStringify(std::string_view const value) {
  return absl::StrCat("\"", absl::CEscape(value), "\"");
}

std::string Tsdb2JsonStringify(bool const value) { return value ? "true" : "false"; }

template <typename Integer>
std::string Tsdb2JsonStringify(
    Integer const value, std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true) {
  return absl::StrCat(value);
}

template <typename Float>
std::string Tsdb2JsonStringify(Float const value,
                               std::enable_if_t<std::is_floating_point_v<Float>, bool> = true) {
  return absl::StrCat(value);
}

std::string Tsdb2JsonStringify(std::nullptr_t) { return "null"; }

template <typename Pointee>
std::string Tsdb2JsonStringify(Pointee const* const value) {
  if (value) {
    return Tsdb2JsonStringify(*value);
  } else {
    return "null";
  }
}

template <typename Element>
std::string Tsdb2JsonStringify(std::vector<Element> const& elements) {
  std::vector<std::string> strings;
  strings.reserve(elements.size());
  for (auto const& element : elements) {
    strings.emplace_back(Tsdb2JsonStringify(element));
  }
  return absl::StrCat("[", absl::StrJoin(strings, ","), "]");
}

template <typename Element, size_t count>
std::string Tsdb2JsonStringify(std::array<Element, count> const& elements) {
  std::vector<std::string> strings;
  strings.reserve(count);
  for (auto const& element : elements) {
    strings.emplace_back(Tsdb2JsonStringify(element));
  }
  return absl::StrCat("[", absl::StrJoin(strings, ","), "]");
}

template <typename... Elements>
std::string Tsdb2JsonStringify(std::tuple<Elements...> const& elements) {
  auto const strings = std::apply(
      [](Elements const&... elements) {
        return std::array<std::string, sizeof...(Elements)>{Tsdb2JsonStringify(elements)...};
      },
      elements);
  return absl::StrCat("[", absl::StrJoin(strings, ","), "]");
}

template <typename First, typename Second>
std::string Tsdb2JsonStringify(std::pair<First, Second> const& pair) {
  auto first = Tsdb2JsonStringify(pair.first);
  auto second = Tsdb2JsonStringify(pair.second);
  return absl::StrCat("[", std::move(first), ",", std::move(second), "]");
}

template <typename Value>
std::string Tsdb2JsonStringify(std::optional<Value> const& value) {
  if (value) {
    return Tsdb2JsonStringify(*value);
  } else {
    return "null";
  }
}

template <typename... Variants>
std::string Tsdb2JsonStringify(std::variant<Variants...> const& value) {
  return std::visit([](auto const& variant) { return Tsdb2JsonStringify(variant); }, value);
}

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
class Object<> {
 protected:
  void StringifyInternal(std::vector<std::string>*) const {}
};

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

  std::string Stringify() const {
    std::vector<std::string> fields;
    fields.reserve(sizeof...(OtherFields) + 1);
    StringifyInternal(&fields);
    return absl::StrCat("{", absl::StrJoin(fields, ","), "}");
  }

  friend std::string Tsdb2JsonStringify(Object const& value) { return value.Stringify(); }

 protected:
  template <typename FieldName, typename Dummy = void>
  struct Getter;

  template <typename FieldName, typename Dummy = void>
  struct ConstGetter;

  void StringifyInternal(std::vector<std::string>* const fields) const {
    fields->emplace_back(absl::StrCat("\"", absl::CEscape(TypeStringMatcher<name...>::value),
                                      "\":", Tsdb2JsonStringify(value_)));
    Object<OtherFields...>::StringifyInternal(fields);
  }

 private:
  Type value_;
};

template <typename Type, char... name, typename... OtherFields>
template <typename Dummy>
struct Object<internal::FieldImpl<Type, TypeStringMatcher<name...>>,
              OtherFields...>::Getter<TypeStringMatcher<name...>, Dummy> {
  explicit ABSL_ATTRIBUTE_ALWAYS_INLINE Getter(Object& obj) : value(obj.value_) {}
  ABSL_ATTRIBUTE_ALWAYS_INLINE Type& operator()() const { return value; }
  Type& value;
};

template <typename Type, char... name, typename... OtherFields>
template <typename Dummy, char... field_name>
struct Object<internal::FieldImpl<Type, TypeStringMatcher<name...>>,
              OtherFields...>::Getter<TypeStringMatcher<field_name...>, Dummy>
    : public Object<OtherFields...>::template Getter<TypeStringMatcher<field_name...>> {
  explicit ABSL_ATTRIBUTE_ALWAYS_INLINE Getter(Object& obj)
      : Object<OtherFields...>::template Getter<TypeStringMatcher<field_name...>>(obj) {}
};

template <typename Type, char... name, typename... OtherFields>
template <typename Dummy>
struct Object<internal::FieldImpl<Type, TypeStringMatcher<name...>>,
              OtherFields...>::ConstGetter<TypeStringMatcher<name...>, Dummy> {
  explicit ABSL_ATTRIBUTE_ALWAYS_INLINE ConstGetter(Object const& obj) : value(obj.value_) {}
  ABSL_ATTRIBUTE_ALWAYS_INLINE Type const& operator()() const { return value; }
  Type const& value;
};

template <typename Type, char... name, typename... OtherFields>
template <typename Dummy, char... field_name>
struct Object<internal::FieldImpl<Type, TypeStringMatcher<name...>>,
              OtherFields...>::ConstGetter<TypeStringMatcher<field_name...>, Dummy>
    : public Object<OtherFields...>::template ConstGetter<TypeStringMatcher<field_name...>> {
  explicit ABSL_ATTRIBUTE_ALWAYS_INLINE ConstGetter(Object const& obj)
      : Object<OtherFields...>::template ConstGetter<TypeStringMatcher<field_name...>>(obj) {}
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

  absl::Status ReadTo(bool* result);

  template <typename Integer,
            std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
  absl::Status ReadTo(Integer* result);

  std::string_view input_;
};

template <typename Value>
absl::StatusOr<Value> Parser::Parse() {
  Value value;
  RETURN_IF_ERROR(ReadTo(&value));
  ConsumeWhitespace();
  if (!input_.empty()) {
    return InvalidSyntaxError();
  }
  return value;
}

absl::Status Parser::ReadTo(bool* const result) {
  ConsumeWhitespace();
  if (absl::ConsumePrefix(&input_, "true")) {
    if (!input_.empty() && absl::ascii_isalnum(input_[0])) {
      return InvalidSyntaxError();
    }
    *result = true;
    return absl::OkStatus();
  } else if (absl::ConsumePrefix(&input_, "false")) {
    if (!input_.empty() && absl::ascii_isalnum(input_[0])) {
      return InvalidSyntaxError();
    }
    *result = false;
    return absl::OkStatus();
  } else {
    return InvalidSyntaxError();
  }
}

template <typename Integer, std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool>>
absl::Status Parser::ReadTo(Integer* const result) {
  ConsumeWhitespace();
  if (input_.empty()) {
    return InvalidSyntaxError();
  }
  int sign = 1;
  if (absl::ConsumePrefix(&input_, "-")) {
    if constexpr (!std::is_signed_v<Integer>) {
      return InvalidSyntaxError();
    }
    sign = -1;
  }
  if (input_.empty() || !std::isdigit(input_[0])) {
    return InvalidSyntaxError();
  }
  Integer number = input_[0] - '0';
  input_.remove_prefix(1);
  if (!number) {
    *result = 0;
    return absl::OkStatus();
  }
  while (!input_.empty() && std::isdigit(input_[0])) {
    number = number * 10 + (input_[0] - '0');
    input_.remove_prefix(1);
  }
  *result = number * sign;
  return absl::OkStatus();
}

}  // namespace internal

template <typename Value>
absl::StatusOr<Value> Parse(std::string_view const input) {
  return internal::Parser(input).Parse<Value>();
}

template <typename Value>
std::string Stringify(Value&& value) {
  return Tsdb2JsonStringify(std::forward<Value>(value));
}

}  // namespace json
}  // namespace common

namespace json = common::json;

}  // namespace tsdb2

#endif  // __TSDB2_COMMON_JSON_H__
