#ifndef __TSDB2_COMMON_LOCK_FREE_HASH_SET_H__
#define __TSDB2_COMMON_LOCK_FREE_HASH_SET_H__

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

// A thread-safe, lock-free, insert-only hash set data structure. The provided API is a subset of
// `std::unordered_set`.
//
// All reads (lookups and iterations) are lockless: all synchronization is performed by
// `lock_free_hash_set` using atomics. Writers are automatically serialized by acquiring an
// exclusive lock on an internal mutex. Inserts / emplacements are the only available modifying
// operations; removals are infeasible.
//
// Under heavily contended read scenarios `lock_free_hash_set` can be much faster than other hash
// set data structures guarded by a mutex.
//
// NOTE: iterations are loosely consistent. If a rehash occurs during an iteration it is quite
// possible that the iterator misses some elements and/or returns some elements more than once.
// However it will never return invalid or corrupted data.
//
// To reduce the number of heap allocations and increase cache friendliness, `lock_free_hash_set`
// uses quadratic open addressing. To speed up lookups even further, values are pre-hashed so that a
// hash doesn't have to be re-calculated for every colliding element and the probing algorithm can
// short-circuit and avoid a full comparison of every colliding element.
template <typename Key, typename Hash = absl::Hash<Key>, typename Equal = std::equal_to<Key>,
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
  };

  using NodeAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
  using NodePtrAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<
      typename NodeAllocator::pointer>;

  struct Array final {
    static inline size_t constexpr kMinCapacity = 32;

    // Returns the minimum number of consecutive `Array` objects to make room for in the array
    // allocator in order to get a single `Array` with `capacity` node slots. This is used in
    // `array_alloc_.allocate` and `array_alloc_.deallocate` calls.
    static size_t GetMinAllocCount(size_t const capacity) {
      size_t const byte_size = sizeof(Array) + (capacity - 1) * sizeof(std::atomic<Node *>);
      // Since the integer division truncates fractional results, add an extra `sizeof(Array)`
      // before dividing to account for a possible spillover.
      return (byte_size + sizeof(Array)) / sizeof(Array);
    }

    // REQUIRES: `array_capacity` must be a power of 2 and must be greater than or equal to
    // `kMinCapacity`.
    explicit Array(size_t const array_capacity, size_t const array_size)
        : capacity(array_capacity), hash_mask(capacity - 1), size(array_size) {
      for (size_t i = 1; i < capacity; ++i) {
        new (data + i) std::atomic<Node *>(nullptr);
      }
    }

    // Returns the index of the i-th slot of the bucket identified by `hash`.
    size_t index(size_t const hash, size_t const i) const {
      return (hash + ((i + i * i) >> 1)) & hash_mask;
    }

    // Inserts the provided node in the array, returning the index at which the node was inserted.
    //
    // NOTE: this function uses relaxed memory ordering because it assumes that the caller owns an
    // exclusive lock. That must be the case because all `lock_free_hash_set` writers are serialized
    // by a mutex.
    size_t InsertNodeRelaxed(Node *node);

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
      while (++index_ < array->capacity) {
        node_ = array->data[index_].load(std::memory_order_acquire);
        if (node_) {
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
        index_ = array->capacity;
      }
      while (index_-- > 0) {
        node_ = array->data[index_].load(std::memory_order_acquire);
        if (node_) {
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
  using reference = value_type &;
  using const_reference = value_type const &;
  using pointer = typename std::allocator_traits<NodeAllocator>::pointer;
  using const_pointer = typename std::allocator_traits<NodeAllocator>::const_pointer;
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

  // TODO: other constructors

  ~lock_free_hash_set();

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
    // TODO: optimize by reserving space and acquiring the mutex only once.
    for (; first != last; ++first) {
      Insert(*first);
    }
  }

  void insert(std::initializer_list<value_type> const list) {
    // TODO: optimize by reserving space and acquiring the mutex only once.
    for (auto &value : list) {
      Insert(value);
    }
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args &&...args) {
    return Emplace(std::forward<Args>(args)...);
  }

  // TODO

  size_type count(key_type const &key) const { return Find(key).is_end() ? 0 : 1; }

  template <typename KeyArg = key_type>
  size_type count(key_arg_t<KeyArg> const &key) const {
    return Find(key).is_end() ? 0 : 1;
  }

  iterator find(key_type const &key) { return Find(key); }
  const_iterator find(key_type const &key) const { return Find(key); }

  template <typename KeyArg = key_type>
  iterator find(key_arg_t<KeyArg> const &key) {
    return Find(key);
  }

  bool contains(key_type const &key) const { return !Find(key).is_end(); }

  template <typename KeyArg = key_type>
  bool contains(key_arg_t<KeyArg> const &key) const {
    return !Find(key).is_end();
  }

  // TODO

 private:
  // No moves & copies because they wouldn't be thread-safe.
  lock_free_hash_set(lock_free_hash_set const &) = delete;
  lock_free_hash_set &operator=(lock_free_hash_set const &) = delete;
  lock_free_hash_set(lock_free_hash_set &&) = delete;
  lock_free_hash_set &operator=(lock_free_hash_set &&) = delete;

  Array *CreateArray(size_t capacity, size_t initial_size) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void DestroyArray(Array *array);

  // Constructs a node without adding it to `nodes_`.
  template <typename... Args>
  Node *CreateFreeNode(Args &&...args);

  template <typename... Args>
  Node *CreateNode(Args &&...args) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void DestroyNode(Node *node);

  template <typename KeyArg>
  Iterator Find(KeyArg const &key) const;

  // Rehashes `array` doubling its capacity, copying over all existing nodes, and adding in
  // `new_node`. Returns a pointer to the new array and the index at which `new_node` was inserted.
  std::pair<Array *, size_t> Grow(Array const *array, Node *new_node)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  template <typename KeyArg>
  std::pair<Iterator, bool> Insert(KeyArg &&key) ABSL_LOCKS_EXCLUDED(mutex_);

  template <typename... Args>
  std::pair<Iterator, bool> Emplace(Args &&...args) ABSL_LOCKS_EXCLUDED(mutex_);

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
size_t lock_free_hash_set<Key, Hash, Equal, Allocator>::Array::InsertNodeRelaxed(Node *const node) {
  size_t i = 0;
  while (true) {
    size_t const j = index(node->hash, i);
    Node *expected = nullptr;
    if (data[j].compare_exchange_strong(expected, node, std::memory_order_relaxed,
                                        std::memory_order_relaxed)) {
      size.fetch_add(1, std::memory_order_relaxed);
      return j;
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
    DestroyArray(array);
  }
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Array *
lock_free_hash_set<Key, Hash, Equal, Allocator>::CreateArray(size_t capacity,
                                                             size_t const initial_size) {
  if (capacity < Array::kMinCapacity) {
    capacity = Array::kMinCapacity;
  }
  Array *const array = array_alloc_.allocate(Array::GetMinAllocCount(capacity));
  new (array) Array(capacity, initial_size);
  arrays_.push_back(array);
  return array;
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
void lock_free_hash_set<Key, Hash, Equal, Allocator>::DestroyArray(Array *const array) {
  auto const capacity = array->capacity;
  array->~Array();
  array_alloc_.deallocate(array, Array::GetMinAllocCount(capacity));
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
  size_t const hash = hasher_(key);
  Array const *const array = ptr_.load(std::memory_order_acquire);
  if (!array) {
    return Iterator(Iterator::kEndIterator, this);
  }
  size_t i = 0;
  while (true) {
    size_t const index = array->index(hash, i);
    Node *const node = array->data[index].load(std::memory_order_acquire);
    if (node) {
      if (hash == node->hash && equal_(key, node->key)) {
        return Iterator(this, index, node);
      } else {
        ++i;
      }
    } else {
      return Iterator(Iterator::kEndIterator, this);
    }
  }
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
std::pair<typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Array *, size_t>
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
  size_t const index = new_array->InsertNodeRelaxed(new_node);
  arrays_.push_back(new_array);
  return std::make_pair(new_array, index);
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
template <typename KeyArg>
std::pair<typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Iterator, bool>
lock_free_hash_set<Key, Hash, Equal, Allocator>::Insert(KeyArg &&key) {
  size_t const hash = hasher_(key);
  absl::MutexLock lock{&mutex_};
  Array *array = ptr_.load(std::memory_order_relaxed);
  bool store_ptr = false;
  if (!array) {
    array = CreateArray(Array::kMinCapacity, 0);
    store_ptr = true;
  }
  size_t i = 0;
  while (true) {
    size_t index = array->index(hash, i);
    Node *node = array->data[index].load(std::memory_order_relaxed);
    if (node) {
      if (hash == node->hash && equal_(key, node->key)) {
        if (store_ptr) {
          ptr_.store(array, std::memory_order_release);
        }
        return std::make_pair(Iterator(this, index, node), false);
      } else {
        ++i;
      }
    } else {
      node = CreateNode(Node::kHashed, hash, std::forward<KeyArg>(key));
      size_t const size = array->size.load(std::memory_order_relaxed);
      if ((size + 1) * 2 > array->capacity) {  // rehash if more than 50% full
        std::tie(array, index) = Grow(array, node);
        ptr_.store(array, std::memory_order_release);
      } else {
        index = array->InsertNodeRelaxed(node);
        if (store_ptr) {
          ptr_.store(array, std::memory_order_release);
        }
      }
      return std::make_pair(Iterator(this, index, node), true);
    }
  }
}

template <typename Key, typename Hash, typename Equal, typename Allocator>
template <typename... Args>
std::pair<typename lock_free_hash_set<Key, Hash, Equal, Allocator>::Iterator, bool>
lock_free_hash_set<Key, Hash, Equal, Allocator>::Emplace(Args &&...args) {
  Node *const new_node = CreateFreeNode(Node::kToHash, hasher_, std::forward<Args>(args)...);
  size_t const hash = new_node->hash;
  absl::MutexLock lock(&mutex_);
  Array *array = ptr_.load(std::memory_order_relaxed);
  bool store_ptr = false;
  if (!array) {
    array = CreateArray(Array::kMinCapacity, 0);
    store_ptr = true;
  }
  size_t i = 0;
  while (true) {
    size_t index = array->index(hash, i);
    Node *const node = array->data[index].load(std::memory_order_relaxed);
    if (node) {
      if (hash == node->hash && equal_(new_node->key, node->key)) {
        DestroyNode(new_node);
        if (store_ptr) {
          ptr_.store(array, std::memory_order_release);
        }
        return std::make_pair(Iterator(this, index, node), false);
      } else {
        ++i;
      }
    } else {
      nodes_.push_back(new_node);
      size_t const size = array->size.load(std::memory_order_relaxed);
      if ((size + 1) * 2 > array->capacity) {  // rehash if more than 50% full
        std::tie(array, index) = Grow(array, new_node);
        ptr_.store(array, std::memory_order_release);
      } else {
        index = array->InsertNodeRelaxed(new_node);
        if (store_ptr) {
          ptr_.store(array, std::memory_order_release);
        }
      }
      return std::make_pair(Iterator(this, index, new_node), true);
    }
  }
}

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_LOCK_FREE_HASH_SET_H__
