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
  using NodeArray = std::vector<NodeEntry, EntryAllocator>;
  using NodeSet = flat_map<std::string, Node, std::less<void>, NodeArray>;

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

    [[nodiscard]] bool empty() const { return node_.empty(); }
    explicit operator bool() const noexcept { return node_.operator bool(); }

    // TODO: value() getter

    void swap(node_type& other) noexcept { node_.swap(other.node_); }
    friend void swap(node_type& lhs, node_type& rhs) noexcept { lhs.swap(rhs); }

   private:
    node_type(node_type const&) = delete;
    node_type& operator=(node_type const&) = delete;

    typename NodeSet::node_type node_;
  };

  // TODO: insert_return_type

  trie_set() = default;

  template <typename InputIt>
  trie_set(InputIt first, InputIt last) {
    for (auto it = first; it != last; ++it) {
      root_.Insert(*it);
      ++size_;
    }
  }

  trie_set(trie_set const& other) = default;
  trie_set(trie_set&& other) noexcept = default;

  trie_set(std::initializer_list<std::string> const init) {
    for (auto const& element : init) {
      root_.Insert(element);
    }
    size_ = init.size();
  }

  trie_set& operator=(trie_set const& other) = default;
  trie_set& operator=(trie_set&& other) noexcept = default;

  trie_set& operator=(std::initializer_list<std::string> const init) {
    for (auto const& element : init) {
      root_.Insert(element);
    }
    size_ = init.size();
    return *this;
  }

  // TODO: iterations

  [[nodiscard]] bool empty() const noexcept { return root_.IsEmpty(); }

  size_type size() const noexcept { return size_; }

  size_type max_size() const noexcept { return root_.children_.max_size(); }

  void clear() noexcept {
    root_.Clear();
    size_ = 0;
  }

  // TODO

  bool contains(std::string_view const key) const { return root_.Contains(key); }

  // TODO

  size_type erase(std::string_view const key) {
    if (root_.Remove(key)) {
      return 1;
    } else {
      return 0;
    }
  }

 private:
  class Node {
   public:
    explicit Node(bool const leaf) : leaf_(leaf) {}

    Node(Node const& other) = default;
    Node& operator=(Node const& other) = default;
    Node(Node&& other) noexcept = default;
    Node& operator=(Node&& other) noexcept = default;

    bool IsEmpty() const { return !leaf_ && children_.empty(); }

    void Clear() {
      leaf_ = false;
      children_.clear();
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
      // `i` is now the length of the longest common prefix.
      if (i == 0) {
        children_.try_emplace(std::string(value), /*leaf=*/true);
        return true;
      }
      if (i < key.size()) {
        Node child = std::move(node);
        node = Node(/*leaf=*/false);
        node.children_.try_emplace(key.substr(i), std::move(child));
        key = key.substr(0, i);
      }
      return node.Insert(value.substr(i));
    }

    bool Remove(std::string_view const value) {
      if (value.empty()) {
        bool const result = leaf_;
        leaf_ = false;
        return result;
      }
      auto const it = children_.lower_bound(value.substr(0, 1));
      if (it == children_.end()) {
        return false;
      }
      auto& [key, node] = *it;
      size_t i = 0;
      // Find the longest common prefix.
      for (; i < value.size() && i < key.size() && value[i] == key[i]; ++i) {
        ;
      }
      // `i` is now the length of the longest common prefix.
      if (i < key.size()) {
        return false;
      }
      bool const result = node.Remove(value.substr(i));
      if (node.IsEmpty()) {
        children_.erase(it);
      }
      return result;
    }

    // TODO

   private:
    friend class node_type;

    bool leaf_;
    NodeSet children_;
  };

  Node root_{/*leaf=*/false};
  size_type size_ = 0;
};

}  // namespace common
}  // namespace tsdb2

#endif  //
