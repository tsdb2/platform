// Fast JSON parsing and serialization library.
//
// This implementation is faster than many others because our parsing results in a native C++ object
// rather than a hash map or other associative data structure of field names to values. Fields can
// then be accessed statically rather than looking up the field name in the dictionary.
//
// Use `tsdb2::json::Parse` to parse a JSON string and `tsdb2::json::Stringify` to serialize a value
// into a JSON string. Both functions have a template argument defining the format
// (`tsdb2::json::Stringify` can infer the template argument from the parameter type). The following
// data types are supported, both in parsing and serialization:
//
//   * bool,
//   * all signed and unsigned integer types,
//   * all floating point types,
//   * std::string,
//   * std::optional (serializes "null" if empty),
//   * std::pair,
//   * std::tuple,
//   * std::array,
//   * std::vector,
//   * std::set / std::unordered_set / absl::flat_hash_set / tsdb2::common::flat_set,
//   * std::map / std::unordered_map / absl::flat_hash_map / tsdb2::common::flat_map,
//   * tsdb2::json::Object,
//   * data types managed by raw pointer, std::unique_ptr, std::shared_ptr, or
//     tsdb2::common::reffed_ptr (serializes "null" if the pointer is null).
//
// Example:
//
//   // TODO
//
//
// NOTE: the root type to parse/stringify doesn't have to be an object or dictionary, it can be any
// supported data type. For example, "true" is a valid JSON string that you can (de)serialize with
// the data type `bool`:
//
//   bool value = true;
//   assert(tsdb2::json::Stringify(value) == "true");
//   ASSIGN_OR_RETURN(value, tsdb2::json::Parse<bool>("false"));
//   assert(value == false);
//

#ifndef __TSDB2_COMMON_JSON_H__
#define __TSDB2_COMMON_JSON_H__

#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/strip.h"
#include "common/flat_map.h"
#include "common/preprocessor.h"
#include "common/reffed_ptr.h"
#include "common/type_string.h"
#include "common/utilities.h"

inline std::string Tsdb2JsonStringify(std::string_view const value) {
  return absl::StrCat("\"", absl::CEscape(value), "\"");
}

inline std::string Tsdb2JsonStringify(bool const value) { return value ? "true" : "false"; }

template <typename Integer, std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
inline std::string Tsdb2JsonStringify(Integer const value) {
  return absl::StrCat(value);
}

template <typename Float, std::enable_if_t<std::is_floating_point_v<Float>, bool> = true>
inline std::string Tsdb2JsonStringify(Float const value) {
  return absl::StrCat(value);
}

inline std::string Tsdb2JsonStringify(std::nullptr_t) { return "null"; }

template <typename Pointee>
inline std::string Tsdb2JsonStringify(Pointee const* const value) {
  if (value) {
    return Tsdb2JsonStringify(*value);
  } else {
    return "null";
  }
}

template <typename Pointee>
inline std::string Tsdb2JsonStringify(std::unique_ptr<Pointee> const& value) {
  if (value) {
    return Tsdb2JsonStringify(*value);
  } else {
    return "null";
  }
}

template <typename Pointee>
inline std::string Tsdb2JsonStringify(std::shared_ptr<Pointee> const& value) {
  if (value) {
    return Tsdb2JsonStringify(*value);
  } else {
    return "null";
  }
}

template <typename Pointee>
inline std::string Tsdb2JsonStringify(tsdb2::common::reffed_ptr<Pointee> const& value) {
  if (value) {
    return Tsdb2JsonStringify(*value);
  } else {
    return "null";
  }
}

template <typename Element>
inline std::string Tsdb2JsonStringify(std::vector<Element> const& elements) {
  std::vector<std::string> strings;
  strings.reserve(elements.size());
  for (auto const& element : elements) {
    strings.emplace_back(Tsdb2JsonStringify(element));
  }
  return absl::StrCat("[", absl::StrJoin(strings, ","), "]");
}

template <typename Element, size_t count>
inline std::string Tsdb2JsonStringify(std::array<Element, count> const& elements) {
  std::vector<std::string> strings;
  strings.reserve(count);
  for (auto const& element : elements) {
    strings.emplace_back(Tsdb2JsonStringify(element));
  }
  return absl::StrCat("[", absl::StrJoin(strings, ","), "]");
}

template <typename... Elements>
inline std::string Tsdb2JsonStringify(std::tuple<Elements...> const& elements) {
  auto const strings = std::apply(
      [](Elements const&... elements) {
        return std::array<std::string, sizeof...(Elements)>{Tsdb2JsonStringify(elements)...};
      },
      elements);
  return absl::StrCat("[", absl::StrJoin(strings, ","), "]");
}

template <typename First, typename Second>
inline std::string Tsdb2JsonStringify(std::pair<First, Second> const& pair) {
  auto const first = Tsdb2JsonStringify(pair.first);
  auto const second = Tsdb2JsonStringify(pair.second);
  return absl::StrCat("[", first, ",", second, "]");
}

template <typename Value>
inline std::string Tsdb2JsonStringify(std::optional<Value> const& value) {
  if (value) {
    return Tsdb2JsonStringify(*value);
  } else {
    return "null";
  }
}

template <typename Dictionary>
inline std::string Tsdb2JsonStringifyDictionary(Dictionary const& dictionary) {
  std::vector<std::string> fields;
  fields.reserve(dictionary.size());
  for (auto const& [key, value] : dictionary) {
    fields.push_back(absl::StrCat("\"", absl::CEscape(key), "\":", Tsdb2JsonStringify(value)));
  }
  return absl::StrCat("{", absl::StrJoin(fields, ","), "}");
}

template <typename Key, typename Value, typename Compare, typename Allocator>
inline std::string Tsdb2JsonStringify(std::map<Key, Value, Compare, Allocator> const& value) {
  return Tsdb2JsonStringifyDictionary(value);
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
inline std::string Tsdb2JsonStringify(
    std::unordered_map<Key, Value, Hash, Equal, Allocator> const& value) {
  return Tsdb2JsonStringifyDictionary(value);
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
inline std::string Tsdb2JsonStringify(
    absl::flat_hash_map<Key, Value, Hash, Equal, Allocator> const& value) {
  return Tsdb2JsonStringifyDictionary(value);
}

template <typename Key, typename Value, typename Compare, typename Representation>
inline std::string Tsdb2JsonStringify(
    tsdb2::common::flat_map<Key, Value, Compare, Representation> const& value) {
  return Tsdb2JsonStringifyDictionary(value);
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

  absl::Status InvalidFormatError() const {
    // TODO: include row and column numbers in the error message.
    return absl::InvalidArgumentError("invalid format");
  }

  static bool IsDigit(char const ch) { return ch >= '0' && ch <= '9'; }

  static bool IsHexDigit(char const ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f');
  }

  static uint8_t ParseHexDigit(char const ch) {
    if (ch >= '0' && ch <= '9') {
      return ch - '0';
    } else if (ch >= 'A' && ch <= 'F') {
      return ch - 'A' + 10;
    } else {
      return ch - 'a' + 10;
    }
  }

  bool PeekDigit() const { return !input_.empty() && IsDigit(input_.front()); }

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

  template <typename Float, std::enable_if_t<std::is_floating_point_v<Float>, bool> = true>
  absl::Status ReadTo(Float* result);

  absl::Status ReadTo(std::string* result);

  template <typename Inner>
  absl::Status ReadTo(std::optional<Inner>* result);

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
  if (!PeekDigit()) {
    return InvalidSyntaxError();
  }
  uint8_t digit = input_.front() - '0';
  input_.remove_prefix(1);
  *result = digit;
  if (!digit) {
    return absl::OkStatus();
  }
  static Integer constexpr kMax = std::numeric_limits<Integer>::max();
  while (PeekDigit()) {
    if (*result > kMax / 10) {
      return InvalidFormatError();
    }
    *result *= 10;
    digit = input_.front() - '0';
    if (*result > kMax - digit) {
      return InvalidFormatError();
    }
    input_.remove_prefix(1);
    *result += digit;
  }
  *result *= sign;
  return absl::OkStatus();
}

template <typename Float, std::enable_if_t<std::is_floating_point_v<Float>, bool>>
absl::Status Parser::ReadTo(Float* const result) {
  ConsumeWhitespace();
  if (input_.empty()) {
    return InvalidSyntaxError();
  }
  int sign = 1;
  if (absl::ConsumePrefix(&input_, "-")) {
    sign = -1;
  }
  if (!PeekDigit()) {
    return InvalidSyntaxError();
  }
  uint8_t digit = input_.front() - '0';
  input_.remove_prefix(1);
  if (!digit) {
    *result = sign * 0.0;
    return absl::OkStatus();
  }
  int64_t mantissa = digit;
  static int64_t constexpr kMaxMantissa = std::numeric_limits<int64_t>::max();
  while (PeekDigit()) {
    if (mantissa > kMaxMantissa / 10) {
      return InvalidFormatError();
    }
    mantissa *= 10;
    digit = input_.front() - '0';
    if (mantissa > kMaxMantissa - digit) {
      return InvalidFormatError();
    }
    input_.remove_prefix(1);
    mantissa += digit;
  }
  int64_t fractional_digits = 0;
  if (absl::ConsumePrefix(&input_, ".")) {
    if (!PeekDigit()) {
      return InvalidSyntaxError();
    }
    ++fractional_digits;
    if (mantissa > kMaxMantissa / 10) {
      return InvalidFormatError();
    }
    mantissa *= 10;
    digit = input_.front() - '0';
    if (mantissa > kMaxMantissa - digit) {
      return InvalidFormatError();
    }
    input_.remove_prefix(1);
    mantissa += digit;
    while (PeekDigit()) {
      ++fractional_digits;
      if (mantissa > kMaxMantissa / 10) {
        return InvalidFormatError();
      }
      mantissa *= 10;
      digit = input_.front() - '0';
      if (mantissa > kMaxMantissa - digit) {
        return InvalidFormatError();
      }
      input_.remove_prefix(1);
      mantissa += digit;
    }
  }
  int exponent = 0;
  if (absl::ConsumePrefix(&input_, "E") || absl::ConsumePrefix(&input_, "e")) {
    int exponent_sign = 1;
    if (absl::ConsumePrefix(&input_, "-")) {
      exponent_sign = -1;
    } else {
      absl::ConsumePrefix(&input_, "+");
    }
    if (!PeekDigit()) {
      return InvalidSyntaxError();
    }
    exponent = exponent_sign * (input_.front() - '0');
    input_.remove_prefix(1);
    static int constexpr kMinExponent = std::numeric_limits<Float>::min_exponent10;
    static int constexpr kMaxExponent = std::numeric_limits<Float>::max_exponent10;
    int const min_exponent = kMinExponent - fractional_digits;
    int const max_exponent = kMaxExponent - fractional_digits;
    while (PeekDigit()) {
      if (exponent_sign < 0) {
        if (exponent < min_exponent / 10) {
          return InvalidFormatError();
        }
      } else {
        if (exponent > max_exponent / 10) {
          return InvalidFormatError();
        }
      }
      exponent *= 10;
      int8_t const digit = input_.front() - '0';
      input_.remove_prefix(1);
      if (exponent_sign < 0) {
        if (exponent < min_exponent - digit) {
          return InvalidFormatError();
        }
        exponent -= digit;
      } else {
        if (exponent > max_exponent + digit) {
          return InvalidFormatError();
        }
        exponent += digit;
      }
    }
  }
  exponent -= fractional_digits;
  *result =
      sign * mantissa * std::pow(static_cast<long double>(10), static_cast<long double>(exponent));
  return absl::OkStatus();
}

template <typename Inner>
absl::Status Parser::ReadTo(std::optional<Inner>* const result) {
  if (absl::ConsumePrefix(&input_, "null")) {
    *result = std::nullopt;
    return absl::OkStatus();
  } else {
    result->emplace();
    return ReadTo<Inner>(&result->value());
  }
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
