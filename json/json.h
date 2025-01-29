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
//   * std::set / std::unordered_set / tsdb2::common::flat_set / tsdb2::common::trie_set /
//     absl::btree_set / absl::flat_hash_set / absl::node_hash_set,
//   * std::map / std::unordered_map / tsdb2::common::flat_map / tsdb2::common::trie_map /
//     absl::btree_map / absl::flat_hash_map / absl::node_hash_map,
//   * tsdb2::json::Object,
//   * data types managed by std::unique_ptr, std::shared_ptr, or tsdb2::common::reffed_ptr
//     (serializing to "null" if the pointer is null).
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
//
// If you have a custom type that you'd like to make JSON-(de)serializable you can simply add friend
// member functions called `Tsdb2JsonParse` and `Tsdb2JsonStringify`, which will make your type
// compatible with `json::Parse` and `json::Stringify` respectively.
//
// The two functions have the following signatures:
//
//   friend absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser, MyType* value);
//   friend void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier, MyType const& value);
//
// Your custom type must also be movable and default-constructible.
//
// Example:
//
//   class Point {
//    private:
//     static char constexpr kPointXField[] = "coord_x";
//     static char constexpr kPointYField[] = "coord_y";
//
//     using JsonPoint = tsdb2::json::Object<
//         tsdb2::json::Field<double, kPointXField>,
//         tsdb2::json::Field<double, kPointYField>>;
//
//    public:
//     explicit Point() : x_(0), y_(0) {}
//     explicit Point(double const x, double const y) : x_(x), y_(y) {}
//
//     double x() const { return x_; }
//     double y() const { return y_; }
//
//     friend absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser, Point* const point) {
//       DEFINE_CONST_OR_RETURN(obj, (parser->ReadObject<JsonPoint>()));
//       point->x_ = obj.get<kPointXField>();
//       point->y_ = obj.get<kPointYField>();
//       return absl::OkStatus();
//     }
//
//     friend void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
//                                    Point const& point) {
//       stringifier->WriteObject(JsonPoint{point.x_, point.y_});
//     }
//
//    private:
//     double x_;
//     double y_;
//   };
//
//
// You'll then be able to do things like:
//
//   auto const status_or_point = tsdb2::json::Parse<Point>(R"json({
//     "coord_x": 12.34,
//     "coord_y": 34.56
//   })json");
//   assert(status_or_point.ok());
//   auto const& point = status_or_point.value();
//   assert(point.x() == 12.34);
//   assert(point.y() == 34.56);
//   auto const json = tsdb2::json::Stringify(point);
//   assert(json == R"({"coord_x":12.34,"coord_y":34.56})");
//
#ifndef __TSDB2_JSON_JSON_H__
#define __TSDB2_JSON_JSON_H__

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
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
#include "absl/strings/charconv.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "common/flat_map.h"
#include "common/flat_set.h"
#include "common/reffed_ptr.h"
#include "common/trie_map.h"
#include "common/trie_set.h"
#include "common/type_string.h"
#include "common/utilities.h"

namespace tsdb2 {
namespace json {

enum class LineFeedType : uint8_t { LF, CRLF, CR };

// Options for parsing.
struct ParseOptions {
  // If true, fields that are not defined in an `Object` template are ignored. If false, extra
  // fields cause a parsing error.
  bool allow_extra_fields = true;

  // When `allow_extra_fields` is true this option determines how to skip extra fields. The standard
  // algorithm scans their values normally and then discards them, but still verifies that the JSON
  // syntax is correct. The fast algorithm skips all input characters without checking the syntax,
  // up to the point where the next field starts.
  bool fast_skipping = false;
};

// Options for stringification.
struct StringifyOptions {
  // Whether the output is formatted with indentations and newlines.
  bool pretty = false;

  // Determines the character sequence to use for line feeds when `pretty` is true. Can be `LF`,
  // `CRLF`, or `CR`.
  LineFeedType line_feed_type = LineFeedType::LF;

  // Number of spaces per tab.
  size_t indent_width = 2;

  // When true, append an extra empty line at the end, independently of the `pretty` flag.
  bool trailing_newline = false;

  // When true, nullable or optional fields are serialized as `null` when they are null / empty.
  // Otherwise they are not serialized.
  bool output_empty_fields = false;
};

namespace internal {

template <typename Type, typename Name>
struct FieldImpl;

template <typename Type, char... ch>
struct FieldImpl<Type, tsdb2::common::TypeStringMatcher<ch...>> {
  using Name = tsdb2::common::TypeStringMatcher<ch...>;
  static std::string_view constexpr name = Name::value;
};

// FOR INTERNAL USE. Checks that none of `Fields` has the same name as `Name`. `Fields` are
// `FieldImpl`s and `Name` must be a `TypeStringMatcher`.
//
// This is used by `Object` to make sure there aren't duplicate names.
template <typename Name, typename... Fields>
struct CheckUniqueName;

template <char... name>
struct CheckUniqueName<tsdb2::common::TypeStringMatcher<name...>> {
  static bool constexpr value = true;
};

template <typename Name, typename Type, typename... OtherFields>
struct CheckUniqueName<Name, FieldImpl<Type, Name>, OtherFields...> {
  static bool constexpr value = false;
};

template <typename FieldName, typename Type, typename Name, typename... OtherFields>
struct CheckUniqueName<FieldName, FieldImpl<Type, Name>, OtherFields...> {
  static bool constexpr value = CheckUniqueName<FieldName, OtherFields...>::value;
};

template <typename Name, typename... Fields>
inline bool constexpr CheckUniqueNameV = CheckUniqueName<Name, Fields...>::value;

std::string EscapeAndQuoteString(std::string_view input);

}  // namespace internal

// Fields for `Object` (see below).
template <typename Type, char const name[]>
using Field = internal::FieldImpl<Type, tsdb2::common::TypeStringT<name>>;

class Parser;
class Stringifier;

// This token is used in the overload resolution of the `Object` constructor to select the overload
// that initializes the fields of the object.
//
// Example:
//
//   char constexpr kCoordXField[] = "coord_x";
//   char constexpr kCoordYField[] = "coord_y";
//   using Point = tsdb2::json::Object<
//       tsdb2::json::Field<double, kCoordXField>,
//       tsdb2::json::Field<double, kCoordYField>>;
//
//   Point obj{tsdb2::json::kInitialize, /*coord_x=*/12, /*coord_y=*/34};
//   assert(obj.get<kCoordXField>() == 12);
//   assert(obj.get<kCoordYField>() == 34);
//
struct Initialize {};
inline Initialize constexpr kInitialize{};

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
  explicit Object(Initialize /*initialize*/) {}
  ~Object() = default;

  Object(Object const&) = default;
  Object& operator=(Object const&) = default;
  Object(Object&&) noexcept = default;
  Object& operator=(Object&&) noexcept = default;

  void swap(Object& other) noexcept {}
  friend void swap(Object& lhs, Object& rhs) noexcept {}

  friend bool operator==(Object const& lhs, Object const& rhs) {
    return lhs.CompareEqualInternal(rhs);
  }

  friend bool operator!=(Object const& lhs, Object const& rhs) {
    return !lhs.CompareEqualInternal(rhs);
  }

  friend bool operator<(Object const& lhs, Object const& rhs) {
    return lhs.CompareLessInternal(rhs);
  }

  friend bool operator<=(Object const& lhs, Object const& rhs) {
    return !rhs.CompareLessInternal(lhs);
  }

  friend bool operator>(Object const& lhs, Object const& rhs) {
    return rhs.CompareLessInternal(lhs);
  }

  friend bool operator>=(Object const& lhs, Object const& rhs) {
    return !lhs.CompareLessInternal(rhs);
  }

  template <typename H>
  friend H AbslHashValue(H h, Object const& value) {
    return h;
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, Object const& value) {
    return state;
  }

  void Clear() {}

  inline std::string Stringify(StringifyOptions const& options) const;
  std::string Stringify() const { return Stringify(StringifyOptions{}); }

 protected:
  friend class Parser;
  friend class Stringifier;

  // NOLINTBEGIN(readability-convert-member-functions-to-static)

  ABSL_ATTRIBUTE_ALWAYS_INLINE void SwapInternal(Object& other) {}
  ABSL_ATTRIBUTE_ALWAYS_INLINE bool CompareEqualInternal(Object const& other) const { return true; }
  ABSL_ATTRIBUTE_ALWAYS_INLINE bool CompareLessInternal(Object const& other) const { return false; }
  ABSL_ATTRIBUTE_ALWAYS_INLINE void ClearInternal() {}

  inline absl::Status ReadField(Parser* parser, std::string_view key);

  void WriteFieldsPretty(Stringifier* /*stringifier*/, bool /*first_field*/) const {}
  void WriteFieldsCompressed(Stringifier* /*stringifier*/, bool /*first_field*/) const {}

  // NOLINTEND(readability-convert-member-functions-to-static)
};

template <typename Type, typename Name, typename... OtherFields>
class Object<internal::FieldImpl<Type, Name>, OtherFields...> : public Object<OtherFields...> {
 public:
  static_assert(internal::CheckUniqueNameV<Name, OtherFields...>, "duplicate field names");

  template <typename FieldName, typename Dummy = void>
  struct FieldTypeImpl;

  template <char const field_name[]>
  using FieldType = typename FieldTypeImpl<tsdb2::common::TypeStringT<field_name>>::Type;

  explicit Object() = default;

  template <typename FirstArg, typename... OtherArgs>
  explicit Object(Initialize /*initialize*/, FirstArg&& value, OtherArgs&&... args)
      : Object<OtherFields...>(kInitialize, std::forward<OtherArgs>(args)...),
        value_(std::forward<FirstArg>(value)) {}

  ~Object() = default;

  Object(Object const&) = default;
  Object& operator=(Object const&) = default;

  Object(Object&&) noexcept = default;
  Object& operator=(Object&&) noexcept = default;

  void swap(Object& other) noexcept { SwapInternal(other); }
  friend void swap(Object& lhs, Object& rhs) noexcept { lhs.SwapInternal(rhs); }

  friend bool operator==(Object const& lhs, Object const& rhs) {
    return lhs.CompareEqualInternal(rhs);
  }

  friend bool operator!=(Object const& lhs, Object const& rhs) {
    return !lhs.CompareEqualInternal(rhs);
  }

  friend bool operator<(Object const& lhs, Object const& rhs) {
    return lhs.CompareLessInternal(rhs);
  }

  friend bool operator<=(Object const& lhs, Object const& rhs) {
    return !rhs.CompareLessInternal(lhs);
  }

  friend bool operator>(Object const& lhs, Object const& rhs) {
    return rhs.CompareLessInternal(lhs);
  }

  friend bool operator>=(Object const& lhs, Object const& rhs) {
    return !lhs.CompareLessInternal(rhs);
  }

  template <typename H>
  friend H AbslHashValue(H h, Object const& value) {
    return H::combine(std::move(h), value.value_,
                      ConstGetter<typename OtherFields::Name>(value)()...);
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, Object const& value) {
    return State::Combine(std::move(state), value.value_,
                          ConstGetter<typename OtherFields::Name>(value)()...);
  }

  template <char const field_name[]>
  auto& get() & noexcept {
    return Getter<tsdb2::common::TypeStringT<field_name>>(*this)();
  }

  template <char const field_name[]>
  auto&& get() && noexcept {
    return std::move(Getter<tsdb2::common::TypeStringT<field_name>>(*this)());
  }

  template <char const field_name[]>
  auto const& get() const& noexcept {
    return ConstGetter<tsdb2::common::TypeStringT<field_name>>(*this)();
  }

  template <char const field_name[]>
  auto const& cget() const& noexcept {
    return ConstGetter<tsdb2::common::TypeStringT<field_name>>(*this)();
  }

  void Clear() { ClearInternal(); }

  inline std::string Stringify(StringifyOptions const& options) const;
  std::string Stringify() const { return Stringify(StringifyOptions{}); }

 protected:
  friend class Parser;
  friend class Stringifier;

  template <typename FieldName, typename Dummy = void>
  struct Getter;

  template <typename FieldName, typename Dummy = void>
  struct ConstGetter;

  ABSL_ATTRIBUTE_ALWAYS_INLINE void SwapInternal(Object& other) {
    Object<OtherFields...>::SwapInternal(other);
    using std::swap;  // ensure ADL
    swap(value_, other.value_);
  }

  ABSL_ATTRIBUTE_ALWAYS_INLINE bool CompareEqualInternal(Object const& other) const {
    return value_ == other.value_ && Object<OtherFields...>::CompareEqualInternal(other);
  }

  ABSL_ATTRIBUTE_ALWAYS_INLINE bool CompareLessInternal(Object const& other) const {
    return value_ < other.value_ ||
           (!(other.value_ < value_) && Object<OtherFields...>::CompareLessInternal(other));
  }

  ABSL_ATTRIBUTE_ALWAYS_INLINE void ClearInternal() {
    Object<OtherFields...>::ClearInternal();
    value_ = Type();
  }

  inline absl::Status ReadField(Parser* parser, std::string_view key);

  inline void WriteFieldsPretty(Stringifier* stringifier, bool first_field) const;
  inline void WriteFieldsCompressed(Stringifier* stringifier, bool first_field) const;

 private:
  Type value_;
};

template <typename Type2, typename Name, typename... OtherFields>
template <typename Dummy>
struct Object<internal::FieldImpl<Type2, Name>, OtherFields...>::FieldTypeImpl<Name, Dummy> {
  using Type = Type2;
};

template <typename Type, typename Name, typename... OtherFields>
template <typename Dummy, char... field_name>
struct Object<internal::FieldImpl<Type, Name>,
              OtherFields...>::FieldTypeImpl<tsdb2::common::TypeStringMatcher<field_name...>, Dummy>
    : public Object<OtherFields...>::template FieldTypeImpl<
          tsdb2::common::TypeStringMatcher<field_name...>> {};

template <typename Type, typename Name, typename... OtherFields>
template <typename Dummy>
struct Object<internal::FieldImpl<Type, Name>, OtherFields...>::Getter<Name, Dummy> {
  explicit ABSL_ATTRIBUTE_ALWAYS_INLINE Getter(Object& obj) : value(obj.value_) {}
  ABSL_ATTRIBUTE_ALWAYS_INLINE Type& operator()() const noexcept { return value; }
  Type& value;
};

template <typename Type, typename Name, typename... OtherFields>
template <typename Dummy, char... field_name>
struct Object<internal::FieldImpl<Type, Name>,
              OtherFields...>::Getter<tsdb2::common::TypeStringMatcher<field_name...>, Dummy>
    : public Object<OtherFields...>::template Getter<
          tsdb2::common::TypeStringMatcher<field_name...>> {
  explicit ABSL_ATTRIBUTE_ALWAYS_INLINE Getter(Object& obj)
      : Object<OtherFields...>::template Getter<tsdb2::common::TypeStringMatcher<field_name...>>(
            obj) {}
};

template <typename Type, typename Name, typename... OtherFields>
template <typename Dummy>
struct Object<internal::FieldImpl<Type, Name>, OtherFields...>::ConstGetter<Name, Dummy> {
  explicit ABSL_ATTRIBUTE_ALWAYS_INLINE ConstGetter(Object const& obj) : value(obj.value_) {}
  ABSL_ATTRIBUTE_ALWAYS_INLINE Type const& operator()() const noexcept { return value; }
  Type const& value;
};

template <typename Type, typename Name, typename... OtherFields>
template <typename Dummy, char... field_name>
struct Object<internal::FieldImpl<Type, Name>,
              OtherFields...>::ConstGetter<tsdb2::common::TypeStringMatcher<field_name...>, Dummy>
    : public Object<OtherFields...>::template ConstGetter<
          tsdb2::common::TypeStringMatcher<field_name...>> {
  explicit ABSL_ATTRIBUTE_ALWAYS_INLINE ConstGetter(Object const& obj)
      : Object<OtherFields...>::template ConstGetter<
            tsdb2::common::TypeStringMatcher<field_name...>>(obj) {}
};

// Our JSON parser.
//
// The syntax is described at https://www.json.org/.
class Parser final {
 public:
  explicit Parser(std::string_view const input, ParseOptions const& options)
      : options_(options), input_(input) {}

  ParseOptions const& options() const { return options_; }

  template <typename Value>
  absl::StatusOr<Value> Parse();

  absl::Status ReadNull();

  absl::StatusOr<bool> ReadBoolean();

  template <typename Integer,
            std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
  absl::StatusOr<Integer> ReadInteger();

  template <typename Float, std::enable_if_t<std::is_floating_point_v<Float>, bool> = true>
  absl::StatusOr<Float> ReadFloat();

  absl::StatusOr<std::string> ReadString();

  template <typename... Fields>
  absl::StatusOr<Object<Fields...>> ReadObjectFields();

  template <typename Object>
  absl::StatusOr<Object> ReadObject() {
    return ObjectReader<Object>{}(this);
  }

  template <typename Value, typename Representation>
  absl::StatusOr<Representation> ReadDictionary();

  template <typename Element>
  absl::StatusOr<std::vector<Element>> ReadVector();

  template <typename Element, size_t N>
  absl::StatusOr<std::array<Element, N>> ReadArray();

  template <typename Element, size_t N>
  absl::Status ReadArray(std::array<Element, N>* result);

  template <typename First, typename Second>
  absl::StatusOr<std::pair<First, Second>> ReadPair();

  template <typename... Values>
  absl::StatusOr<std::tuple<Values...>> ReadTuple();

  template <typename Value, typename Representation>
  absl::StatusOr<Representation> ReadSet();

  template <typename Value>
  absl::StatusOr<std::optional<Value>> ReadValueOrNull();

  template <typename Value>
  absl::Status ReadPointeeOrNull(std::unique_ptr<Value>* ptr);

 private:
  friend class Object<>;

  template <typename Object>
  struct ObjectReader;

  template <typename... Fields>
  struct ObjectReader<Object<Fields...>> {
    absl::StatusOr<Object<Fields...>> operator()(Parser* const parser) const {
      return parser->ReadObjectFields<Fields...>();
    }
  };

  static absl::Status InvalidSyntaxError() {
    // TODO: include row and column numbers in the error message.
    return absl::InvalidArgumentError("invalid JSON syntax");
  }

  static absl::Status InvalidFormatError() {
    // TODO: include row and column numbers in the error message.
    return absl::InvalidArgumentError("invalid format");
  }

  static bool IsWhitespace(char const ch) {
    return ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t';
  }

  static bool IsDigit(char const ch) { return ch >= '0' && ch <= '9'; }

  static bool IsHexDigit(char ch);

  static uint8_t ParseHexDigit(char ch);

  bool PeekDigit() const { return !input_.empty() && IsDigit(input_.front()); }

  bool ConsumePrefix(std::string_view const prefix) { return absl::ConsumePrefix(&input_, prefix); }

  // Consumes the specified prefix or returns a syntax error.
  absl::Status RequirePrefix(std::string_view prefix);

  // Consumes the specified prefix or returns a format error.
  absl::Status ExpectPrefix(std::string_view prefix);

  void ConsumeWhitespace();

  absl::StatusOr<std::string_view> ConsumeInteger();
  absl::StatusOr<std::string_view> ConsumeFloat();

  absl::Status SkipString(size_t& offset);

  absl::Status FastSkipArray(size_t& offset);
  absl::Status FastSkipObject(size_t& offset);
  absl::Status FastSkipField();

  absl::Status SkipStringPartial();
  absl::Status SkipObjectPartial();
  absl::Status SkipArrayPartial();
  absl::Status SkipValue();

  absl::Status SkipField(bool fast);

  ParseOptions const options_;
  std::string_view input_;
};

}  // namespace json
}  // namespace tsdb2

// Forward declarations for all predefined `Tsdb2JsonParse` overloads.

absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser, bool* result);

template <typename Integer, std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser, Integer* result);

template <typename Float, std::enable_if_t<std::is_floating_point_v<Float>, bool> = true>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser, Float* result);

absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser, std::string* result);

template <typename... Fields>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser, tsdb2::json::Object<Fields...>* result);

template <typename Value, typename Compare, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser,
                            std::map<std::string, Value, Compare, Allocator>* result);

template <typename Value, typename Hash, typename Equal, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser,
                            std::unordered_map<std::string, Value, Hash, Equal, Allocator>* result);

template <typename Value, typename Hash, typename Equal, typename Allocator>
absl::Status Tsdb2JsonParse(
    tsdb2::json::Parser* parser,
    absl::flat_hash_map<std::string, Value, Hash, Equal, Allocator>* result);

template <typename Value, typename Hash, typename Equal, typename Allocator>
absl::Status Tsdb2JsonParse(
    tsdb2::json::Parser* parser,
    absl::node_hash_map<std::string, Value, Hash, Equal, Allocator>* result);

template <typename Value, typename Compare, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser,
                            absl::btree_map<std::string, Value, Compare, Allocator>* result);

template <typename Value, typename Compare, typename Allocator>
absl::Status Tsdb2JsonParse(
    tsdb2::json::Parser* parser,
    tsdb2::common::flat_map<std::string, Value, Compare, Allocator>* result);

template <typename Value, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser,
                            tsdb2::common::trie_map<Value, Allocator>* result);

template <typename Element>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser, std::vector<Element>* result);

template <typename Element, size_t N>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser, std::array<Element, N>* result);

template <typename First, typename Second>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser, std::pair<First, Second>* result);

template <typename... Values>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser, std::tuple<Values...>* result);

template <typename Value, typename Compare, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser,
                            std::set<Value, Compare, Allocator>* result);

template <typename Value, typename Hash, typename Equal, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser,
                            std::unordered_set<Value, Hash, Equal, Allocator>* result);

template <typename Value, typename Hash, typename Equal, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser,
                            absl::flat_hash_set<Value, Hash, Equal, Allocator>* result);

template <typename Value, typename Hash, typename Equal, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser,
                            absl::node_hash_set<Value, Hash, Equal, Allocator>* result);

template <typename Value, typename Compare, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser,
                            absl::btree_set<Value, Compare, Allocator>* result);

template <typename Value, typename Compare, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser,
                            tsdb2::common::flat_set<Value, Compare, Allocator>* result);

template <typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser,
                            tsdb2::common::trie_set<Allocator>* result);

template <typename Value>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser, std::optional<Value>* result);

template <typename Value>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser, std::unique_ptr<Value>* result);

template <typename Value>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser, std::shared_ptr<Value>* result);

template <typename Value>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* parser, tsdb2::common::reffed_ptr<Value>* result);

// Forward declarations for all predefined `Tsdb2JsonStringify` overloads.

void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier, bool value);

template <typename Integer, std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier, Integer value);

template <typename Float, std::enable_if_t<std::is_floating_point_v<Float>, bool> = true>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier, Float value);

void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier, char const value[]);

void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier, std::string_view value);

template <typename... Fields>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier,
                        tsdb2::json::Object<Fields...> const& value);

template <typename Value, typename Compare, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier,
                        std::map<std::string, Value, Compare, Allocator> const& value);

template <typename Value, typename Hash, typename Equal, typename Allocator>
void Tsdb2JsonStringify(
    tsdb2::json::Stringifier* stringifier,
    std::unordered_map<std::string, Value, Hash, Equal, Allocator> const& value);

template <typename Value, typename Hash, typename Equal, typename Allocator>
void Tsdb2JsonStringify(
    tsdb2::json::Stringifier* stringifier,
    absl::flat_hash_map<std::string, Value, Hash, Equal, Allocator> const& value);

template <typename Value, typename Hash, typename Equal, typename Allocator>
void Tsdb2JsonStringify(
    tsdb2::json::Stringifier* stringifier,
    absl::node_hash_map<std::string, Value, Hash, Equal, Allocator> const& value);

template <typename Value, typename Compare, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier,
                        absl::btree_map<std::string, Value, Compare, Allocator> const& value);

template <typename Value, typename Compare, typename Allocator>
void Tsdb2JsonStringify(
    tsdb2::json::Stringifier* stringifier,
    tsdb2::common::flat_map<std::string, Value, Compare, Allocator> const& value);

template <typename Value, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier,
                        tsdb2::common::trie_map<Value, Allocator> const& value);

template <typename Element>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier, std::vector<Element> const& value);

template <typename Element, size_t N>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier, std::array<Element, N> const& value);

template <typename First, typename Second>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier,
                        std::pair<First, Second> const& value);

template <typename... Elements>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier,
                        std::tuple<Elements...> const& value);

template <typename Value, typename Compare, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier,
                        std::set<Value, Compare, Allocator> const& value);

template <typename Value, typename Hash, typename Equal, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier,
                        std::unordered_set<Value, Hash, Equal, Allocator> const& value);

template <typename Value, typename Hash, typename Equal, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier,
                        absl::flat_hash_set<Value, Hash, Equal, Allocator> const& value);

template <typename Value, typename Hash, typename Equal, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier,
                        absl::node_hash_set<Value, Hash, Equal, Allocator> const& value);

template <typename Value, typename Compare, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier,
                        absl::btree_set<Value, Compare, Allocator> const& value);

template <typename Value, typename Compare, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier,
                        tsdb2::common::flat_set<Value, Compare, Allocator> const& value);

template <typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier,
                        tsdb2::common::trie_set<Allocator> const& value);

template <typename Value>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier, std::optional<Value> const& value);

template <typename Value>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier, std::unique_ptr<Value> const& value);

template <typename Value>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier, std::shared_ptr<Value> const& value);

template <typename Value>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* stringifier,
                        tsdb2::common::reffed_ptr<Value> const& value);

namespace tsdb2 {
namespace json {

template <typename Value>
absl::StatusOr<Value> Parser::Parse() {
  Value value;
  RETURN_IF_ERROR(Tsdb2JsonParse(this, &value));
  ConsumeWhitespace();
  if (!input_.empty()) {
    return InvalidSyntaxError();
  }
  return std::move(value);
}

template <typename Integer, std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool>>
absl::StatusOr<Integer> Parser::ReadInteger() {
  ConsumeWhitespace();
  if (input_.empty()) {
    return InvalidSyntaxError();
  }
  DEFINE_CONST_OR_RETURN(number, ConsumeInteger());
  Integer result{};
  auto const [ptr, ec] =
      std::from_chars(number.data(), number.data() + number.size(), result, /*base=*/10);
  if (ec != std::errc()) {
    return InvalidFormatError();
  }
  return result;
}

template <typename Float, std::enable_if_t<std::is_floating_point_v<Float>, bool>>
absl::StatusOr<Float> Parser::ReadFloat() {
  ConsumeWhitespace();
  if (input_.empty()) {
    return InvalidSyntaxError();
  }
  DEFINE_CONST_OR_RETURN(number, ConsumeFloat());
  Float value{};
  auto const [ptr, ec] = absl::from_chars(number.data(), number.data() + number.size(), value,
                                          absl::chars_format::general);
  if (ec != std::errc()) {
    return InvalidFormatError();
  }
  return value;
}

namespace internal {

// Used by the object parsing algorithm at the end of parsing to check that all non-optional fields
// have been found in the input. The specializations provide `operator()` implementations returning
// true iff all required fields have been found. The `keys` argument is the set of keys found in the
// JSON input.
//
// Optional fields are defined as those with type `std::optional`, `std::unique_ptr`,
// `std::shared_ptr`, or `tsdb2::common::reffed_ptr`.
template <typename... Fields>
struct CheckFieldPresence;

template <>
struct CheckFieldPresence<> {
  ABSL_ATTRIBUTE_ALWAYS_INLINE bool operator()(
      tsdb2::common::flat_set<std::string> const& /*keys*/) const {
    return true;
  }
};

template <typename Type, typename Name, typename... OtherFields>
struct CheckFieldPresence<FieldImpl<Type, Name>, OtherFields...> {
  ABSL_ATTRIBUTE_ALWAYS_INLINE bool operator()(
      tsdb2::common::flat_set<std::string> const& keys) const {
    return keys.contains(Name::value) && CheckFieldPresence<OtherFields...>()(keys);
  }
};

template <typename OptionalType, typename Name, typename... OtherFields>
struct CheckFieldPresence<FieldImpl<std::optional<OptionalType>, Name>, OtherFields...> {
  ABSL_ATTRIBUTE_ALWAYS_INLINE bool operator()(
      tsdb2::common::flat_set<std::string> const& keys) const {
    return CheckFieldPresence<OtherFields...>()(keys);
  }
};

template <typename OptionalType, typename Name, typename... OtherFields>
struct CheckFieldPresence<FieldImpl<std::unique_ptr<OptionalType>, Name>, OtherFields...> {
  ABSL_ATTRIBUTE_ALWAYS_INLINE bool operator()(
      tsdb2::common::flat_set<std::string> const& keys) const {
    return CheckFieldPresence<OtherFields...>()(keys);
  }
};

template <typename OptionalType, typename Name, typename... OtherFields>
struct CheckFieldPresence<FieldImpl<std::shared_ptr<OptionalType>, Name>, OtherFields...> {
  ABSL_ATTRIBUTE_ALWAYS_INLINE bool operator()(
      tsdb2::common::flat_set<std::string> const& keys) const {
    return CheckFieldPresence<OtherFields...>()(keys);
  }
};

template <typename OptionalType, typename Name, typename... OtherFields>
struct CheckFieldPresence<FieldImpl<tsdb2::common::reffed_ptr<OptionalType>, Name>,
                          OtherFields...> {
  ABSL_ATTRIBUTE_ALWAYS_INLINE bool operator()(
      tsdb2::common::flat_set<std::string> const& keys) const {
    return CheckFieldPresence<OtherFields...>()(keys);
  }
};

}  // namespace internal

template <typename... Fields>
absl::StatusOr<Object<Fields...>> Parser::ReadObjectFields() {
  ConsumeWhitespace();
  RETURN_IF_ERROR(ExpectPrefix("{"));
  ConsumeWhitespace();
  tsdb2::common::flat_set<std::string> keys;
  if (ConsumePrefix("}")) {
    if (internal::CheckFieldPresence<Fields...>{}(keys)) {
      return Object<Fields...>();
    } else {
      return InvalidFormatError();
    }
  }
  Object<Fields...> result;
  keys.reserve(sizeof...(Fields));
  while (!input_.empty()) {
    DEFINE_CONST_OR_RETURN(key, ReadString());
    auto const [unused_it, inserted] = keys.emplace(key);
    if (!inserted) {
      return InvalidFormatError();  // duplicate key
    }
    ConsumeWhitespace();
    RETURN_IF_ERROR(RequirePrefix(":"));
    RETURN_IF_ERROR(result.ReadField(this, key));
    ConsumeWhitespace();
    if (ConsumePrefix(",")) {
      ConsumeWhitespace();
    } else if (ConsumePrefix("}")) {
      if (internal::CheckFieldPresence<Fields...>{}(keys)) {
        return std::move(result);
      } else {
        return InvalidFormatError();
      }
    } else {
      return InvalidSyntaxError();
    }
  }
  return InvalidSyntaxError();
}

template <typename Value, typename Representation>
absl::StatusOr<Representation> Parser::ReadDictionary() {
  ConsumeWhitespace();
  RETURN_IF_ERROR(ExpectPrefix("{"));
  ConsumeWhitespace();
  if (ConsumePrefix("}")) {
    return Representation();
  }
  Representation result;
  while (true) {
    DEFINE_VAR_OR_RETURN(key, ReadString());
    ConsumeWhitespace();
    RETURN_IF_ERROR(RequirePrefix(":"));
    Value value;
    RETURN_IF_ERROR(Tsdb2JsonParse(this, &value));
    auto const [unused_it, inserted] = result.try_emplace(std::move(key), std::move(value));
    if (!inserted) {
      return InvalidFormatError();  // duplicate key
    }
    ConsumeWhitespace();
    if (ConsumePrefix("}")) {
      return std::move(result);
    }
    RETURN_IF_ERROR(RequirePrefix(","));
    ConsumeWhitespace();
  }
}

template <typename Element>
absl::StatusOr<std::vector<Element>> Parser::ReadVector() {
  ConsumeWhitespace();
  RETURN_IF_ERROR(ExpectPrefix("["));
  ConsumeWhitespace();
  if (ConsumePrefix("]")) {
    return std::vector<Element>();
  }
  std::vector<Element> result;
  while (!input_.empty()) {
    Element element;
    RETURN_IF_ERROR(Tsdb2JsonParse(this, &element));
    result.emplace_back(std::move(element));
    ConsumeWhitespace();
    if (ConsumePrefix(",")) {
      ConsumeWhitespace();
    } else if (ConsumePrefix("]")) {
      return std::move(result);
    } else {
      return InvalidSyntaxError();
    }
  }
  return InvalidSyntaxError();
}

template <typename Element, size_t N>
absl::StatusOr<std::array<Element, N>> Parser::ReadArray() {
  std::array<Element, N> result;
  RETURN_IF_ERROR((ReadArray<Element, N>(&result)));
  return std::move(result);
}

template <typename Element, size_t N>
absl::Status Parser::ReadArray(std::array<Element, N>* const result) {
  ConsumeWhitespace();
  RETURN_IF_ERROR(ExpectPrefix("["));
  if constexpr (N > 0) {
    RETURN_IF_ERROR(Tsdb2JsonParse(this, result->data()));
  }
  for (size_t i = 1; i < N; ++i) {
    ConsumeWhitespace();
    RETURN_IF_ERROR(RequirePrefix(","));
    RETURN_IF_ERROR(Tsdb2JsonParse(this, &((*result)[i])));
  }
  ConsumeWhitespace();
  return RequirePrefix("]");
}

template <typename First, typename Second>
absl::StatusOr<std::pair<First, Second>> Parser::ReadPair() {
  ConsumeWhitespace();
  RETURN_IF_ERROR(ExpectPrefix("["));
  std::pair<First, Second> result;
  ConsumeWhitespace();
  RETURN_IF_ERROR(Tsdb2JsonParse(this, &result.first));
  ConsumeWhitespace();
  RETURN_IF_ERROR(RequirePrefix(","));
  ConsumeWhitespace();
  RETURN_IF_ERROR(Tsdb2JsonParse(this, &result.second));
  ConsumeWhitespace();
  RETURN_IF_ERROR(RequirePrefix("]"));
  return std::move(result);
}

template <typename... Values>
absl::StatusOr<std::tuple<Values...>> Parser::ReadTuple() {
  ConsumeWhitespace();
  RETURN_IF_ERROR(ExpectPrefix("["));
  std::tuple<Values...> result;
  RETURN_IF_ERROR(std::apply(
      [this](auto&... elements) {
        bool first = true;
        absl::Status status = absl::OkStatus();
        (void)((ConsumeWhitespace(),
                (first ? !(first = false) : (status.Update(RequirePrefix(",")), status.ok())) &&
                    (status.Update(Tsdb2JsonParse(this, &elements)), status.ok())) &&
               ...);
        return status;
      },
      result));
  ConsumeWhitespace();
  RETURN_IF_ERROR(RequirePrefix("]"));
  return std::move(result);
}

template <typename Value, typename Representation>
absl::StatusOr<Representation> Parser::ReadSet() {
  ConsumeWhitespace();
  RETURN_IF_ERROR(RequirePrefix("["));
  ConsumeWhitespace();
  if (ConsumePrefix("]")) {
    return Representation();
  }
  Representation result;
  while (true) {
    Value value;
    RETURN_IF_ERROR(Tsdb2JsonParse(this, &value));
    auto const [unused_it, inserted] = result.emplace(std::move(value));
    if (!inserted) {
      return InvalidFormatError();  // duplicate element
    }
    ConsumeWhitespace();
    if (ConsumePrefix("]")) {
      return std::move(result);
    }
    RETURN_IF_ERROR(RequirePrefix(","));
    ConsumeWhitespace();
  }
}

template <typename Value>
absl::StatusOr<std::optional<Value>> Parser::ReadValueOrNull() {
  ConsumeWhitespace();
  if (ConsumePrefix("null")) {
    return std::nullopt;
  } else {
    Value value;
    RETURN_IF_ERROR(Tsdb2JsonParse(this, &value));
    return std::make_optional<Value>(std::move(value));
  }
}

template <typename Value>
absl::Status Parser::ReadPointeeOrNull(std::unique_ptr<Value>* const ptr) {
  ConsumeWhitespace();
  if (ConsumePrefix("null")) {
    ptr->reset();
    return absl::OkStatus();
  } else {
    if (!*ptr) {
      *ptr = std::make_unique<Value>();
    }
    return Tsdb2JsonParse(this, ptr->get());
  }
}

// Low-level API to produce JSON outputs. Supports both formatted and compressed output.
//
// The syntax is described at https://www.json.org/.
class Stringifier final {
 public:
  explicit Stringifier(StringifyOptions const& options)
      : options_(options), line_feed_(MakeLineFeed(options_.line_feed_type)) {}

  StringifyOptions const& options() const { return options_; }

  void WriteNull();

  void WriteBoolean(bool value);

  template <typename Integer,
            std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
  void WriteInteger(Integer value);

  template <typename Float, std::enable_if_t<std::is_floating_point_v<Float>, bool> = true>
  void WriteFloat(Float value);

  void WriteString(std::string_view value);

  template <typename... Fields>
  void WriteObject(Object<Fields...> const& value);

  template <typename Representation>
  void WriteDictionary(Representation const& dictionary);

  template <typename FirstIterator, typename EndIterator>
  void WriteSequence(FirstIterator&& first, EndIterator&& end);

  template <typename First, typename Second>
  void WritePair(std::pair<First, Second> const& value);

  template <typename... Values>
  void WriteTuple(std::tuple<Values...> const& value);

  template <typename Representation>
  void WriteSet(Representation const& value);

  std::string Finish() &&;

 private:
  template <typename... Fields>
  friend class Object;

  static std::string MakeLineFeed(LineFeedType type);

  // Increases the current indentation level by 1.
  void Indent();

  // Decreases the current indentation level by 1.
  void Dedent();

  // Appends a number of tabs corresponding to the current indentation level to the output. Each
  // "tab" is actually two spaces.
  void WriteIndentation();

  template <typename Representation>
  void WriteDictionaryPretty(Representation const& dictionary);

  template <typename Representation>
  void WriteDictionaryCompressed(Representation const& dictionary);

  template <typename Iterator>
  void WriteSequencePretty(Iterator first, Iterator end);

  template <typename Iterator>
  void WriteSequenceCompressed(Iterator first, Iterator end);

  StringifyOptions const options_;
  std::string const line_feed_;

  // The current number of indentations.
  size_t indentation_level_ = 0;
  std::vector<absl::Cord> indentation_cords_;

  absl::Cord output_;
};

template <typename Integer, std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool>>
void Stringifier::WriteInteger(Integer value) {
  output_.Append(absl::StrCat(value));
}

template <typename Float, std::enable_if_t<std::is_floating_point_v<Float>, bool>>
void Stringifier::WriteFloat(Float const value) {
  output_.Append(absl::StrCat(value));
}

template <typename... Fields>
void Stringifier::WriteObject(Object<Fields...> const& value) {
  if constexpr (!sizeof...(Fields)) {
    output_.Append("{}");
    return;
  }
  output_.Append("{");
  if (options_.pretty) {
    Indent();
    value.WriteFieldsPretty(this, /*first_field=*/true);
    output_.Append(line_feed_);
    Dedent();
    WriteIndentation();
  } else {
    value.WriteFieldsCompressed(this, /*first_field=*/true);
  }
  output_.Append("}");
}

template <typename Representation>
void Stringifier::WriteDictionary(Representation const& dictionary) {
  if (options_.pretty) {
    WriteDictionaryPretty(dictionary);
  } else {
    WriteDictionaryCompressed(dictionary);
  }
}

template <typename FirstIterator, typename EndIterator>
void Stringifier::WriteSequence(FirstIterator&& first, EndIterator&& end) {
  if (options_.pretty) {
    WriteSequencePretty(std::forward<FirstIterator>(first), std::forward<EndIterator>(end));
  } else {
    WriteSequenceCompressed(std::forward<FirstIterator>(first), std::forward<EndIterator>(end));
  }
}

template <typename First, typename Second>
void Stringifier::WritePair(std::pair<First, Second> const& value) {
  output_.Append("[");
  Tsdb2JsonStringify(this, value.first);
  if (options_.pretty) {
    output_.Append(", ");
  } else {
    output_.Append(",");
  }
  Tsdb2JsonStringify(this, value.second);
  output_.Append("]");
}

template <typename... Values>
void Stringifier::WriteTuple(std::tuple<Values...> const& value) {
  output_.Append("[");
  std::apply(
      [this](auto const&... elements) {
        absl::Cord const comma{options_.pretty ? ", " : ","};
        bool first = true;
        ((first ? (void)(first = false) : output_.Append(comma),
          Tsdb2JsonStringify(this, elements)),
         ...);
      },
      value);
  output_.Append("]");
}

template <typename Representation>
void Stringifier::WriteSet(Representation const& value) {
  if (options_.pretty) {
    WriteSequencePretty(value.begin(), value.end());
  } else {
    WriteSequenceCompressed(value.begin(), value.end());
  }
}

template <typename Representation>
void Stringifier::WriteDictionaryPretty(Representation const& dictionary) {
  if (dictionary.empty()) {
    output_.Append("{}");
    return;
  }
  output_.Append("{");
  Indent();
  bool first = true;
  for (auto const& [key, value] : dictionary) {
    if (first) {
      first = false;
    } else {
      output_.Append(",");
    }
    output_.Append(line_feed_);
    WriteIndentation();
    WriteString(key);
    output_.Append(": ");
    Tsdb2JsonStringify(this, value);
  }
  output_.Append(line_feed_);
  Dedent();
  if (!first) {
    WriteIndentation();
  }
  output_.Append("}");
}

template <typename Representation>
void Stringifier::WriteDictionaryCompressed(Representation const& dictionary) {
  output_.Append("{");
  bool first = true;
  for (auto const& [key, value] : dictionary) {
    if (first) {
      first = false;
    } else {
      output_.Append(",");
    }
    WriteString(key);
    output_.Append(":");
    Tsdb2JsonStringify(this, value);
  }
  output_.Append("}");
}

template <typename Iterator>
void Stringifier::WriteSequencePretty(Iterator first, Iterator const end) {
  if (first == end) {
    output_.Append("[]");
    return;
  }
  output_.Append("[");
  output_.Append(line_feed_);
  Indent();
  WriteIndentation();
  Tsdb2JsonStringify(this, *first);
  for (++first; first != end; ++first) {
    output_.Append(",");
    output_.Append(line_feed_);
    WriteIndentation();
    Tsdb2JsonStringify(this, *first);
  }
  output_.Append(line_feed_);
  Dedent();
  WriteIndentation();
  output_.Append("]");
}

template <typename Iterator>
void Stringifier::WriteSequenceCompressed(Iterator first, Iterator const end) {
  if (first == end) {
    output_.Append("[]");
    return;
  }
  output_.Append("[");
  Tsdb2JsonStringify(this, *first);
  for (++first; first != end; ++first) {
    output_.Append(",");
    Tsdb2JsonStringify(this, *first);
  }
  output_.Append("]");
}

}  // namespace json
}  // namespace tsdb2

// Predefined implementations of `Tsdb2JsonParse` follow.

template <typename Integer, std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool>>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser, Integer* const result) {
  ASSIGN_OR_RETURN(*result, parser->ReadInteger<Integer>());
  return absl::OkStatus();
}

template <typename Float, std::enable_if_t<std::is_floating_point_v<Float>, bool>>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser, Float* const result) {
  ASSIGN_OR_RETURN(*result, parser->ReadFloat<Float>());
  return absl::OkStatus();
}

template <typename... Fields>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            tsdb2::json::Object<Fields...>* const result) {
  result->Clear();
  auto status_or_object = parser->ReadObjectFields<Fields...>();
  if (!status_or_object.ok()) {
    return std::move(status_or_object).status();
  }
  *result = std::move(status_or_object).value();
  return absl::OkStatus();
}

template <typename Value, typename Compare, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            std::map<std::string, Value, Compare, Allocator>* const result) {
  ASSIGN_OR_RETURN(
      *result, (parser->ReadDictionary<Value, std::map<std::string, Value, Compare, Allocator>>()));
  return absl::OkStatus();
}

template <typename Value, typename Hash, typename Equal, typename Allocator>
absl::Status Tsdb2JsonParse(
    tsdb2::json::Parser* const parser,
    std::unordered_map<std::string, Value, Hash, Equal, Allocator>* const result) {
  ASSIGN_OR_RETURN(
      *result,
      (parser->ReadDictionary<Value,
                              std::unordered_map<std::string, Value, Hash, Equal, Allocator>>()));
  return absl::OkStatus();
}

template <typename Value, typename Hash, typename Equal, typename Allocator>
absl::Status Tsdb2JsonParse(
    tsdb2::json::Parser* const parser,
    absl::flat_hash_map<std::string, Value, Hash, Equal, Allocator>* const result) {
  ASSIGN_OR_RETURN(
      *result,
      (parser->ReadDictionary<Value,
                              absl::flat_hash_map<std::string, Value, Hash, Equal, Allocator>>()));
  return absl::OkStatus();
}

template <typename Value, typename Hash, typename Equal, typename Allocator>
absl::Status Tsdb2JsonParse(
    tsdb2::json::Parser* const parser,
    absl::node_hash_map<std::string, Value, Hash, Equal, Allocator>* const result) {
  ASSIGN_OR_RETURN(
      *result,
      (parser->ReadDictionary<Value,
                              absl::node_hash_map<std::string, Value, Hash, Equal, Allocator>>()));
  return absl::OkStatus();
}

template <typename Value, typename Compare, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            absl::btree_map<std::string, Value, Compare, Allocator>* const result) {
  ASSIGN_OR_RETURN(
      *result,
      (parser->ReadDictionary<Value, absl::btree_map<std::string, Value, Compare, Allocator>>()));
  return absl::OkStatus();
}

template <typename Value, typename Compare, typename Allocator>
absl::Status Tsdb2JsonParse(
    tsdb2::json::Parser* const parser,
    tsdb2::common::flat_map<std::string, Value, Compare, Allocator>* const result) {
  ASSIGN_OR_RETURN(
      *result,
      (parser->ReadDictionary<Value,
                              tsdb2::common::flat_map<std::string, Value, Compare, Allocator>>()));
  return absl::OkStatus();
}
template <typename Value, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            tsdb2::common::trie_map<Value, Allocator>* const result) {
  ASSIGN_OR_RETURN(*result,
                   (parser->ReadDictionary<Value, tsdb2::common::trie_map<Value, Allocator>>()));
  return absl::OkStatus();
}

template <typename Element>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser, std::vector<Element>* const result) {
  ASSIGN_OR_RETURN(*result, parser->ReadVector<Element>());
  return absl::OkStatus();
}

template <typename Element, size_t N>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            std::array<Element, N>* const result) {
  return parser->ReadArray<Element, N>(result);
}

template <typename First, typename Second>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            std::pair<First, Second>* const result) {
  ASSIGN_OR_RETURN(*result, (parser->ReadPair<First, Second>()));
  return absl::OkStatus();
}

template <typename... Values>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            std::tuple<Values...>* const result) {
  ASSIGN_OR_RETURN(*result, (parser->ReadTuple<Values...>()));
  return absl::OkStatus();
}

template <typename Value, typename Compare, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            std::set<Value, Compare, Allocator>* const result) {
  ASSIGN_OR_RETURN(*result, (parser->ReadSet<Value, std::set<Value, Compare, Allocator>>()));
  return absl::OkStatus();
}

template <typename Value, typename Hash, typename Equal, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            std::unordered_set<Value, Hash, Equal, Allocator>* const result) {
  ASSIGN_OR_RETURN(*result,
                   (parser->ReadSet<Value, std::unordered_set<Value, Hash, Equal, Allocator>>()));
  return absl::OkStatus();
}

template <typename Value, typename Hash, typename Equal, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            absl::flat_hash_set<Value, Hash, Equal, Allocator>* const result) {
  ASSIGN_OR_RETURN(*result,
                   (parser->ReadSet<Value, absl::flat_hash_set<Value, Hash, Equal, Allocator>>()));
  return absl::OkStatus();
}

template <typename Value, typename Hash, typename Equal, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            absl::node_hash_set<Value, Hash, Equal, Allocator>* const result) {
  ASSIGN_OR_RETURN(*result,
                   (parser->ReadSet<Value, absl::node_hash_set<Value, Hash, Equal, Allocator>>()));
  return absl::OkStatus();
}

template <typename Value, typename Compare, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            absl::btree_set<Value, Compare, Allocator>* const result) {
  ASSIGN_OR_RETURN(*result, (parser->ReadSet<Value, absl::btree_set<Value, Compare, Allocator>>()));
  return absl::OkStatus();
}

template <typename Value, typename Compare, typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            tsdb2::common::flat_set<Value, Compare, Allocator>* const result) {
  ASSIGN_OR_RETURN(*result,
                   (parser->ReadSet<Value, tsdb2::common::flat_set<Value, Compare, Allocator>>()));
  return absl::OkStatus();
}

template <typename Allocator>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            tsdb2::common::trie_set<Allocator>* const result) {
  ASSIGN_OR_RETURN(*result, (parser->ReadSet<std::string, tsdb2::common::trie_set<Allocator>>()));
  return absl::OkStatus();
}

template <typename Value>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser, std::optional<Value>* const result) {
  ASSIGN_OR_RETURN(*result, parser->ReadValueOrNull<Value>());
  return absl::OkStatus();
}

template <typename Value>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            std::unique_ptr<Value>* const result) {
  return parser->ReadPointeeOrNull<Value>(result);
}

template <typename Value>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            std::shared_ptr<Value>* const result) {
  std::unique_ptr<Value> ptr;
  RETURN_IF_ERROR(parser->ReadPointeeOrNull(&ptr));
  *result = std::move(ptr);
  return absl::OkStatus();
}

template <typename Value>
absl::Status Tsdb2JsonParse(tsdb2::json::Parser* const parser,
                            tsdb2::common::reffed_ptr<Value>* const result) {
  std::unique_ptr<Value> ptr;
  RETURN_IF_ERROR(parser->ReadPointeeOrNull(&ptr));
  *result = std::move(ptr);
  return absl::OkStatus();
}

// Predefined implementations of `Tsdb2JsonStringify`.

template <typename Integer, std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool>>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier, Integer const value) {
  stringifier->WriteInteger(value);
}

template <typename Float, std::enable_if_t<std::is_floating_point_v<Float>, bool>>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier, Float const value) {
  stringifier->WriteFloat(value);
}

template <typename... Fields>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        tsdb2::json::Object<Fields...> const& value) {
  stringifier->WriteObject(value);
}

template <typename Value, typename Compare, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        std::map<std::string, Value, Compare, Allocator> const& value) {
  stringifier->WriteDictionary(value);
}

template <typename Value, typename Hash, typename Equal, typename Allocator>
void Tsdb2JsonStringify(
    tsdb2::json::Stringifier* const stringifier,
    std::unordered_map<std::string, Value, Hash, Equal, Allocator> const& value) {
  stringifier->WriteDictionary(value);
}

template <typename Value, typename Hash, typename Equal, typename Allocator>
void Tsdb2JsonStringify(
    tsdb2::json::Stringifier* const stringifier,
    absl::flat_hash_map<std::string, Value, Hash, Equal, Allocator> const& value) {
  stringifier->WriteDictionary(value);
}

template <typename Value, typename Hash, typename Equal, typename Allocator>
void Tsdb2JsonStringify(
    tsdb2::json::Stringifier* const stringifier,
    absl::node_hash_map<std::string, Value, Hash, Equal, Allocator> const& value) {
  stringifier->WriteDictionary(value);
}

template <typename Value, typename Compare, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        absl::btree_map<std::string, Value, Compare, Allocator> const& value) {
  stringifier->WriteDictionary(value);
}

template <typename Value, typename Compare, typename Allocator>
void Tsdb2JsonStringify(
    tsdb2::json::Stringifier* const stringifier,
    tsdb2::common::flat_map<std::string, Value, Compare, Allocator> const& value) {
  stringifier->WriteDictionary(value);
}

template <typename Value, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        tsdb2::common::trie_map<Value, Allocator> const& value) {
  stringifier->WriteDictionary(value);
}

template <typename Element>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        std::vector<Element> const& value) {
  stringifier->WriteSequence(value.begin(), value.end());
}

template <typename Element, size_t N>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        std::array<Element, N> const& value) {
  stringifier->WriteSequence(value.begin(), value.end());
}

template <typename First, typename Second>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        std::pair<First, Second> const& value) {
  stringifier->WritePair(value);
}

template <typename... Elements>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        std::tuple<Elements...> const& value) {
  stringifier->WriteTuple(value);
}

template <typename Value, typename Compare, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        std::set<Value, Compare, Allocator> const& value) {
  stringifier->WriteSet(value);
}

template <typename Value, typename Hash, typename Equal, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        std::unordered_set<Value, Hash, Equal, Allocator> const& value) {
  stringifier->WriteSet(value);
}

template <typename Value, typename Hash, typename Equal, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        absl::flat_hash_set<Value, Hash, Equal, Allocator> const& value) {
  stringifier->WriteSet(value);
}

template <typename Value, typename Hash, typename Equal, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        absl::node_hash_set<Value, Hash, Equal, Allocator> const& value) {
  stringifier->WriteSet(value);
}

template <typename Value, typename Compare, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        absl::btree_set<Value, Compare, Allocator> const& value) {
  stringifier->WriteSet(value);
}

template <typename Value, typename Compare, typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        tsdb2::common::flat_set<Value, Compare, Allocator> const& value) {
  stringifier->WriteSet(value);
}

template <typename Allocator>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        tsdb2::common::trie_set<Allocator> const& value) {
  stringifier->WriteSet(value);
}

template <typename Value>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        std::optional<Value> const& value) {
  if (value) {
    Tsdb2JsonStringify(stringifier, *value);
  } else {
    stringifier->WriteNull();
  }
}

template <typename Value>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        std::unique_ptr<Value> const& value) {
  if (value) {
    Tsdb2JsonStringify(stringifier, *value);
  } else {
    stringifier->WriteNull();
  }
}

template <typename Value>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        std::shared_ptr<Value> const& value) {
  if (value) {
    Tsdb2JsonStringify(stringifier, *value);
  } else {
    stringifier->WriteNull();
  }
}

template <typename Value>
void Tsdb2JsonStringify(tsdb2::json::Stringifier* const stringifier,
                        tsdb2::common::reffed_ptr<Value> const& value) {
  if (value) {
    Tsdb2JsonStringify(stringifier, *value);
  } else {
    stringifier->WriteNull();
  }
}

namespace tsdb2 {
namespace json {

inline std::string Object<>::Stringify(StringifyOptions const& options) const {
  Stringifier stringifier{options};
  stringifier.WriteObject(*this);
  return std::move(stringifier).Finish();
}

template <typename Type, typename Name, typename... OtherFields>
inline std::string Object<internal::FieldImpl<Type, Name>, OtherFields...>::Stringify(
    StringifyOptions const& options) const {
  Stringifier stringifier{options};
  stringifier.WriteObject(*this);
  return std::move(stringifier).Finish();
}

inline absl::Status Object<>::ReadField(  // NOLINT(readability-convert-member-functions-to-static)
    Parser* const parser, std::string_view const key) {
  auto const& options = parser->options();
  if (options.allow_extra_fields) {
    return parser->SkipField(/*fast=*/options.fast_skipping);
  } else {
    return absl::InvalidArgumentError(absl::StrCat("invalid field \"", absl::CEscape(key), "\""));
  }
}

template <typename Type, typename Name, typename... OtherFields>
inline absl::Status Object<internal::FieldImpl<Type, Name>, OtherFields...>::ReadField(
    Parser* const parser, std::string_view const key) {
  if (key != Name::value) {
    return Object<OtherFields...>::ReadField(parser, key);
  } else {
    return Tsdb2JsonParse(parser, &value_);
  }
}

namespace internal {

// FOR INTERNAL USE. This algorithm decides whether a field must be written out in the output of a
// stringifier or nor. It's specialized for optional / nullable types because we don't output
// anything when a field value is null or an empty optional.
template <typename Type>
struct FieldDecider {
  bool operator()(Stringifier* const stringifier, Type const& value) const { return true; }
};

template <typename Inner>
struct FieldDecider<std::optional<Inner>> {
  bool operator()(Stringifier* const stringifier, std::optional<Inner> const& value) const {
    return value.has_value() || stringifier->options().output_empty_fields;
  }
};

template <typename Inner>
struct FieldDecider<std::unique_ptr<Inner>> {
  bool operator()(Stringifier* const stringifier, std::unique_ptr<Inner> const& value) const {
    return value != nullptr || stringifier->options().output_empty_fields;
  }
};

template <typename Inner>
struct FieldDecider<std::shared_ptr<Inner>> {
  bool operator()(Stringifier* const stringifier, std::shared_ptr<Inner> const& value) const {
    return value != nullptr || stringifier->options().output_empty_fields;
  }
};

template <typename Inner>
struct FieldDecider<tsdb2::common::reffed_ptr<Inner>> {
  bool operator()(Stringifier* const stringifier,
                  tsdb2::common::reffed_ptr<Inner> const& value) const {
    return value != nullptr || stringifier->options().output_empty_fields;
  }
};

}  // namespace internal

template <typename Type, typename Name, typename... OtherFields>
inline void Object<internal::FieldImpl<Type, Name>, OtherFields...>::WriteFieldsPretty(
    Stringifier* const stringifier, bool const first_field) const {
  if (!internal::FieldDecider<Type>{}(stringifier, value_)) {
    return Object<OtherFields...>::WriteFieldsPretty(stringifier, first_field);
  }
  if (!first_field) {
    stringifier->output_.Append(",");
  }
  stringifier->output_.Append(stringifier->line_feed_);
  stringifier->WriteIndentation();
  stringifier->WriteString(Name::value);
  stringifier->output_.Append(": ");
  Tsdb2JsonStringify(stringifier, value_);
  Object<OtherFields...>::WriteFieldsPretty(stringifier, false);
}

template <typename Type, typename Name, typename... OtherFields>
inline void Object<internal::FieldImpl<Type, Name>, OtherFields...>::WriteFieldsCompressed(
    Stringifier* const stringifier, bool const first_field) const {
  if (!internal::FieldDecider<Type>{}(stringifier, value_)) {
    return Object<OtherFields...>::WriteFieldsCompressed(stringifier, first_field);
  }
  if (!first_field) {
    stringifier->output_.Append(",");
  }
  stringifier->WriteString(Name::value);
  stringifier->output_.Append(":");
  Tsdb2JsonStringify(stringifier, value_);
  Object<OtherFields...>::WriteFieldsCompressed(stringifier, false);
}

template <typename Value>
absl::StatusOr<Value> Parse(std::string_view const input, ParseOptions const& options) {
  return Parser(input, options).Parse<Value>();
}

template <typename Value>
absl::StatusOr<Value> Parse(std::string_view const input) {
  return Parser(input, ParseOptions{}).Parse<Value>();
}

template <typename Value>
std::string Stringify(Value&& value, StringifyOptions const& options) {
  Stringifier stringifier{options};
  Tsdb2JsonStringify(&stringifier, std::forward<Value>(value));
  return std::move(stringifier).Finish();
}

template <typename Value>
std::string Stringify(Value&& value) {
  return Stringify(std::forward<Value>(value), StringifyOptions());
}

}  // namespace json
}  // namespace tsdb2

#endif  // __TSDB2_JSON_JSON_H__
