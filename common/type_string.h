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

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_TYPE_STRING_H__
