#ifndef __TSDB2_COMMON_TRIE_MAP_H__
#define __TSDB2_COMMON_TRIE_MAP_H__

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
    return lhs.size_ == rhs.size_ && lhs.roots_ == rhs.roots_;
  }

  friend bool operator!=(trie_map const& lhs, trie_map const& rhs) { return !operator==(lhs, rhs); }

  // TODO: other comparison operators.

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

  // TODO

  bool contains(std::string_view const key) const { return root().Contains(key); }

  // TODO

 private:
  // Returns the root node of the tree (see the comment on `roots_` below).
  Node& root() { return roots_.begin()->second; }
  Node const& root() const { return roots_.begin()->second; }

  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS allocator_type alloc_;

  // To facilitate the implementation of the iterator advancement algorithm we maintain a list of
  // roots rather than a single root so that we can always rely on `NodeSet` iterators at every
  // level of recursion, but in reality `roots_` must always contain exactly one element, the real
  // root. The empty string we're using here as a key is irrelevant.
  NodeSet roots_{{"", Node(alloc_)}};

  // Number of strings in the trie.
  size_type size_ = 0;
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_TRIE_MAP_H__
