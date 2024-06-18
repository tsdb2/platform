#ifndef __TSDB2_COMMON_LOCK_FREE_HASH_MAP_H__
#define __TSDB2_COMMON_LOCK_FREE_HASH_MAP_H__

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <utility>

#include "common/lock_free_container_internal.h"
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
// while if it's not thread-safe it will embed it as const, preventing certain operations. Examples:
//
//   // The values of this map are not modifiable. Whole entries can still be erased.
//   lock_free_hash_map<int, int> int_map;
//
//   // The values of this map can be modified using atomic operations.
//   lock_free_hash_map<int, KnownThreadSafe<std::atomic<int>>> atomic_int_map;
//
template <typename Value>
using KnownThreadSafe = internal::lock_free_container::KnownThreadSafe<Value>;

// A lock-free, thread-safe hash map data structure. The provided API is a subset of
// `std::unordered_map`.
//
// All reads (lookups and iterations) are lockless: all synchronization is performed by
// `lock_free_hash_map` using atomics. Writers are automatically serialized by acquiring an
// exclusive lock on an internal mutex.
//
// Under heavily contended read scenarios `lock_free_hash_map` can be much faster than other hash
// map data structures guarded by a mutex. On the flip side, `lock_free_hash_map` does not free any
// memory upon element erasure, as doing so is infeasible with atomic-based synchronization. The
// memory used by the elements and internal element arrays is freed up only at destruction time, so
// you should use `lock_free_hash_map` only if you don't need to perform many erasures.
//
// NOTE: iterations are loosely consistent. If a rehash occurs during an iteration it is quite
// possible that the iterator misses some elements and/or returns some elements more than once.
// However it will never return invalid or corrupted data.
//
// To reduce the number of heap allocations and increase cache friendliness, `lock_free_hash_map`
// uses quadratic open addressing. To speed up lookups even further, values are pre-hashed so that a
// hash doesn't have to be re-calculated for every colliding element and the probing algorithm can
// short-circuit and avoid a full comparison of every colliding element.
template <typename Key, typename Value,
          typename Hash = internal::lock_free_container::DefaultHashT<Key>,
          typename Equal = internal::lock_free_container::DefaultEqT<Key>,
          typename Allocator = std::allocator<Key>>
class lock_free_hash_map
    : private internal::lock_free_container::RawLockFreeHash<Key, Value, Hash, Equal, Allocator> {
 private:
  using Base = internal::lock_free_container::RawLockFreeHash<Key, Value, Hash, Equal, Allocator>;

 public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = typename Base::ValueType;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using hasher = Hash;
  using key_equal = Equal;
  using allocator_type = typename Base::AllocatorType;
  using allocator_traits = typename Base::AllocatorTraits;
  using reference = value_type&;
  using const_reference = value_type const&;
  using pointer = typename allocator_traits::pointer;
  using const_pointer = typename allocator_traits::const_pointer;
  using iterator = typename Base::Iterator;
  using const_iterator = typename Base::ConstIterator;

  template <typename Arg>
  using key_arg_t =
      typename internal::lock_free_container::key_arg<Hash, Equal>::template type<key_type, Arg>;

  lock_free_hash_map() = default;

  explicit lock_free_hash_map(Hash const& hash, Equal const& equal = Equal(),
                              Allocator const& alloc = Allocator())
      : Base(hash, equal, alloc) {}

  explicit lock_free_hash_map(Allocator const& alloc)
      : lock_free_hash_map(Hash(), Equal(), alloc) {}

  lock_free_hash_map(Hash const& hash, Allocator const& alloc)
      : lock_free_hash_map(hash, Equal(), alloc) {}

  // TODO

  using Base::begin;
  using Base::cbegin;
  using Base::cend;
  using Base::end;
  using Base::get_alloc;

  // Returns the number of available slots in the hash map.
  //
  // WARNING: the value returned by this function is purely advisory. By the time the function
  // returns, the data structure may have been rehashed any number of times. If you need to know the
  // exact capacity you need to implement your own synchronization (typically using a mutex).
  using Base::capacity;

  // Ensures the hash map has room for at least `size` elements, including some extra space to
  // account for the maximum load factor. This is a no-op if the hash map already has enough
  // capacity.
  void reserve(size_type const size) { Base::Reserve(size); }

  // Returns the number of elements in the hash set.
  //
  // WARNING: the value returned by this function is purely advisory. By the time the function
  // returns, any number of changes may have occurred in parallel. If you need to know the exact
  // number of elements you need to implement your own synchronization (typically using a mutex).
  using Base::size;

  // Indicates whether the hash map is empty. Equivalent to `size() == 0`.
  //
  // WARNING: this function relies on `size()`, so the returned value is purely advisory. By the
  // time the function returns, any number of changes may have occurred in parallel. If you need to
  // know the exact number of elements you need to implement your own synchronization (typically
  // using a mutex).
  [[nodiscard]] bool empty() const noexcept { return Base::is_empty(); }

  // Erases all elements from the hash map.
  //
  // NOTE: as explained above, the memory taken by the removed elements is not actually freed. This
  // function will simply cause the hash map as whole to no longer point to any previous slot array.
  void clear() noexcept { Base::Clear(); }

  std::pair<iterator, bool> insert(value_type const& value) { return Base::Insert(value); }

  template <class InputIt>
  void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      Base::Insert(*first);
    }
  }

  void insert(std::initializer_list<value_type> const list) {
    Base::ReserveExtra(list.end() - list.begin());
    for (auto& value : list) {
      Base::Insert(value);
    }
  }

  // TODO

  template <typename KeyArg = key_type>
  size_type count(key_arg_t<KeyArg> const& key) const {
    return Base::is_end_iterator(Base::Find(key)) ? 0 : 1;
  }

  template <typename KeyArg = key_type>
  iterator find(key_arg_t<KeyArg> const& key) {
    return Base::Find(key);
  }

  template <typename KeyArg = key_type>
  const_iterator find(key_arg_t<KeyArg> const& key) const {
    return Base::Find(key);
  }

  template <typename KeyArg = key_type>
  bool contains(key_arg_t<KeyArg> const& key) const {
    return !Base::is_end_iterator(Base::Find(key));
  }

  // Swaps the content of this hash set with `other`. This algorithm is not lockless.
  void swap(lock_free_hash_map& other) { Base::Swap(other); }
};

}  // namespace common
}  // namespace tsdb2

namespace std {

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
void swap(::tsdb2::common::lock_free_hash_map<Key, Value, Hash, Equal, Allocator>& lhs,
          ::tsdb2::common::lock_free_hash_map<Key, Value, Hash, Equal, Allocator>& rhs) {
  lhs.swap(rhs);
}

}  // namespace std

#endif  // __TSDB2_COMMON_LOCK_FREE_HASH_MAP_H__
