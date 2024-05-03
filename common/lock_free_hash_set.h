#ifndef __TSDB2_COMMON_LOCK_FREE_HASH_SET_H__
#define __TSDB2_COMMON_LOCK_FREE_HASH_SET_H__

#include <atomic>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/hash/hash.h"
#include "absl/synchronization/mutex.h"
#include "common/lock_free_container_internal.h"

namespace tsdb2 {
namespace common {

// A lock-free, thread-safe hash set data structure. The provided API is similar to
// `std::unordered_set`.
//
// All reads (lookups and iterations) are lockless: all synchronization is performed by
// `lock_free_hash_set` using atomics. Writers are automatically serialized by acquiring an
// exclusive lock on an internal mutex.
//
// Under heavily contended read scenarios `lock_free_hash_set` can be much faster than other hash
// set data structures guarded by a mutex. On the flip side, `lock_free_hash_set` does not free any
// memory upon element erasure, as doing so is infeasible when using atomic-based synchronization.
// The memory used by the elements and internal element arrays is freed up only at destruction time,
// so you should use `lock_free_hash_set` only if you don't need to perform many erasures.
//
// To reduce the number of heap allocations and increase cache friendliness, `lock_free_hash_set`
// uses quadratic open addressing. To speed up lookups even further, values are pre-hashed so that a
// hash doesn't have to be re-calculated for every colliding element and the probing algorithm can
// short-circuit and avoid a full comparison of every colliding element.
template <typename Key, typename Hash = absl::Hash<Key>, typename Equal = std::equal_to<Key>,
          typename Allocator = std::allocator<Key>>
class lock_free_hash_set {
 private:
  struct Node final {
    explicit Node(Key const &node_key, size_t const node_hash) : key(node_key), hash(node_hash) {}

    explicit Node(Key &&node_key, size_t const node_hash)
        : key(std::move(node_key)), hash(node_hash) {}

    template <typename... Args>
    explicit Node(Hash const &hasher, Args &&...args)
        : key(std::forward<Args>(args)...), hash(hasher(key)) {}

    Node(Node const &) = delete;
    Node &operator=(Node const &) = delete;
    Node(Node &&) = delete;
    Node &operator=(Node &&) = delete;

    Key const key;
    size_t const hash;
  };

  using NodeAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
  using NodePtrAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<
      typename NodeAllocator::pointer>;

  struct Array final {
    static inline size_t constexpr kMinCapacity = 32;

    // REQUIRES: `array_capacity` must be a power of 2 and must be greater than or equal to
    // `kMinCapacity`.
    explicit Array(size_t const array_capacity, size_t const array_size)
        : capacity(array_capacity), hash_mask(capacity - 1), size(array_size) {
      for (size_t i = 1; i < capacity; ++i) {
        new (data + i) std::atomic<Node *>(nullptr);
      }
    }

    // Inserts the provided node in the array.
    //
    // NOTE: this function uses relaxed memory ordering because it assumes that the caller owns an
    // exclusive lock. That must be the case because all `lock_free_hash_set` writers are serialized
    // by a mutex.
    void InsertNodeRelaxed(Node *node);

    // Number of `data` slots. This is always a power of 2.
    size_t const capacity;

    // Equal to `capacity-1`. We use it to wrap slot indices when probing so that they don't exceed
    // the bounds of `data`. Since `capacity` is always a power of 2, `hash_mask` can be bitwise
    // ANDed with the index, and that's faster than using the modulo operator.
    size_t const hash_mask;

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
    std::atomic<Node *> data[1] = {nullptr};
  };

  using ArrayAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Array>;
  using ArrayPtrAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<
      typename ArrayAllocator::pointer>;

  class Iterator final {
   public:
    Iterator() : array_(nullptr), index_(0), node_(nullptr) {}

    Iterator(Iterator const &) = default;
    Iterator &operator=(Iterator const &) = default;
    Iterator(Iterator &&) noexcept = default;
    Iterator &operator=(Iterator &&) noexcept = default;

    friend bool operator==(Iterator const &lhs, Iterator const &rhs) {
      return lhs.Tie() == rhs.Tie();
    }

    friend bool operator!=(Iterator const &lhs, Iterator const &rhs) {
      return lhs.Tie() != rhs.Tie();
    }

    friend bool operator<(Iterator const &lhs, Iterator const &rhs) {
      return lhs.Tie() < rhs.Tie();
    }

    friend bool operator<=(Iterator const &lhs, Iterator const &rhs) {
      return lhs.Tie() <= rhs.Tie();
    }

    friend bool operator>(Iterator const &lhs, Iterator const &rhs) {
      return lhs.Tie() > rhs.Tie();
    }

    friend bool operator>=(Iterator const &lhs, Iterator const &rhs) {
      return lhs.Tie() >= rhs.Tie();
    }

    operator bool() const { return node_ != nullptr; }

    Key const &operator*() const { return node_->key; }

    Iterator &operator++() ABSL_NO_THREAD_SAFETY_ANALYSIS {
      while (++index_ < array_->capacity) {
        Node *const node = array_->data[index_].load(std::memory_order_acquire);
        if (node) {
          node_ = node;
          return *this;
        }
      }
      node_ = nullptr;
      return *this;
    }

    Iterator operator++(int) {
      Iterator it = *this;
      operator++();
      return it;
    }

    Iterator &operator--() {
      while (index_ > 0) {
        Node *const node = array_->data[--index_].load(std::memory_order_acquire);
        if (node) {
          node_ = node;
          return *this;
        }
      }
      node_ = nullptr;
      return *this;
    }

    Iterator operator--(int) {
      Iterator it = *this;
      operator--();
      return it;
    }

   private:
    friend class lock_free_hash_set;

    // Constructs an iterator at the specified position. `memory_order` indicates how to load the
    // atomic pointer to the element at that position; use relaxed order if the mutex of the parent
    // container is locked exclusively, otherwise use acquire order.
    explicit Iterator(Array const *const array, size_t const index,
                      std::memory_order const memory_order)
        : array_(array), index_(index), node_(nullptr) {
      if (array_) {
        Node *const node = array_->data[index_].load(memory_order);
        if (node) {
          node_ = node;
        } else {
          operator++();
        }
      }
    }

    // Constructs an iterator at the specified position. `node` must be the element at
    // `array->data[index]`.
    explicit Iterator(Array const *const array, size_t const index, Node *const node)
        : array_(array), index_(index), node_(node) {}

    // Constructs the end iterator.
    explicit Iterator(Array const *const array)
        : array_(array), index_(array_ ? array_->capacity : 0), node_(nullptr) {}

    auto Tie() const { return std::tie(array_, index_); }

    Array const *array_;
    size_t index_;
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
  using reference = value_type &;
  using const_reference = value_type const &;
  using pointer = typename std::allocator_traits<NodeAllocator>::pointer;
  using const_pointer = typename std::allocator_traits<NodeAllocator>::const_pointer;
  using iterator = Iterator;
  using const_iterator = Iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  // TODO

  template <typename Arg>
  using key_arg_t =
      typename internal::lock_free_container::key_arg<Hash, Equal>::template type<key_type, Arg>;

  // TODO

  lock_free_hash_set() = default;

  explicit lock_free_hash_set(Hash const &hash, Equal const &equal = Equal(),
                              Allocator const &alloc = Allocator())
      : hasher_(hash),
        equal_(equal),
        array_alloc_(alloc),
        node_alloc_(alloc),
        arrays_(alloc),
        nodes_(alloc) {}

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

  lock_free_hash_set(lock_free_hash_set &&other) noexcept;

  lock_free_hash_set &operator=(lock_free_hash_set &&other) noexcept;

  lock_free_hash_set(std::initializer_list<value_type> init, Hash const &hash = Hash(),
                     Equal const &equal = Equal(), Allocator const &alloc = Allocator())
      : hasher_(hash),
        equal_(equal),
        array_alloc_(alloc),
        node_alloc_(alloc),
        arrays_(alloc),
        nodes_(alloc) {
    for (auto const &key : init) {
      insert(key);
    }
  }

  // TODO

  ~lock_free_hash_set();

  // TODO

  iterator begin() const noexcept ABSL_LOCKS_EXCLUDED(mutex_) {
    return Iterator(ptr_.load(std::memory_order_acquire), 0, std::memory_order_acquire);
  }

  const_iterator cbegin() const noexcept ABSL_LOCKS_EXCLUDED(mutex_) {
    return Iterator(ptr_.load(std::memory_order_acquire), 0, std::memory_order_acquire);
  }

  iterator end() const noexcept ABSL_LOCKS_EXCLUDED(mutex_) {
    return Iterator(ptr_.load(std::memory_order_acquire));
  }

  const_iterator cend() const noexcept ABSL_LOCKS_EXCLUDED(mutex_) {
    return Iterator(ptr_.load(std::memory_order_acquire));
  }

  // TODO

  // Returns the number of available slots in the hash set.
  //
  // WARNING: the value returned by this function is purely advisory. By the time the function
  // returns, the data structure may have been rehashed any number of times. If you need to know the
  // exact capacity you need to implement your own synchronization (typically using a mutex).
  size_type capacity() const noexcept {
    Array const *const array = ptr_.load(std::memory_order_acquire);
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
  size_type size() const noexcept {
    Array const *const array = ptr_.load(std::memory_order_acquire);
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
    absl::MutexLock lock(&mutex_);
    ptr_.store(nullptr, std::memory_order_release);
  }

  std::pair<iterator, bool> insert(value_type const &key) { return Insert(key); }

  std::pair<iterator, bool> insert(value_type &&key) { return Insert(std::move(key)); }

  template <typename InputIt>
  void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  void insert(std::initializer_list<value_type> const init) {
    for (auto const &key : init) {
      insert(key);
    }
  }

  // TODO

  // Emplaces the specified element and returns true if emplacement happened (i.e. the element was
  // not present in the hash set prior to this call) and false otherwise.
  template <typename... Args>
  std::pair<iterator, bool> emplace(Args &&...args) ABSL_LOCKS_EXCLUDED(mutex_);

  // TODO

  size_type count(key_type const &key) const { return Find(key) != nullptr ? 1 : 0; }

  template <typename KeyArg = key_type>
  size_type count(key_arg_t<KeyArg> const &key) const {
    return Find(key) != nullptr ? 1 : 0;
  }

  // TODO

  bool contains(key_type const &key) const { return Find(key) != nullptr; }

  template <typename KeyArg = key_type>
  bool contains(key_arg_t<KeyArg> const &key) const {
    return Find(key) != nullptr;
  }

  // TODO

 private:
  // Rehash (halving the capacity) when the number of elements becomes less than half the capacity.
  // Respecting `Array::kMinCapacity` has priority: a rehash is not performed if there are only
  // `Array::kMinCapacity` slots.
  static inline double constexpr kMinLoadFactor = 0.5;

  // Rehash (doubling the capacity) when the number of elements exceeds 75% of the capacity.
  static inline double constexpr kMaxLoadFactor = 0.75;

  Array *CreateArray(size_t capacity, size_t initial_size) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  template <typename... Args>
  Node *CreateNode(Args &&...args) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void DestroyNode(Node *node) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Array *Grow(Array const *array, Node *new_node) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  template <typename KeyArg = Key>
  Node *Find(key_arg_t<KeyArg> const &key) const;

  template <typename KeyArg>
  std::pair<Iterator, bool> Insert(KeyArg &&key) ABSL_LOCKS_EXCLUDED(mutex_);

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
void lock_free_hash_set<Key, Hash, Equal, Allocator>::Array::InsertNodeRelaxed(Node *const node) {
  size_t const offset = node->hash & hash_mask;
  size_t i = 0;
  while (true) {
    size_t const index = (offset + i * i) & hash_mask;
    Node *expected = nullptr;
    if (data[index].compare_exchange_strong(expected, node, std::memory_order_relaxed,
                                            std::memory_order_relaxed)) {
      size.fetch_add(1, std::memory_order_relaxed);
      return;
    } else {
      ++i;
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
    array->~Array();
    array_alloc_.deallocate(array, 1);
  }
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
template <typename... Args>
std::pair<typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Iterator, bool>
lock_free_hash_set<Key, Hash, Equal, Allocator>::emplace(Args &&...args) {
  Node *const new_node = CreateNode(hasher_, std::forward<Args>(args)...);
  size_t const hash = new_node->hash;
  absl::MutexLock lock(&mutex_);
  Array *array = ptr_.load(std::memory_order_relaxed);
  bool store_ptr = false;
  if (!array) {
    array = CreateArray(Array::kMinCapacity, 0);
    store_ptr = true;
  }
  size_t const mask = array->hash_mask;
  size_t const offset = hash & mask;
  size_t i = 0;
  while (true) {
    size_t const index = (offset + i * i) & mask;
    Node const *const node = array->data[index].load(std::memory_order_relaxed);
    if (node) {
      if (hash == node->hash && equal_(new_node->key, node->key)) {
        DestroyNode(new_node);
        if (store_ptr) {
          ptr_.store(array, std::memory_order_release);
        }
        return std::make_pair(Iterator(array, index, node), false);
      } else {
        ++i;
      }
    } else {
      size_t const size = array->size.load(std::memory_order_relaxed);
      if (size + 1 > kMaxLoadFactor * array->capacity) {
        array = Grow(array, new_node);
        ptr_.store(array, std::memory_order_release);
      } else {
        array->InsertNodeRelaxed(new_node);
        if (store_ptr) {
          ptr_.store(array, std::memory_order_release);
        }
      }
      return std::make_pair(Iterator(array, index, new_node), true);
    }
  }
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
void lock_free_hash_set<Key, Hash, Equal, Allocator>::DestroyNode(Node *const node) {
  node->~Node();
  node_alloc_.deallocate(node, 1);
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Array *
lock_free_hash_set<Key, Hash, Equal, Allocator>::CreateArray(size_t capacity,
                                                             size_t const initial_size) {
  if (capacity < Array::kMinCapacity) {
    capacity = Array::kMinCapacity;
  }
  size_t const byte_size = sizeof(Array) + (capacity - 1) * sizeof(std::atomic<Node *>);
  // Add an extra `sizeof(Array)` before dividing to account for a possible spillover.
  Array *const array = array_alloc_.allocate((byte_size + sizeof(Array)) / sizeof(Array));
  new (array) Array(capacity, initial_size);
  arrays_.push_back(array);
  return array;
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
template <typename... Args>
typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Node *
lock_free_hash_set<Key, Hash, Equal, Allocator>::CreateNode(Args &&...args) {
  auto const node = node_alloc_.allocate(1);
  new (node) Node(std::forward<Args>(args)...);
  nodes_.push_back(node);
  return node;
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Array *
lock_free_hash_set<Key, Hash, Equal, Allocator>::Grow(Array const *const array,
                                                      Node *const new_node) {
  Array *const new_array =
      CreateArray(array->capacity * 2, array->size.load(std::memory_order_relaxed) + 1);
  for (size_t i = 0; i < array->capacity; ++i) {
    Node *const node = array->data[i].load(std::memory_order_relaxed);
    if (node) {
      new_array->InsertNodeRelaxed(node);
    }
  }
  new_array->InsertNodeRelaxed(new_node);
  arrays_.push_back(new_array);
  return new_array;
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
template <typename KeyArg>
typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Node *
lock_free_hash_set<Key, Hash, Equal, Allocator>::Find(key_arg_t<KeyArg> const &key) const {
  size_t const hash = hasher_(key);
  Array const *const array = ptr_.load(std::memory_order_acquire);
  if (!array) {
    return nullptr;
  }
  size_t const mask = array->hash_mask;
  size_t const offset = hash & mask;
  size_t i = 0;
  while (true) {
    Node *const node = array->data[(offset + i * i) & mask].load(std::memory_order_acquire);
    if (node) {
      if (hash == node->hash && equal_(key, node->key)) {
        return node;
      } else {
        ++i;
      }
    } else {
      return nullptr;
    }
  }
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
template <typename KeyArg>
std::pair<typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Iterator, bool>
lock_free_hash_set<Key, Hash, Equal, Allocator>::Insert(KeyArg &&key) {
  size_t const hash = hasher_(key);
  absl::MutexLock lock(&mutex_);
  Array *array = ptr_.load(std::memory_order_relaxed);
  bool store_ptr = false;
  if (!array) {
    array = CreateArray(Array::kMinCapacity, 0);
    store_ptr = true;
  }
  size_t const mask = array->hash_mask;
  size_t const offset = hash & mask;
  size_t i = 0;
  while (true) {
    size_t const index = (offset + i * i) & mask;
    Node *node = array->data[index].load(std::memory_order_relaxed);
    if (node) {
      if (hash == node->hash && equal_(key, node->key)) {
        if (store_ptr) {
          ptr_.store(array, std::memory_order_release);
        }
        return std::make_pair(Iterator(array, index, node), false);
      } else {
        ++i;
      }
    } else {
      node = CreateNode(std::forward<KeyArg>(key), hash);
      size_t const size = array->size.load(std::memory_order_relaxed);
      if (size + 1 > kMaxLoadFactor * array->capacity) {
        array = Grow(array, node);
        ptr_.store(array, std::memory_order_release);
      } else {
        array->InsertNodeRelaxed(node);
        if (store_ptr) {
          ptr_.store(array, std::memory_order_release);
        }
      }
      return std::make_pair(Iterator(array, index, node), true);
    }
  }
}

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_LOCK_FREE_HASH_SET_H__
