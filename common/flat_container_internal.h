#ifndef __TSDB2_COMMON_FLAT_CONTAINER_INTERNAL_H__
#define __TSDB2_COMMON_FLAT_CONTAINER_INTERNAL_H__

#include <array>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

namespace tsdb2 {
namespace common {

// This token is used in overload resolution to select flat container constructors that are
// initialized with pre-sorted backing containers.
//
// Example:
//
//   std::vector<int> v{3, 1, 2};
//   std::sort(v.begin(), v.end());
//   flat_set<int> fs{kSortedDeduplicatedContainer, std::move(v)};
//
struct SortedDeduplicatedContainer {};
inline SortedDeduplicatedContainer constexpr kSortedDeduplicatedContainer;

namespace internal {

template <typename Key>
struct DefaultCompare {
  using Type = std::less<Key>;
};

template <>
struct DefaultCompare<std::string> {
  using Type = std::less<void>;
};

template <typename Key>
using DefaultCompareT = typename DefaultCompare<Key>::Type;

namespace flat_container {

template <typename Compare, typename IsTransparent = void>
struct key_arg {
  template <typename KeyType, typename KeyArg>
  using type = KeyType;
};

template <typename Compare>
struct key_arg<Compare, std::void_t<typename Compare::is_transparent>> {
  template <typename KeyType, typename KeyArg>
  using type = KeyArg;
};

}  // namespace flat_container

// `HasAllocator` is used in SFINAE to determine whether the underlying representation of a flat
// container uses allocators. For example, std::vector uses allocators but std::array doesn't. Based
// on this condition, flat containers will or will not add extra members such as `get_allocator`.
template <typename Representation, typename = void>
struct HasAllocator : public std::false_type {};

template <typename Representation>
struct HasAllocator<Representation, std::void_t<typename Representation::allocator_type>>
    : public std::true_type {};

template <typename Representation>
inline bool constexpr HasAllocatorV = HasAllocator<Representation>::value;

struct EmptyInitializerList {
  constexpr EmptyInitializerList() = default;
};

// This template struct contains a constexpr version of std::swap, needed because std::swap itself
// is not constexpr until C++20.
//
// TODO: remove when migrating to C++20.
template <typename T>
struct ConstexprHelper;

template <typename T>
struct ConstexprHelper {
  static constexpr void Swap(T& lhs, T& rhs) {
    T temp = static_cast<T&&>(lhs);
    lhs = static_cast<T&&>(rhs);
    rhs = static_cast<T&&>(temp);
  }
};

// We need an ad hoc specialization for std::pair because std::pair is not constexpr-movable or
// -copyable until C++20.
template <typename First, typename Second>
struct ConstexprHelper<std::pair<First, Second>> {
  static constexpr void Swap(std::pair<First, Second>& lhs, std::pair<First, Second>& rhs) {
    ConstexprHelper<First>::Swap(lhs.first, rhs.first);
    ConstexprHelper<Second>::Swap(lhs.second, rhs.second);
  }
};

// Constexpr routine to initialize fixed flat containers (cfr. `fixed_flat_*_of`).
//
// No need to be super efficient, a simple O(N^2) selection sort will do.
//
// TODO: `std::sort` is constexpr in C++20, consider migrating.
template <typename T, size_t N, typename Compare>
constexpr void ConstexprSort(std::array<T, N>& array, Compare const& cmp) {
  for (size_t i = 0; i < N - 1; ++i) {
    size_t j = i;
    for (size_t k = i + 1; k < N; ++k) {
      if (cmp(array[k], array[j])) {
        j = k;
      }
    }
    if (j != i) {
      ConstexprHelper<T>::Swap(array[i], array[j]);
    }
  }
}

// Aborts the program if a fixed flat container is mistakenly being initialized with duplicate
// elements.
template <typename T, size_t N, typename Compare>
constexpr void ConstexprCheckDuplications(std::array<T, N> const& array, Compare const& cmp) {
  for (size_t i = 1; i < N; ++i) {
    if (!cmp(array[i - 1], array[i])) {
      std::abort();
    }
  }
}

}  // namespace internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_FLAT_CONTAINER_INTERNAL_H__
