#ifndef __TSDB2_COMMON_FIXED_H__
#define __TSDB2_COMMON_FIXED_H__

#include <utility>

namespace tsdb2 {
namespace common {

template <typename T, typename Unused>
struct Fixed {
  using Type = T;
  static T value(T&& t) { return std::move(t); }
};

template <typename T, typename Unused>
using FixedT = typename Fixed<T, Unused>::Type;

template <typename Unused, typename T>
inline constexpr T FixedV(T&& t) {
  return Fixed<T, Unused>::value(std::forward<T>(t));
}

template <typename Unused>
struct FixedTrue {
  static inline bool constexpr value = true;
};

template <typename Unused>
inline bool constexpr FixedTrueV = FixedTrue<Unused>::value;

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_FIXED_H__
