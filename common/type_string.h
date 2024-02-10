#ifndef __TSDB2_COMMON_TYPE_STRING_H__
#define __TSDB2_COMMON_TYPE_STRING_H__

#include <cstddef>
#include <string_view>

namespace tsdb2 {
namespace common {

template <char const... ch>
struct TypeStringMatcher {
 private:
  static char constexpr array[] = {ch...};

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
