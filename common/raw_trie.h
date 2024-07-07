#ifndef __TSDB2_COMMON_RAW_TRIE_H__
#define __TSDB2_COMMON_RAW_TRIE_H__

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/strip.h"
#include "common/flat_map.h"
#include "common/re.h"
#include "common/re/dfa.h"
#include "common/re/nfa.h"

namespace tsdb2 {
namespace common {
namespace internal {

template <typename Label>
struct TrieTraits;

template <>
struct TrieTraits<bool> {
  using Mapped = void;

  static bool ConstructLabel(bool const value) { return value; }

  static bool TestLabel(bool const label) { return label; }

  static bool ResetLabel(bool& label) {
    bool const result = label;
    label = false;
    return result;
  }
};

template <typename Value>
struct TrieTraits<std::optional<Value>> {
  using Mapped = Value;

  template <typename... Args>
  static std::optional<Value> ConstructLabel(Args&&... args) {
    return std::optional<Value>(std::in_place, std::forward<Args>(args)...);
  }

  static bool TestLabel(std::optional<Value> const& label) { return label.has_value(); }

  static bool ResetLabel(std::optional<Value>& label) {
    bool const result = label.has_value();
    label.reset();
    return result;
  }
};

// A trie node. This is the core implementation of the tries used by `trie_set` and `trie_map`.
//
// `Label` is the type used to label the nodes, which must be convertible to `bool`. `trie_set`
// uses `bool` itself, while `trie_map` uses `std::optional<Value>` with `Value` being the mapped
// type of the `trie_map`. When converted to `bool`, `true` indicates that the trie node is terminal
// and `false` indicates that it's not.
//
// Both `trie_set` and `trie_map` can be implemented by embedding a `TrieNode::NodeSet` field. This
// node set must contain exactly one element at all times. The key of the element must be the empty
// string, while the value represents the root node of the trie.
//
// NOTE: some of the algorithms implemented here are static and require the caller to supply the
// root `NodeSet` rather than just working on `this`. That is because they involve iterators, so
// they need the begin and end iterators of the root node set to create the first state frame of the
// iterator.
template <typename Label, typename Allocator>
class TrieNode {
 public:
  using NodeEntry = std::pair<std::string, TrieNode>;
  using EntryAllocator =
      typename std::allocator_traits<Allocator>::template rebind_alloc<NodeEntry>;
  using EntryAllocatorTraits = std::allocator_traits<EntryAllocator>;
  using NodeArray = std::vector<NodeEntry, EntryAllocator>;
  using NodeSet = flat_map<std::string, TrieNode, std::less<void>, NodeArray>;

 private:
  using Traits = TrieTraits<Label>;

  // `StateFrame` is used by iterators to turn recursive algorithms into iterative ones. For
  // example, rather than providing a recursive algorithm scanning all the nodes of the trie, the
  // `trie_set` and `trie_map` API must provide iterators with `operator++` allowing the user to
  // iteratively step to the next node.
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
  // The `reverse` flag indicates whether the `StateFrame` embeds reverse iterators or not. Reverse
  // iterators are retrieved by calling `rbegin()` / `rend()` on a node set.
  //
  template <bool reverse>
  struct StateFrame;

  template <>
  struct StateFrame<false> {
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

  template <>
  struct StateFrame<true> {
    explicit StateFrame(NodeSet& nodes) : pos(nodes.rbegin()), end(nodes.rend()) {}
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

    typename NodeSet::reverse_iterator pos;
    typename NodeSet::reverse_iterator end;
  };

  using DirectStateFrame = StateFrame<false>;

 public:
  // Base class for iterators.
  template <bool reverse>
  class BaseIterator {
   public:
    BaseIterator(BaseIterator const&) = default;
    BaseIterator& operator=(BaseIterator const&) = default;
    BaseIterator(BaseIterator&&) noexcept = default;
    BaseIterator& operator=(BaseIterator&&) noexcept = default;

    friend bool operator==(BaseIterator const& lhs, BaseIterator const& rhs) {
      return lhs.frames_ == rhs.frames_;
    }

    friend bool operator!=(BaseIterator const& lhs, BaseIterator const& rhs) {
      return lhs.frames_ != rhs.frames_;
    }

   protected:
    friend class TrieNode;

    // Constructs the "begin" iterator.
    // REQUIRES: `roots` must not be empty.
    explicit BaseIterator(NodeSet const& roots);

    // Constructs the "end" iterator.
    explicit BaseIterator() = default;

    // Constructs an iterator with the given state frames.
    explicit BaseIterator(std::vector<StateFrame<reverse>> frames) : frames_(std::move(frames)) {}

    // Returns true iff this is the end iterator.
    bool is_end() const { return frames_.empty(); }

    std::string GetKey() const;

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

    std::vector<StateFrame<reverse>> frames_;
  };

  using DirectBaseIterator = BaseIterator<false>;

 private:
  // Bidirectional node iterator.
  template <typename Value, bool reverse>
  class GenericIterator : public BaseIterator<reverse> {
   public:
    GenericIterator(GenericIterator const&) = default;
    GenericIterator& operator=(GenericIterator const&) = default;
    GenericIterator(GenericIterator&&) noexcept = default;
    GenericIterator& operator=(GenericIterator&&) noexcept = default;

    template <typename Alias = Value, std::enable_if_t<std::is_void_v<Alias>, bool> = true>
    std::string operator*() const {
      return this->GetKey();
    }

    template <typename Alias = Value, std::enable_if_t<std::is_void_v<Alias>, bool> = true>
    std::unique_ptr<std::string const> operator->() const {
      return std::make_unique<std::string const>(this->GetKey());
    }

    template <typename Alias = Value, std::enable_if_t<!std::is_void_v<Alias>, bool> = true>
    std::pair<std::string const, Alias&> operator*() const {
      auto& node = this->frames_.back().pos->second;
      return {this->GetKey(), node.label_.value()};
    }

    template <typename Alias = Value, std::enable_if_t<!std::is_void_v<Alias>, bool> = true>
    std::unique_ptr<std::pair<std::string const, Alias&>> operator->() const {
      auto& node = this->frames_.back().pos->second;
      return std::make_unique<std::pair<std::string const, Alias&>>(this->GetKey(),
                                                                    node.label_.value());
    }

    GenericIterator& operator++() {
      this->Advance();
      return *this;
    }

    GenericIterator operator++(int) {
      GenericIterator result = *this;
      this->Advance();
      return result;
    }

    void swap(GenericIterator& other) { this->frames_.swap(other.frames_); }

   private:
    friend class TrieNode;
    explicit GenericIterator(NodeSet const& roots) : BaseIterator<reverse>(roots) {}
    explicit GenericIterator() = default;
    explicit GenericIterator(std::vector<StateFrame<reverse>> frames)
        : BaseIterator<reverse>(std::move(frames)) {}
  };

  // Bidirectional const iterator.
  template <typename Value, bool reverse>
  class GenericConstIterator : public BaseIterator<reverse> {
   public:
    GenericConstIterator(GenericConstIterator const&) = default;
    GenericConstIterator& operator=(GenericConstIterator const&) = default;
    GenericConstIterator(GenericConstIterator&&) noexcept = default;
    GenericConstIterator& operator=(GenericConstIterator&&) noexcept = default;

    GenericConstIterator(GenericIterator<Value, reverse> const& other)
        : BaseIterator<reverse>(other.frames_) {}
    GenericConstIterator(GenericIterator<Value, reverse>&& other) noexcept
        : BaseIterator<reverse>(std::move(other.frames_)) {}

    template <typename Alias = Value, std::enable_if_t<std::is_void_v<Alias>, bool> = true>
    std::string operator*() const {
      return this->GetKey();
    }

    template <typename Alias = Value, std::enable_if_t<std::is_void_v<Alias>, bool> = true>
    std::unique_ptr<std::string const> operator->() const {
      return std::make_unique<std::string const>(this->GetKey());
    }

    template <typename Alias = Value, std::enable_if_t<!std::is_void_v<Alias>, bool> = true>
    std::pair<std::string const, Alias const&> operator*() const {
      auto& node = this->frames_.back().pos->second;
      return {this->GetKey(), node.label_.value()};
    }

    template <typename Alias = Value, std::enable_if_t<!std::is_void_v<Alias>, bool> = true>
    std::unique_ptr<std::pair<std::string const, Alias const&>> operator->() const {
      auto& node = this->frames_.back().pos->second;
      return std::make_unique<std::pair<std::string const, Alias const&>>(this->GetKey(),
                                                                          node.label_.value());
    }

    GenericConstIterator& operator++() {
      this->Advance();
      return *this;
    }

    GenericConstIterator operator++(int) {
      GenericConstIterator result = *this;
      this->Advance();
      return result;
    }

    void swap(GenericConstIterator& other) { this->frames_.swap(other.frames_); }

   private:
    friend class TrieNode;
    explicit GenericConstIterator(NodeSet const& roots) : BaseIterator<reverse>(roots) {}
    explicit GenericConstIterator() = default;
    explicit GenericConstIterator(std::vector<StateFrame<reverse>> frames)
        : BaseIterator<reverse>(std::move(frames)) {}
  };

 public:
  using Iterator = GenericIterator<typename Traits::Mapped, false>;
  using ConstIterator = GenericConstIterator<typename Traits::Mapped, false>;
  using ReverseIterator = GenericIterator<typename Traits::Mapped, true>;
  using ConstReverseIterator = GenericConstIterator<typename Traits::Mapped, true>;

  template <typename... Args>
  explicit TrieNode(EntryAllocator const& alloc, Args&&... args)
      : label_(std::forward<Args>(args)...), children_(alloc) {}

  TrieNode(TrieNode const& other) = default;
  TrieNode& operator=(TrieNode const& other) = default;
  TrieNode(TrieNode&& other) noexcept = default;
  TrieNode& operator=(TrieNode&& other) noexcept = default;

  EntryAllocator get_allocator() const noexcept {
    return EntryAllocatorTraits::select_on_container_copy_construction(children_.get_allocator());
  }

  template <typename H>
  friend H AbslHashValue(H h, TrieNode const& node) {
    return H::combine(std::move(h), node.label_, node.children_);
  }

  friend bool operator==(TrieNode const& lhs, TrieNode const& rhs) {
    return lhs.Tie() == rhs.Tie();
  }

  friend bool operator!=(TrieNode const& lhs, TrieNode const& rhs) {
    return lhs.Tie() != rhs.Tie();
  }

  friend bool operator<(TrieNode const& lhs, TrieNode const& rhs) { return lhs.Tie() < rhs.Tie(); }

  friend bool operator<=(TrieNode const& lhs, TrieNode const& rhs) {
    return lhs.Tie() <= rhs.Tie();
  }

  friend bool operator>(TrieNode const& lhs, TrieNode const& rhs) { return lhs.Tie() > rhs.Tie(); }

  friend bool operator>=(TrieNode const& lhs, TrieNode const& rhs) {
    return lhs.Tie() >= rhs.Tie();
  }

  static Iterator begin(NodeSet const& roots) { return Iterator(roots); }
  static ConstIterator cbegin(NodeSet const& roots) { return ConstIterator(roots); }
  static Iterator end() { return Iterator(); }
  static ConstIterator cend() { return ConstIterator(); }
  static ReverseIterator rbegin(NodeSet const& roots) { return ReverseIterator(roots); }
  static ConstReverseIterator crbegin(NodeSet const& roots) { return ConstReverseIterator(roots); }
  static ReverseIterator rend() { return ReverseIterator(); }
  static ConstReverseIterator crend() { return ConstReverseIterator(); }

  // Indicates whether the trie rooted at this node is empty.
  bool IsEmpty() const { return !TestLabel() && children_.empty(); }

  // Deletes all elements from the trie rooted at this node.
  void Clear();

  // Finds the element with the specified `key` and returns an iterator to it, or returns the end
  // iterator if the element is not found.
  static Iterator Find(NodeSet const& roots, std::string_view key);

  // Finds the element with the specified `key` and returns a const iterator to it, or returns the
  // end iterator if the element is not found.
  static ConstIterator FindConst(NodeSet const& roots, std::string_view const key) {
    return ConstIterator(Find(roots, key));
  }

  // Determines whether the trie rooted at this node contains the specified key.
  bool Contains(std::string_view key) const;

  // Determines whether the trie rooted at this node contains any strings matching the provided
  // regular expression.
  bool Contains(RE const& re) const {
    auto const& automaton = re.automaton_;
    if (automaton->IsDeterministic()) {
      return Contains(regexp_internal::DFA::Runner(automaton.get()));
    } else {
      return Contains(regexp_internal::NFA::Runner(automaton.get()));
    }
  }

  // Finds the first element whose key is greater than or equal to `key`. Returns the end iterator
  // if no such element exists.
  static Iterator LowerBound(NodeSet const& roots, std::string_view key);

  // Finds the first element whose key is greater than or equal to `key`. Returns the end iterator
  // if no such element exists.
  static ConstIterator LowerBoundConst(NodeSet const& roots, std::string_view const key) {
    return ConstIterator(LowerBound(roots, key));
  }

  // Finds the first element whose key is strictly greater than `key`. Returns the end iterator if
  // no such element exists.
  static Iterator UpperBound(NodeSet const& roots, std::string_view key);

  // Finds the first element whose key is strictly greater than `key`. Returns the end iterator if
  // no such element exists.
  static ConstIterator UpperBoundConst(NodeSet const& roots, std::string_view const key) {
    return ConstIterator(UpperBound(roots, key));
  }

  // Inserts a new element if one with the specified `key` is not already present. If an element is
  // inserted, the mapped value is constructed in place from the provided `args`. The first element
  // of the returned pair is an iterator to either the newly added node or a preexisting one with
  // the same key, while the second element is a boolean indicating whether insertion happened.
  template <typename... Args>
  static std::pair<Iterator, bool> Insert(NodeSet* roots, std::string_view key, Args&&... args);

  // Removes the element with the specified `key` from the trie rooted at this node. Does nothing if
  // no such element exists. Returns a boolean indicating whether removal happened.
  bool Remove(std::string_view key);

  // Removes the value referred to by the specified iterator and returns an iterator to the next
  // element. All other iterators of this container are invalidated.
  //
  // WARNING: the iterator MUST be valid and dereferenceable, otherwise the behavior is undefined.
  // This is consistent with STL containers.
  static Iterator Remove(NodeSet* const roots, Iterator it) {
    return Remove(roots, ConstIterator(it));
  }

  // Removes the value referred to by the specified const iterator and returns an iterator to the
  // next element. All other iterators of this container are invalidated.
  //
  // WARNING: the iterator MUST be valid and dereferenceable, otherwise the behavior is undefined.
  // This is consistent with STL containers.
  static Iterator Remove(NodeSet* roots, ConstIterator it);

  // Removes the value referred to by the specified const iterator. All other iterators of this
  // container are invalidated.
  //
  // This method is faster than `Remove` because it avoids copying the input iterator and doesn't
  // need to advance it to the next element.
  //
  // WARNING: the iterator MUST be valid and dereferenceable, otherwise the behavior is undefined.
  // This is consistent with STL containers.
  static void RemoveFast(NodeSet* roots, DirectBaseIterator const& it);

 private:
  auto Tie() const { return std::tie(label_, children_); }

  bool TestLabel() const { return Traits::TestLabel(label_); }

  template <typename... Args>
  bool TrySetLabel(Args&&... args) {
    if (Traits::TestLabel(label_)) {
      return false;
    } else {
      label_ = Traits::ConstructLabel(std::forward<Args>(args)...);
      return true;
    }
  }

  bool ResetLabel() { return Traits::ResetLabel(label_); }

  typename NodeSet::iterator LowerBound(std::string_view needle);

  template <typename Automaton>
  bool Contains(Automaton const& automaton) const;

  template <typename... Args>
  std::pair<Iterator, bool> InsertChild(std::vector<DirectStateFrame> frames, std::string_view key,
                                        Args&&... args);

  Label label_;
  NodeSet children_;
};

template <typename Label, typename Allocator>
template <bool reverse>
TrieNode<Label, Allocator>::BaseIterator<reverse>::BaseIterator(NodeSet const& roots) {
  frames_.emplace_back(roots);
  TrieNode<Label, Allocator> const* root;
  if constexpr (reverse) {
    root = &(roots.rbegin()->second);
  } else {
    root = &(roots.begin()->second);
  }
  if (!root->TestLabel()) {
    Advance();
  }
}

template <typename Label, typename Allocator>
template <bool reverse>
std::string TrieNode<Label, Allocator>::BaseIterator<reverse>::GetKey() const {
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

template <typename Label, typename Allocator>
template <bool reverse>
void TrieNode<Label, Allocator>::BaseIterator<reverse>::NextNode() {
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

template <typename Label, typename Allocator>
template <bool reverse>
void TrieNode<Label, Allocator>::BaseIterator<reverse>::Advance() {
  while (NextNode(), !frames_.empty()) {
    auto const& frame = frames_.back();
    auto const& node = frame.pos->second;
    if (node.TestLabel()) {
      return;
    }
  }
}

template <typename Label, typename Allocator>
void TrieNode<Label, Allocator>::Clear() {
  ResetLabel();
  children_.clear();
}

template <typename Label, typename Allocator>
typename TrieNode<Label, Allocator>::Iterator TrieNode<Label, Allocator>::Find(
    NodeSet const& roots, std::string_view key) {
  std::vector<DirectStateFrame> frames{DirectStateFrame(roots)};
  while (!key.empty()) {
    auto& node = frames.back().pos->second;
    auto const end = node.children_.end();
    auto const it = node.children_.lower_bound(key.substr(0, 1));
    if (it == end || !absl::ConsumePrefix(&key, it->first)) {
      return Iterator();
    }
    frames.emplace_back(it, end);
  }
  auto const& node = frames.back().pos->second;
  if (node.TestLabel()) {
    return Iterator(std::move(frames));
  } else {
    return Iterator();
  }
}

template <typename Label, typename Allocator>
bool TrieNode<Label, Allocator>::Contains(std::string_view const key) const {
  if (key.empty()) {
    return TestLabel();
  }
  auto const it = children_.lower_bound(key.substr(0, 1));
  if (it == children_.end()) {
    return false;
  }
  auto const& [prefix, node] = *it;
  if (absl::StartsWith(key, prefix)) {
    return node.Contains(key.substr(prefix.size()));
  } else {
    return false;
  }
}

template <typename Label, typename Allocator>
typename TrieNode<Label, Allocator>::NodeSet::iterator TrieNode<Label, Allocator>::LowerBound(
    std::string_view const needle) {
  auto const end = children_.end();
  auto it = children_.lower_bound(needle.substr(0, 1));
  if (it == end) {
    return it;
  }
  auto const prefix = it->first;
  if (prefix[0] > needle[0]) {
    return it;
  }
  size_t i = 1;
  while (i < needle.size() && i < prefix.size()) {
    if (needle[i] != prefix[i]) {
      return ++it;
    } else {
      ++i;
    }
  }
  return it;
}

template <typename Label, typename Allocator>
typename TrieNode<Label, Allocator>::Iterator TrieNode<Label, Allocator>::LowerBound(
    NodeSet const& roots, std::string_view key) {
  std::vector<DirectStateFrame> frames{DirectStateFrame(roots)};
  while (!key.empty()) {
    auto& node = frames.back().pos->second;
    auto const it = node.LowerBound(key);
    auto const end = node.children_.end();
    frames.emplace_back(it, end);
    if (it == end) {
      break;
    }
    auto const prefix = it->first;
    if (key.size() < prefix.size() || !absl::ConsumePrefix(&key, prefix)) {
      break;
    }
  }
  auto const& frame = frames.back();
  bool const leaf = frame.pos != frame.end && frame.pos->second.TestLabel();
  Iterator result{std::move(frames)};
  if (!leaf) {
    result.Advance();
  }
  return result;
}

template <typename Label, typename Allocator>
typename TrieNode<Label, Allocator>::Iterator TrieNode<Label, Allocator>::UpperBound(
    NodeSet const& roots, std::string_view key) {
  std::vector<DirectStateFrame> frames{DirectStateFrame(roots)};
  while (!key.empty()) {
    auto& node = frames.back().pos->second;
    auto const it = node.LowerBound(key);
    auto const end = node.children_.end();
    frames.emplace_back(it, end);
    if (it == end) {
      break;
    }
    auto const prefix = it->first;
    if (key.size() < prefix.size() || !absl::ConsumePrefix(&key, prefix)) {
      auto const& node = it->second;
      Iterator result{std::move(frames)};
      if (!node.TestLabel()) {
        result.Advance();
      }
      return result;
    }
  }
  Iterator result{std::move(frames)};
  result.Advance();
  return result;
}

template <typename Label, typename Allocator>
template <typename Automaton>
bool TrieNode<Label, Allocator>::Contains(Automaton const& automaton) const {
  for (auto const& [key, child] : children_) {
    Automaton child_automaton = automaton;
    if (child_automaton.Step(key)) {
      if (child.TestLabel()) {
        Automaton finish_automaton = child_automaton;
        if (finish_automaton.Finish()) {
          return true;
        }
      }
      if (child.Contains(child_automaton)) {
        return true;
      }
    }
  }
  return false;
}

template <typename Label, typename Allocator>
template <typename... Args>
std::pair<typename TrieNode<Label, Allocator>::Iterator, bool>
TrieNode<Label, Allocator>::InsertChild(std::vector<DirectStateFrame> frames,
                                        std::string_view const value, Args&&... args) {
  auto const [it, unused_inserted] = children_.try_emplace(
      std::string(value), children_.get_allocator(), std::forward<Args>(args)...);
  frames.emplace_back(it, children_.end());
  return std::make_pair(Iterator(std::move(frames)), true);
}

template <typename Label, typename Allocator>
template <typename... Args>
std::pair<typename TrieNode<Label, Allocator>::Iterator, bool> TrieNode<Label, Allocator>::Insert(
    NodeSet* const roots, std::string_view key, Args&&... args) {
  std::vector<DirectStateFrame> frames{DirectStateFrame(*roots)};
  while (!key.empty()) {
    auto& node = frames.back().pos->second;
    auto const it = node.children_.lower_bound(key.substr(0, 1));
    if (it == node.children_.end()) {
      return node.InsertChild(std::move(frames), key, std::forward<Args>(args)...);
    }
    auto& [prefix, child] = *it;
    size_t i = 0;
    // Find the longest common prefix.
    while (i < key.size() && i < prefix.size() && key[i] == prefix[i]) {
      ++i;
    }
    // `i` is now the length of the longest common prefix.
    if (i == 0) {
      return node.InsertChild(std::move(frames), key, std::forward<Args>(args)...);
    }
    frames.emplace_back(it, node.children_.end());
    if (i < prefix.size()) {
      TrieNode temp = std::move(child);
      child = TrieNode(node.children_.get_allocator());
      child.children_.try_emplace(prefix.substr(i), std::move(temp));
      prefix.resize(i);
    }
    key.remove_prefix(i);
  }
  auto& node = frames.back().pos->second;
  bool const inserted = node.TrySetLabel(std::forward<Args>(args)...);
  return std::make_pair(Iterator(std::move(frames)), inserted);
}

template <typename Label, typename Allocator>
bool TrieNode<Label, Allocator>::Remove(std::string_view const key) {
  if (key.empty()) {
    return ResetLabel();
  }
  auto const it = children_.lower_bound(key.substr(0, 1));
  if (it == children_.end()) {
    return false;
  }
  auto& [prefix, node] = *it;
  size_t i = 0;
  // Find the longest common prefix.
  while (i < key.size() && i < prefix.size() && key[i] == prefix[i]) {
    ++i;
  }
  // `i` is now the length of the longest common prefix.
  if (i < prefix.size()) {
    return false;
  }
  bool const result = node.Remove(key.substr(i));
  if (node.IsEmpty()) {
    children_.erase(it);
  }
  return result;
}

template <typename Label, typename Allocator>
typename TrieNode<Label, Allocator>::Iterator TrieNode<Label, Allocator>::Remove(
    NodeSet* const roots, ConstIterator it) {
  auto& frames = it.frames_;
  NodeSet* nodes = roots;
  if (frames.size() > 1) {
    nodes = &(frames[frames.size() - 2].pos->second.children_);
  }
  auto& last_frame = frames.back();
  auto& node = last_frame.pos->second;
  if (node.children_.size() > 1) {
    node.ResetLabel();
  } else if (node.children_.size() > 0) {
    auto child_node = node.children_.extract(node.children_.begin());
    last_frame.pos->first += child_node.key();
    node = std::move(child_node).mapped();
  } else {
    last_frame.pos = nodes->erase(last_frame.pos);
    last_frame.end = nodes->end();
    if (last_frame.pos != last_frame.end && last_frame.pos->second.TestLabel()) {
      return Iterator(std::move(it.frames_));
    }
  }
  it.Advance();
  return Iterator(std::move(it.frames_));
}

template <typename Label, typename Allocator>
void TrieNode<Label, Allocator>::RemoveFast(NodeSet* const roots, DirectBaseIterator const& it) {
  auto const& frames = it.frames_;
  NodeSet* nodes = roots;
  if (frames.size() > 1) {
    nodes = &(frames[frames.size() - 2].pos->second.children_);
  }
  auto const& last_frame = frames.back();
  auto& node = last_frame.pos->second;
  if (node.children_.size() > 1) {
    node.ResetLabel();
  } else if (node.children_.size() > 0) {
    auto child_node = node.children_.extract(node.children_.begin());
    last_frame.pos->first += child_node.key();
    node = std::move(child_node).mapped();
  } else {
    nodes->erase(last_frame.pos);
  }
}

}  // namespace internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RAW_TRIE_H__
