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
//   * std::set / std::unordered_set / tsdb2::common::flat_set / absl::btree_set /
//     absl::flat_hash_set / absl::node_hash_set,
//   * std::map / std::unordered_map / tsdb2::common::flat_map / absl::btree_map /
//     absl::flat_hash_map / absl::node_hash_map,
//   * tsdb2::json::Object,
//   * data types managed by std::unique_ptr or std::shared_ptr (serializing to "null" if the
//     pointer is null).
//
// Example:
//
//   char constexpr kFieldName1[] = "lorem";
//   char constexpr kFieldName2[] = "ipsum";
//   char constexpr kFieldName3[] = "dolor";
//   char constexpr kFieldName4[] = "sit";
//   char constexpr kFieldName5[] = "amet";
//   char constexpr kFieldName6[] = "consectetur";
//   char constexpr kFieldName7[] = "adipisci";
//   char constexpr kFieldName8[] = "elit";
//
//   using TestObject = json::Object<
//       json::Field<int, kFieldName1>,
//       json::Field<bool, kFieldName2>,
//       json::Field<std::string, kFieldName3>,
//       json::Field<double, kFieldName4>,
//       json::Field<std::vector<int>, kFieldName5>,
//       json::Field<std::tuple<int, bool, std::string>, kFieldName6>,
//       json::Field<std::optional<double>, kFieldName7>,
//       json::Field<std::unique_ptr<std::string>, kFieldName8>>;
//
//   auto const status_or_object = tsdb2::json::Parse<TestObject>(R"json({
//         "lorem": 42,
//         "ipsum": true,
//         "dolor": "foobar",
//         "sit": 3.14,
//         "amet": [1, 2, 3],
//         "consectetur": [43, false, "barbaz"],
//         "adipisci": 2.71,
//         "elit": "bazqux"
//       })json");
//   assert(status_or_object.ok());
//
//   auto const& object = status_or_object.value();
//   assert(object.get<kFieldName1>() == 42);
//   assert(object.get<kFieldName2>() == true);
//   assert(object.get<kFieldName3>() == "foobar");
//   assert(object.get<kFieldName4>() == 3.14);
//   assert(object.get<kFieldName5>() == std::vector<int>{1, 2, 3});
//   assert(object.get<kFieldName6>() == std::make_tuple(43, false, std::string("barbaz")));
//   assert(object.get<kFieldName7>().value() == 2.71);
//   assert(*(object.get<kFieldName8>()) == "bazqux");
//
//
// NOTE: this JSON framework supports parsing and serializing both `tsdb2::json::Object` objects and
// associative containers (std::map, std::unordered_map, absl::flat_hash_map, etc.). The tradeoff
// between the two approaches is a tradeoff between compilation performance and runtime performance:
// at runtime, the fields of a `tsdb2::json::Object` instance can be accessed very efficiently with
// a single memory lookup, but `tsdb2::json::Object` is a template class that relies on type strings
// and therefore very slow to compile. By contrast, various associative containers compile fast but
// have various runtime performance characteristics that are necessarily worse than
// `tsdb::json::Object` (at a minimum, looking up a field requires scanning the full name string).
// Choose the approach that suits your use case best.
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
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#include "absl/container/node_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/strip.h"
#include "common/flat_map.h"
#include "common/flat_set.h"
#include "common/type_string.h"
#include "common/utilities.h"

namespace tsdb2 {
namespace common {
namespace json {
namespace internal {

std::string EscapeAndQuoteString(std::string_view input);

}  // namespace internal
}  // namespace json
}  // namespace common
}  // namespace tsdb2

inline std::string Tsdb2JsonStringify(char const value[]) {
  return tsdb2::common::json::internal::EscapeAndQuoteString(value);
}

inline std::string Tsdb2JsonStringify(std::string_view const value) {
  return tsdb2::common::json::internal::EscapeAndQuoteString(value);
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
    fields.emplace_back(absl::StrCat(tsdb2::common::json::internal::EscapeAndQuoteString(key), ":",
                                     Tsdb2JsonStringify(value)));
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

template <typename Key, typename Value, typename Compare, typename Allocator>
inline std::string Tsdb2JsonStringify(
    absl::btree_map<Key, Value, Compare, Allocator> const& value) {
  return Tsdb2JsonStringifyDictionary(value);
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
inline std::string Tsdb2JsonStringify(
    absl::flat_hash_map<Key, Value, Hash, Equal, Allocator> const& value) {
  return Tsdb2JsonStringifyDictionary(value);
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
inline std::string Tsdb2JsonStringify(
    absl::node_hash_map<Key, Value, Hash, Equal, Allocator> const& value) {
  return Tsdb2JsonStringifyDictionary(value);
}

template <typename Key, typename Value, typename Compare, typename Representation>
inline std::string Tsdb2JsonStringify(
    tsdb2::common::flat_map<Key, Value, Compare, Representation> const& value) {
  return Tsdb2JsonStringifyDictionary(value);
}

template <typename Set>
inline std::string Tsdb2JsonStringifySet(Set const& set) {
  std::vector<std::string> elements;
  elements.reserve(set.size());
  for (auto const& element : set) {
    elements.emplace_back(Tsdb2JsonStringify(element));
  }
  return absl::StrCat("[", absl::StrJoin(elements, ","), "]");
}

template <typename Element, typename Compare, typename Allocator>
inline std::string Tsdb2JsonStringify(std::set<Element, Compare, Allocator> const& value) {
  return Tsdb2JsonStringifySet(value);
}

template <typename Element, typename Hash, typename Equal, typename Allocator>
inline std::string Tsdb2JsonStringify(
    std::unordered_set<Element, Hash, Equal, Allocator> const& value) {
  return Tsdb2JsonStringifySet(value);
}

template <typename Element, typename Compare, typename Allocator>
inline std::string Tsdb2JsonStringify(absl::btree_set<Element, Compare, Allocator> const& value) {
  return Tsdb2JsonStringifySet(value);
}

template <typename Element, typename Hash, typename Equal, typename Allocator>
inline std::string Tsdb2JsonStringify(
    absl::flat_hash_set<Element, Hash, Equal, Allocator> const& value) {
  return Tsdb2JsonStringifySet(value);
}

template <typename Element, typename Hash, typename Equal, typename Allocator>
inline std::string Tsdb2JsonStringify(
    absl::node_hash_set<Element, Hash, Equal, Allocator> const& value) {
  return Tsdb2JsonStringifySet(value);
}

template <typename Element, typename Compare, typename Representation>
inline std::string Tsdb2JsonStringify(
    tsdb2::common::flat_set<Element, Compare, Representation> const& value) {
  return Tsdb2JsonStringifySet(value);
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
  static inline std::string_view constexpr name = Name::value;
};

// Checks that none of `Fields` has the same name as `Name`. `Fields` are `FieldImpl`s and `Name`
// must be a `TypeStringMatcher`.
//
// This is used by `Object` to make sure there aren't duplicate names.
template <typename Name, typename... Fields>
struct CheckUniqueName;

template <char... name>
struct CheckUniqueName<TypeStringMatcher<name...>> {
  static inline bool constexpr value = true;
};

template <typename Name, typename Type, typename... OtherFields>
struct CheckUniqueName<Name, FieldImpl<Type, Name>, OtherFields...> {
  static inline bool constexpr value = false;
};

template <typename FieldName, typename Type, typename Name, typename... OtherFields>
struct CheckUniqueName<FieldName, FieldImpl<Type, Name>, OtherFields...> {
  static inline bool constexpr value = CheckUniqueName<FieldName, OtherFields...>::value;
};

template <typename Name, typename... Fields>
inline bool constexpr CheckUniqueNameV = CheckUniqueName<Name, Fields...>::value;

class Parser;

}  // namespace internal

// Fields for `Object` (see below).
template <typename Type, char const name[]>
using Field = internal::FieldImpl<Type, TypeStringT<name>>;

// Represents an object that can be parsed from JSON and serialized to JSON. The template arguments
// must be `Field` types describing the fields in the object. We use `TypeString`s to specify field
// names along with their types in the template instantiation.
//
// Example:
//
//   namespace json = tsdb2::json;
//
//   char constexpr kFieldName1[] = "lorem";
//   char constexpr kFieldName2[] = "ipsum";
//   char constexpr kFieldName3[] = "dolor";
//   char constexpr kFieldName4[] = "sit";
//   char constexpr kFieldName5[] = "amet";
//   char constexpr kFieldName6[] = "consectetur";
//   char constexpr kFieldName7[] = "adipisci";
//   char constexpr kFieldName8[] = "elit";
//
//   using TestObject = json::Object<
//       json::Field<int, kFieldName1>,
//       json::Field<bool, kFieldName2>,
//       json::Field<std::string, kFieldName3>,
//       json::Field<double, kFieldName4>,
//       json::Field<std::vector<int>, kFieldName5>,
//       json::Field<std::tuple<int, bool, std::string>, kFieldName6>,
//       json::Field<std::optional<double>, kFieldName7>,
//       json::Field<std::unique_ptr<std::string>, kFieldName8>>;
//
// Fields can then be accessed efficiently using the `get` function template:
//
//   TestObject obj;
//   obj.get<kFieldName1>() = 42;
//   assert(obj.get<kFieldName1>() == 42);
//
// `Object` also provides all comparison operators (==, !=, <, <=, >, and >=) and a specialization
// of `AbslHashValue`, making it suitable for most containers.
template <typename... Fields>
class Object;

template <>
class Object<> {
 public:
  explicit Object() = default;

  Object(Object const&) = default;
  Object& operator=(Object const&) = default;
  Object(Object&&) noexcept = default;
  Object& operator=(Object&&) noexcept = default;

  void swap(Object& other) noexcept {}
  friend void swap(Object& lhs, Object& rhs) noexcept {}

  bool operator==(Object const& other) const { return true; }
  bool operator!=(Object const& other) const { return false; }
  bool operator<(Object const& other) const { return false; }
  bool operator<=(Object const& other) const { return true; }
  bool operator>(Object const& other) const { return false; }
  bool operator>=(Object const& other) const { return true; }

  template <typename H>
  friend H AbslHashValue(H h, Object const& value) {
    return h;
  }

  void Clear() {}

  std::string Stringify() const { return "{}"; }

  friend std::string Tsdb2JsonStringify(Object const& value) { return value.Stringify(); }

 protected:
  friend class internal::Parser;

  ABSL_ATTRIBUTE_ALWAYS_INLINE void SwapInternal(Object& other) {}

  ABSL_ATTRIBUTE_ALWAYS_INLINE void ClearInternal() {}

  ABSL_ATTRIBUTE_ALWAYS_INLINE absl::Status ReadField(internal::Parser* const parser,
                                                      std::string_view const field_name);

  ABSL_ATTRIBUTE_ALWAYS_INLINE void StringifyInternal(std::vector<std::string>*) const {}
};

template <typename Type, typename Name, typename... OtherFields>
class Object<internal::FieldImpl<Type, Name>, OtherFields...> : public Object<OtherFields...> {
 public:
  static_assert(internal::CheckUniqueNameV<Name, OtherFields...>, "duplicate field names");

  explicit Object() = default;

  Object(Object const&) = default;
  Object& operator=(Object const&) = default;

  Object(Object&&) noexcept = default;
  Object& operator=(Object&&) noexcept = default;

  void swap(Object& other) noexcept { SwapInternal(other); }
  friend void swap(Object& lhs, Object& rhs) noexcept { lhs.SwapInternal(rhs); }

  bool operator==(Object const& other) const {
    return value_ == other.value_ && Object<OtherFields...>::operator==(other);
  }

  bool operator!=(Object const& other) const {
    return value_ != other.value_ || Object<OtherFields...>::operator!=(other);
  }

  bool operator<(Object const& other) const {
    return value_ < other.value_ ||
           (value_ == other.value_ && Object<OtherFields...>::operator<(other));
  }

  bool operator<=(Object const& other) const { return !(other < *this); }

  bool operator>(Object const& other) const { return other < *this; }

  bool operator>=(Object const& other) const { return !(*this < other); }

  template <typename H>
  friend H AbslHashValue(H h, Object const& value) {
    return H::combine(std::move(h), value.value_,
                      ConstGetter<typename OtherFields::Name>(value)()...);
  }

  template <char const field_name[]>
  auto& get() & {
    return Getter<TypeStringT<field_name>>(*this)();
  }

  template <char const field_name[]>
  auto&& get() && {
    return std::move(Getter<TypeStringT<field_name>>(*this)());
  }

  template <char const field_name[]>
  auto const& get() const& {
    return ConstGetter<TypeStringT<field_name>>(*this)();
  }

  void Clear() { ClearInternal(); }

  std::string Stringify() const {
    std::vector<std::string> fields;
    fields.reserve(sizeof...(OtherFields) + 1);
    StringifyInternal(&fields);
    return absl::StrCat("{", absl::StrJoin(fields, ","), "}");
  }

  friend std::string Tsdb2JsonStringify(Object const& value) { return value.Stringify(); }

 protected:
  friend class internal::Parser;

  template <typename FieldName, typename Dummy = void>
  struct Getter;

  template <typename FieldName, typename Dummy = void>
  struct ConstGetter;

  ABSL_ATTRIBUTE_ALWAYS_INLINE void SwapInternal(Object& other) {
    Object<OtherFields...>::SwapInternal(other);
    using std::swap;  // ensure ADL
    swap(value_, other.value_);
  }

  ABSL_ATTRIBUTE_ALWAYS_INLINE void ClearInternal() {
    Object<OtherFields...>::ClearInternal();
    value_ = Type();
  }

  ABSL_ATTRIBUTE_ALWAYS_INLINE absl::Status ReadField(internal::Parser* const parser,
                                                      std::string_view const field_name);

  ABSL_ATTRIBUTE_ALWAYS_INLINE void StringifyInternal(
      std::vector<std::string>* const fields) const {
    fields->emplace_back(
        absl::StrCat(internal::EscapeAndQuoteString(Name::value), ":", Tsdb2JsonStringify(value_)));
    Object<OtherFields...>::StringifyInternal(fields);
  }

 private:
  Type value_;
};

template <typename Type, typename Name, typename... OtherFields>
template <typename Dummy>
struct Object<internal::FieldImpl<Type, Name>, OtherFields...>::Getter<Name, Dummy> {
  explicit ABSL_ATTRIBUTE_ALWAYS_INLINE Getter(Object& obj) : value(obj.value_) {}
  ABSL_ATTRIBUTE_ALWAYS_INLINE Type& operator()() const { return value; }
  Type& value;
};

template <typename Type, typename Name, typename... OtherFields>
template <typename Dummy, char... field_name>
struct Object<internal::FieldImpl<Type, Name>,
              OtherFields...>::Getter<TypeStringMatcher<field_name...>, Dummy>
    : public Object<OtherFields...>::template Getter<TypeStringMatcher<field_name...>> {
  explicit ABSL_ATTRIBUTE_ALWAYS_INLINE Getter(Object& obj)
      : Object<OtherFields...>::template Getter<TypeStringMatcher<field_name...>>(obj) {}
};

template <typename Type, typename Name, typename... OtherFields>
template <typename Dummy>
struct Object<internal::FieldImpl<Type, Name>, OtherFields...>::ConstGetter<Name, Dummy> {
  explicit ABSL_ATTRIBUTE_ALWAYS_INLINE ConstGetter(Object const& obj) : value(obj.value_) {}
  ABSL_ATTRIBUTE_ALWAYS_INLINE Type const& operator()() const { return value; }
  Type const& value;
};

template <typename Type, typename Name, typename... OtherFields>
template <typename Dummy, char... field_name>
struct Object<internal::FieldImpl<Type, Name>,
              OtherFields...>::ConstGetter<TypeStringMatcher<field_name...>, Dummy>
    : public Object<OtherFields...>::template ConstGetter<TypeStringMatcher<field_name...>> {
  explicit ABSL_ATTRIBUTE_ALWAYS_INLINE ConstGetter(Object const& obj)
      : Object<OtherFields...>::template ConstGetter<TypeStringMatcher<field_name...>>(obj) {}
};

namespace internal {

class Parser {
 public:
  explicit Parser(std::string_view const input) : input_(input) {}

  template <typename Value>
  absl::StatusOr<Value> Parse();

 private:
  template <typename... Fields>
  friend class ::tsdb2::common::json::Object;

  absl::Status InvalidSyntaxError() const {
    // TODO: include row and column numbers in the error message.
    return absl::InvalidArgumentError("invalid JSON syntax");
  }

  absl::Status InvalidFormatError() const {
    // TODO: include row and column numbers in the error message.
    return absl::InvalidArgumentError("invalid format");
  }

  static bool IsWhitespace(char const ch) {
    return ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t';
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

  ABSL_ATTRIBUTE_ALWAYS_INLINE bool ConsumePrefix(std::string_view const prefix) {
    return absl::ConsumePrefix(&input_, prefix);
  }

  absl::Status RequirePrefix(std::string_view const prefix) {
    if (ConsumePrefix(prefix)) {
      return absl::OkStatus();
    } else {
      return InvalidSyntaxError();
    }
  }

  void ConsumeWhitespace() {
    size_t offset = 0;
    while (offset < input_.size() && IsWhitespace(input_[offset])) {
      ++offset;
    }
    input_.remove_prefix(offset);
  }

  absl::Status SkipStringPartial();
  absl::Status SkipObjectPartial();
  absl::Status SkipArrayPartial();
  absl::Status SkipValue();

  absl::Status ReadTo(bool* result);

  template <typename Integer,
            std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
  absl::Status ReadTo(Integer* result);

  template <typename Float, std::enable_if_t<std::is_floating_point_v<Float>, bool> = true>
  absl::Status ReadTo(Float* result);

  absl::Status ReadTo(std::string* result);

  template <typename Inner>
  absl::Status ReadTo(std::optional<Inner>* result);

  template <typename First, typename Second>
  absl::Status ReadTo(std::pair<First, Second>* result);

  template <typename... Elements>
  absl::Status ReadTo(std::tuple<Elements...>* result);

  template <typename Element, size_t length>
  absl::Status ReadTo(std::array<Element, length>* result);

  template <typename Element>
  absl::Status ReadTo(std::vector<Element>* result);

  template <typename... Fields>
  absl::Status ReadTo(Object<Fields...>* result);

  template <typename Element, typename Dictionary>
  absl::Status ReadToDictionary(Dictionary* result);

  template <typename Element, typename Compare, typename Allocator>
  absl::Status ReadTo(std::map<std::string, Element, Compare, Allocator>* const result) {
    return ReadToDictionary<Element>(result);
  }

  template <typename Element, typename Hash, typename Equal, typename Allocator>
  absl::Status ReadTo(
      std::unordered_map<std::string, Element, Hash, Equal, Allocator>* const result) {
    return ReadToDictionary<Element>(result);
  }

  template <typename Element, typename Compare, typename Allocator>
  absl::Status ReadTo(absl::btree_map<std::string, Element, Compare, Allocator>* const result) {
    return ReadToDictionary<Element>(result);
  }

  template <typename Element, typename Hash, typename Equal, typename Allocator>
  absl::Status ReadTo(
      absl::flat_hash_map<std::string, Element, Hash, Equal, Allocator>* const result) {
    return ReadToDictionary<Element>(result);
  }

  template <typename Element, typename Hash, typename Equal, typename Allocator>
  absl::Status ReadTo(
      absl::node_hash_map<std::string, Element, Hash, Equal, Allocator>* const result) {
    return ReadToDictionary<Element>(result);
  }

  template <typename Element, typename Compare, typename Representation>
  absl::Status ReadTo(flat_map<std::string, Element, Compare, Representation>* const result) {
    return ReadToDictionary<Element>(result);
  }

  template <typename Element, typename Set>
  absl::Status ReadToSet(Set* result);

  template <typename Element, typename Compare, typename Allocator>
  absl::Status ReadTo(std::set<Element, Compare, Allocator>* const result) {
    return ReadToSet<Element>(result);
  }

  template <typename Element, typename Hash, typename Equal, typename Allocator>
  absl::Status ReadTo(std::unordered_set<Element, Hash, Equal, Allocator>* const result) {
    return ReadToSet<Element>(result);
  }

  template <typename Element, typename Compare, typename Allocator>
  absl::Status ReadTo(absl::btree_set<Element, Compare, Allocator>* const result) {
    return ReadToSet<Element>(result);
  }

  template <typename Element, typename Hash, typename Equal, typename Allocator>
  absl::Status ReadTo(absl::flat_hash_set<Element, Hash, Equal, Allocator>* const result) {
    return ReadToSet<Element>(result);
  }

  template <typename Element, typename Hash, typename Equal, typename Allocator>
  absl::Status ReadTo(absl::node_hash_set<Element, Hash, Equal, Allocator>* const result) {
    return ReadToSet<Element>(result);
  }

  template <typename Element, typename Compare, typename Representation>
  absl::Status ReadTo(flat_set<Element, Compare, Representation>* const result) {
    return ReadToSet<Element>(result);
  }

  template <typename Pointee>
  absl::Status ReadTo(std::unique_ptr<Pointee>* const result) {
    ConsumeWhitespace();
    *result = nullptr;
    if (ConsumePrefix("null")) {
      return absl::OkStatus();
    }
    *result = std::make_unique<Pointee>();
    return ReadTo(result->get());
  }

  template <typename Pointee>
  absl::Status ReadTo(std::shared_ptr<Pointee>* const result) {
    ConsumeWhitespace();
    *result = nullptr;
    if (ConsumePrefix("null")) {
      return absl::OkStatus();
    }
    *result = std::make_shared<Pointee>();
    return ReadTo(result->get());
  }

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
  if (ConsumePrefix("-")) {
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
  if (ConsumePrefix("-")) {
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
  if (ConsumePrefix(".")) {
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
  if (ConsumePrefix("E") || ConsumePrefix("e")) {
    int exponent_sign = 1;
    if (ConsumePrefix("-")) {
      exponent_sign = -1;
    } else {
      ConsumePrefix("+");
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
  ConsumeWhitespace();
  if (ConsumePrefix("null")) {
    *result = std::nullopt;
    return absl::OkStatus();
  } else {
    if (!*result) {
      result->emplace();
    }
    return ReadTo(&(result->value()));
  }
}

template <typename First, typename Second>
absl::Status Parser::ReadTo(std::pair<First, Second>* const result) {
  ConsumeWhitespace();
  RETURN_IF_ERROR(RequirePrefix("["));
  RETURN_IF_ERROR(ReadTo(&(result->first)));
  ConsumeWhitespace();
  RETURN_IF_ERROR(RequirePrefix(","));
  RETURN_IF_ERROR(ReadTo(&(result->second)));
  ConsumeWhitespace();
  return RequirePrefix("]");
}

template <typename... Elements>
absl::Status Parser::ReadTo(std::tuple<Elements...>* const result) {
  ConsumeWhitespace();
  RETURN_IF_ERROR(RequirePrefix("["));
  RETURN_IF_ERROR(std::apply(
      [this](auto&... elements) {
        bool first = true;
        auto status = absl::OkStatus();
        static_cast<void>((((first ? !(first = false) : (ConsumeWhitespace(), ConsumePrefix(",")))
                                ? (status = ReadTo(&elements))
                                : (status = InvalidSyntaxError()))
                               .ok() &&
                           ...));
        return status;
      },
      *result));
  ConsumeWhitespace();
  return RequirePrefix("]");
}

template <typename Element, size_t length>
absl::Status Parser::ReadTo(std::array<Element, length>* const result) {
  ConsumeWhitespace();
  RETURN_IF_ERROR(RequirePrefix("["));
  RETURN_IF_ERROR(std::apply(
      [this](auto&... elements) {
        bool first = true;
        auto status = absl::OkStatus();
        static_cast<void>((((first ? !(first = false) : (ConsumeWhitespace(), ConsumePrefix(",")))
                                ? (status = ReadTo(&elements))
                                : (status = InvalidSyntaxError()))
                               .ok() &&
                           ...));
        return status;
      },
      *result));
  ConsumeWhitespace();
  return RequirePrefix("]");
}

template <typename Element>
absl::Status Parser::ReadTo(std::vector<Element>* const result) {
  ConsumeWhitespace();
  RETURN_IF_ERROR(RequirePrefix("["));
  ConsumeWhitespace();
  result->clear();
  if (ConsumePrefix("]")) {
    return absl::OkStatus();
  }
  while (!input_.empty()) {
    auto& element = result->emplace_back();
    RETURN_IF_ERROR(ReadTo(&element));
    ConsumeWhitespace();
    if (ConsumePrefix(",")) {
      ConsumeWhitespace();
    } else if (ConsumePrefix("]")) {
      return absl::OkStatus();
    } else {
      return InvalidSyntaxError();
    }
  }
  return InvalidSyntaxError();
}

// Used by the object parsing algorithm at the end of parsing to check that all non-optional fields
// have been found in the input. The specializations provide `operator()` implementations returning
// true iff all required fields have been found. The `keys` argument is the set of keys found in the
// JSON input.
//
// Optional fields are defined as those with type `std::optional`, `std::unique_ptr`, or
// `std::shared_ptr`.
template <typename... Fields>
struct CheckFieldPresence;

template <>
struct CheckFieldPresence<> {
  ABSL_ATTRIBUTE_ALWAYS_INLINE bool operator()(flat_set<std::string> const&) const { return true; }
};

template <typename Type, typename Name, typename... OtherFields>
struct CheckFieldPresence<FieldImpl<Type, Name>, OtherFields...> {
  ABSL_ATTRIBUTE_ALWAYS_INLINE bool operator()(flat_set<std::string> const& keys) const {
    return keys.contains(Name::value) && CheckFieldPresence<OtherFields...>()(keys);
  }
};

template <typename OptionalType, typename Name, typename... OtherFields>
struct CheckFieldPresence<FieldImpl<std::optional<OptionalType>, Name>, OtherFields...> {
  ABSL_ATTRIBUTE_ALWAYS_INLINE bool operator()(flat_set<std::string> const& keys) const {
    return CheckFieldPresence<OtherFields...>()(keys);
  }
};

template <typename OptionalType, typename Name, typename... OtherFields>
struct CheckFieldPresence<FieldImpl<std::unique_ptr<OptionalType>, Name>, OtherFields...> {
  ABSL_ATTRIBUTE_ALWAYS_INLINE bool operator()(flat_set<std::string> const& keys) const {
    return CheckFieldPresence<OtherFields...>()(keys);
  }
};

template <typename OptionalType, typename Name, typename... OtherFields>
struct CheckFieldPresence<FieldImpl<std::shared_ptr<OptionalType>, Name>, OtherFields...> {
  ABSL_ATTRIBUTE_ALWAYS_INLINE bool operator()(flat_set<std::string> const& keys) const {
    return CheckFieldPresence<OtherFields...>()(keys);
  }
};

template <typename... Fields>
absl::Status Parser::ReadTo(Object<Fields...>* const result) {
  ConsumeWhitespace();
  RETURN_IF_ERROR(RequirePrefix("{"));
  ConsumeWhitespace();
  result->Clear();
  flat_set<std::string> keys;
  if (ConsumePrefix("}")) {
    if (CheckFieldPresence<Fields...>()(keys)) {
      return absl::OkStatus();
    } else {
      return InvalidFormatError();
    }
  }
  keys.reserve(sizeof...(Fields));
  while (!input_.empty()) {
    std::string key;
    RETURN_IF_ERROR(ReadTo(&key));
    auto const [unused_it, inserted] = keys.emplace(key);
    if (!inserted) {
      return InvalidFormatError();  // duplicate key
    }
    ConsumeWhitespace();
    RETURN_IF_ERROR(RequirePrefix(":"));
    RETURN_IF_ERROR(result->ReadField(this, key));
    ConsumeWhitespace();
    if (ConsumePrefix(",")) {
      ConsumeWhitespace();
    } else if (ConsumePrefix("}")) {
      if (CheckFieldPresence<Fields...>()(keys)) {
        return absl::OkStatus();
      } else {
        return InvalidFormatError();
      }
    } else {
      return InvalidSyntaxError();
    }
  }
  return InvalidSyntaxError();
}

template <typename Element, typename Dictionary>
absl::Status Parser::ReadToDictionary(Dictionary* const result) {
  ConsumeWhitespace();
  RETURN_IF_ERROR(RequirePrefix("{"));
  ConsumeWhitespace();
  result->clear();
  if (ConsumePrefix("}")) {
    return absl::OkStatus();
  }
  while (!input_.empty()) {
    std::string key;
    RETURN_IF_ERROR(ReadTo(&key));
    ConsumeWhitespace();
    RETURN_IF_ERROR(RequirePrefix(":"));
    Element element;
    RETURN_IF_ERROR(ReadTo(&element));
    auto const [unused_it, inserted] = result->try_emplace(std::move(key), std::move(element));
    if (!inserted) {
      return InvalidFormatError();
    }
    ConsumeWhitespace();
    if (ConsumePrefix(",")) {
      ConsumeWhitespace();
    } else if (ConsumePrefix("}")) {
      return absl::OkStatus();
    } else {
      return InvalidSyntaxError();
    }
  }
  return InvalidSyntaxError();
}

template <typename Element, typename Set>
absl::Status Parser::ReadToSet(Set* const result) {
  ConsumeWhitespace();
  RETURN_IF_ERROR(RequirePrefix("["));
  ConsumeWhitespace();
  result->clear();
  if (ConsumePrefix("]")) {
    return absl::OkStatus();
  }
  while (!input_.empty()) {
    Element element;
    RETURN_IF_ERROR(ReadTo(&element));
    result->emplace(std::move(element));
    ConsumeWhitespace();
    if (ConsumePrefix(",")) {
      ConsumeWhitespace();
    } else if (ConsumePrefix("]")) {
      return absl::OkStatus();
    } else {
      return InvalidSyntaxError();
    }
  }
  return InvalidSyntaxError();
}

}  // namespace internal

ABSL_ATTRIBUTE_ALWAYS_INLINE absl::Status Object<>::ReadField(internal::Parser* const parser,
                                                              std::string_view const field_name) {
  return parser->SkipValue();
}

template <typename Type, typename Name, typename... OtherFields>
ABSL_ATTRIBUTE_ALWAYS_INLINE absl::Status
Object<internal::FieldImpl<Type, Name>, OtherFields...>::ReadField(
    internal::Parser* const parser, std::string_view const field_name) {
  if (field_name != Name::value) {
    return Object<OtherFields...>::ReadField(parser, field_name);
  } else {
    return parser->ReadTo(&value_);
  }
}

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
