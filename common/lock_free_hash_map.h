#ifndef __TSDB2_COMMON_LOCK_FREE_HASH_MAP_H__
#define __TSDB2_COMMON_LOCK_FREE_HASH_MAP_H__

#include "common/raw_lock_free_hash.h"

namespace tsdb2 {
namespace common {

// Used to annotate `lock_free_hash_map` values when they are known to be thread-safe. Examples:
//
//   // Foo is thread-safe.
//   struct Foo {
//     // ...
//   };
//
//   // Bar is not thread-safe.
//   struct Bar {
//     // ...
//   };
//
//   lock_free_hash_map<int, KnownThreadSafe<Foo>> foo_map;
//
//   lock_free_hash_map<int, Bar> bar_map;
//
// When the mapped value is thread-safe `lock_free_hash_map` will embed it in its nodes as it is,
// while if it's not thread-safe it will also add an `absl::Mutex` to guard it during certain
// operations. All read paths will still be lock-free, but operations like updating the value
// associated to a key will acquire the mutex.
//
// NOTE: if a type is not thread-safe you can still mark it as `KnownThreadSafe` if it's const. That
// way you'll avoid the mutex. Another option for some types is to wrap them in `std::atomic`.
// Examples:
//
//   lock_free_hash_map<int, int> locked_int_map;  // this one has a mutex in each element
//   lock_free_hash_map<int, KnownThreadSafe<int const>> int_map;
//   lock_free_hash_map<int, KnownThreadSafe<std::atomic<int>>> int_map;
//
template <typename Value>
using KnownThreadSafe = internal::lock_free_container::KnownThreadSafe<Value>;

// TODO

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_LOCK_FREE_HASH_MAP_H__
