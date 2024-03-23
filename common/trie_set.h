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
#include "absl/strings/strip.h"
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
//  * The worst-case space complexity of our iterators is linear in the length of the stored string.
//    trie_set iterators are cheap to move but relatively expensive to copy.
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
  using pointer = typename std::allocator_traits<Allocator>::pointer;
  using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
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
    insert(first, last);
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
    insert(init);
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
    insert(init);
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

  template <typename H>
  friend H AbslHashValue(H h, trie_set const& set) {
    return H::combine(std::move(h), set.roots_);
  }

  [[nodiscard]] bool empty() const noexcept { return root().IsEmpty(); }

  size_type size() const noexcept { return size_; }

  size_type max_size() const noexcept { return root().children_.max_size(); }

  void clear() noexcept {
    root().Clear();
    size_ = 0;
  }

  std::pair<iterator, bool> insert(std::string_view const value) {
    auto [it, inserted] = Node::Insert(&roots_, value);
    if (inserted) {
      ++size_;
    }
    return std::make_pair(std::move(it), inserted);
  }

  template <class InputIt>
  void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  void insert(std::initializer_list<std::string> const init) {
    for (auto const& value : init) {
      insert(value);
    }
  }

  iterator erase(const_iterator pos) {
    auto it = Node::Remove(&roots_, std::move(pos));
    --size_;
    return it;
  }

  iterator erase(const_iterator& first, const_iterator const& last) {
    while (first != last) {
      first = erase(std::move(first));
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

  // TODO

  size_type count(std::string_view const key) const { return root().Contains(key) ? 1 : 0; }

  iterator find(std::string_view const key) { return Node::Find(roots_, key); }

  const_iterator find(std::string_view const key) const { return Node::Find(roots_, key); }

  bool contains(std::string_view const key) const { return root().Contains(key); }

  // TODO

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
    explicit StateFrame(NodeSet& nodes) : pos(nodes.begin()), end(nodes.end()) {}
    explicit StateFrame(NodeSet const& nodes) : StateFrame(const_cast<NodeSet&>(nodes)) {}

    explicit StateFrame(typename NodeSet::iterator pos_it, typename NodeSet::iterator end_it)
        : pos(std::move(pos_it)), end(std::move(end_it)) {}

    StateFrame(StateFrame const&) = default;
    StateFrame& operator=(StateFrame const&) = default;
    StateFrame(StateFrame&&) noexcept = default;
    StateFrame& operator=(StateFrame&&) noexcept = default;

    friend bool operator==(StateFrame const& lhs, StateFrame const& rhs) {
      if (lhs.pos != lhs.end) {
        return rhs.pos != rhs.end && lhs.pos->first == rhs.pos->first;
      } else {
        return rhs.pos == rhs.end;
      }
    }

    friend bool operator!=(StateFrame const& lhs, StateFrame const& rhs) { return !(lhs == rhs); }

    typename NodeSet::iterator pos;
    typename NodeSet::iterator end;
  };

  // A trie node. This is the core implementation of the trie.
  //
  // NOTE: some of the algorithms implemented here are static and require the caller to provide the
  // root `NodeSet` (which is the `roots_` field of `trie_set`). That is because they involve
  // iterators, so they need the root node set to create the first state frame of the iterator.
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

    template <typename H>
    friend H AbslHashValue(H h, Node const& node) {
      return H::combine(std::move(h), node.leaf_, node.children_);
    }

    // Indicates whether the trie rooted at this node is empty.
    bool IsEmpty() const { return !leaf_ && children_.empty(); }

    // Deletes all elements from the trie rooted at this node.
    void Clear();

    // Finds the element with the specified `value` and returns an iterator to it, or returns the
    // end iterator if the element is not found.
    static Iterator Find(NodeSet const& roots, std::string_view value);

    // Determines whether the trie rooted at this node contains the specified string.
    bool Contains(std::string_view value) const;

    // Inserts the specified `value` if it's not already present. The first element of the returned
    // pair is an iterator to either the newly added node or to a preexisting one with the same
    // `value`, while the second element is a boolean indicating whether insertion happened.
    static std::pair<Iterator, bool> Insert(NodeSet* roots, std::string_view value);

    // Removes the specified `value` from the trie rooted at this node.
    bool Remove(std::string_view value);

    // Removes the value referred to by the specified iterator and returns an iterator to the next
    // element.
    static Iterator Remove(NodeSet* roots, Iterator it);

   private:
    friend class Iterator;

    std::pair<Iterator, bool> InsertChild(std::vector<StateFrame> frames, std::string_view value);

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

    // Advances the iterator to the next node. The next node is found by attempting the following,
    // in order:
    //
    //  1. descend to the leftmost child;
    //  2. if the current node has no children, advance to the next peer;
    //  3. if the current node has no peers, backtrack to the parent (by removing the last frame)
    //     and repeat #2.
    //
    // NOTE: this algorithm won't necessarily find the next *leaf* node, it will simply advance to
    // the next one. The caller needs to repeat until either a leaf node is found or there are no
    // more nodes. That is achieved by the `Advance` method.
    void NextNode();

    // Advances the iterator to the next node repeatedly until either the next leaf node is found or
    // there are no more nodes. In the latter case the iterator becomes the end iterator of the
    // trie.
    //
    // This is the main iterator advancement algorithm invoked by `operator++`.
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
typename trie_set<Allocator>::Iterator trie_set<Allocator>::Node::Find(NodeSet const& roots,
                                                                       std::string_view value) {
  std::vector<StateFrame> frames{StateFrame(roots)};
  while (!value.empty()) {
    auto& node = frames.back().pos->second;
    auto const it = node.children_.lower_bound(value.substr(0, 1));
    if (it == node.children_.end()) {
      return Iterator();
    }
    if (!absl::ConsumePrefix(&value, it->first)) {
      return Iterator();
    }
    frames.emplace_back(it, node.children_.end());
  }
  auto const& node = frames.back().pos->second;
  if (node.leaf_) {
    return Iterator(std::move(frames));
  } else {
    return Iterator();
  }
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
std::pair<typename trie_set<Allocator>::Iterator, bool> trie_set<Allocator>::Node::InsertChild(
    std::vector<StateFrame> frames, std::string_view const value) {
  auto const [it, unused_inserted] =
      children_.try_emplace(std::string(value), /*leaf=*/true, children_.get_allocator());
  frames.emplace_back(it, children_.end());
  return std::make_pair(Iterator(std::move(frames)), true);
}

template <typename Allocator>
std::pair<typename trie_set<Allocator>::Iterator, bool> trie_set<Allocator>::Node::Insert(
    NodeSet* const roots, std::string_view value) {
  std::vector<StateFrame> frames{StateFrame(*roots)};
  while (!value.empty()) {
    auto& node = frames.back().pos->second;
    auto const it = node.children_.lower_bound(value.substr(0, 1));
    if (it == node.children_.end()) {
      return node.InsertChild(std::move(frames), value);
    }
    auto& [key, child] = *it;
    size_t i = 0;
    // Find the longest common prefix.
    while (i < value.size() && i < key.size() && value[i] == key[i]) {
      ++i;
    }
    // `i` is now the length of the longest common prefix.
    if (i == 0) {
      return node.InsertChild(std::move(frames), value);
    }
    frames.emplace_back(it, node.children_.end());
    if (i < key.size()) {
      Node temp = std::move(child);
      child = Node(/*leaf=*/false, node.children_.get_allocator());
      child.children_.try_emplace(key.substr(i), std::move(temp));
      key.resize(i);
    }
    value.remove_prefix(i);
  }
  auto& node = frames.back().pos->second;
  bool const inserted = !node.leaf_;
  node.leaf_ = true;
  return std::make_pair(Iterator(std::move(frames)), inserted);
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
typename trie_set<Allocator>::Iterator trie_set<Allocator>::Node::Remove(NodeSet* const roots,
                                                                         Iterator it) {
  auto& frames = it.frames_;
  NodeSet* nodes = roots;
  if (frames.size() > 1) {
    nodes = &(frames[frames.size() - 2].pos->second.children_);
  }
  auto& last_frame = frames.back();
  auto& node = last_frame.pos->second;
  if (node.children_.size() > 1) {
    node.leaf_ = false;
    it.Advance();
  } else if (node.children_.size() > 0) {
    auto& child = node.children_.begin()->second;
    node = std::move(child);
    it.Advance();
  } else {
    auto pos = nodes->erase(last_frame.pos);
    if (pos != nodes->end()) {
      last_frame.pos = std::move(pos);
      return it;
    }
    frames.pop_back();
    while (!frames.empty()) {
      auto& frame = frames.back();
      if (++frame.pos != frame.end) {
        return it;
      } else {
        frames.pop_back();
      }
    }
  }
  return it;
}

template <typename Allocator>
trie_set<Allocator>::Iterator::Iterator(NodeSet const& roots) {
  frames_.emplace_back(roots);
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
  if (!node.children_.empty()) {
    frames_.emplace_back(node.children_);
    return;
  }
  do {
    auto& frame = frames_.back();
    if (++frame.pos != frame.end) {
      return;
    } else {
      frames_.pop_back();
    }
  } while (!frames_.empty());
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
