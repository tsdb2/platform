// This header provides type strings, a tool to pass strings as template arguments. Under the hood
// it converts a char array to a template parameter pack of chars.
//
// The `TypeStringMatcher` template has the char parameter pack and can be used to match type
// strings in your template specializations, while the `TypeString` template and the `TypeStringT`
// convenience alias can be used to convert a char array to the parameter pack.
//
// Example:
//
//   template <typename Text>
//   struct Message;
//
//   template <char... text>
//   struct Message<TypeStringMatcher<text...>> {
//     void Print() {
//       std::cout << TypeStringMatcher<text...>::value << std::endl;
//     }
//   };
//
//   char constexpr kHelloMessage[] = "Hello!";
//
//   Message<TypeStringT<kHelloMessage>> m;
//   m.Print();
//
//
// In the above example, `TypeStringT<kHelloMessage>` is aliased to
// `TypeStringMatcher<'H', 'e', 'l', 'l', 'o', '!'>`.
//
// NOTE: unfortunately C++ doesn't allow passing string literals to template parameters directly, so
// writing things like `TypeStringT<"Hello!">` results in a syntax error. It's required that the
// array provided to `TypeString` and `TypeStringT` is an actual linker symbol, so it has to be
// declared as a constant in namespace scope. It must also be constexpr, otherwise the compiler
// won't be able to scan the characters at compile time.
//
// By converting the char array to a parameter pack, the type string implementation will deduplicate
// arrays with different linker symbols containing the same characters. Example:
//
//   char constexpr kString1[] = "lorem ipsum";
//   char constexpr kString2[] = "lorem ipsum";
//   assert(std::is_same_v<TypeStringT<kString1>, TypeStringT<kString2>>);
//
// The `TSDB2_TYPE_STRING` macro allows creating type string types without creating a linker symbol
// for the string, but the length of the literals is limited to 80 characters. It works by expanding
// to a template instantiation containing the provided string literal 80 times, using the subscript
// operator to select a different character each time, padding with zeroes wherever the index
// exceeds the length of the literal, and then removing the padding. It's recommended to use it only
// for short strings.

#ifndef __TSDB2_COMMON_TYPE_STRING_H__
#define __TSDB2_COMMON_TYPE_STRING_H__

#include <cstddef>
#include <string_view>

namespace tsdb2 {
namespace common {

template <char const... ch>
struct TypeStringMatcher {
 private:
  static char constexpr array[sizeof...(ch) + 1] = {ch..., 0};

 public:
  static std::string_view constexpr value{array, sizeof...(ch)};
};

namespace type_string_internal {

template <typename Matcher, char ch>
struct Append;

template <char... ch>
struct Reverse;

template <char ch>
struct Append<TypeStringMatcher<>, ch> {
  using Matcher = TypeStringMatcher<ch>;
};

template <char... ch, char last>
struct Append<TypeStringMatcher<ch...>, last> {
  using Matcher = TypeStringMatcher<ch..., last>;
};

template <>
struct Reverse<> {
  using Matcher = TypeStringMatcher<>;
};

template <char first, char... rest>
struct Reverse<first, rest...> {
  using Matcher = typename Append<typename Reverse<rest...>::Matcher, first>::Matcher;
};

template <char const str[], size_t offset, char... ch>
struct Scan;

template <char const str[], size_t offset>
struct Scan<str, offset> {
  using Matcher = typename Scan<str, offset + 1, str[offset]>::Matcher;
};

template <char const str[], size_t offset, char const... chars>
struct Scan<str, offset, 0, chars...> {
  using Matcher = typename Reverse<chars...>::Matcher;
};

template <char const str[], size_t offset, char const first, char const... rest>
struct Scan<str, offset, first, rest...> {
  using Matcher = typename Scan<str, offset + 1, str[offset], first, rest...>::Matcher;
};

}  // namespace type_string_internal

template <char const str[]>
struct TypeString {
  using Matcher = typename type_string_internal::Scan<str, 0>::Matcher;
};

template <char const str[]>
using TypeStringT = typename TypeString<str>::Matcher;

namespace type_string_internal {

template <size_t length>
constexpr char CharAt(char const array[], size_t const index) {
  return index < length ? array[index] : 0;
}

template <char... ch>
struct RemovePadding;

template <>
struct RemovePadding<> {
  using Matcher = TypeStringMatcher<>;
};

template <char... ch>
struct RemovePadding<0, ch...> {
  using Matcher = typename RemovePadding<ch...>::Matcher;
};

template <char first, char... ch>
struct RemovePadding<first, ch...> {
  using Matcher = typename Reverse<first, ch...>::Matcher;
};

template <char... ch>
using RemovePaddingT = typename RemovePadding<ch...>::Matcher;

}  // namespace type_string_internal

#define TSDB2_TYPE_STRING(string)                                                \
  ::tsdb2::common::type_string_internal::RemovePaddingT<                         \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 79), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 78), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 77), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 76), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 75), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 74), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 73), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 72), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 71), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 70), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 69), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 68), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 67), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 66), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 65), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 64), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 63), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 62), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 61), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 60), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 59), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 58), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 57), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 56), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 55), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 54), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 53), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 52), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 51), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 50), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 49), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 48), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 47), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 46), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 45), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 44), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 43), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 42), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 41), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 40), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 39), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 38), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 37), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 36), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 35), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 34), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 33), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 32), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 31), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 30), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 29), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 28), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 27), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 26), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 25), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 24), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 23), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 22), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 21), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 20), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 19), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 18), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 17), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 16), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 15), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 14), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 13), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 12), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 11), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 10), \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 9),  \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 8),  \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 7),  \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 6),  \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 5),  \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 4),  \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 3),  \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 2),  \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 1),  \
      ::tsdb2::common::type_string_internal::CharAt<sizeof(string)>(string, 0)>

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_TYPE_STRING_H__
