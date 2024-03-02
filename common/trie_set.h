#ifndef __TSDB2_COMMON_TRIE_SET_H__
#define __TSDB2_COMMON_TRIE_SET_H__

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/strings/match.h"
#include "common/flat_map.h"

namespace tsdb2 {
namespace common {

// trie_set is a set of strings implemented as a compressed trie, aka a radix tree. The provided API
// is similar to std::set<std::string>.
template <typename Allocator = std::allocator<std::string>>
class trie_set {
 private:
  class Node;
  using NodeEntry = std::pair<std::string, Node>;
  using EntryAllocator =
      typename std::allocator_traits<Allocator>::template rebind_alloc<NodeEntry>;
  using NodeSet = std::vector<NodeEntry, EntryAllocator>;

 public:
  using key_type = std::string;
  using value_type = key_type;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using allocator_type = EntryAllocator;
  using allocator_traits = std::allocator_traits<allocator_type>;
  using reference = value_type&;
  using const_reference = value_type const&;
  using pointer = typename std::allocator_traits<allocator_type>::pointer;
  using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;

  // TODO: iterator type aliases

  class node_type {
   public:
    node_type(node_type&&) noexcept = default;
    node_type& operator=(node_type&&) noexcept = default;

    [[nodiscard]] bool empty() const { return !it_; }
    explicit operator bool() const noexcept { return it_; }

    allocator_type get_allocator() const { parent_->get_allocator(); }

    // TODO: value() getter

    void swap(node_type& other) noexcept {
      std::swap(parent_, other.parent_);
      std::swap(it_, other.it_);
    }

    friend void swap(node_type& lhs, node_type& rhs) noexcept { lhs.swap(rhs); }

   private:
    node_type(node_type const&) = delete;
    node_type& operator=(node_type const&) = delete;

    trie_set* parent_;
    typename NodeSet::iterator it_;
  };

  // TODO: insert_return_type

  trie_set() = default;

  explicit trie_set(Allocator const& alloc)
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)) {}

  template <typename InputIt>
  trie_set(InputIt first, InputIt last, Allocator const& alloc = Allocator())
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)) {
    for (auto it = first; it != last; ++it) {
      root_.Insert(*it);
      ++size_;
    }
  }

  trie_set(trie_set const& other)
      : alloc_(allocator_traits::select_on_container_copy_construction(other.alloc_)),
        root_(other.root_),
        size_(other.size_) {}

  trie_set(trie_set const& other, Allocator const& alloc)
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)),
        root_(other.root_),
        size_(other.size_) {}

  trie_set(trie_set&& other) noexcept
      : alloc_(allocator_traits::select_on_container_copy_construction(other.alloc_)),
        root_(std::move(other.root_)),
        size_(other.size_) {}

  trie_set(trie_set&& other, Allocator const& alloc) noexcept
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)),
        root_(std::move(other.root_)),
        size_(other.size_) {}

  trie_set(std::initializer_list<std::string> const init, Allocator const& alloc = Allocator())
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)) {
    for (auto const& element : init) {
      root_.Insert(element);
    }
    size_ = init.size();
  }

  trie_set& operator=(trie_set const& other) {
    alloc_ = allocator_traits::select_on_container_copy_construction(other.alloc_);
    root_ = other.root_;
    size_ = other.size_;
    return *this;
  }

  trie_set& operator=(trie_set&& other) noexcept {
    alloc_ = std::move(other.alloc_);
    root_ = std::move(other.root_);
    size_ = other.size_;
    return *this;
  }

  trie_set& operator=(std::initializer_list<std::string> const init) {
    for (auto const& element : init) {
      root_.Insert(element);
    }
    size_ = init.size();
    return *this;
  }

  allocator_type get_allocator() const noexcept {
    return allocator_traits::select_on_container_copy_construction(alloc_);
  }

  // TODO: iterations

  [[nodiscard]] bool empty() const noexcept { return !size_; }

  size_type size() const noexcept { return size_; }

  size_type max_size() const noexcept { return allocator_traits::max_size(alloc_); }

  void clear() noexcept {
    root_.Clear();
    size_ = 0;
  }

  // TODO

  bool contains(std::string_view const value) const { return root_.Contains(value); }

  // TODO

 private:
  class Node {
   public:
    explicit Node(bool const leaf) : leaf_(leaf) {}

    Node(Node const& other) { *this = other; }

    Node& operator=(Node const& other) {
      children_.clear();
      children_.reserve(other.children_.size());
      for (auto const& other_child : other.children_) {
        children_.emplace(other_child);
      }
      leaf_ = other.leaf_;
      return *this;
    }

    Node(Node&& other) noexcept { *this = std::move(other); }

    Node& operator=(Node&& other) noexcept {
      children_ = std::move(other.children_);
      leaf_ = other.leaf_;
      return *this;
    }

    void Clear() {
      children_.clear();
      leaf_ = false;
    }

    bool Contains(std::string_view const value) const {
      if (value.empty()) {
        return leaf_;
      }
      auto const it = children_.lower_bound(value.substr(0, 1));
      if (it == children_.end()) {
        return false;
      }
      auto const& [key, node] = *it;
      if (absl::StartsWith(value, key)) {
        return node.Contains(value.substr(key.size()));
      } else {
        return false;
      }
    }

    bool Insert(std::string_view const value) {
      if (value.empty()) {
        bool const result = !leaf_;
        leaf_ = true;
        return result;
      }
      auto const it = children_.lower_bound(value.substr(0, 1));
      if (it == children_.end()) {
        children_.try_emplace(std::string(value), /*leaf=*/true);
        return true;
      }
      auto& [key, node] = *it;
      size_t i = 0;
      // Find the longest common prefix.
      for (; i < value.size() && i < key.size() && value[i] == key[i]; ++i) {
        ;
      }
      if (i == 0) {
        children_.try_emplace(std::string(value), /*leaf=*/true);
        return true;
      }
      // `i` is now the length of the longest common prefix.
      if (i < key.size()) {
        Node child = std::move(node);
        node = Node(/*leaf=*/false);
        node.children_.try_emplace(key.substr(i), std::move(child));
        key = key.substr(0, i);
      }
      return node.Insert(value.substr(i));
    }

    // TODO

   private:
    friend class node_type;

    bool leaf_;
    flat_map<std::string, Node, std::less<void>, NodeSet> children_;
  };

  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS EntryAllocator alloc_;

  Node root_{/*leaf=*/false};
  size_type size_ = 0;
};

}  // namespace common
}  // namespace tsdb2

#endif  //
