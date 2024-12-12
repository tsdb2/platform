// Backport of std::to_array (see https://en.cppreference.com/w/cpp/container/array/to_array).

#ifndef __TSDB2_COMMON_TO_ARRAY_H__
#define __TSDB2_COMMON_TO_ARRAY_H__

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace tsdb2 {
namespace common {

namespace detail {

template <typename T, size_t N, size_t... I>
constexpr std::array<std::remove_cv_t<T>, N> to_array_impl(
    T (&a)[N], std::index_sequence<I...> /*index_sequence*/) {
  return {{a[I]...}};
}

template <typename T, size_t N, size_t... I>
constexpr std::array<std::remove_cv_t<T>, N> to_array_impl(
    T (&&a)[N], std::index_sequence<I...> /*index_sequence*/) {
  return {{std::move(a[I])...}};
}

}  // namespace detail

template <typename T, size_t N>
constexpr auto to_array(T (&a)[N]) {
  return detail::to_array_impl(a, std::make_index_sequence<N>{});
}

template <typename T, size_t N>
constexpr auto to_array(T (&&a)[N]) {
  return detail::to_array_impl(std::move(a), std::make_index_sequence<N>{});
}

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_TO_ARRAY_H__
