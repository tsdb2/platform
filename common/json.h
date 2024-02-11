#ifndef __TSDB2_COMMON_JSON_H__
#define __TSDB2_COMMON_JSON_H__

#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/preprocessor.h"
#include "common/type_string.h"

namespace tsdb2 {
namespace common {
namespace json {

template <typename Type, typename Name>
struct Field;

template <typename Type, char... ch>
struct Field<Type, TypeStringMatcher<ch...>> {
  using Name = TypeStringMatcher<ch...>;
  static std::string_view constexpr name = Name::value;
};

#define _TSDB2_JSON_GET_FIELD_NAME(Type, name) name

#define _TSDB2_JSON_FIELD_NAME_IMPL(ObjectName, field_name)                                 \
  static ::std::string_view constexpr TSDB2_PP_CAT(                                         \
      k, TSDB2_PP_CAT(ObjectName, TSDB2_PP_CAT(_, TSDB2_PP_CAT(field_name, _FieldName)))) = \
      TSDB2_PP_STRINGIFY(field_name);

#define _TSDB2_JSON_FIELD_NAME(ObjectName, field) \
  _TSDB2_JSON_FIELD_NAME_IMPL(ObjectName, _TSDB2_JSON_GET_FIELD_NAME field)

#define _TSDB2_JSON_FIELD_NAMES(ObjectName, ...) \
  TSDB2_PP_FOR_EACH(_TSDB2_JSON_FIELD_NAME, ObjectName, __VA_ARGS__)

#define _TSDB2_JSON_FIELD_UNPACKED(Type, name) Type name;
#define _TSDB2_JSON_FIELD(_, field) _TSDB2_JSON_FIELD_UNPACKED field
#define _TSDB2_JSON_FIELDS(...) TSDB2_PP_FOR_EACH(_TSDB2_JSON_FIELD, _, __VA_ARGS__)

#define _TSDB2_JSON_OBJECT(Name, ...)        \
  _TSDB2_JSON_FIELD_NAMES(Name, __VA_ARGS__) \
  struct Name {                              \
    _TSDB2_JSON_FIELDS(__VA_ARGS__)          \
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
    // TODO
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
  // TODO
}

}  // namespace internal

template <typename Value>
absl::StatusOr<Value> Parse(std::string_view const input) {
  return internal::Parser(input).Parse<Value>();
}

}  // namespace json
}  // namespace common

using namespace common::json;

}  // namespace tsdb2

#endif  // __TSDB2_COMMON_JSON_H__
