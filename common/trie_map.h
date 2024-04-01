#ifndef __TSDB2_COMMON_TRIE_MAP_H__
#define __TSDB2_COMMON_TRIE_MAP_H__

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "absl/base/attributes.h"
#include "common/raw_trie.h"

namespace tsdb2 {
namespace common {

// trie_map is an associative container mapping strings to arbitrary values. trie_map is implemented
// as a compressed trie, aka a radix tree. The provided API is similar to `std::map<std::string,
// Value>`.
//
// Notable differences between std::map and trie_map:
//
//  * Node handles are not supported. That is because, by definition, a trie node doesn't have all
//    the information about its key, so most of the node API wouldn't make sense.
//  * The worst-case space complexity of our iterators is linear in the length of the stored string.
//    trie_map iterators are cheap to move but relatively expensive to copy.
//  * An `emplace` method is not provided because in order to be inserted in the trie a string must
//    be split, so it cannot be emplaced. Note that a `try_emplace` method is still provided because
//    in that case we can perform in-place construction of the value.
//
template <typename Value, typename Allocator = std::allocator<std::pair<std::string const, Value>>>
class trie_map {
 private:
  using Node = internal::TrieNode<std::optional<Value>, Allocator>;
  using NodeSet = typename Node::NodeSet;

 public:
  using key_type = std::string;
  using mapped_type = Value;
  using value_type = std::pair<std::string const, Value>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using allocator_type = typename Node::EntryAllocator;
  using allocator_traits = std::allocator_traits<allocator_type>;
  using reference = value_type&;
  using const_reference = value_type const&;
  using pointer = typename std::allocator_traits<Allocator>::pointer;
  using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
  using iterator = typename Node::Iterator;
  using const_iterator = typename Node::ConstIterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  trie_map() = default;

  explicit trie_map(allocator_type const& alloc)
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)) {}

  template <class InputIt>
  trie_map(InputIt first, InputIt last, allocator_type const& alloc = allocator_type())
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)) {
    insert(first, last);
  }

  trie_map(trie_map const& other)
      : alloc_(allocator_traits::select_on_container_copy_construction(other.alloc_)),
        roots_(other.roots_),
        size_(other.size_) {}

  trie_map(trie_map const& other, allocator_type const& alloc)
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)),
        roots_(other.roots_),
        size_(other.size_) {}

  trie_map(trie_map&& other) noexcept
      : alloc_(allocator_traits::select_on_container_copy_construction(other.alloc_)),
        roots_(std::move(other.roots_)),
        size_(other.size_) {}

  trie_map(trie_map&& other, allocator_type const& alloc) noexcept
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)),
        roots_(std::move(other.roots_)),
        size_(other.size_) {}

  trie_map(std::initializer_list<value_type> const init,
           allocator_type const& alloc = allocator_type())
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)) {
    insert(init);
  }

  allocator_type get_allocator() const noexcept {
    return allocator_traits::select_on_container_copy_construction(alloc_);
  }

  Value& at(std::string_view const key) {
    auto const it = Node::Find(roots_, key);
    if (it != Node::end()) {
      return it->second;
    } else {
      throw std::out_of_range(std::string(key));
    }
  }

  Value const& at(std::string_view const key) const {
    auto const it = Node::FindConst(roots_, key);
    if (it != Node::cend()) {
      return it->second;
    } else {
      throw std::out_of_range(std::string(key));
    }
  }

  Value& operator[](std::string_view const key) {
    auto const [it, unused_inserted] = Node::Insert(roots_, key);
    return it->second;
  }

  iterator begin() noexcept { return Node::begin(roots_); }
  const_iterator begin() const noexcept { return Node::cbegin(roots_); }
  const_iterator cbegin() const noexcept { return Node::cbegin(roots_); }
  iterator end() noexcept { return Node::end(); }
  const_iterator end() const noexcept { return Node::cend(); }
  const_iterator cend() const noexcept { return Node::cend(); }

  // TODO: reverse iterations

  template <typename H>
  friend H AbslHashValue(H h, trie_map const& set) {
    return H::combine(std::move(h), set.roots_);
  }

  friend bool operator==(trie_map const& lhs, trie_map const& rhs) {
    return lhs.roots_ == rhs.roots_;
  }

  friend bool operator!=(trie_map const& lhs, trie_map const& rhs) {
    return lhs.roots_ != rhs.roots_;
  }

  friend bool operator<(trie_map const& lhs, trie_map const& rhs) {
    return lhs.roots_ < rhs.roots_;
  }

  friend bool operator<=(trie_map const& lhs, trie_map const& rhs) {
    return lhs.roots_ <= rhs.roots_;
  }

  friend bool operator>(trie_map const& lhs, trie_map const& rhs) {
    return lhs.roots_ > rhs.roots_;
  }

  friend bool operator>=(trie_map const& lhs, trie_map const& rhs) {
    return lhs.roots_ >= rhs.roots_;
  }

  [[nodiscard]] bool empty() const noexcept { return root().IsEmpty(); }

  size_type size() const noexcept { return size_; }

  size_type max_size() const noexcept { return root().children_.max_size(); }

  void clear() noexcept {
    root().Clear();
    size_ = 0;
  }

  std::pair<iterator, bool> insert(value_type const& value) {
    auto result = Node::Insert(&roots_, value.first, value.second);
    if (result.second) {
      ++size_;
    }
    return result;
  }

  std::pair<iterator, bool> insert(value_type&& value) {
    auto result = Node::Insert(&roots_, value.first, std::move(value.second));
    if (result.second) {
      ++size_;
    }
    return result;
  }

  template <class InputIt>
  void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  void insert(std::initializer_list<value_type> const init) {
    for (auto const& value : init) {
      insert(value);
    }
  }

  template <typename V>
  std::pair<iterator, bool> insert_or_assign(std::string_view const key, V&& value) {
    auto const [it, inserted] = Node::Insert(&roots_, key, std::move(value));
    if (inserted) {
      ++size_;
    } else {
      it->second = std::move(value);
    }
    return std::make_pair(it, inserted);
  }

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(std::string_view const key, Args&&... args) {
    auto result = Node::Insert(&roots_, key, std::forward<Args>(args)...);
    if (result.second) {
      ++size_;
    }
    return result;
  }

  iterator erase(iterator pos) {
    iterator result = Node::Remove(std::move(pos));
    --size_;
    return result;
  }

  iterator erase(const_iterator pos) {
    iterator result = Node::Remove(std::move(pos));
    --size_;
    return result;
  }

  void erase_fast(iterator const& pos) {
    Node::RemoveFast(&roots_, pos);
    --size_;
  }

  void erase_fast(const_iterator const& pos) {
    Node::RemoveFast(&roots_, pos);
    --size_;
  }

  iterator erase(const_iterator first, const_iterator const& last) {
    if (last.is_end()) {
      while (first != last) {
        first = erase(std::move(first));
      }
    } else {
      // The `erase` calls will invalidate `last` if it's not the end iterator, so we must compare
      // the keys.
      auto const last_key = GetKey(last);
      while (GetKey(first) != last_key) {
        first = erase(std::move(first));
      }
    }
    return first;
  }

  size_type erase(std::string_view const key) {
    if (root().Remove(key)) {
      --size_;
      return 1;
    } else {
      return 0;
    }
  }

  void swap(trie_map const& other) noexcept {
    if (allocator_traits::propagate_on_container_swap::value) {
      std::swap(alloc_, other.alloc_);
    }
    std::swap(roots_, other.roots_);
    std::swap(size_, other.size_);
  }

  // TODO: merge

  size_type count(std::string_view const key) const { return root().Contains(key) ? 1 : 0; }

  iterator find(std::string_view const key) { return Node::Find(&roots_, key); }

  const_iterator find(std::string_view const key) const { return Node::FindConst(&roots_, key); }

  bool contains(std::string_view const key) const { return root().Contains(key); }

  iterator lower_bound(std::string_view const key) { return Node::LowerBound(&roots_, key); }

  const_iterator lower_bound(std::string_view const key) const {
    return Node::LowerBoundConst(&roots_, key);
  }

  iterator upper_bound(std::string_view const key) { return Node::UpperBound(&roots_, key); }

  const_iterator upper_bound(std::string_view const key) const {
    return Node::UpperBoundConst(&roots_, key);
  }

  std::pair<iterator, iterator> equal_range(std::string_view const key) {
    return std::make_pair(lower_bound(key), upper_bound(key));
  }

  std::pair<const_iterator, const_iterator> equal_range(std::string_view const key) const {
    return std::make_pair(lower_bound(key), upper_bound(key));
  }

 private:
  // Returns the root node of the tree (see the comment on `roots_` below).
  Node& root() { return roots_.begin()->second; }
  Node const& root() const { return roots_.begin()->second; }

  // Returns the key of an element referred to by an iterator. Works for both iterators and const
  // iterators.
  //
  // The arrow operator of our trie iterators perform a heap allocation, so for the best performance
  // we need to use the star operator instead. But the latter requires destructuring the returned
  // pair into an intermediate variable, so we need a convenience function like this one.
  static std::string GetKey(typename Node::BaseIterator const& it) {
    auto const [key, unused] = *it;
    return key;
  }

  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS allocator_type alloc_;

  // To facilitate the implementation of the iterator advancement algorithm we maintain a list of
  // roots rather than a single root so that we can always rely on `NodeSet` iterators at every
  // level of recursion, but in reality `roots_` must always contain exactly one element, the real
  // root. The empty string we're using here as a key is irrelevant.
  NodeSet roots_{{"", Node(alloc_)}};

  // Number of elements in the trie.
  size_type size_ = 0;
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_TRIE_MAP_H__
