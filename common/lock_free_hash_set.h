#ifndef __TSDB2_COMMON_LOCK_FREE_HASH_SET_H__
#define __TSDB2_COMMON_LOCK_FREE_HASH_SET_H__

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/hash/hash.h"
#include "absl/synchronization/mutex.h"

namespace tsdb2 {
namespace common {

// A lock-free, thread-safe hash set data structure.
//
// Readers can access the data without any locking; all synchronization is performed by
// `lock_free_hash_set` using atomics. Writers are automatically serialized by acquiring an
// exclusive lock on an internal mutex. Iterators acquire a shared lock on the mutex and release it
// when they're destroyed.
//
// To reduce the number of heap allocations and increase cache friendliness, `lock_free_hash_set`
// uses quadratic open addressing. To speed up lookups even further, values are pre-hashed so that a
// hash doesn't have to be re-calculated for every colliding element and the probing algorithm can
// short-circuit and avoid a full comparison of every colliding element.
//
// Under heavily contended read scenarios `lock_free_hash_set` can be much faster than other hash
// set data structures guarded by a mutex. On the flip side, `lock_free_hash_set` does not free any
// memory upon element erasure, as doing so is infeasible when using atomic-based synchronization.
// The memory used by the elements and internal element arrays is freed up only at destruction time,
// so you should use `lock_free_hash_set` only if you don't need to perform many erasures.
template <typename Key, typename Hash = absl::Hash<Key>, typename Equal = std::equal_to<Key>,
          typename Allocator = std::allocator<Key>>
class lock_free_hash_set {
 private:
  class Node {
    explicit Node(Hash const &hasher, Key &&key) : key(std::move(key)), hash(hasher(key)) {}

    template <typename... Args>
    explicit Node(Hash const &hash, Args &&...args)
        : key(std::forward<Args>(args)...), hash(hasher(key)) {}

    Node(Node const &) = delete;
    Node &operator=(Node const &) = delete;
    Node(Node &&) = delete;
    Node &operator=(Node &&) = delete;

    Key key;
    size_t const hash;
  };

  using NodeAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;

  struct Array final {
    static inline size_t constexpr kMinCapacity = 32;

    // REQUIRES: `array_capacity` must be a power of 2.
    explicit Array(size_t const array_capacity, size_t const array_size)
        : capacity(array_capacity), hash_mask(capacity - 1), size(array_size) {}

    // Number of `data` slots. This is always a power of 2.
    size_t const capacity;

    // Equal to `capacity-1`. We use it to wrap slot indices when probing so that they don't exceed
    // the bounds of `data`. Since `capacity` is always a power of 2, `hash_mask` can be bitwise
    // ANDed with the index, and that's much faster than using the modulo operator.
    size_t const hash_mask;  // == capacity - 1

    // The number of elements in the array, i.e. the number of non-null `data` elements. This is
    // only an advisory value used to implement methods like `lock_free_hash_set::size()`, because
    // keeping it atomically in sync with the content of `data` is impossible.
    std::atomic<size_t> size;

    // The actual array of pointers. `Array` objects are always allocated with extra trailing space
    // so that `data` has exactly `capacity` elements.
    //
    // NOTE: since `data` is defined as a one-element array, the (implicit) `~Array` destructor
    // destroys none of these `Node` pointers but the first, and of course it won't destroy the
    // nodes themselves. Those are not problems because the nodes are owned and destroyed by the
    // `nodes_` vector in the parent class, and atomic pointers are trivially destructible so
    // they don't require destruction.
    std::atomic<Node *> data[1];
  };

  using ArrayAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Array>;

  using ArrayPtrAllocator =
      typename std::allocator_traits<Allocator>::template rebind_alloc<Array *>;

  class Iterator {
   public:
    Iterator() : array_(nullptr) {}

    Iterator(Iterator const &) = default;
    Iterator &operator=(Iterator const &) = default;
    Iterator(Iterator &&) noexcept = default;
    Iterator &operator=(Iterator &&) noexcept = default;

    Key &operator*() const { return *(array_->data[index_].load(std::memory_order_relaxed)); }

    // TODO

   private:
    Array const *array_;
    size_t index_;
  };

 public:
  using key_type = Key;
  using value_type = Key;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using hasher = Hash;
  using key_equal = Equal;
  using allocator_type = NodeAllocator;
  using reference = value_type &;
  using const_reference = value_type const &;
  using pointer = typename std::allocator_traits<NodeAllocator>::pointer;
  using const_pointer = typename std::allocator_traits<NodeAllocator>::const_pointer;
  using iterator = Iterator;

  // TODO

  lock_free_hash_set() = default;

  explicit lock_free_hash_set(Hash const &hash, Equal const &equal = Equal(),
                              Allocator const &alloc = Allocator())
      : hasher_(hash), equal_(equal), array_alloc_(alloc), arrays_(alloc), nodes_(alloc) {}

  lock_free_hash_set(Hash const &hash, Allocator const &alloc)
      : lock_free_hash_set(hash, Equal(), alloc) {}

  explicit lock_free_hash_set(Allocator const &alloc)
      : lock_free_hash_set(Hash(), Equal(), alloc) {}

  template <typename InputIt>
  lock_free_hash_set(InputIt first, InputIt last, Hash const &hash = Hash(),
                     Equal const &equal = Equal(), Allocator const &alloc = Allocator());

  template <typename InputIt>
  lock_free_hash_set(InputIt first, InputIt last, Allocator const &alloc)
      : lock_free_hash_set(first, last, Hash(), Equal(), alloc) {}

  template <typename InputIt>
  lock_free_hash_set(InputIt first, InputIt last, Hash const &hash, Allocator const &alloc)
      : lock_free_hash_set(first, last, hash, Equal(), alloc) {}

  lock_free_hash_set(lock_free_hash_set const &other);

  lock_free_hash_set &operator=(lock_free_hash_set const &other);

  lock_free_hash_set(lock_free_hash_set &&other);

  lock_free_hash_set &operator=(lock_free_hash_set &&other);

  // TODO

  // Returns the number of available slots in the hash set.
  //
  // WARNING: the value returned by this function is purely advisory. By the time the function
  // returns, the data structure may have been rehashed any number of times. If you need to know the
  // exact capacity you need to implement your own synchronization (typically using a mutex).
  size_t capacity() const {
    Array const *const array = ptr_.load(std::memory_order_relaxed);
    if (array) {
      return array->capacity;
    } else {
      return 0;
    }
  }

  // Returns the number of elements in the hash set.
  //
  // WARNING: the value returned by this function is purely advisory. By the time the function
  // returns, any number of changes may have occurred in parallel. If you need to know the exact
  // number of elements you need to implement your own synchronization (typically using a mutex).
  size_t size() const {
    Array const *const array = ptr_.load(std::memory_order_relaxed);
    if (array) {
      return array->size.load(std::memory_order_relaxed);
    } else {
      return 0;
    }
  }

  // Indicates whether the hash set is empty. Equivalent to `size() == 0`.
  //
  // WARNING: this function relies on `size()`, so the returned value is purely advisory. By the
  // time the function returns, any number of changes may have occurred in parallel. If you need to
  // know the exact number of elements you need to implement your own synchronization (typically
  // using a mutex).
  bool empty() const { return size() == 0; }

  // Erases all elements from the hash set.
  //
  // NOTE: as explained above, the memory taken by the removed elements is not actually freed. This
  // function will simply cause the hash set as whole to no longer point to any previous slot array.
  void clear() {
    absl::WriterMutexLock lock(&mutex_);
    ptr_.store(nullptr, std::memory_order_release);
  }

  std::pair<iterator, bool> insert(Key const &key);

  // TODO

 private:
  // Rehash (halving the capacity) when the number of elements becomes less than half the capacity.
  // Respecting `Array::kMinCapacity` has priority: a rehash is not performed if there are only
  // `Array::kMinCapacity` slots.
  static inline double constexpr kMinLoadFactor = 0.5;

  // Rehash (doubling the capacity) when the number of elements exceeds 75% of the capacity.
  static inline double constexpr kMaxLoadFactor = 0.75;

  Array *CreateArray(size_t capacity, size_t initial_size) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  template <typename Needle>
  Node *Find(Needle const &needle, size_t const hash) const;

  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS Hash hasher_;
  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS Equal equal_;
  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS ArrayAllocator array_alloc_;

  // All arrays and elements ever allocated by this hash set are kept in the `arrays_` and `nodes_`
  // vectors. They are freed up only when the whole hash set is destroyed. This allows us to
  // implement CAS algorithms without incurring ABA bugs.

  absl::Mutex mutable mutex_;
  std::vector<Array *, ArrayPtrAllocator> arrays_ ABSL_GUARDED_BY(mutex_);
  std::vector<Node, NodeAllocator> nodes_ ABSL_GUARDED_BY(mutex_);

  // Points to the current data.
  std::atomic<Array *> ptr_ = nullptr;
};

template <typename Key, typename Hash, typename Equal, typename Allocator>
std::pair<typename lock_free_hash_set<Key, Hash, Equal, Allocator>::iterator, bool>
lock_free_hash_set<Key, Hash, Equal, Allocator>::insert(Key const &key) {
  size_t const hash = hasher_(key);
  absl::WriterMutexLock lock(&mutex_);
  Array const *array = ptr_.load(std::memory_order_relaxed);
  bool store_ptr = false;
  if (!array) {
    array = CreateArray(Array::kMinCapacity, 1);
    store_ptr = true;
  }
  size_t const mask = array->hash_mask;
  size_t const offset = hash & mask;
  size_t i = 0;
  while (true) {
    Node *const node = array->data[(offset + i * i) & mask].load(std::memory_order_relaxed);
    if (node) {
      if (hash == node->hash && equal_(key, node->key)) {
        if (store_ptr) {
          ptr_.store(std::memory_order_release);
        }
        return false;
      } else {
        ++i;
      }
    } else {
      array = Grow(array);
      ptr_.store(std::memory_order_release);
      return true;
    }
  }
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Array *
lock_free_hash_set<Key, Hash, Equal, Allocator>::CreateArray(size_t capacity,
                                                             size_t const initial_size) const {
  if (capacity < Array::kMinCapacity) {
    capacity = Array::kMinCapacity;
  }
  size_t const byte_size = sizeof(Array) + (capacity - 1) * sizeof(std::atomic<Node *>);
  size_t const remainder = byte_size % sizeof(Array);
  Array *const array = array_alloc_.allocate((byte_size + remainder) / sizeof(Array));
  new (array) Array(capacity, initial_size);
  arrays_.push_back(array);
  return array;
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
template <typename Needle>
typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Node *
lock_free_hash_set<Key, Hash, Equal, Allocator>::Find(Needle const &needle,
                                                      size_t const hash) const {
  Array const *const array = ptr_.load(std::memory_order_relaxed);
  if (!array) {
    return nullptr;
  }
  auto const mask = array->hash_mask;
  auto const &data = array->data;
  size_t const offset = hash & mask;
  size_t i = 0;
  while (true) {
    Node *const node = data[(offset + i * i) & mask].load(std::memory_order_relaxed);
    if (!node) {
      return nullptr;
    }
    if (hash == node->hash && equal_(needle, node->key)) {
      return node;
    }
    ++i;
  }
}

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_LOCK_FREE_HASH_SET_H__
