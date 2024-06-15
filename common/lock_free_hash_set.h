#ifndef __TSDB2_COMMON_LOCK_FREE_HASH_SET_H__
#define __TSDB2_COMMON_LOCK_FREE_HASH_SET_H__

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/hash/hash.h"
#include "absl/synchronization/mutex.h"
#include "common/lock_free_container_internal.h"

namespace tsdb2 {
namespace common {

// A lock-free, thread-safe hash set data structure. The provided API is a subset of
// `std::unordered_set`.
//
// All reads (lookups and iterations) are lockless: all synchronization is performed by
// `lock_free_hash_set` using atomics. Writers are automatically serialized by acquiring an
// exclusive lock on an internal mutex.
//
// Under heavily contended read scenarios `lock_free_hash_set` can be much faster than other hash
// set data structures guarded by a mutex. On the flip side, `lock_free_hash_set` does not free any
// memory upon element erasure, as doing so is infeasible with atomic-based synchronization. The
// memory used by the elements and internal element arrays is freed up only at destruction time, so
// you should use `lock_free_hash_set` only if you don't need to perform many erasures.
//
// NOTE: iterations are loosely consistent. If a rehash occurs during an iteration it is quite
// possible that the iterator misses some elements and/or returns some elements more than once.
// However it will never return invalid or corrupted data.
//
// To reduce the number of heap allocations and increase cache friendliness, `lock_free_hash_set`
// uses quadratic open addressing. To speed up lookups even further, values are pre-hashed so that a
// hash doesn't have to be re-calculated for every colliding element and the probing algorithm can
// short-circuit and avoid a full comparison of every colliding element.
template <typename Key, typename Hash = internal::lock_free_container::DefaultHashT<Key>,
          typename Equal = internal::lock_free_container::DefaultEqT<Key>,
          typename Allocator = std::allocator<Key>>
class lock_free_hash_set {
 private:
  // Holds a single (pre-hashed) element.
  //
  // Both the key and the hash are const because there's no requirement of thread safety on the
  // keys, so const ensures thread safety. Besides, nodes are always allocated on the heap so
  // there's no need to copy or move them.
  struct Node final {
    // Resolves the `Node` constructor overload that receives the caller-provided hash.
    struct Hashed {};
    static inline Hashed constexpr kHashed;

    // Resolves the `Node` constructor overload that hashes the key.
    struct ToHash {};
    static inline ToHash constexpr kToHash;

    // Constructs a node with the caller-provided hash.
    template <typename... Args>
    explicit Node(Hashed const &, size_t const node_hash, Args &&...args)
        : key(std::forward<Args>(args)...), hash(node_hash) {}

    // Constructs a node, hashing it automatically.
    template <typename... Args>
    explicit Node(ToHash const &, Hash const &hasher, Args &&...args)
        : key(std::forward<Args>(args)...), hash(hasher(key)) {}

    Node(Node const &) = delete;
    Node &operator=(Node const &) = delete;
    Node(Node &&) = delete;
    Node &operator=(Node &&) = delete;

    Key const key;
    size_t const hash;
    std::atomic<bool> deleted{false};
  };

  using NodeAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
  using NodePtrAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<
      typename NodeAllocator::pointer>;

  struct Array final {
    // The minimum capacity of an array in log2 format. It's 2**5 = 32.
    static inline uint8_t constexpr kMinCapacityLog2 = 5;

    // Returns the minimum number of consecutive `Array` objects to make room for in the array
    // allocator in order to get a single `Array` with `capacity` node slots. This is used in
    // `array_alloc_.allocate` and `array_alloc_.deallocate` calls.
    static size_t GetMinAllocCount(size_t const capacity) {
      size_t const byte_size = sizeof(Array) + (capacity - 1) * sizeof(std::atomic<Node *>);
      // Since the integer division truncates fractional results, add an extra `sizeof(Array)`
      // before dividing to account for a possible spillover.
      return (byte_size + sizeof(Array)) / sizeof(Array);
    }

    // REQUIRES: `array_capacity_log2` must be greater than or equal to `kMinCapacityLog2`.
    explicit Array(uint8_t const array_capacity_log2) : capacity_log2(array_capacity_log2) {
      auto const count = capacity();
      for (size_t i = 1; i < count; ++i) {
        new (data + i) std::atomic<Node *>(nullptr);
      }
    }

    Array(Array const &) = delete;
    Array &operator=(Array const &) = delete;
    Array(Array &&) = delete;
    Array &operator=(Array &&) = delete;

    size_t capacity() const { return size_t{1} << capacity_log2; }
    size_t hash_mask() const { return capacity() - 1; }

    // Inserts the provided node in the array, returning the index at which the node was inserted.
    //
    // NOTE: this function uses relaxed memory ordering because it assumes that the caller owns an
    // exclusive lock. That must be the case because all `lock_free_hash_set` writers are serialized
    // by a mutex.
    size_t InsertNodeRelaxed(Node *node);

    // Number of `data` slots. This is always a power of 2.
    uint8_t const capacity_log2;

    // The number of elements in the array, i.e. the number of non-null, non-deleted `data`
    // elements. This is only an advisory value used to implement methods like
    // `lock_free_hash_set::size()`, because keeping it atomically in sync with the content of
    // `data` is impossible.
    std::atomic<size_t> size{0};

    // The actual array of pointers. `Array` objects are always allocated with extra trailing space
    // so that `data` has exactly `capacity` elements.
    //
    // NOTE: since `data` is defined as a one-element array, the (implicit) `~Array` destructor
    // destroys none of these `Node` pointers but the first, and of course it won't destroy the
    // nodes themselves. Those are not problems because the nodes are owned and destroyed by the
    // `nodes_` vector in the parent class, and atomic pointers are trivially destructible so
    // they don't require destruction.
    std::atomic<Node *> data[1] = {nullptr};
  };

  using ArrayAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Array>;
  using ArrayPtrAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<
      typename ArrayAllocator::pointer>;

  class Iterator final {
   public:
    Iterator() : parent_(nullptr), index_(0), node_(nullptr) {}

    Iterator(Iterator const &) = default;
    Iterator &operator=(Iterator const &) = default;
    Iterator(Iterator &&) noexcept = default;
    Iterator &operator=(Iterator &&) noexcept = default;

    friend bool operator==(Iterator const &lhs, Iterator const &rhs) {
      if (!lhs.parent_) {
        return !rhs.parent_;
      }
      if (lhs.parent_ != rhs.parent_) {
        return false;
      }
      if (lhs.index_ < 0) {
        return rhs.index_ < 0;
      }
      return lhs.index_ == rhs.index_;
    }

    friend bool operator!=(Iterator const &lhs, Iterator const &rhs) {
      return !operator==(lhs, rhs);
    }

    friend bool operator<(Iterator const &lhs, Iterator const &rhs) {
      if (!lhs.parent_) {
        return rhs.parent_ != nullptr;
      }
      if (!rhs.parent_) {
        return false;
      }
      if (lhs.parent_ < rhs.parent_) {
        return true;
      } else if (rhs.parent_ < lhs.parent_) {
        return false;
      } else if (lhs.index_ < 0) {
        return false;
      } else if (rhs.index_ < 0) {
        return true;
      } else {
        return lhs.index_ < rhs.index_;
      }
    }

    friend bool operator<=(Iterator const &lhs, Iterator const &rhs) {
      return !operator<(rhs, lhs);
    }

    friend bool operator>(Iterator const &lhs, Iterator const &rhs) { return operator<(rhs, lhs); }

    friend bool operator>=(Iterator const &lhs, Iterator const &rhs) {
      return !operator<(lhs, rhs);
    }

    Key const &operator*() const { return node_->key; }
    Key const *operator->() const { return &(node_->key); }

    Iterator &operator++() {
      node_ = nullptr;
      auto const array = parent_->ptr_.load(std::memory_order_acquire);
      if (!array) {
        index_ = -1;
        return *this;
      }
      auto const capacity = array->capacity();
      while (++index_ < capacity) {
        auto const node = array->data[index_].load(std::memory_order_acquire);
        if (node && !node->deleted.load(std::memory_order_relaxed)) {
          node_ = node;
          return *this;
        }
      }
      index_ = -1;
      return *this;
    }

    Iterator operator++(int) {
      Iterator it = *this;
      operator++();
      return it;
    }

    Iterator &operator--() {
      node_ = nullptr;
      auto const array = parent_->ptr_.load(std::memory_order_acquire);
      if (!array) {
        index_ = -1;
        return *this;
      }
      if (index_ < 0) {
        index_ = array->capacity();
      }
      while (index_-- > 0) {
        auto const node = array->data[index_].load(std::memory_order_acquire);
        if (node && !node->deleted.load(std::memory_order_relaxed)) {
          node_ = node;
          return *this;
        }
      }
      return *this;
    }

    Iterator operator--(int) {
      Iterator it = *this;
      operator--();
      return it;
    }

   private:
    friend class lock_free_hash_set;

    struct BeginIterator {};
    static inline BeginIterator constexpr kBeginIterator;

    struct EndIterator {};
    static inline EndIterator constexpr kEndIterator;

    // Constructs the begin iterator.
    explicit Iterator(BeginIterator const &, lock_free_hash_set const *const parent)
        : parent_(parent), index_(-1), node_(nullptr) {
      operator++();
    }

    // Constructs the end iterator.
    explicit Iterator(EndIterator const &, lock_free_hash_set const *const parent)
        : parent_(parent), index_(-1), node_(nullptr) {}

    // Constructs an iterator for a specific node & position.
    explicit Iterator(lock_free_hash_set const *const parent, size_t const index, Node *const node)
        : parent_(parent), index_(index), node_(node) {}

    // Indicates whether this is the end iterator of some hash set. Undefined behavior if the
    // iterator is default-constructed or copied / moved from a default-constructed one.
    bool is_end() const { return index_ < 0; }

    // Pointer to the parent hash set. nullptr means this is an empty, default-constructed iterator.
    lock_free_hash_set const *parent_;

    // Index to the current element. A negative value means this is the end iterator.
    intptr_t index_;

    // Caches the current element so that we can provide better consistency guarantees and avoid
    // dereferencing null pointers if a writer modifies the hash set while one or more threads are
    // enumerating it. This field is set to nullptr when the iterator passes the end.
    Node *node_;
  };

 public:
  using key_type = Key;
  using value_type = Key;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using hasher = Hash;
  using key_equal = Equal;
  using allocator_type = NodeAllocator;
  using allocator_traits = std::allocator_traits<allocator_type>;
  using reference = value_type &;
  using const_reference = value_type const &;
  using pointer = typename allocator_traits::pointer;
  using const_pointer = typename allocator_traits::const_pointer;
  using iterator = Iterator;
  using const_iterator = Iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  template <typename Arg>
  using key_arg_t =
      typename internal::lock_free_container::key_arg<Hash, Equal>::template type<key_type, Arg>;

  lock_free_hash_set() = default;

  explicit lock_free_hash_set(Hash const &hasher, Equal const &equal = Equal(),
                              Allocator const &alloc = Allocator())
      : hasher_(hasher),
        equal_(equal),
        array_alloc_(alloc),
        node_alloc_(alloc),
        arrays_(alloc),
        nodes_(alloc) {}

  lock_free_hash_set(Hash const &hasher, Allocator const &alloc)
      : lock_free_hash_set(hasher, Equal(), alloc) {}

  explicit lock_free_hash_set(Allocator const &alloc)
      : lock_free_hash_set(Hash(), Equal(), alloc) {}

  template <typename InputIt>
  lock_free_hash_set(InputIt first, InputIt last, Hash const &hasher = Hash(),
                     Equal const &equal = Equal(), Allocator const &alloc = Allocator())
      : hasher_(hasher),
        equal_(equal),
        array_alloc_(alloc),
        node_alloc_(alloc),
        arrays_(alloc),
        nodes_(alloc) {
    insert(first, last);
  }

  template <typename InputIt>
  lock_free_hash_set(InputIt first, InputIt last, Allocator const &alloc)
      : lock_free_hash_set(first, last, Hash(), Equal(), alloc) {}

  template <typename InputIt>
  lock_free_hash_set(InputIt first, InputIt last, Hash const &hasher, Allocator const &alloc)
      : lock_free_hash_set(first, last, hasher, Equal(), alloc) {}

  lock_free_hash_set(std::initializer_list<value_type> const init, Hash const &hasher = Hash(),
                     Equal const &equal = Equal(), Allocator const &alloc = Allocator())
      : lock_free_hash_set(hasher, equal, alloc) {
    insert(init);
  }

  lock_free_hash_set(std::initializer_list<value_type> init, Allocator const &alloc)
      : lock_free_hash_set(init, Hash(), Equal(), alloc) {}

  lock_free_hash_set(std::initializer_list<value_type> init, Hash const &hasher,
                     Allocator const &alloc)
      : lock_free_hash_set(init, hasher, Equal(), alloc) {}

  ~lock_free_hash_set();

  allocator_type get_alloc() const noexcept {
    return allocator_traits::select_on_container_copy_construction(node_alloc_);
  }

  iterator begin() noexcept { return Iterator(Iterator::kBeginIterator, this); }
  const_iterator begin() const noexcept { return Iterator(Iterator::kBeginIterator, this); }
  const_iterator cbegin() const noexcept { return Iterator(Iterator::kBeginIterator, this); }
  iterator end() noexcept { return Iterator(Iterator::kEndIterator, this); }
  const_iterator end() const noexcept { return Iterator(Iterator::kEndIterator, this); }
  const_iterator cend() const noexcept { return Iterator(Iterator::kEndIterator, this); }

  // Returns the number of available slots in the hash set.
  //
  // WARNING: the value returned by this function is purely advisory. By the time the function
  // returns, the data structure may have been rehashed any number of times. If you need to know the
  // exact capacity you need to implement your own synchronization (typically using a mutex).
  size_type capacity() const noexcept {
    auto const array = ptr_.load(std::memory_order_acquire);
    if (array) {
      return array->capacity();
    } else {
      return 0;
    }
  }

  // Ensures the hash set has room for at least `size` elements, including some extra space to
  // account for the maximum load factor. This is a no-op if the hash set already has enough
  // capacity.
  void reserve(size_type const size) { Reserve(size); }

  // Returns the number of elements in the hash set.
  //
  // WARNING: the value returned by this function is purely advisory. By the time the function
  // returns, any number of changes may have occurred in parallel. If you need to know the exact
  // number of elements you need to implement your own synchronization (typically using a mutex).
  size_type size() const noexcept {
    auto const array = ptr_.load(std::memory_order_acquire);
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
  [[nodiscard]] bool empty() const noexcept { return size() == 0; }

  // Erases all elements from the hash set.
  //
  // NOTE: as explained above, the memory taken by the removed elements is not actually freed. This
  // function will simply cause the hash set as whole to no longer point to any previous slot array.
  void clear() noexcept {
    absl::MutexLock lock{&mutex_};
    ptr_.store(nullptr, std::memory_order_release);
  }

  std::pair<iterator, bool> insert(value_type const &value) { return Insert(value); }

  template <class InputIt>
  void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      Insert(*first);
    }
  }

  void insert(std::initializer_list<value_type> const list) {
    ReserveExtra(list.end() - list.begin());
    for (auto &value : list) {
      Insert(value);
    }
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args &&...args) {
    return Emplace(std::forward<Args>(args)...);
  }

  template <typename KeyArg = key_type>
  size_type erase(key_arg_t<KeyArg> const &key) {
    return Erase(key) ? 1 : 0;
  }

  size_type count(key_type const &key) const { return Find(key).is_end() ? 0 : 1; }

  template <typename KeyArg = key_type>
  size_type count(key_arg_t<KeyArg> const &key) const {
    return Find(key).is_end() ? 0 : 1;
  }

  template <typename KeyArg = key_type>
  iterator find(key_arg_t<KeyArg> const &key) {
    return Find(key);
  }

  template <typename KeyArg = key_type>
  const_iterator find(key_arg_t<KeyArg> const &key) const {
    return Find(key);
  }

  template <typename KeyArg = key_type>
  bool contains(key_arg_t<KeyArg> const &key) const {
    return !Find(key).is_end();
  }

  // Swaps the content of this hash set with `other`. This algorithm is not lockless.
  void swap(lock_free_hash_set &other) { Swap(*this, other); }

 private:
  struct Fraction {
    size_t numerator;
    size_t denominator;
  };

  static inline Fraction constexpr kMinLoadFactor{.numerator = 1, .denominator = 4};
  static inline Fraction constexpr kMaxLoadFactor{.numerator = 3, .denominator = 4};

  // No moves & copies because they wouldn't be thread-safe.
  lock_free_hash_set(lock_free_hash_set const &) = delete;
  lock_free_hash_set &operator=(lock_free_hash_set const &) = delete;
  lock_free_hash_set(lock_free_hash_set &&) = delete;
  lock_free_hash_set &operator=(lock_free_hash_set &&) = delete;

  // Rounds `value` up to the next power of 2 and return the exponent of such power. For example, if
  // `value` is 60 it's rounded up to 64 and 6 is returned. This is used to calculate the array
  // capacity in log2 format, as required by `GetOrCreateArray`.
  static uint8_t NextExponentOf2(uint64_t value);

  // Returns `a / b` rounded up to the nearest integer.
  static inline size_t DivideAndRoundUp(size_t const a, size_t const b) { return (a + b - 1) / b; }

  Array *CreateFreeArray(uint8_t capacity_log2);

  Array *GetOrCreateArray(uint8_t capacity_log2) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void DestroyArray(Array *array);

  // Constructs a node without adding it to `nodes_`.
  template <typename... Args>
  Node *CreateFreeNode(Args &&...args);

  template <typename... Args>
  Node *CreateNode(Args &&...args) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void DestroyNode(Node *node);

  template <typename KeyArg>
  Iterator Find(KeyArg const &key) const;

  // Internal implementation of `Reserve` and `ReserveExtra`.
  Array *ReserveLocked(Array *array, uint8_t min_capacity_log2)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Reserves space for at least `num_elements` elements, rehashing if the current capacity doesn't
  // allow it. The maximum load factor is taken into account, so the resulting capacity is
  // guaranteed to be greater than or equal to `NextPowerOf2(size / kMaxLoadFactor)`.
  void Reserve(size_t num_elements) ABSL_LOCKS_EXCLUDED(mutex_);

  // Reserves space for at least the specified number of *new* elements. This is equivalent to
  // calling `Reserve(size() + num_new_elements)` atomically.
  void ReserveExtra(size_t num_new_elements) ABSL_LOCKS_EXCLUDED(mutex_);

  // Inserts a node in the hash set assuming it's empty. `ptr_` must be `nullptr` before entering
  // this function.
  //
  // Used by `Insert` and `Emplace` as a fallback for the case of empty hash set.
  std::pair<Iterator, bool> InsertFirstNode(Node *node) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Performs a re-hash and insertion, doubling the capacity of `old_array`. All elements are copied
  // over, the new node is added, and the new array is stored in `ptr_`. The return value is the
  // index at which the new node was inserted.
  size_t Grow(Array const *old_array, Node *new_node) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Performs a re-hash creating a new array with half the capacity of `old_array`. All elements are
  // copied over and the new array is stored in `ptr_`.
  //
  // REQUIRES: `old_array->size` MUST be strictly less than `old_array->capacity() *
  // kMinLoadFactor`.
  void Shrink(Array const *old_array) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  template <typename KeyArg>
  std::pair<Iterator, bool> Insert(KeyArg &&key) ABSL_LOCKS_EXCLUDED(mutex_);

  template <typename... Args>
  std::pair<Iterator, bool> Emplace(Args &&...args) ABSL_LOCKS_EXCLUDED(mutex_);

  template <typename KeyArg>
  bool Erase(KeyArg const &key) ABSL_LOCKS_EXCLUDED(mutex_);

  static void Swap(lock_free_hash_set &lhs, lock_free_hash_set &rhs)
      ABSL_LOCKS_EXCLUDED(lhs.mutex_, rhs.mutex_);

  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS Hash hasher_;
  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS Equal equal_;
  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS ArrayAllocator array_alloc_;
  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS NodeAllocator node_alloc_;

  // All arrays and elements ever allocated by this hash set are kept in the `arrays_` and `nodes_`
  // vectors. They are freed up only when the whole hash set is destroyed. This allows us to
  // implement CAS algorithms without incurring ABA bugs.

  absl::Mutex mutable mutex_;
  std::vector<typename ArrayAllocator::pointer, ArrayPtrAllocator> arrays_ ABSL_GUARDED_BY(mutex_);
  std::vector<typename NodeAllocator::pointer, NodePtrAllocator> nodes_ ABSL_GUARDED_BY(mutex_);

  // Points to the current data.
  std::atomic<Array *> ptr_ = nullptr;
};

template <typename Key, typename Hash, typename Equal, typename Allocator>
size_t lock_free_hash_set<Key, Hash, Equal, Allocator>::Array::InsertNodeRelaxed(
    Node *const new_node) {
  size_t const mask = hash_mask();
  for (size_t i = 0, j = new_node->hash;; ++i, j += i) {
    auto const index = j & mask;
    auto const node = data[index].load(std::memory_order_relaxed);
    if (!node || node->deleted.load(std::memory_order_relaxed)) {
      data[index].store(new_node, std::memory_order_relaxed);
      size.fetch_add(1, std::memory_order_relaxed);
      return index;
    }
  }
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
lock_free_hash_set<Key, Hash, Equal, Allocator>::~lock_free_hash_set() {
  absl::MutexLock lock{&mutex_};
  for (auto const node : nodes_) {
    DestroyNode(node);
  }
  for (auto const array : arrays_) {
    DestroyArray(array);
  }
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
uint8_t lock_free_hash_set<Key, Hash, Equal, Allocator>::NextExponentOf2(uint64_t value) {
  --value;
  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
  value |= value >> 32;
  ++value;
  uint8_t log2 = 0;
  while ((value & 1) == 0) {
    ++log2;
    value >>= 1;
  }
  return log2;
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Array *
lock_free_hash_set<Key, Hash, Equal, Allocator>::CreateFreeArray(uint8_t const capacity_log2) {
  size_t const capacity = size_t{1} << capacity_log2;
  Array *const array = array_alloc_.allocate(Array::GetMinAllocCount(capacity));
  new (array) Array(capacity_log2);
  return array;
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Array *
lock_free_hash_set<Key, Hash, Equal, Allocator>::GetOrCreateArray(uint8_t capacity_log2) {
  if (capacity_log2 < Array::kMinCapacityLog2) {
    capacity_log2 = Array::kMinCapacityLog2;
  }
  auto const index = capacity_log2 - Array::kMinCapacityLog2;
  if (index < arrays_.size()) {
    auto &array = arrays_[index];
    if (array) {
      auto const capacity = array->capacity();
      for (size_t i = 0; i < capacity; ++i) {
        array->data[i].store(nullptr, std::memory_order_relaxed);
      }
      array->size.store(0, std::memory_order_relaxed);
    } else {
      array = CreateFreeArray(capacity_log2);
    }
    return array;
  } else {
    arrays_.reserve(index + 1);
    for (auto i = arrays_.size(); i < index; ++i) {
      arrays_.emplace_back(nullptr);
    }
    auto const array = CreateFreeArray(capacity_log2);
    arrays_.emplace_back(array);
    return array;
  }
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
void lock_free_hash_set<Key, Hash, Equal, Allocator>::DestroyArray(Array *const array) {
  if (array) {
    auto const capacity = array->capacity();
    array->~Array();
    array_alloc_.deallocate(array, Array::GetMinAllocCount(capacity));
  }
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
template <typename... Args>
typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Node *
lock_free_hash_set<Key, Hash, Equal, Allocator>::CreateFreeNode(Args &&...args) {
  auto const node = node_alloc_.allocate(1);
  new (node) Node(std::forward<Args>(args)...);
  return node;
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
template <typename... Args>
typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Node *
lock_free_hash_set<Key, Hash, Equal, Allocator>::CreateNode(Args &&...args) {
  auto const node = CreateFreeNode(std::forward<Args>(args)...);
  nodes_.push_back(node);
  return node;
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
void lock_free_hash_set<Key, Hash, Equal, Allocator>::DestroyNode(Node *const node) {
  node->~Node();
  node_alloc_.deallocate(node, 1);
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
template <typename KeyArg>
typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Iterator
lock_free_hash_set<Key, Hash, Equal, Allocator>::Find(KeyArg const &key) const {
  Array const *const array = ptr_.load(std::memory_order_acquire);
  if (!array) {
    return Iterator(Iterator::kEndIterator, this);
  }
  size_t const hash = hasher_(key);
  size_t const mask = array->hash_mask();
  for (size_t i = 0, j = hash;; ++i, j += i) {
    auto const index = j & mask;
    auto const node = array->data[index].load(std::memory_order_acquire);
    if (!node) {
      return Iterator(Iterator::kEndIterator, this);
    }
    if (node->hash == hash && equal_(key, node->key)) {
      if (node->deleted.load(std::memory_order_relaxed)) {
        return Iterator(Iterator::kEndIterator, this);
      } else {
        return Iterator(this, index, node);
      }
    }
  }
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Array *
lock_free_hash_set<Key, Hash, Equal, Allocator>::ReserveLocked(Array *const array,
                                                               uint8_t const min_capacity_log2) {
  auto const new_array = GetOrCreateArray(min_capacity_log2);
  if (array) {
    auto const old_capacity = array->capacity();
    for (size_t i = 0; i < old_capacity; ++i) {
      auto const node = array->data[i].load(std::memory_order_relaxed);
      if (node && !node->deleted.load(std::memory_order_relaxed)) {
        new_array->InsertNodeRelaxed(node);
      }
    }
  }
  return new_array;
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
void lock_free_hash_set<Key, Hash, Equal, Allocator>::Reserve(size_t const num_elements) {
  if (!num_elements) {
    return;
  }
  auto const min_capacity =
      DivideAndRoundUp(num_elements * kMaxLoadFactor.denominator, kMaxLoadFactor.numerator);
  auto const min_capacity_log2 = std::max(Array::kMinCapacityLog2, NextExponentOf2(min_capacity));
  absl::MutexLock lock{&mutex_};
  auto const array = ptr_.load(std::memory_order_relaxed);
  if (array && array->capacity_log2 >= min_capacity_log2) {
    return;
  }
  ptr_.store(ReserveLocked(array, min_capacity_log2), std::memory_order_release);
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
void lock_free_hash_set<Key, Hash, Equal, Allocator>::ReserveExtra(size_t const num_new_elements) {
  if (!num_new_elements) {
    return;
  }
  absl::MutexLock lock{&mutex_};
  auto const array = ptr_.load(std::memory_order_relaxed);
  auto const current_size = array ? array->size.load(std::memory_order_relaxed) : 0;
  auto const min_capacity = DivideAndRoundUp(
      (current_size + num_new_elements) * kMaxLoadFactor.denominator, kMaxLoadFactor.numerator);
  auto const min_capacity_log2 = std::max(Array::kMinCapacityLog2, NextExponentOf2(min_capacity));
  if (array && array->capacity_log2 >= min_capacity_log2) {
    return;
  }
  ptr_.store(ReserveLocked(array, min_capacity_log2), std::memory_order_release);
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
std::pair<typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Iterator, bool>
lock_free_hash_set<Key, Hash, Equal, Allocator>::InsertFirstNode(Node *const node) {
  auto const array = GetOrCreateArray(Array::kMinCapacityLog2);
  auto const index = node->hash & array->hash_mask();
  array->data[index].store(node, std::memory_order_relaxed);
  array->size.store(1, std::memory_order_relaxed);
  ptr_.store(array, std::memory_order_release);
  return std::make_pair(Iterator(this, index, node), true);
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
size_t lock_free_hash_set<Key, Hash, Equal, Allocator>::Grow(Array const *const old_array,
                                                             Node *const new_node) {
  auto const new_array = GetOrCreateArray(old_array->capacity_log2 + 1);
  auto const old_capacity = old_array->capacity();
  for (size_t i = 0; i < old_capacity; ++i) {
    auto const node = old_array->data[i].load(std::memory_order_relaxed);
    if (node && !node->deleted.load(std::memory_order_relaxed)) {
      new_array->InsertNodeRelaxed(node);
    }
  }
  auto const node_index = new_array->InsertNodeRelaxed(new_node);
  ptr_.store(new_array, std::memory_order_release);
  return node_index;
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
void lock_free_hash_set<Key, Hash, Equal, Allocator>::Shrink(Array const *const old_array) {
  if (old_array->capacity_log2 <= Array::kMinCapacityLog2) {
    return;
  }
  auto const new_array = GetOrCreateArray(old_array->capacity_log2 - 1);
  auto const old_capacity = old_array->capacity();
  for (size_t i = 0; i < old_capacity; ++i) {
    auto const node = old_array->data[i].load(std::memory_order_relaxed);
    if (node && !node->deleted.load(std::memory_order_relaxed)) {
      new_array->InsertNodeRelaxed(node);
    }
  }
  ptr_.store(new_array, std::memory_order_release);
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
template <typename KeyArg>
std::pair<typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Iterator, bool>
lock_free_hash_set<Key, Hash, Equal, Allocator>::Insert(KeyArg &&key) {
  auto const hash = hasher_(key);
  absl::MutexLock lock{&mutex_};
  auto array = ptr_.load(std::memory_order_relaxed);
  if (!array) {
    return InsertFirstNode(CreateNode(Node::kHashed, hash, std::forward<KeyArg>(key)));
  }
  size_t const mask = array->hash_mask();
  for (size_t i = 0, j = hash;; ++i, j += i) {
    auto index = j & mask;
    auto const node = array->data[index].load(std::memory_order_relaxed);
    if (!node) {
      break;
    }
    if (node->hash == hash && equal_(key, node->key)) {
      if (!node->deleted.exchange(false, std::memory_order_relaxed)) {
        return std::make_pair(Iterator(this, index, node), false);
      }
      auto const size = array->size.fetch_add(1, std::memory_order_relaxed) + 1;
      if (size * kMaxLoadFactor.denominator > array->capacity() * kMaxLoadFactor.numerator) {
        index = Grow(array, node);
      }
      return std::make_pair(Iterator(this, index, node), true);
    }
  }
  auto const node = CreateNode(Node::kHashed, hash, std::forward<KeyArg>(key));
  auto const size = array->size.load(std::memory_order_relaxed);
  if ((size + 1) * kMaxLoadFactor.denominator > array->capacity() * kMaxLoadFactor.numerator) {
    return std::make_pair(Iterator(this, Grow(array, node), node), true);
  } else {
    return std::make_pair(Iterator(this, array->InsertNodeRelaxed(node), node), true);
  }
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
template <typename... Args>
std::pair<typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Iterator, bool>
lock_free_hash_set<Key, Hash, Equal, Allocator>::Emplace(Args &&...args) {
  auto const new_node = CreateFreeNode(Node::kToHash, hasher_, std::forward<Args>(args)...);
  absl::MutexLock lock{&mutex_};
  auto array = ptr_.load(std::memory_order_relaxed);
  if (!array) {
    nodes_.push_back(new_node);
    return InsertFirstNode(new_node);
  }
  size_t const mask = array->hash_mask();
  for (size_t i = 0, j = new_node->hash;; ++i, j += i) {
    auto index = j & mask;
    auto const node = array->data[index].load(std::memory_order_relaxed);
    if (!node) {
      break;
    }
    if (new_node->hash == node->hash && equal_(new_node->key, node->key)) {
      DestroyNode(new_node);
      if (!node->deleted.exchange(false, std::memory_order_relaxed)) {
        return std::make_pair(Iterator(this, index, node), false);
      }
      auto const size = array->size.fetch_add(1, std::memory_order_relaxed) + 1;
      if (size * kMaxLoadFactor.denominator > array->capacity() * kMaxLoadFactor.numerator) {
        index = Grow(array, node);
      }
      return std::make_pair(Iterator(this, index, node), true);
    }
  }
  nodes_.push_back(new_node);
  auto const size = array->size.load(std::memory_order_relaxed);
  if ((size + 1) * kMaxLoadFactor.denominator > array->capacity() * kMaxLoadFactor.numerator) {
    return std::make_pair(Iterator(this, Grow(array, new_node), new_node), true);
  } else {
    return std::make_pair(Iterator(this, array->InsertNodeRelaxed(new_node), new_node), true);
  }
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
template <typename KeyArg>
bool lock_free_hash_set<Key, Hash, Equal, Allocator>::Erase(KeyArg const &key) {
  auto const hash = hasher_(key);
  absl::MutexLock lock{&mutex_};
  auto array = ptr_.load(std::memory_order_relaxed);
  if (!array) {
    return false;
  }
  auto const mask = array->hash_mask();
  for (size_t i = 0, j = hash;; ++i, j += i) {
    auto const node = array->data[j & mask].load(std::memory_order_relaxed);
    if (!node) {
      return false;
    }
    if (node->hash == hash && equal_(key, node->key)) {
      if (node->deleted.exchange(true, std::memory_order_relaxed)) {
        return false;
      }
      auto const size = array->size.fetch_sub(1, std::memory_order_relaxed) - 1;
      if (size * kMinLoadFactor.denominator < array->capacity() * kMinLoadFactor.numerator) {
        Shrink(array);
      }
      return true;
    }
  }
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
void lock_free_hash_set<Key, Hash, Equal, Allocator>::Swap(lock_free_hash_set &lhs,
                                                           lock_free_hash_set &rhs) {
  if (&rhs < &lhs) {
    // Make sure the mutexes are locked in a deterministic order by ordering the two hash sets by
    // their memory address. This avoids deadlocks when `Swap` is called concurrently by multiple
    // threads.
    return Swap(rhs, lhs);
  }
  absl::MutexLock lhs_lock{&lhs.mutex_};
  absl::MutexLock rhs_lock{&rhs.mutex_};
  if (std::allocator_traits<ArrayAllocator>::propagate_on_container_swap::value) {
    std::swap(lhs.array_alloc_, rhs.array_alloc_);
  }
  if (std::allocator_traits<NodeAllocator>::propagate_on_container_swap::value) {
    std::swap(lhs.node_alloc_, rhs.node_alloc_);
  }
  std::swap(lhs.arrays_, rhs.arrays_);
  std::swap(lhs.nodes_, rhs.nodes_);
  auto ptr = lhs.ptr_.load(std::memory_order_relaxed);
  ptr = rhs.ptr_.exchange(ptr, std::memory_order_relaxed);
  lhs.ptr_.store(ptr, std::memory_order_relaxed);
}

}  // namespace common
}  // namespace tsdb2

namespace std {

template <typename Key, typename Hash, typename Equal, typename Allocator>
void swap(::tsdb2::common::lock_free_hash_set<Key, Hash, Equal, Allocator> &lhs,
          ::tsdb2::common::lock_free_hash_set<Key, Hash, Equal, Allocator> &rhs) {
  lhs.swap(rhs);
}

}  // namespace std

#endif  // __TSDB2_COMMON_LOCK_FREE_HASH_SET_H__
