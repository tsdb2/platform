#ifndef __TSDB2_COMMON_LOCK_FREE_CONTAINER_INTERNAL_H__
#define __TSDB2_COMMON_LOCK_FREE_CONTAINER_INTERNAL_H__

#include <type_traits>

namespace tsdb2 {
namespace common {
namespace internal {
namespace lock_free_container {

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
