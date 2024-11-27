#ifndef __TSDB2_COMMON_RAW_LOCK_FREE_HASH_H__
#define __TSDB2_COMMON_RAW_LOCK_FREE_HASH_H__

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/synchronization/mutex.h"

namespace tsdb2 {
namespace common {
namespace internal {
namespace lock_free_container {

// Holds a single (pre-hashed) element.
//
// The main template stores both a key and a value and is used by `lock_free_hash_map`.
template <typename Key, typename Value>
struct Node final {
  using DataType = std::pair<Key const, Value>;

  // Resolves the `Node` constructor overload that receives the caller-provided hash.
  struct Hashed {};
  static inline Hashed constexpr kHashed;

  // Resolves the `Node` constructor overload that hashes the key.
  struct ToHash {};
  static inline ToHash constexpr kToHash;

  static Key const &ToKey(DataType const &data) { return data.first; }

  // Constructs a node with the caller-provided hash.
  template <typename... Args>
  explicit Node(Hashed const, size_t const node_hash, Args &&...args)
      : data(std::forward<Args>(args)...), hash(node_hash) {}

  // Piecewise-constructs a node with the caller-provided hash.
  template <typename KeyArg, typename... ValueArgs>
  explicit Node(Hashed const, size_t const node_hash, std::piecewise_construct_t const,
                KeyArg &&key_arg, ValueArgs &&...value_args)
      : data(std::piecewise_construct, std::forward_as_tuple(std::forward<KeyArg>(key_arg)),
             std::forward_as_tuple(std::forward<ValueArgs>(value_args)...)),
        hash(node_hash) {}

  // Constructs a node, hashing it automatically with the given hasher.
  template <typename Hash, typename... Args>
  explicit Node(ToHash const, Hash const &hasher, Args &&...args)
      : data(std::forward<Args>(args)...), hash(hasher(data.first)) {}

  // Piecewise-constructs a node, hashing it automatically with the given hasher.
  template <typename Hash, typename KeyArg, typename... ValueArgs>
  explicit Node(ToHash const, Hash const &hasher, std::piecewise_construct_t const,
                KeyArg &&key_arg, ValueArgs &&...value_args)
      : data(std::piecewise_construct, std::forward_as_tuple(std::forward<KeyArg>(key_arg)),
             std::forward_as_tuple(std::forward<ValueArgs>(value_args)...)),
        hash(hasher(data.first)) {}

  Node(Node const &) = delete;
  Node &operator=(Node const &) = delete;
  Node(Node &&) = delete;
  Node &operator=(Node &&) = delete;

  Key const &key() const { return data.first; }

  DataType data;
  size_t const hash;
  std::atomic<bool> deleted{false};
};

// Node specialization for `void` values (i.e. for `lock_free_hash_set`).
template <typename Key>
struct Node<Key, void> final {
  using DataType = Key const;

  // Resolves the `Node` constructor overload that receives the caller-provided hash.
  struct Hashed {};
  static inline Hashed constexpr kHashed;

  // Resolves the `Node` constructor overload that hashes the key.
  struct ToHash {};
  static inline ToHash constexpr kToHash;

  static Key const &ToKey(DataType const &data) { return data; }

  // Constructs a node with the caller-provided hash.
  template <typename... Args>
  explicit Node(Hashed const, size_t const node_hash, Args &&...args)
      : data(std::forward<Args>(args)...), hash(node_hash) {}

  // Constructs a node, hashing it automatically.
  template <typename Hash, typename... Args>
  explicit Node(ToHash const, Hash const &hasher, Args &&...args)
      : data(std::forward<Args>(args)...), hash(hasher(data)) {}

  Node(Node const &) = delete;
  Node &operator=(Node const &) = delete;
  Node(Node &&) = delete;
  Node &operator=(Node &&) = delete;

  Key const &key() const { return data; }

  DataType data;
  size_t const hash;
  std::atomic<bool> deleted{false};
};

// Internal implementation of `lock_free_hash_set` and `lock_free_hash_map`.
template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
class RawLockFreeHash {
 private:
  using Node = Node<Key, Value>;
  using NodeAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
  using NodePtr = typename NodeAllocator::pointer;
  using NodePtrAllocator =
      typename std::allocator_traits<Allocator>::template rebind_alloc<NodePtr>;

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
    // REQUIRES: the parent's mutex must be locked exclusively.
    size_t InsertNodeLocked(Node *node);

    // Number of `data` slots. This is always a power of 2.
    uint8_t const capacity_log2;

    // The number of elements in the array, i.e. the number of non-null, non-deleted `data`
    // elements. This is only an advisory value used to implement methods like
    // `RawLockFreeHash::size()`, because keeping it atomically in sync with the content of `data`
    // is impossible.
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
  using ArrayPtr = typename ArrayAllocator::pointer;
  using ArrayPtrAllocator =
      typename std::allocator_traits<Allocator>::template rebind_alloc<ArrayPtr>;

  // Token for overload resolution of begin iterator constructors.
  struct BeginIterator {};
  static inline BeginIterator constexpr kBeginIterator;

  // Token for overload resolution of end iterator constructors.
  struct EndIterator {};
  static inline EndIterator constexpr kEndIterator;

  class BaseIterator {
   public:
    explicit BaseIterator() : parent_(nullptr), index_(0), node_(nullptr) {}

    BaseIterator(BaseIterator const &) = default;
    BaseIterator &operator=(BaseIterator const &) = default;
    BaseIterator(BaseIterator &&) noexcept = default;
    BaseIterator &operator=(BaseIterator &&) noexcept = default;

    friend bool operator==(BaseIterator const &lhs, BaseIterator const &rhs) {
      return lhs.node_ == rhs.node_;
    }

    friend bool operator!=(BaseIterator const &lhs, BaseIterator const &rhs) {
      return lhs.node_ != rhs.node_;
    }

   protected:
    // Constructs the begin iterator.
    explicit BaseIterator(BeginIterator const, RawLockFreeHash const &parent)
        : parent_(&parent), index_(-1), node_(nullptr) {
      Advance();
    }

    // Constructs the end iterator.
    explicit BaseIterator(EndIterator const, RawLockFreeHash const &parent)
        : parent_(&parent), index_(-1), node_(nullptr) {}

    // Constructs an iterator for a specific node & position.
    explicit BaseIterator(RawLockFreeHash const &parent, size_t const index, Node *const node)
        : parent_(&parent), index_(index), node_(node) {}

    // Returns the current index, or a negative value if this is the end iterator or a
    // default-constructed iterator.
    intptr_t index() const { return index_; }

    // Returns the current node, or nullptr if this is the end iterator or a default-constructed
    // iterator.
    Node *node() const { return node_; }

    // Indicates whether this is the end iterator of some hash table. Undefined behavior if the
    // iterator is default-constructed or copied / moved from a default-constructed one.
    bool is_end() const { return index_ < 0; }

    void Advance() {
      node_ = nullptr;
      auto const array = parent_->ptr_.load(std::memory_order_acquire);
      if (!array) {
        index_ = -1;
        return;
      }
      auto const capacity = array->capacity();
      while (++index_ < capacity) {
        auto const node = array->data[index_].load(std::memory_order_acquire);
        if (node && !node->deleted.load(std::memory_order_relaxed)) {
          node_ = node;
          return;
        }
      }
      index_ = -1;
    }

    void MoveBack() {
      node_ = nullptr;
      auto const array = parent_->ptr_.load(std::memory_order_acquire);
      if (!array) {
        index_ = -1;
        return;
      }
      if (index_ < 0) {
        index_ = array->capacity();
      }
      while (index_-- > 0) {
        auto const node = array->data[index_].load(std::memory_order_acquire);
        if (node && !node->deleted.load(std::memory_order_relaxed)) {
          node_ = node;
          return;
        }
      }
    }

   private:
    // Pointer to the parent hash table. nullptr means this is an empty, default-constructed
    // iterator.
    RawLockFreeHash const *parent_;

    // Index to the current element. A negative value means this is the end iterator.
    intptr_t index_;

    // Caches the current element so that we can provide better consistency guarantees and avoid
    // dereferencing null pointers if a writer modifies the hash table while one or more threads are
    // enumerating it. This field is set to nullptr when the iterator passes the end.
    Node *node_;
  };

 public:
  class Iterator final : public BaseIterator {
   public:
    explicit Iterator() = default;

    Iterator(Iterator const &) = default;
    Iterator &operator=(Iterator const &) = default;
    Iterator(Iterator &&) noexcept = default;
    Iterator &operator=(Iterator &&) noexcept = default;

    typename Node::DataType &operator*() const { return this->node()->data; }
    typename Node::DataType *operator->() const { return &(this->node()->data); }

    Iterator &operator++() {
      BaseIterator::Advance();
      return *this;
    }

    Iterator operator++(int) {
      Iterator it = *this;
      operator++();
      return it;
    }

    Iterator &operator--() {
      BaseIterator::MoveBack();
      return *this;
    }

    Iterator operator--(int) {
      Iterator it = *this;
      operator--();
      return it;
    }

   private:
    friend class RawLockFreeHash;
    friend class ConstIterator;

    // Constructs the begin iterator.
    explicit Iterator(BeginIterator const, RawLockFreeHash const &parent)
        : BaseIterator(kBeginIterator, parent) {}

    // Constructs the end iterator.
    explicit Iterator(EndIterator const, RawLockFreeHash const &parent)
        : BaseIterator(kEndIterator, parent) {}

    // Constructs an iterator for a specific node & position.
    explicit Iterator(RawLockFreeHash const &parent, size_t const index, Node *const node)
        : BaseIterator(parent, index, node) {}

    using BaseIterator::is_end;
  };

  class ConstIterator final : public BaseIterator {
   public:
    explicit ConstIterator() = default;

    ConstIterator(ConstIterator const &) = default;
    ConstIterator &operator=(ConstIterator const &) = default;
    ConstIterator(ConstIterator &&) noexcept = default;
    ConstIterator &operator=(ConstIterator &&) noexcept = default;

    ConstIterator(Iterator const &other) : BaseIterator(other) {}
    ConstIterator &operator=(Iterator const &other) { return BaseIterator::operator=(other); }

    ConstIterator(Iterator &&other) noexcept : BaseIterator(other) {}
    ConstIterator &operator=(Iterator &&other) noexcept {
      return BaseIterator::operator=(std::move(other));
    }

    typename Node::DataType const &operator*() const { return this->node()->data; }
    typename Node::DataType const *operator->() const { return &(this->node()->data); }

    ConstIterator &operator++() {
      BaseIterator::Advance();
      return *this;
    }

    ConstIterator operator++(int) {
      ConstIterator it = *this;
      operator++();
      return it;
    }

    ConstIterator &operator--() {
      BaseIterator::MoveBack();
      return *this;
    }

    ConstIterator operator--(int) {
      ConstIterator it = *this;
      operator--();
      return it;
    }

   private:
    friend class RawLockFreeHash;

    // Constructs the begin iterator.
    explicit ConstIterator(BeginIterator const, RawLockFreeHash const &parent)
        : BaseIterator(kBeginIterator, parent) {}

    // Constructs the end iterator.
    explicit ConstIterator(EndIterator const, RawLockFreeHash const &parent)
        : BaseIterator(kEndIterator, parent) {}

    // Constructs an iterator for a specific node & position.
    explicit ConstIterator(RawLockFreeHash const &parent, size_t const index, Node *const node)
        : BaseIterator(parent, index, node) {}

    using BaseIterator::index;
    using BaseIterator::is_end;
    using BaseIterator::node;
  };

  using ValueType = typename Node::DataType;
  using AllocatorType = NodeAllocator;
  using AllocatorTraits = std::allocator_traits<AllocatorType>;

  explicit RawLockFreeHash() = default;

  explicit RawLockFreeHash(Hash const &hasher, Equal const &equal = Equal(),
                           Allocator const &alloc = Allocator())
      : hasher_(hasher),
        equal_(equal),
        node_alloc_(alloc),
        array_alloc_(alloc),
        nodes_(alloc),
        arrays_(alloc) {}

  ~RawLockFreeHash();

  AllocatorType get_alloc() const noexcept {
    return AllocatorTraits::select_on_container_copy_construction(node_alloc_);
  }

  Iterator begin() noexcept { return Iterator(kBeginIterator, *this); }
  ConstIterator begin() const noexcept { return ConstIterator(kBeginIterator, *this); }
  ConstIterator cbegin() const noexcept { return ConstIterator(kBeginIterator, *this); }
  Iterator end() noexcept { return Iterator(kEndIterator, *this); }
  ConstIterator end() const noexcept { return ConstIterator(kEndIterator, *this); }
  ConstIterator cend() const noexcept { return ConstIterator(kEndIterator, *this); }

  static bool is_end_iterator(Iterator const &it) { return it.is_end(); }
  static bool is_end_iterator(ConstIterator const &it) { return it.is_end(); }

  // Returns the number of available slots in the hash table.
  //
  // WARNING: the value returned by this function is purely advisory. By the time the function
  // returns, the data structure may have been rehashed any number of times. If you need to know the
  // exact capacity you need to implement your own synchronization (typically using a mutex).
  size_t capacity() const noexcept {
    auto const array = ptr_.load(std::memory_order_acquire);
    if (array) {
      return array->capacity();
    } else {
      return 0;
    }
  }

  // Returns the number of elements in the hash table.
  //
  // WARNING: the value returned by this function is purely advisory. By the time the function
  // returns, any number of changes may have occurred in parallel. If you need to know the exact
  // number of elements you need to implement your own synchronization (typically using a mutex).
  size_t size() const noexcept {
    auto const array = ptr_.load(std::memory_order_acquire);
    if (array) {
      return array->size.load(std::memory_order_relaxed);
    } else {
      return 0;
    }
  }

  // Indicates whether the hash table is empty. Equivalent to `size() == 0`.
  //
  // WARNING: this function relies on `size()`, so the returned value is purely advisory. By the
  // time the function returns, any number of changes may have occurred in parallel. If you need to
  // know the exact number of elements you need to implement your own synchronization (typically
  // using a mutex).
  bool is_empty() const noexcept { return size() == 0; }

  // Returns the maximum load factor, that is the maximum number of elements in relation to the
  // capacity that can be inserted without triggering a rehash.
  float max_load_factor() const noexcept { return kMaxLoadFactorFloat; }

  // Returns the current load factor, that is the number if elements in relation to the capacity.
  //
  // NOTE: this function relies on `size()`, so the returned value is purely advisory. By the time
  // the function returns, any number of changes may have occurred in parallel.
  float load_factor() const noexcept {
    auto const array = ptr_.load(std::memory_order_acquire);
    if (!array) {
      return 0;
    }
    return static_cast<float>(array->size.load(std::memory_order_relaxed)) /
           static_cast<float>(array->capacity());
  }

  Hash const &hash_function() const noexcept { return hasher_; }
  Equal const &key_eq() const noexcept { return equal_; }

  template <typename KeyArg>
  Iterator Find(KeyArg const &key) {
    return FindInternal<Iterator>(key);
  }

  template <typename KeyArg>
  ConstIterator Find(KeyArg const &key) const {
    return FindInternal<ConstIterator>(key);
  }

  // Reserves space for at least `num_elements` elements, rehashing if the current capacity doesn't
  // allow it. The maximum load factor is taken into account, so the resulting capacity is
  // guaranteed to be greater than or equal to `NextPowerOf2(size / kMaxLoadFactor)`.
  void Reserve(size_t num_elements) ABSL_LOCKS_EXCLUDED(mutex_);

  // Reserves space for at least the specified number of *new* elements. This is equivalent to
  // calling `Reserve(size() + num_new_elements)` atomically.
  void ReserveExtra(size_t num_new_elements) ABSL_LOCKS_EXCLUDED(mutex_);

  std::pair<Iterator, bool> Insert(ValueType &&value) {
    return InsertInternal(Node::ToKey(value), std::move(value));
  }

  std::pair<Iterator, bool> Insert(ValueType const &value) { return Insert(ValueType(value)); }

  // Inserts many elements. Better than calling `Insert` multiple times because `InsertMany`
  // acquires the mutex only once. Even if the mutex is uncontended `InsertMany` is faster than
  // `Insert` because the latter attempts an optimistic lock-free lookup, which we don't need for
  // the use case of `InsertMany`.
  //
  // `reserve_count` is the expected number of new elements, typically `last - first`. It's used to
  // reserve space before inserting the elements. It can be set 0, in which case no extra space is
  // reserved beforehand but the hash table will grow as necessary during insertion.
  template <typename InputIt>
  void InsertMany(InputIt first, InputIt last, size_t reserve_count) ABSL_LOCKS_EXCLUDED(mutex_);

  template <typename KeyArg, typename ValueArg>
  std::pair<Iterator, bool> InsertOrAssign(KeyArg &&key, ValueArg &&value)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Used by `lock_free_hash_map`'s subscript operator.
  std::pair<Iterator, bool> InsertDefaultValue(Key const &key) {
    return InsertInternal(key, std::piecewise_construct, key);
  }

  template <typename... Args>
  std::pair<Iterator, bool> Emplace(Args &&...args) ABSL_LOCKS_EXCLUDED(mutex_);

  template <typename KeyArg>
  bool Erase(KeyArg const &key) ABSL_LOCKS_EXCLUDED(mutex_);

  bool Erase(ConstIterator const &it) ABSL_LOCKS_EXCLUDED(mutex_);

  bool Erase(Iterator const &it) { return Erase(ConstIterator(it)); }

  void Clear() noexcept;

  // Swaps the content of this hash table with `other`. All existing iterators are invalidated. This
  // algorithm is not lockless.
  void Swap(RawLockFreeHash &other) { Swap(*this, other); }

  // Swaps the content of two hash tables. All existing iterators are invalidated. This algorithm is
  // not lockless.
  static void Swap(RawLockFreeHash &lhs, RawLockFreeHash &rhs);

 private:
  struct Fraction {
    size_t numerator;
    size_t denominator;
  };

  static inline Fraction constexpr kMaxLoadFactor{.numerator = 1, .denominator = 2};
  static inline float constexpr kMaxLoadFactorFloat =
      static_cast<float>(kMaxLoadFactor.numerator) / static_cast<float>(kMaxLoadFactor.denominator);

  // No moves & copies because they wouldn't be thread-safe.
  RawLockFreeHash(RawLockFreeHash const &) = delete;
  RawLockFreeHash &operator=(RawLockFreeHash const &) = delete;
  RawLockFreeHash(RawLockFreeHash &&) = delete;
  RawLockFreeHash &operator=(RawLockFreeHash &&) = delete;

  // Rounds `value` up to the next power of 2. Used by `NextExponentOf2`.
  static uint64_t NextPowerOf2(uint64_t value);

  // Rounds `value` up to the next power of 2 and return the exponent of such power. For example, if
  // `value` is 60 it's rounded up to 64 and 6 is returned. This is used to calculate the array
  // capacity in log2 format, as required by `Array`.
  static uint8_t NextExponentOf2(uint64_t value);

  // Returns `a / b` rounded up to the nearest integer.
  static size_t DivideAndRoundUp(size_t const a, size_t const b) { return (a + b - 1) / b; }

  Array *CreateArray(uint8_t capacity_log2) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void DestroyArray(ArrayPtr array);

  // Constructs a node without adding it to `nodes_`.
  template <typename... Args>
  NodePtr CreateFreeNode(Args &&...args);

  template <typename... Args>
  Node *CreateNode(Args &&...args) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void DestroyNode(NodePtr node);

  template <typename Iterator, typename KeyArg>
  Iterator FindInArray(Array const &array, KeyArg const &key, size_t hash) const;

  template <typename Iterator, typename KeyArg>
  Iterator FindInternal(KeyArg const &key) const;

  // Returns the minimum array capacity (in log2 format) required to store `num_elements`.
  static uint8_t GetMinCapacityLog2(size_t const num_elements) {
    auto const min_capacity =
        DivideAndRoundUp(num_elements * kMaxLoadFactor.denominator, kMaxLoadFactor.numerator);
    return std::max(Array::kMinCapacityLog2, NextExponentOf2(min_capacity));
  }

  // Internal implementation of `Reserve` and `ReserveExtra`.
  Array *ReserveLocked(Array *array, uint8_t min_capacity_log2)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Inserts a node assuming the hash table is empty. `ptr_` must be `nullptr` before entering this
  // function.
  //
  // Used by `Insert` and `Emplace` as a fallback for the case of empty hash table.
  std::pair<Iterator, bool> InsertFirstNode(Node *node) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Inserts a new node assuming the hash table doesn't already contain one with the same key. It's
  // okay if one or more existing nodes have a hash collision with the new one.
  //
  // Used by `Insert` and `Emplace` as a fallback for the case of inserting a new node that isn't
  // already found in the hash table.
  std::pair<Iterator, bool> InsertNewNode(Array *array, Node *node)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Internal implementation of `Insert` methods.
  template <typename... Args>
  std::pair<Iterator, bool> InsertInternal(Key const &key, Args &&...args)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool EraseExistingNode(Array *array, Node *node) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  template <typename KeyArg>
  bool EraseLocked(Array *array, KeyArg const &key, size_t hash)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Performs a re-hash and insertion, doubling the capacity of `old_array`. All elements are copied
  // over, the new node is added, and the new array is stored in `ptr_`. The return value is the
  // index at which the new node was inserted.
  size_t Grow(Array const *old_array, Node *new_node) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS Hash hasher_;
  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS Equal equal_;
  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS NodeAllocator node_alloc_;
  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS ArrayAllocator array_alloc_;

  // All arrays and elements ever allocated by this hash table are kept in the `arrays_` and
  // `nodes_` vectors. They are freed up only when the whole hash table is destroyed. This allows us
  // to implement CAS algorithms without incurring ABA bugs.

  absl::Mutex mutable mutex_;
  std::vector<NodePtr, NodePtrAllocator> nodes_ ABSL_GUARDED_BY(mutex_);
  std::vector<ArrayPtr, ArrayPtrAllocator> arrays_ ABSL_GUARDED_BY(mutex_);

  // Points to the current data.
  std::atomic<Array *> ptr_ = nullptr;
};

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
size_t RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::Array::InsertNodeLocked(
    Node *const new_node) {
  size_t const mask = hash_mask();
  for (size_t i = new_node->hash, j = 0;; i += ++j) {
    auto const index = i & mask;
    auto const node = data[index].load(std::memory_order_relaxed);
    if (!node || node->deleted.load(std::memory_order_relaxed)) {
      data[index].store(new_node, std::memory_order_release);
      size.fetch_add(1, std::memory_order_relaxed);
      return index;
    }
  }
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
bool RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::EraseExistingNode(Array *const array,
                                                                            Node *const node) {
  if (node->deleted.exchange(true, std::memory_order_relaxed)) {
    return false;
  }
  array->size.fetch_sub(1, std::memory_order_relaxed);
  return true;
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
template <typename KeyArg>
bool RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::EraseLocked(Array *const array,
                                                                      KeyArg const &key,
                                                                      size_t const hash) {
  auto const mask = array->hash_mask();
  for (size_t i = hash, j = 0;; i += ++j) {
    auto const node = array->data[i & mask].load(std::memory_order_relaxed);
    if (!node) {
      return false;
    }
    if (node->hash == hash && equal_(key, node->key())) {
      return EraseExistingNode(array, node);
    }
  }
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::~RawLockFreeHash() {
  absl::MutexLock lock{&mutex_};
  ptr_.store(nullptr, std::memory_order_release);
  for (auto const array : arrays_) {
    DestroyArray(array);
  }
  for (auto const node : nodes_) {
    DestroyNode(node);
  }
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
void RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::Reserve(size_t const num_elements) {
  if (!num_elements) {
    return;
  }
  auto const min_capacity_log2 = GetMinCapacityLog2(num_elements);
  absl::MutexLock lock{&mutex_};
  auto const array = ptr_.load(std::memory_order_relaxed);
  if (!array || array->capacity_log2 < min_capacity_log2) {
    ptr_.store(ReserveLocked(array, min_capacity_log2), std::memory_order_release);
  }
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
void RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::ReserveExtra(
    size_t const num_new_elements) {
  if (!num_new_elements) {
    return;
  }
  absl::MutexLock lock{&mutex_};
  auto const array = ptr_.load(std::memory_order_relaxed);
  auto const current_size = array ? array->size.load(std::memory_order_relaxed) : 0;
  auto const min_capacity_log2 = GetMinCapacityLog2(current_size + num_new_elements);
  if (!array || array->capacity_log2 < min_capacity_log2) {
    ptr_.store(ReserveLocked(array, min_capacity_log2), std::memory_order_release);
  }
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
template <typename InputIt>
void RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::InsertMany(InputIt first,
                                                                     InputIt const last,
                                                                     size_t const reserve_count) {
  absl::MutexLock lock{&mutex_};
  auto array = ptr_.load(std::memory_order_relaxed);
  auto const current_size = array ? array->size.load(std::memory_order_relaxed) : 0;
  auto const min_capacity_log2 = GetMinCapacityLog2(current_size + reserve_count);
  if (!array || array->capacity_log2 < min_capacity_log2) {
    array = ReserveLocked(array, min_capacity_log2);
  }
  size_t const mask = array->hash_mask();
  for (; first != last; ++first) {
    auto const &key = Node::ToKey(*first);
    auto const hash = hasher_(key);
    bool skip = false;
    for (size_t i = hash, j = 0;; i += ++j) {
      auto const index = i & mask;
      auto const node = array->data[index].load(std::memory_order_relaxed);
      if (!node) {
        break;
      }
      if (node->hash == hash && equal_(key, node->key())) {
        if (!node->deleted.load(std::memory_order_relaxed)) {
          skip = true;
        }
        break;
      }
    }
    if (!skip) {
      auto const node = CreateNode(Node::kHashed, hash, *first);
      auto const size = array->size.load(std::memory_order_relaxed);
      if ((size + 1) * kMaxLoadFactor.denominator > array->capacity() * kMaxLoadFactor.numerator) {
        Grow(array, node);
        array = ptr_.load(std::memory_order_relaxed);
      } else {
        array->InsertNodeLocked(node);
      }
    }
  }
  ptr_.store(array, std::memory_order_release);
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
template <typename KeyArg, typename ValueArg>
std::pair<typename RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::Iterator, bool>
RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::InsertOrAssign(KeyArg &&key,
                                                                    ValueArg &&value) {
  auto const hash = hasher_(key);
  auto array = ptr_.load(std::memory_order_acquire);
  if (array) {
    auto it = FindInArray<Iterator>(*array, key, hash);
    if (!it.is_end()) {
      it->second = std::forward<ValueArg>(value);
      return std::make_pair(std::move(it), false);
    }
  }
  absl::MutexLock lock{&mutex_};
  array = ptr_.load(std::memory_order_relaxed);
  if (!array) {
    return InsertFirstNode(CreateNode(Node::kHashed, hash, std::piecewise_construct,
                                      std::forward<KeyArg>(key), std::forward<ValueArg>(value)));
  }
  size_t const mask = array->hash_mask();
  for (size_t i = hash, j = 0;; i += ++j) {
    auto const index = i & mask;
    auto const node = array->data[index].load(std::memory_order_relaxed);
    if (!node) {
      break;
    }
    if (node->hash == hash && equal_(key, node->key())) {
      if (node->deleted.load(std::memory_order_relaxed)) {
        break;
      } else {
        node->data.second = std::forward<ValueArg>(value);
        return std::make_pair(Iterator(*this, index, node), false);
      }
    }
  }
  return InsertNewNode(array, CreateNode(Node::kHashed, hash, std::piecewise_construct,
                                         std::forward<KeyArg>(key), std::forward<ValueArg>(value)));
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
template <typename... Args>
std::pair<typename RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::Iterator, bool>
RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::Emplace(Args &&...args) {
  auto const new_node = CreateFreeNode(Node::kToHash, hasher_, std::forward<Args>(args)...);
  auto array = ptr_.load(std::memory_order_acquire);
  if (array) {
    auto it = FindInArray<Iterator>(*array, new_node->key(), new_node->hash);
    if (!it.is_end()) {
      DestroyNode(new_node);
      return std::make_pair(std::move(it), false);
    }
  }
  absl::MutexLock lock{&mutex_};
  array = ptr_.load(std::memory_order_relaxed);
  if (!array) {
    nodes_.push_back(new_node);
    return InsertFirstNode(&*new_node);
  }
  size_t const mask = array->hash_mask();
  for (size_t i = new_node->hash, j = 0;; i += ++j) {
    auto const index = i & mask;
    auto const node = array->data[index].load(std::memory_order_relaxed);
    if (!node) {
      break;
    }
    if (new_node->hash == node->hash && equal_(new_node->key(), node->key())) {
      if (node->deleted.load(std::memory_order_relaxed)) {
        break;
      } else {
        DestroyNode(new_node);
        return std::make_pair(Iterator(*this, index, node), false);
      }
    }
  }
  nodes_.push_back(new_node);
  return InsertNewNode(array, &*new_node);
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
template <typename KeyArg>
bool RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::Erase(KeyArg const &key) {
  auto const hash = hasher_(key);
  absl::MutexLock lock{&mutex_};
  auto array = ptr_.load(std::memory_order_relaxed);
  if (!array) {
    return false;
  }
  return EraseLocked(array, key, hash);
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
bool RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::Erase(ConstIterator const &it) {
  if (it.is_end()) {
    return false;
  }
  absl::MutexLock lock{&mutex_};
  auto array = ptr_.load(std::memory_order_relaxed);
  if (!array) {
    return false;
  }
  auto const it_index = it.index();
  auto const it_node = it.node();
  if (it_index < array->capacity()) {
    auto const node = array->data[it_index].load(std::memory_order_relaxed);
    if (node->hash == it_node->hash && equal_(node->key(), it_node->key())) {
      return EraseExistingNode(array, node);
    }
  }
  return EraseLocked(array, it_node->key(), it_node->hash);
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
void RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::Clear() noexcept {
  absl::MutexLock lock{&mutex_};
  ptr_.store(nullptr, std::memory_order_release);
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
void RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::Swap(RawLockFreeHash &lhs,
                                                               RawLockFreeHash &rhs) {
  if (&lhs == &rhs) {
    return;
  } else if (&rhs < &lhs) {
    // Make sure the mutexes are locked in a deterministic order by ordering the two hash tables by
    // their memory address. This avoids deadlocks when `Swap` is called concurrently by multiple
    // threads.
    return Swap(rhs, lhs);
  }
  absl::MutexLock lhs_lock{&lhs.mutex_};
  absl::MutexLock rhs_lock{&rhs.mutex_};
  using std::swap;  // ensure ADL
  if (std::allocator_traits<NodeAllocator>::propagate_on_container_swap::value) {
    swap(lhs.node_alloc_, rhs.node_alloc_);
  }
  if (std::allocator_traits<ArrayAllocator>::propagate_on_container_swap::value) {
    swap(lhs.array_alloc_, rhs.array_alloc_);
  }
  swap(lhs.nodes_, rhs.nodes_);
  swap(lhs.arrays_, rhs.arrays_);
  auto ptr = lhs.ptr_.load(std::memory_order_relaxed);
  ptr = rhs.ptr_.exchange(ptr, std::memory_order_relaxed);
  lhs.ptr_.store(ptr, std::memory_order_relaxed);
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
uint64_t RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::NextPowerOf2(uint64_t value) {
  if (!value) {
    return 1;
  }
  --value;
  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
  value |= value >> 32;
  return ++value;
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
uint8_t RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::NextExponentOf2(uint64_t value) {
  value = NextPowerOf2(value);
  uint8_t log2 = 0;
  while ((value & 1) == 0) {
    ++log2;
    value >>= 1;
  }
  return log2;
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
typename RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::Array *
RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::CreateArray(uint8_t const capacity_log2) {
  size_t const capacity = size_t{1} << capacity_log2;
  ArrayPtr const array_ptr = array_alloc_.allocate(Array::GetMinAllocCount(capacity));
  arrays_.emplace_back(array_ptr);
  Array *const array = &*array_ptr;
  new (array) Array(capacity_log2);
  return array;
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
void RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::DestroyArray(ArrayPtr const array) {
  if (array) {
    auto const capacity = array->capacity();
    array->~Array();
    array_alloc_.deallocate(array, Array::GetMinAllocCount(capacity));
  }
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
template <typename... Args>
typename RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::NodePtr
RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::CreateFreeNode(Args &&...args) {
  auto const node = node_alloc_.allocate(1);
  new (&*node) Node(std::forward<Args>(args)...);
  return node;
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
template <typename... Args>
typename RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::Node *
RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::CreateNode(Args &&...args) {
  auto const node = CreateFreeNode(std::forward<Args>(args)...);
  nodes_.push_back(node);
  return &*node;
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
void RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::DestroyNode(NodePtr const node) {
  node->~Node();
  node_alloc_.deallocate(node, 1);
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
template <typename Iterator, typename KeyArg>
Iterator RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::FindInArray(Array const &array,
                                                                          KeyArg const &key,
                                                                          size_t const hash) const {
  size_t const mask = array.hash_mask();
  for (size_t i = hash, j = 0;; i += ++j) {
    auto const index = i & mask;
    auto const node = array.data[index].load(std::memory_order_acquire);
    if (!node) {
      return Iterator(kEndIterator, *this);
    }
    if (node->hash == hash && equal_(key, node->key())) {
      if (node->deleted.load(std::memory_order_relaxed)) {
        return Iterator(kEndIterator, *this);
      } else {
        return Iterator(*this, index, node);
      }
    }
  }
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
template <typename Iterator, typename KeyArg>
Iterator RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::FindInternal(
    KeyArg const &key) const {
  Array const *const array = ptr_.load(std::memory_order_acquire);
  if (array) {
    return FindInArray<Iterator>(*array, key, hasher_(key));
  } else {
    return Iterator(kEndIterator, *this);
  }
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
typename RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::Array *
RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::ReserveLocked(
    Array *const array, uint8_t const min_capacity_log2) {
  auto const new_array = CreateArray(min_capacity_log2);
  if (array) {
    auto const old_capacity = array->capacity();
    for (size_t i = 0; i < old_capacity; ++i) {
      auto const node = array->data[i].load(std::memory_order_relaxed);
      if (node && !node->deleted.load(std::memory_order_relaxed)) {
        new_array->InsertNodeLocked(node);
      }
    }
  }
  return new_array;
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
std::pair<typename RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::Iterator, bool>
RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::InsertFirstNode(Node *const node) {
  auto const array = CreateArray(Array::kMinCapacityLog2);
  auto const index = array->InsertNodeLocked(node);
  ptr_.store(array, std::memory_order_release);
  return std::make_pair(Iterator(*this, index, node), true);
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
std::pair<typename RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::Iterator, bool>
RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::InsertNewNode(Array *const array,
                                                                   Node *const node) {
  auto const size = array->size.load(std::memory_order_relaxed);
  if ((size + 1) * kMaxLoadFactor.denominator > array->capacity() * kMaxLoadFactor.numerator) {
    return std::make_pair(Iterator(*this, Grow(array, node), node), true);
  } else {
    return std::make_pair(Iterator(*this, array->InsertNodeLocked(node), node), true);
  }
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
template <typename... Args>
std::pair<typename RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::Iterator, bool>
RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::InsertInternal(Key const &key,
                                                                    Args &&...args) {
  auto const hash = hasher_(key);
  auto array = ptr_.load(std::memory_order_acquire);
  if (array) {
    auto it = FindInArray<Iterator>(*array, key, hash);
    if (!it.is_end()) {
      return std::make_pair(std::move(it), false);
    }
  }
  absl::MutexLock lock{&mutex_};
  array = ptr_.load(std::memory_order_relaxed);
  if (!array) {
    return InsertFirstNode(CreateNode(Node::kHashed, hash, std::forward<Args>(args)...));
  }
  size_t const mask = array->hash_mask();
  for (size_t i = hash, j = 0;; i += ++j) {
    auto const index = i & mask;
    auto const node = array->data[index].load(std::memory_order_relaxed);
    if (!node) {
      break;
    }
    if (node->hash == hash && equal_(key, node->key())) {
      if (node->deleted.load(std::memory_order_relaxed)) {
        break;
      } else {
        return std::make_pair(Iterator(*this, index, node), false);
      }
    }
  }
  return InsertNewNode(array, CreateNode(Node::kHashed, hash, std::forward<Args>(args)...));
}

template <typename Key, typename Value, typename Hash, typename Equal, typename Allocator>
size_t RawLockFreeHash<Key, Value, Hash, Equal, Allocator>::Grow(Array const *const old_array,
                                                                 Node *const new_node) {
  auto const new_array = CreateArray(old_array->capacity_log2 + 1);
  auto const old_capacity = old_array->capacity();
  for (size_t i = 0; i < old_capacity; ++i) {
    auto const node = old_array->data[i].load(std::memory_order_relaxed);
    if (node && !node->deleted.load(std::memory_order_relaxed)) {
      new_array->InsertNodeLocked(node);
    }
  }
  auto const node_index = new_array->InsertNodeLocked(new_node);
  ptr_.store(new_array, std::memory_order_release);
  return node_index;
}

}  // namespace lock_free_container
}  // namespace internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RAW_LOCK_FREE_HASH_H__
