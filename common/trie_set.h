#ifndef __TSDB2_COMMON_TRIE_SET_H__
#define __TSDB2_COMMON_TRIE_SET_H__

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "common/flat_map.h"

namespace tsdb2 {
namespace common {

// trie_set is a set of strings implemented as a compressed trie, aka a radix tree. The provided API
// is similar to `std::set<std::string>`.
//
// Notable differences between std::set<std::string> and trie_set:
//
//  * Inserting nodes from node handles is not supported. That is because, by definition, a trie
//    node doesn't have all the information about its key, so it wouldn't make sense to reinsert it
//    under a completely different key prefix.
//  * For the same reasons above, retrieving the full key of a node is not supported by `node_type`.
//    Once extracted, a trie node can only be destroyed.
//  * TODO
//
template <typename Allocator = std::allocator<std::string>>
class trie_set {
 private:
  class Node;
  class Iterator;

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
  using iterator = Iterator;
  using const_iterator = Iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  class node_type {
   public:
    explicit node_type() = default;

    node_type(node_type&&) noexcept = default;
    node_type& operator=(node_type&&) noexcept = default;

    [[nodiscard]] bool empty() const { return node_.empty(); }
    explicit operator bool() const noexcept { return node_.operator bool(); }

    allocator_type get_allocator() const noexcept {
      return allocator_traits::select_on_container_copy_construction(node_.get_allocator());
    }

    void swap(node_type& other) noexcept { node_.swap(other.node_); }
    friend void swap(node_type& lhs, node_type& rhs) noexcept { lhs.swap(rhs); }

   private:
    node_type(node_type const&) = delete;
    node_type& operator=(node_type const&) = delete;

    typename NodeSet::node_type node_;
  };

  trie_set() = default;

  explicit trie_set(allocator_type const& alloc)
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)) {}

  template <typename InputIt>
  trie_set(InputIt first, InputIt last, allocator_type const& alloc = allocator_type())
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)) {
    auto& root_node = root();
    for (auto it = first; it != last; ++it) {
      root_node.Insert(*it);
      ++size_;
    }
  }

  trie_set(trie_set const& other)
      : alloc_(allocator_traits::select_on_container_copy_construction(other.alloc_)),
        roots_(other.roots_),
        size_(other.size_) {}

  trie_set(trie_set const& other, allocator_type const& alloc)
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)),
        roots_(other.roots_),
        size_(other.size_) {}

  trie_set(trie_set&& other) noexcept
      : alloc_(allocator_traits::select_on_container_copy_construction(other.alloc_)),
        roots_(std::move(other.roots_)),
        size_(other.size_) {}

  trie_set(trie_set&& other, allocator_type const& alloc) noexcept
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)),
        roots_(std::move(other.roots_)),
        size_(other.size_) {}

  trie_set(std::initializer_list<std::string> const init,
           allocator_type const& alloc = allocator_type())
      : alloc_(allocator_traits::select_on_container_copy_construction(alloc)) {
    auto& root_node = root();
    for (auto const& element : init) {
      root_node.Insert(element);
    }
    size_ = init.size();
  }

  trie_set& operator=(trie_set const& other) {
    alloc_ = allocator_traits::select_on_container_copy_construction(other.alloc_);
    roots_ = other.roots_;
    size_ = other.size_;
    return *this;
  }

  trie_set& operator=(trie_set&& other) noexcept {
    alloc_ = std::move(other.alloc_);
    roots_ = std::move(other.roots_);
    size_ = other.size_;
    return *this;
  }

  trie_set& operator=(std::initializer_list<std::string> const init) {
    auto& root_node = root();
    for (auto const& element : init) {
      root_node.Insert(element);
    }
    size_ = init.size();
    return *this;
  }

  allocator_type get_allocator() const noexcept {
    return allocator_traits::select_on_container_copy_construction(alloc_);
  }

  iterator begin() noexcept { return Iterator(roots_); }
  const_iterator begin() const noexcept { return Iterator(roots_); }
  const_iterator cbegin() const noexcept { return Iterator(roots_); }
  iterator end() noexcept { return Iterator(); }
  const_iterator end() const noexcept { return Iterator(); }
  const_iterator cend() const noexcept { return Iterator(); }

  // TODO: reverse iterations

  [[nodiscard]] bool empty() const noexcept { return root().IsEmpty(); }

  size_type size() const noexcept { return size_; }

  size_type max_size() const noexcept { return root().children_.max_size(); }

  void clear() noexcept {
    root().Clear();
    size_ = 0;
  }

  // TODO

  bool contains(std::string_view const key) const { return root().Contains(key); }

  // TODO

  size_type erase(std::string_view const key) { return root().Remove(key) ? 1 : 0; }

 private:
  // `StateFrame` is used by iterators to turn recursive algorithms into iterative ones. For
  // example, rather than providing a recursive algorithm scanning all the nodes of the trie, our
  // API must provide iterators with `operator++` allowing the user to iteratively step to the next
  // node.
  //
  // These algorithms would normally require recursive calls keeping their data in the system call
  // stack, but in iterative form the stack has to be managed manually as a vector of state frames.
  //
  // Each frame contains two `NodeSet` iterators. The first iterator points to the current element
  // the algorithm is working on, while the second one is the end iterator and allows an iterative
  // call to determine whether the "recursive call" corresponding to that stack frame has completed.
  //
  // Those two iterators correspond to the local variables we'd have in a recursive algorithm. See
  // the `pos` and `end` variables in the following pseudo-code:
  //
  //   void trie_set::Node::Scan() {
  //     auto end = children_.end();
  //     for (auto pos = children_.begin(); pos != end; ++pos) {
  //       DoSomething(*pos);
  //       pos->Scan();
  //     }
  //   }
  //
  struct StateFrame {
    friend bool operator==(StateFrame const& lhs, StateFrame const& rhs) {
      if (lhs.pos != lhs.end) {
        return rhs.pos != rhs.end && lhs.pos->first == rhs.pos->first;
      } else {
        return rhs.pos == rhs.end;
      }
    }

    friend bool operator!=(StateFrame const& lhs, StateFrame const& rhs) { return !(lhs == rhs); }

    typename NodeSet::const_iterator pos;
    typename NodeSet::const_iterator end;
  };

  // A trie node. This is the core implementation of the trie.
  class Node {
   public:
    explicit Node(bool const leaf, allocator_type const& alloc) : leaf_(leaf), children_(alloc) {}

    Node(Node const& other) = default;
    Node& operator=(Node const& other) = default;
    Node(Node&& other) noexcept = default;
    Node& operator=(Node&& other) noexcept = default;

    allocator_type get_allocator() const noexcept {
      return allocator_traits::select_on_container_copy_construction(children_.get_allocator());
    }

    bool IsEmpty() const { return !leaf_ && children_.empty(); }

    // Deletes all elements from the trie rooted at this node.
    void Clear();

    // Determines whether the trie rooted at this node contains the specified string.
    bool Contains(std::string_view value) const;

    // Inserts the specified `value` in the trie rooted at this node if the value is not already
    // present. The first element of the returned pair is an iterator to either the newly added node
    // or to a preexisting one with the same `value`, while the second element is a boolean
    // indicating whether insertion happened.
    bool Insert(std::string_view value);

    // Removes the specified `value` from the trie rooted at this node.
    bool Remove(std::string_view value);

   private:
    friend class Iterator;

    bool leaf_;
    NodeSet children_;
  };

  // Iterator implementation satisfying the requirements of LegacyBidirectionalIterator.
  class Iterator {
   public:
    Iterator(Iterator const&) = default;
    Iterator& operator=(Iterator const&) = default;
    Iterator(Iterator&&) noexcept = default;
    Iterator& operator=(Iterator&&) noexcept = default;

    friend bool operator==(Iterator const& lhs, Iterator const& rhs) {
      return lhs.frames_ == rhs.frames_;
    }

    friend bool operator!=(Iterator const& lhs, Iterator const& rhs) {
      return lhs.frames_ != rhs.frames_;
    }

    std::string operator*() const;

    Iterator& operator++() {
      Advance();
      return *this;
    }

    Iterator operator++(int) {
      Iterator result = *this;
      Advance();
      return result;
    }

    // TODO: operator--

    void swap(Iterator& other) { frames_.swap(other.frames_); }
    friend void swap(Iterator& lhs, Iterator& rhs) { lhs.swap(rhs); }

   private:
    friend class trie_set;
    friend class Node;

    // Constructs the "begin" iterator.
    // REQUIRES: `roots` must not be empty.
    explicit Iterator(NodeSet const& roots);

    // Constructs the "end" iterator.
    explicit Iterator() = default;

    // Constructs an iterator with the given state frames.
    explicit Iterator(std::vector<StateFrame> frames) : frames_(std::move(frames)) {}

    void NextNode();
    void Advance();

    std::vector<StateFrame> frames_;
  };

  // Returns the root node of the tree (see the comment on `roots_` below).
  Node& root() { return roots_.begin()->second; }
  Node const& root() const { return roots_.begin()->second; }

  ABSL_ATTRIBUTE_NO_UNIQUE_ADDRESS EntryAllocator alloc_;

  // To facilitate the implementation of the iterator advancement algorithm we maintain a list of
  // roots rather than a single root so that we can always rely on `NodeSet` iterators at every
  // level of recursion, but in reality `roots_` must always contain exactly one element, the real
  // root. The empty string we're using here as a key is irrelevant.
  NodeSet roots_{{"", Node(/*leaf=*/false, alloc_)}};

  // Number of strings in the trie.
  size_type size_ = 0;
};

template <typename Allocator>
void trie_set<Allocator>::Node::Clear() {
  leaf_ = false;
  children_.clear();
}

template <typename Allocator>
bool trie_set<Allocator>::Node::Contains(std::string_view const value) const {
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

template <typename Allocator>
bool trie_set<Allocator>::Node::Insert(std::string_view const value) {
  if (value.empty()) {
    bool const result = !leaf_;
    leaf_ = true;
    return result;
  }
  auto const it = children_.lower_bound(value.substr(0, 1));
  if (it == children_.end()) {
    children_.try_emplace(std::string(value), /*leaf=*/true, children_.get_allocator());
    return true;
  }
  auto& [key, node] = *it;
  size_t i = 0;
  // Find the longest common prefix.
  while (i < value.size() && i < key.size() && value[i] == key[i]) {
    ++i;
  }
  // `i` is now the length of the longest common prefix.
  if (i == 0) {
    children_.try_emplace(std::string(value), /*leaf=*/true, children_.get_allocator());
    return true;
  }
  if (i < key.size()) {
    Node child = std::move(node);
    node = Node(/*leaf=*/false, children_.get_allocator());
    node.children_.try_emplace(key.substr(i), std::move(child));
    key = key.substr(0, i);
  }
  return node.Insert(value.substr(i));
}

template <typename Allocator>
bool trie_set<Allocator>::Node::Remove(std::string_view const value) {
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
  while (i < value.size() && i < key.size() && value[i] == key[i]) {
    ++i;
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

template <typename Allocator>
trie_set<Allocator>::Iterator::Iterator(NodeSet const& roots) {
  frames_.push_back({
      .pos = roots.begin(),
      .end = roots.end(),
  });
  auto const& root = roots.begin()->second;
  if (!root.leaf_) {
    Advance();
  }
}

template <typename Allocator>
std::string trie_set<Allocator>::Iterator::operator*() const {
  size_t size = 0;
  for (auto const& frame : frames_) {
    size += frame.pos->first.size();
  }
  std::string key;
  key.reserve(size);
  for (auto const& frame : frames_) {
    key += frame.pos->first;
  }
  return key;
}

template <typename Allocator>
void trie_set<Allocator>::Iterator::NextNode() {
  auto const& frame = frames_.back();
  auto const& node = frame.pos->second;
  auto begin = node.children_.begin();
  auto end = node.children_.end();
  if (begin != end) {
    frames_.push_back({
        .pos = std::move(begin),
        .end = std::move(end),
    });
  } else {
    do {
      auto& frame = frames_.back();
      if (++frame.pos != frame.end) {
        return;
      } else {
        frames_.pop_back();
      }
    } while (!frames_.empty());
  }
}

template <typename Allocator>
void trie_set<Allocator>::Iterator::Advance() {
  while (NextNode(), !frames_.empty()) {
    auto const& frame = frames_.back();
    auto const& node = frame.pos->second;
    if (node.leaf_) {
      return;
    }
  }
}

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_TRIE_SET_H__
