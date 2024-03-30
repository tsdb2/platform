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
//  * Node handles are not supported. That is because, by definition, a trie node doesn't have all
//    the information about its key, so most of the node API wouldn't make sense.
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
    clear();
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

  friend bool operator==(trie_set const& lhs, trie_set const& rhs) {
    return lhs.size_ == rhs.size_ && lhs.roots_ == rhs.roots_;
  }

  friend bool operator!=(trie_set const& lhs, trie_set const& rhs) { return !operator==(lhs, rhs); }

  // TODO: other comparison operators.

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
    if (last.is_end()) {
      while (first != last) {
        first = erase(std::move(first));
      }
    } else {
      // the `erase` calls will invalidate `last` if it's not the end iterator, so we must compare
      // the keys.
      auto const last_key = *last;
      while (*first != last_key) {
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

  void swap(trie_set<Allocator>& other) {
    if (std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
      std::swap(alloc_, other.alloc_);
    }
    std::swap(roots_, other.roots_);
    std::swap(size_, other.size_);
  }

  friend void swap(trie_set<Allocator>& lhs, trie_set<Allocator>& rhs) { lhs.swap(rhs); }

  size_type count(std::string_view const key) const { return root().Contains(key) ? 1 : 0; }

  iterator find(std::string_view const key) { return Node::Find(roots_, key); }

  const_iterator find(std::string_view const key) const { return Node::Find(roots_, key); }

  bool contains(std::string_view const key) const { return root().Contains(key); }

  iterator lower_bound(std::string_view const key) { return Node::LowerBound(roots_, key); }

  const_iterator lower_bound(std::string_view const key) const {
    return Node::LowerBound(roots_, key);
  }

  iterator upper_bound(std::string_view const key) { return Node::UpperBound(roots_, key); }

  const_iterator upper_bound(std::string_view const key) const {
    return Node::UpperBound(roots_, key);
  }

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

    friend bool operator==(Node const& lhs, Node const& rhs) { return lhs.Tie() == rhs.Tie(); }
    friend bool operator!=(Node const& lhs, Node const& rhs) { return lhs.Tie() != rhs.Tie(); }

    // Indicates whether the trie rooted at this node is empty.
    bool IsEmpty() const { return !leaf_ && children_.empty(); }

    // Deletes all elements from the trie rooted at this node.
    void Clear();

    // Finds the element with the specified `value` and returns an iterator to it, or returns the
    // end iterator if the element is not found.
    static Iterator Find(NodeSet const& roots, std::string_view value);

    // Determines whether the trie rooted at this node contains the specified string.
    bool Contains(std::string_view value) const;

    // Finds the first element that is greater than or equal to `value`. Returns the end iterator if
    // no such element exists.
    static Iterator LowerBound(NodeSet const& roots, std::string_view value);

    // Finds the first element that is strictly greater than `value`. Returns the end iterator if no
    // such element exists.
    static Iterator UpperBound(NodeSet const& roots, std::string_view value);

    // Inserts the specified `value` if it's not already present. The first element of the returned
    // pair is an iterator to either the newly added node or to a preexisting one with the same
    // `value`, while the second element is a boolean indicating whether insertion happened.
    static std::pair<Iterator, bool> Insert(NodeSet* roots, std::string_view value);

    // Removes the specified `value` from the trie rooted at this node.
    bool Remove(std::string_view value);

    // Removes the value referred to by the specified iterator and returns an iterator to the next
    // element. All other iterators of this container are invalidated.
    //
    // WARNING: the iterator MUST be valid and dereferenceable, otherwise the behavior is undefined.
    // This is consistent with STL containers.
    static Iterator Remove(NodeSet* roots, Iterator it);

   private:
    friend class Iterator;

    auto Tie() const { return std::tie(leaf_, children_); }

    typename NodeSet::iterator LowerBound(std::string_view needle);

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

    std::unique_ptr<std::string> operator->() const {
      return std::make_unique<std::string>(operator*());
    }

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

    // Returns true iff this is the end iterator.
    bool is_end() const { return frames_.empty(); }

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
    auto const end = node.children_.end();
    auto const it = node.children_.lower_bound(value.substr(0, 1));
    if (it == end || !absl::ConsumePrefix(&value, it->first)) {
      return Iterator();
    }
    frames.emplace_back(it, end);
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
typename trie_set<Allocator>::NodeSet::iterator trie_set<Allocator>::Node::LowerBound(
    std::string_view const needle) {
  auto const end = children_.end();
  auto it = children_.lower_bound(needle.substr(0, 1));
  if (it == end) {
    return it;
  }
  auto const key = it->first;
  if (key[0] > needle[0]) {
    return it;
  }
  size_t i = 1;
  while (i < needle.size() && i < key.size()) {
    if (needle[i] != key[i]) {
      return ++it;
    } else {
      ++i;
    }
  }
  return it;
}

template <typename Allocator>
typename trie_set<Allocator>::Iterator trie_set<Allocator>::Node::LowerBound(
    NodeSet const& roots, std::string_view value) {
  std::vector<StateFrame> frames{StateFrame(roots)};
  while (!value.empty()) {
    auto& node = frames.back().pos->second;
    auto const it = node.LowerBound(value);
    auto const end = node.children_.end();
    frames.emplace_back(it, end);
    if (it == end) {
      break;
    }
    auto const key = it->first;
    if (value.size() < key.size() || !absl::ConsumePrefix(&value, key)) {
      break;
    }
  }
  auto const& frame = frames.back();
  bool const leaf = frame.pos != frame.end && frame.pos->second.leaf_;
  Iterator result{std::move(frames)};
  if (!leaf) {
    result.Advance();
  }
  return result;
}

template <typename Allocator>
typename trie_set<Allocator>::Iterator trie_set<Allocator>::Node::UpperBound(
    NodeSet const& roots, std::string_view value) {
  std::vector<StateFrame> frames{StateFrame(roots)};
  while (!value.empty()) {
    auto& node = frames.back().pos->second;
    auto const it = node.LowerBound(value);
    auto const end = node.children_.end();
    frames.emplace_back(it, end);
    if (it == end) {
      break;
    }
    auto const key = it->first;
    if (value.size() < key.size() || !absl::ConsumePrefix(&value, key)) {
      auto const& node = it->second;
      Iterator result{std::move(frames)};
      if (!node.leaf_) {
        result.Advance();
      }
      return result;
    }
  }
  Iterator result{std::move(frames)};
  result.Advance();
  return result;
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
  } else if (node.children_.size() > 0) {
    auto child_node = node.children_.extract(node.children_.begin());
    last_frame.pos->first += child_node.key();
    node = std::move(child_node).mapped();
  } else {
    last_frame.pos = nodes->erase(last_frame.pos);
    last_frame.end = nodes->end();
    if (last_frame.pos != last_frame.end && last_frame.pos->second.leaf_) {
      return it;
    }
  }
  it.Advance();
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
  if (frame.pos != frame.end) {
    auto const& node = frame.pos->second;
    if (!node.children_.empty()) {
      frames_.emplace_back(node.children_);
      return;
    }
  } else {
    frames_.pop_back();
  }
  while (!frames_.empty()) {
    auto& frame = frames_.back();
    if (++frame.pos != frame.end) {
      return;
    } else {
      frames_.pop_back();
    }
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
