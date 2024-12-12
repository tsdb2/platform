#ifndef __TSDB2_COMMON_LOCK_FREE_CONTAINER_INTERNAL_H__
#define __TSDB2_COMMON_LOCK_FREE_CONTAINER_INTERNAL_H__

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>

#include "absl/hash/hash.h"

namespace tsdb2 {
namespace common {
namespace internal {
namespace lock_free_container {

template <typename Hash, typename Equal, typename HasIsTransparent = void,
          typename EqualIsTransparent = void>
struct HashEqAreTransparent : std::false_type {};

template <typename Hash, typename Equal>
struct HashEqAreTransparent<Hash, Equal, std::void_t<typename Hash::is_transparent>,
                            std::void_t<typename Equal::is_transparent>> : std::true_type {};

template <typename Hash, typename Equal>
inline bool constexpr HashEqAreTransparentV = HashEqAreTransparent<Hash, Equal>::value;

template <typename Key>
struct DefaultHashEq {
  using Hash = absl::Hash<Key>;
  using Eq = std::equal_to<Key>;
};

template <>
struct DefaultHashEq<std::string> {
  struct Hash {
    using is_transparent = void;
    size_t operator()(std::string_view const value) const { return absl::HashOf(value); }
  };
  struct Eq {
    using is_transparent = void;
    bool operator()(std::string_view const lhs, std::string_view const rhs) const {
      return lhs == rhs;
    }
  };
};

template <typename Key>
using DefaultHashT = typename DefaultHashEq<Key>::Hash;

template <typename Key>
using DefaultEqT = typename DefaultHashEq<Key>::Eq;

template <typename Hash, typename Equal, typename HashIsTransparent = void,
          typename EqualIsTransparent = void>
struct key_arg {
  template <typename KeyType, typename KeyArg>
  using type = KeyType;
};

template <typename Hash, typename Equal>
struct key_arg<Hash, Equal, std::void_t<typename Hash::is_transparent>,
               std::void_t<typename Equal::is_transparent>> {
  template <typename KeyType, typename KeyArg>
  using type = KeyArg;
};

}  // namespace lock_free_container
}  // namespace internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_LOCK_FREE_CONTAINER_INTERNAL_H__
