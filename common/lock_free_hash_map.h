#ifndef __TSDB2_COMMON_LOCK_FREE_HASH_MAP_H__
#define __TSDB2_COMMON_LOCK_FREE_HASH_MAP_H__

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>
#include <vector>

#include "common/lock_free_container_internal.h"
#include "common/raw_lock_free_hash.h"

namespace tsdb2 {
namespace common {

// A lock-free, thread-safe hash map data structure. The provided API is a subset of
// `std::unordered_map`.
//
// All reads (lookups and iterations) are lockless: all synchronization is performed by
// `lock_free_hash_map` using atomics. Writers are automatically serialized by acquiring an
// exclusive lock on an internal mutex.
//
// NOTE: thread safety of the mapped values is delegated to the user. Certain operations such as
// assigning to a looked up value are not thread-safe if the `Value` type itself is not thread-safe.
// Examples:
//
//   lock_free_hash_map<int, int> int_map1;
//   int_map[42] = 123;  // ERROR: this operation is not thread-safe!
//
//   lock_free_hash_map<int, int const> int_map2;
//   int_map.try_emplace(42, 123);  // this is fine, but the value cannot be modified.
//
//   lock_free_hash_map<int, std::atomic<int>> int_map3;
//   int_map[42] = 123;  // this is OK because atomics are thread-safe.
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

  template <typename InputIt>
  lock_free_hash_map(InputIt first, InputIt last, Hash const& hash = Hash(),
                     Equal const& equal = Equal(), Allocator const& alloc = Allocator())
      : lock_free_hash_map(hash, equal, alloc) {
    insert(first, last);
  }

  template <typename InputIt>
  lock_free_hash_map(InputIt first, InputIt last, Allocator const& alloc)
      : lock_free_hash_map(first, last, Hash(), Equal(), alloc) {}

  template <typename InputIt>
  lock_free_hash_map(InputIt first, InputIt last, Hash const& hash, Allocator const& alloc)
      : lock_free_hash_map(first, last, hash, Equal(), alloc) {}

  lock_free_hash_map(std::initializer_list<value_type> const init, Hash const& hash = Hash(),
                     Equal const& equal = Equal(), Allocator const& alloc = Allocator())
      : lock_free_hash_map(hash, equal, alloc) {
    insert(init);
  }

  lock_free_hash_map(std::initializer_list<value_type> const init, Allocator const& alloc)
      : lock_free_hash_map(init, Hash(), Equal(), alloc) {}

  lock_free_hash_map(std::initializer_list<value_type> const init, Hash const& hash,
                     Allocator const& alloc)
      : lock_free_hash_map(init, hash, Equal(), alloc) {}

  using Base::begin;
  using Base::cbegin;
  using Base::cend;
  using Base::end;
  using Base::get_alloc;
  using Base::hash_function;
  using Base::key_eq;

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

  // Returns the maximum load factor, that is the maximum number of elements in relation to the
  // capacity that can be inserted without triggering a rehash.
  using Base::max_load_factor;

  // Returns the current load factor, that is the number if elements in relation to the capacity.
  //
  // NOTE: this function relies on `size()`, so the returned value is purely advisory. By the time
  // the function returns, any number of changes may have occurred in parallel.
  using Base::load_factor;

  // Erases all elements from the hash map.
  //
  // NOTE: as explained above, the memory taken by the removed elements is not actually freed. This
  // function will simply cause the hash map as whole to no longer point to any previous slot array.
  void clear() noexcept { Base::Clear(); }

  std::pair<iterator, bool> insert(value_type const& value) { return Base::Insert(value); }

  std::pair<iterator, bool> insert(value_type&& value) { return Base::Insert(std::move(value)); }

  template <typename InputIt>
  void insert(InputIt const first, InputIt const last) {
    // Don't specify a reserve count because we don't know if we can calculate `last - first`
    // efficiently.
    Base::InsertMany(first, last, /*reserve_count=*/0);
  }

  void insert(std::initializer_list<value_type> const list) {
    auto const first = list.begin();
    auto const last = list.end();
    Base::InsertMany(first, last, /*reserve_count=*/last - first);
  }

  template <typename ValueArg>
  std::pair<iterator, bool> insert_or_assign(Key const& key, ValueArg&& value) {
    return Base::InsertOrAssign(key, std::forward<ValueArg>(value));
  }

  template <typename ValueArg>
  std::pair<iterator, bool> insert_or_assign(Key&& key, ValueArg&& value) {
    return Base::InsertOrAssign(std::move(key), std::forward<ValueArg>(value));
  }

  template <
      typename KeyArg, typename ValueArg, typename HashAlias = Hash, typename EqualAlias = Equal,
      std::enable_if_t<internal::lock_free_container::HashEqAreTransparentV<HashAlias, EqualAlias>,
                       bool> = true>
  std::pair<iterator, bool> insert_or_assign(KeyArg&& key, ValueArg&& value) {
    return Base::InsertOrAssign(std::forward<KeyArg>(key), std::forward<ValueArg>(value));
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args) {
    return Base::Emplace(std::forward<Args>(args)...);
  }

  template <typename KeyArg, typename... ValueArgs>
  std::pair<iterator, bool> try_emplace(KeyArg&& key_arg, ValueArgs&&... value_args) {
    return Base::Emplace(std::piecewise_construct, std::forward<KeyArg>(key_arg),
                         std::forward<ValueArgs>(value_args)...);
  }

  template <typename KeyArg = key_type>
  size_type erase(key_arg_t<KeyArg> const& key) {
    return Base::Erase(key) ? 1 : 0;
  }

  size_type erase(const_iterator const it) { return Base::Erase(it) ? 1 : 0; }

  template <typename KeyArg = key_type>
  Value& at(key_arg_t<KeyArg> const& key) {
    auto const it = Base::Find(key);
    return it->second;
  }

  template <typename KeyArg = key_type>
  Value const& at(key_arg_t<KeyArg> const& key) const {
    auto const it = Base::Find(key);
    return it->second;
  }

  template <typename KeyArg = key_type>
  Value& operator[](key_arg_t<KeyArg> const& key) {
    auto const [it, unused] = Base::InsertDefaultValue(key);
    return it->second;
  }

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

  // Swaps the content of two hash maps. All existing iterators are invalidated. This algorithm is
  // not lockless.
  void swap(lock_free_hash_map& other) noexcept { Base::Swap(other); }
  friend void swap(lock_free_hash_map& lhs, lock_free_hash_map& rhs) noexcept { lhs.swap(rhs); }
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_LOCK_FREE_HASH_MAP_H__
