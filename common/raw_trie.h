#ifndef __TSDB2_COMMON_RAW_TRIE_H__
#define __TSDB2_COMMON_RAW_TRIE_H__

#include <algorithm>
#include <cstddef>
#include <cstdint>
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
#include "common/re/automaton.h"
#include "common/reffed_ptr.h"

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

// A trie node. This is the core trie implementation used by `trie_set` and `trie_map`.
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

  using Stepper = std::unique_ptr<regexp_internal::AbstractAutomaton::AbstractStepper>;

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
  //   void TrieNode::Scan() {
  //     auto end = children_.end();
  //     for (auto pos = children_.begin(); pos != end; ++pos) {
  //       DoSomething(*pos);
  //       pos->Scan();
  //     }
  //   }
  //
  // The `reverse` flag indicates whether the `StateFrame` embeds reverse iterators or not. Reverse
  // iterators are retrieved by calling `rbegin()` / `rend()` on a node set.
  template <bool reverse>
  class StateFrame;

  // Specializes `StateFrame` for forward iterators.
  template <>
  class StateFrame<false> {
   public:
    explicit StateFrame(NodeSet& nodes) : pos_(nodes.begin()), end_(nodes.end()) {}
    explicit StateFrame(NodeSet const& nodes) : StateFrame(const_cast<NodeSet&>(nodes)) {}

    explicit StateFrame(typename NodeSet::iterator pos_it, typename NodeSet::iterator end_it)
        : pos_(std::move(pos_it)), end_(std::move(end_it)) {}

    ~StateFrame() = default;

    StateFrame(StateFrame const&) = default;
    StateFrame& operator=(StateFrame const&) = default;
    StateFrame(StateFrame&&) noexcept = default;
    StateFrame& operator=(StateFrame&&) noexcept = default;

    void swap(StateFrame& other) noexcept {
      using std::swap;  // ensure ADL
      swap(pos_, other.pos_);
      swap(end_, other.end_);
    }

    friend void swap(StateFrame& lhs, StateFrame& rhs) noexcept { lhs.swap(rhs); }

    friend bool operator==(StateFrame const& lhs, StateFrame const& rhs) {
      if (lhs.pos_ != lhs.end_) {
        return rhs.pos_ != rhs.end_ && lhs.pos_->first == rhs.pos_->first;
      } else {
        return rhs.pos_ == rhs.end_;
      }
    }

    friend bool operator!=(StateFrame const& lhs, StateFrame const& rhs) { return !(lhs == rhs); }

    bool at_end() const { return pos_ == end_; }
    std::string_view key() const { return pos_->first; }
    TrieNode& node() const { return pos_->second; }

    bool Advance() { return ++pos_ != end_; }

    void AppendToKey(std::string_view const suffix) { pos_->first += suffix; }

    void EraseFrom(NodeSet* const nodes) & {
      pos_ = nodes->erase(pos_);
      end_ = nodes->end();
    }

    void EraseFrom(NodeSet* const nodes) && { nodes->erase(pos_); }

   private:
    typename NodeSet::iterator pos_;
    typename NodeSet::iterator end_;
  };

  // Specializes `StateFrame` for reverse iterators.
  template <>
  class StateFrame<true> {
   public:
    explicit StateFrame(NodeSet& nodes) : pos_(nodes.rbegin()), end_(nodes.rend()) {}
    explicit StateFrame(NodeSet const& nodes) : StateFrame(const_cast<NodeSet&>(nodes)) {}
    ~StateFrame() = default;

    StateFrame(StateFrame const&) = default;
    StateFrame& operator=(StateFrame const&) = default;
    StateFrame(StateFrame&&) noexcept = default;
    StateFrame& operator=(StateFrame&&) noexcept = default;

    void swap(StateFrame& other) noexcept {
      using std::swap;  // ensure ADL
      swap(pos_, other.pos_);
      swap(end_, other.end_);
    }

    friend void swap(StateFrame& lhs, StateFrame& rhs) noexcept { lhs.swap(rhs); }

    friend bool operator==(StateFrame const& lhs, StateFrame const& rhs) {
      if (lhs.pos_ != lhs.end_) {
        return rhs.pos_ != rhs.end_ && lhs.pos_->first == rhs.pos_->first;
      } else {
        return rhs.pos_ == rhs.end_;
      }
    }

    friend bool operator!=(StateFrame const& lhs, StateFrame const& rhs) { return !(lhs == rhs); }

    bool at_end() const { return pos_ == end_; }
    std::string_view key() const { return pos_->first; }
    TrieNode& node() const { return pos_->second; }

    bool Advance() { return ++pos_ != end_; }

   private:
    typename NodeSet::reverse_iterator pos_;
    typename NodeSet::reverse_iterator end_;
  };

  using DirectStateFrame = StateFrame<false>;

  // Base class of the state frames used in `FilteredView`s.
  template <bool reverse>
  class BaseFilteredStateFrame : public StateFrame<reverse> {
   public:
    using Base = StateFrame<reverse>;

    explicit BaseFilteredStateFrame(NodeSet& nodes, Stepper const& parent_stepper)
        : Base(nodes),
          parent_stepper_(parent_stepper.get()),
          stepper_(parent_stepper ? parent_stepper->Clone() : Stepper()) {}

    explicit BaseFilteredStateFrame(NodeSet const& nodes, Stepper const& parent_stepper)
        : Base(nodes),
          parent_stepper_(parent_stepper.get()),
          stepper_(parent_stepper ? parent_stepper->Clone() : Stepper()) {}

    explicit BaseFilteredStateFrame(typename NodeSet::iterator pos_it,
                                    typename NodeSet::iterator end_it,
                                    Stepper const& parent_stepper)
        : Base(std::move(pos_it), std::move(end_it)),
          parent_stepper_(parent_stepper.get()),
          stepper_(parent_stepper ? parent_stepper->Clone() : Stepper()) {}

    ~BaseFilteredStateFrame() = default;

    BaseFilteredStateFrame(BaseFilteredStateFrame const&) = default;
    BaseFilteredStateFrame& operator=(BaseFilteredStateFrame const&) = default;
    BaseFilteredStateFrame(BaseFilteredStateFrame&&) noexcept = default;
    BaseFilteredStateFrame& operator=(BaseFilteredStateFrame&&) noexcept = default;

    void swap(BaseFilteredStateFrame& other) noexcept {
      Base::swap(other);
      using std::swap;  // ensure ADL
      swap(parent_stepper_, other.parent_stepper_);
      swap(stepper_, other.stepper_);
    }

    friend void swap(BaseFilteredStateFrame& lhs, BaseFilteredStateFrame& rhs) noexcept {
      lhs.swap(rhs);
    }

    Stepper const& stepper() const { return stepper_; }

    bool Advance() {
      if (Base::Advance()) {
        if (parent_stepper_ != nullptr) {
          stepper_ = parent_stepper_->Clone();
        }
        return true;
      } else {
        return false;
      }
    }

    intptr_t MatchPrefix() {
      auto const& key = Base::key();
      if (!stepper_) {
        return key.size();
      }
      intptr_t matched_length = 0;
      for (char const ch : key) {
        if (stepper_->Finish(ch)) {
          clear_stepper();
          return matched_length;
        } else if (!stepper_->Step(ch)) {
          return -1;
        }
        ++matched_length;
      }
      return matched_length;
    }

   private:
    void clear_stepper() {
      parent_stepper_ = nullptr;
      stepper_.reset();
    }

    regexp_internal::AbstractAutomaton::AbstractStepper const* parent_stepper_;  // not owned
    Stepper stepper_;
  };

  template <bool reverse>
  class FilteredStateFrame : public BaseFilteredStateFrame<reverse> {
   public:
    using BaseFilteredStateFrame<reverse>::swap;
    template <typename... Args>
    explicit FilteredStateFrame(Args&&... args)
        : BaseFilteredStateFrame<reverse>(std::forward<Args>(args)...) {}
    ~FilteredStateFrame() = default;
    FilteredStateFrame(FilteredStateFrame const&) = default;
    FilteredStateFrame& operator=(FilteredStateFrame const&) = default;
    FilteredStateFrame(FilteredStateFrame&&) noexcept = default;
    FilteredStateFrame& operator=(FilteredStateFrame&&) noexcept = default;
  };

  template <bool reverse>
  class PrefixFilteredStateFrame : public BaseFilteredStateFrame<reverse> {
   public:
    using BaseFilteredStateFrame<reverse>::swap;
    template <typename... Args>
    explicit PrefixFilteredStateFrame(Args&&... args)
        : BaseFilteredStateFrame<reverse>(std::forward<Args>(args)...) {}
    ~PrefixFilteredStateFrame() = default;
    PrefixFilteredStateFrame(PrefixFilteredStateFrame const&) = default;
    PrefixFilteredStateFrame& operator=(PrefixFilteredStateFrame const&) = default;
    PrefixFilteredStateFrame(PrefixFilteredStateFrame&&) noexcept = default;
    PrefixFilteredStateFrame& operator=(PrefixFilteredStateFrame&&) noexcept = default;
  };

  template <typename StateFrameType>
  static std::string GetElementKey(std::vector<StateFrameType> const& frames);

 public:
  // Base class of all iterators.
  //
  // `StateFrame` is the type of state frames contained in the iterator, which may vary based on the
  // type of iterators and other variables in the frame.
  //
  // The following state frame types are supported:
  //
  //   - direct unfiltered frames (`StateFrame<false>`);
  //   - reverse unfiltered frames (`StateFrame<true>`);
  //   - direct filtered frames (`FilteredStateFrame<false>`);
  //   - reverse filtered frames (`FilteredStateFrame<true>`);
  //   - direct prefix-filtered frames (`PrefixFilteredStateFrame<false>`);
  //   - reverse prefix-filtered frames (`PrefixFilteredStateFrame<true>`).
  //
  // Direct state frames are used by forward iterators, while reverse ones are used by reverse
  // iterators. Filtered state frames are used by the iterators of filtered views, while unfiltered
  // ones are for regular iterations. Prefix-filtered state frames are used by views that are
  // filtered via prefix matching of a regular expression, as opposed to filtered views which use
  // full matching.
  template <typename StateFrame>
  class BaseIterator;

  // Specializes `BaseIterator` for unfiltered state frames.
  template <bool reverse>
  class BaseIterator<StateFrame<reverse>> {
   public:
    BaseIterator(BaseIterator const&) = default;
    BaseIterator& operator=(BaseIterator const&) = default;
    BaseIterator(BaseIterator&&) noexcept = default;
    BaseIterator& operator=(BaseIterator&&) noexcept = default;

    void swap(BaseIterator& other) noexcept { std::swap(frames_, other.frames_); }
    friend void swap(BaseIterator& lhs, BaseIterator& rhs) noexcept { lhs.swap(rhs); }

    friend bool operator==(BaseIterator const& lhs, BaseIterator const& rhs) {
      return lhs.frames_ == rhs.frames_;
    }

    friend bool operator!=(BaseIterator const& lhs, BaseIterator const& rhs) {
      return lhs.frames_ != rhs.frames_;
    }

   protected:
    friend class TrieNode;

    // Constructs the "begin" iterator.
    explicit BaseIterator(NodeSet const& roots) {
      if (!roots.empty()) {
        auto const& frame = frames_.emplace_back(roots);
        if (!frame.node().TestLabel()) {
          Advance();
        }
      }
    }

    // Constructs the "end" iterator.
    explicit BaseIterator() = default;

    // Constructs an iterator with the given state frames.
    explicit BaseIterator(std::vector<StateFrame<reverse>> frames) : frames_(std::move(frames)) {}

    ~BaseIterator() = default;

    // Returns true iff this is the end iterator.
    bool is_end() const { return frames_.empty(); }

    std::string GetKey() const { return GetElementKey(frames_); }
    TrieNode& node() const { return frames_.back().node(); }

    // Advances the iterator to the next node repeatedly until either the next terminal node is
    // found or there are no more nodes. In the latter case the iterator becomes the end iterator of
    // the trie.
    //
    // This is the main iterator advancement algorithm invoked by `operator++`.
    void Advance() {
      while (NextNode(), !frames_.empty()) {
        auto const& frame = frames_.back();
        auto const& node = frame.node();
        if (node.TestLabel()) {
          return;
        }
      }
    }

   private:
    // Advances the iterator to the next node. The next node is found by attempting the following,
    // in order:
    //
    //  1. descend to the leftmost child;
    //  2. if the current node has no children, advance to the next peer;
    //  3. if the current node has no peers, backtrack to the parent (by removing the last frame)
    //     and repeat #2.
    //
    // NOTE: this algorithm won't necessarily find the next *terminal* node, it will simply advance
    // to the next one. The caller needs to repeat until either a terminal node is found or there
    // are no more nodes. That is achieved by the `Advance` method.
    void NextNode() {
      auto const& frame = frames_.back();
      if (frame.at_end()) {
        frames_.pop_back();
      } else {
        auto const& node = frame.node();
        if (!node.children_.empty()) {
          frames_.emplace_back(node.children_);
          return;
        }
      }
      while (!frames_.empty()) {
        auto& frame = frames_.back();
        if (frame.Advance()) {
          return;
        } else {
          frames_.pop_back();
        }
      }
    }

    std::vector<StateFrame<reverse>> frames_;
  };

  using DirectBaseIterator = BaseIterator<StateFrame<false>>;

  // Base class for all filtered iterators.
  template <typename StateFrame>
  class BaseFilteredIterator {
   public:
    BaseFilteredIterator(BaseFilteredIterator const&) = default;
    BaseFilteredIterator& operator=(BaseFilteredIterator const&) = default;
    BaseFilteredIterator(BaseFilteredIterator&&) noexcept = default;
    BaseFilteredIterator& operator=(BaseFilteredIterator&&) noexcept = default;

    void swap(BaseFilteredIterator& other) noexcept { std::swap(frames_, other.frames_); }

    friend void swap(BaseFilteredIterator& lhs, BaseFilteredIterator& rhs) noexcept {
      lhs.swap(rhs);
    }

    friend bool operator==(BaseFilteredIterator const& lhs, BaseFilteredIterator const& rhs) {
      return lhs.frames_ == rhs.frames_;
    }

    friend bool operator!=(BaseFilteredIterator const& lhs, BaseFilteredIterator const& rhs) {
      return lhs.frames_ != rhs.frames_;
    }

   protected:
    explicit BaseFilteredIterator() = default;
    explicit BaseFilteredIterator(std::vector<StateFrame> frames) : frames_(std::move(frames)) {}

    ~BaseFilteredIterator() = default;

    // Returns true iff this is the end iterator.
    bool is_end() const { return frames_.empty(); }

    std::string GetKey() const { return GetElementKey(frames_); }
    TrieNode& node() const { return frames_.back().node(); }

    // Advances the iterator to the next node by attempting the following, in order:
    //
    //  1. descend to the leftmost child;
    //  2. if the current node has no children, advance to the next peer;
    //  3. if the current node has no peers, backtrack to the parent (by removing the last frame)
    //     and repeat #2;
    //
    // NOTE: this algorithm won't necessarily find the next *terminal* node, much less the next
    // terminal node matching the regular expression pattern in the filter. It will simply advance
    // to the next node of the trie. The caller needs to repeat until either a matching terminal
    // node is found or there are no more nodes. That is achieved by the `Advance` method.
    void NextNode() {
      auto const& frame = frames_.back();
      if (frame.at_end()) {
        frames_.pop_back();
      } else {
        auto const& node = frame.node();
        if (!node.children_.empty()) {
          frames_.emplace_back(node.children_, frame.stepper());
          return;
        }
      }
      while (!frames_.empty()) {
        auto& frame = frames_.back();
        if (frame.Advance()) {
          return;
        } else {
          frames_.pop_back();
        }
      }
    }

    std::vector<StateFrame> frames_;
  };

  // Specializes `BaseIterator` for filtered views.
  template <bool reverse>
  class BaseIterator<FilteredStateFrame<reverse>>
      : public BaseFilteredIterator<FilteredStateFrame<reverse>> {
   public:
    using BaseFilteredIterator<FilteredStateFrame<reverse>>::swap;

    BaseIterator(BaseIterator const&) = default;
    BaseIterator& operator=(BaseIterator const&) = default;
    BaseIterator(BaseIterator&&) noexcept = default;
    BaseIterator& operator=(BaseIterator&&) noexcept = default;

   protected:
    // Constructs the "begin" iterator.
    explicit BaseIterator(NodeSet const& roots,
                          reffed_ptr<regexp_internal::AbstractAutomaton> const& automaton) {
      if (!roots.empty()) {
        Base::frames_.emplace_back(roots, roots.empty() ? Stepper() : automaton->MakeStepper());
        MaybeAdvance();
      }
    }

    // Constructs the "end" iterator.
    explicit BaseIterator() = default;

    ~BaseIterator() = default;

    // Advances the iterator to the next node repeatedly until either the next matching terminal
    // node is found or there are no more nodes. In the latter case the iterator becomes the end
    // iterator of the trie.
    //
    // This is the main iterator advancement algorithm invoked by `operator++`.
    void Advance() {
      Base::NextNode();
      MaybeAdvance();
    }

   private:
    using Base = BaseFilteredIterator<FilteredStateFrame<reverse>>;

    // Used by the constructor to advance to the next matching node only if the first node doesn't
    // match.
    void MaybeAdvance() {
      auto& frames = Base::frames_;
      for (; !frames.empty(); Base::NextNode()) {
        auto& frame = frames.back();
        auto const& stepper = frame.stepper();
        if (stepper->Step(frame.key())) {
          auto const& node = frame.node();
          if (node.TestLabel() && stepper->Finish()) {
            return;
          }
        }
      }
    }
  };

  // Specializes `BaseIterator` for filtered views.
  template <bool reverse>
  class BaseIterator<PrefixFilteredStateFrame<reverse>>
      : public BaseFilteredIterator<PrefixFilteredStateFrame<reverse>> {
   public:
    using BaseFilteredIterator<PrefixFilteredStateFrame<reverse>>::swap;

    BaseIterator(BaseIterator const&) = default;
    BaseIterator& operator=(BaseIterator const&) = default;
    BaseIterator(BaseIterator&&) noexcept = default;
    BaseIterator& operator=(BaseIterator&&) noexcept = default;

   protected:
    // Constructs the "begin" iterator.
    explicit BaseIterator(NodeSet const& roots,
                          reffed_ptr<regexp_internal::AbstractAutomaton> const& automaton) {
      if (!roots.empty()) {
        Base::frames_.emplace_back(roots, roots.empty() ? Stepper() : automaton->MakeStepper());
        MaybeAdvance();
      }
    }

    // Constructs the "end" iterator.
    explicit BaseIterator() = default;

    ~BaseIterator() = default;

    // Advances the iterator to the next node repeatedly until either the next matching terminal
    // node is found or there are no more nodes. In the latter case the iterator becomes the end
    // iterator of the trie.
    //
    // This is the main iterator advancement algorithm invoked by `operator++`.
    void Advance() {
      Base::NextNode();
      MaybeAdvance();
    }

   private:
    using Base = BaseFilteredIterator<PrefixFilteredStateFrame<reverse>>;

    void MaybeAdvance() {
      auto& frames = Base::frames_;
      while (!frames.empty()) {
        auto& frame = frames.back();
        auto const& key = frame.key();
        auto const& node = frame.node();
        auto const matched_length = frame.MatchPrefix();
        if (matched_length < 0) {
          if (frame.Advance()) {
            continue;
          }
        } else if (matched_length < key.size()) {
          if (node.TestLabel()) {
            return;
          }
        } else if (node.TestLabel()) {
          auto const& stepper = frame.stepper();
          if (!stepper || stepper->Finish()) {
            return;
          }
        }
        Base::NextNode();
      }
    }
  };

 private:
  // Bidirectional node iterator.
  template <typename StateFrame>
  class GenericIterator : public BaseIterator<StateFrame> {
   public:
    using BaseIterator<StateFrame>::swap;

    ~GenericIterator() = default;

    GenericIterator(GenericIterator const&) = default;
    GenericIterator& operator=(GenericIterator const&) = default;
    GenericIterator(GenericIterator&&) noexcept = default;
    GenericIterator& operator=(GenericIterator&&) noexcept = default;

    template <typename Value = typename Traits::Mapped,
              std::enable_if_t<std::is_void_v<Value>, bool> = true>
    std::string operator*() const {
      return this->GetKey();
    }

    template <typename Value = typename Traits::Mapped,
              std::enable_if_t<std::is_void_v<Value>, bool> = true>
    std::unique_ptr<std::string const> operator->() const {
      return std::make_unique<std::string const>(this->GetKey());
    }

    template <typename Value = typename Traits::Mapped,
              std::enable_if_t<!std::is_void_v<Value>, bool> = true>
    std::pair<std::string const, Value&> operator*() const {
      return {this->GetKey(), this->node().label_.value()};
    }

    template <typename Value = typename Traits::Mapped,
              std::enable_if_t<!std::is_void_v<Value>, bool> = true>
    std::unique_ptr<std::pair<std::string const, Value&>> operator->() const {
      return std::make_unique<std::pair<std::string const, Value&>>(this->GetKey(),
                                                                    this->node().label_.value());
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

   private:
    friend class TrieNode;

    template <typename... Args>
    explicit GenericIterator(Args&&... args)
        : BaseIterator<StateFrame>(std::forward<Args>(args)...) {}
  };

  // Bidirectional const iterator.
  template <typename StateFrame>
  class GenericConstIterator : public BaseIterator<StateFrame> {
   public:
    using BaseIterator<StateFrame>::swap;

    ~GenericConstIterator() = default;

    GenericConstIterator(GenericConstIterator const&) = default;
    GenericConstIterator& operator=(GenericConstIterator const&) = default;
    GenericConstIterator(GenericConstIterator&&) noexcept = default;
    GenericConstIterator& operator=(GenericConstIterator&&) noexcept = default;

    // NOLINTBEGIN(google-explicit-constructor)
    GenericConstIterator(GenericIterator<StateFrame> const& other)
        : BaseIterator<StateFrame>(other.frames_) {}
    GenericConstIterator(GenericIterator<StateFrame>&& other) noexcept
        : BaseIterator<StateFrame>(std::move(other.frames_)) {}
    // NOLINTEND(google-explicit-constructor)

    template <typename Value = typename Traits::Mapped,
              std::enable_if_t<std::is_void_v<Value>, bool> = true>
    std::string operator*() const {
      return this->GetKey();
    }

    template <typename Value = typename Traits::Mapped,
              std::enable_if_t<std::is_void_v<Value>, bool> = true>
    std::unique_ptr<std::string const> operator->() const {
      return std::make_unique<std::string const>(this->GetKey());
    }

    template <typename Value = typename Traits::Mapped,
              std::enable_if_t<!std::is_void_v<Value>, bool> = true>
    std::pair<std::string const, Value const&> operator*() const {
      return {this->GetKey(), this->node().label_.value()};
    }

    template <typename Value = typename Traits::Mapped,
              std::enable_if_t<!std::is_void_v<Value>, bool> = true>
    std::unique_ptr<std::pair<std::string const, Value const&>> operator->() const {
      return std::make_unique<std::pair<std::string const, Value const&>>(
          this->GetKey(), this->node().label_.value());
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

   private:
    friend class TrieNode;

    template <typename... Args>
    explicit GenericConstIterator(Args&&... args)
        : BaseIterator<StateFrame>(std::forward<Args>(args)...) {}
  };

 public:
  using Iterator = GenericIterator<StateFrame<false>>;
  using ConstIterator = GenericConstIterator<StateFrame<false>>;
  using ReverseIterator = GenericIterator<StateFrame<true>>;
  using ConstReverseIterator = GenericConstIterator<StateFrame<true>>;
  using FilteredIterator = GenericIterator<FilteredStateFrame<false>>;
  using ConstFilteredIterator = GenericConstIterator<FilteredStateFrame<false>>;
  using ReverseFilteredIterator = GenericIterator<FilteredStateFrame<true>>;
  using ConstReverseFilteredIterator = GenericConstIterator<FilteredStateFrame<true>>;
  using PrefixFilteredIterator = GenericIterator<PrefixFilteredStateFrame<false>>;
  using ConstPrefixFilteredIterator = GenericConstIterator<PrefixFilteredStateFrame<false>>;
  using ReversePrefixFilteredIterator = GenericIterator<PrefixFilteredStateFrame<true>>;
  using ConstReversePrefixFilteredIterator = GenericConstIterator<PrefixFilteredStateFrame<true>>;

  // Provides a view of the trie filtered by a regular expression, allowing the user to enumerate
  // only the elements whose key matches the regular expression.
  //
  // Under the hood `FilteredView` uses efficient algorithms that can entirely skip mismatching
  // subtrees, so it's much more efficient than just iterating over all elements and checking each
  // one against the regular expression.
  //
  // You can get a `FilteredView` instance by calling `TrieNode::Filter`.
  //
  // NOTE: the `FilteredView` refers to the parent trie internally, so the trie must not be moved or
  // destroyed while one or more `FilteredView` instances exist. It is okay to move and copy the
  // `FilteredView` itself.
  class FilteredView {
   public:
    ~FilteredView() = default;
    FilteredView(FilteredView const&) = default;
    FilteredView& operator=(FilteredView const&) = default;
    FilteredView(FilteredView&&) noexcept = default;
    FilteredView& operator=(FilteredView&&) noexcept = default;

    void swap(FilteredView& other) noexcept {
      using std::swap;  // ensure ADL
      swap(roots_, other.roots_);
      swap(automaton_, other.automaton_);
    }

    friend void swap(FilteredView& lhs, FilteredView& rhs) noexcept { lhs.swap(rhs); }

    FilteredIterator begin() { return FilteredIterator(*roots_, automaton_); }
    ConstFilteredIterator begin() const { return ConstFilteredIterator(*roots_, automaton_); }
    ConstFilteredIterator cbegin() const { return ConstFilteredIterator(*roots_, automaton_); }

    FilteredIterator end() { return FilteredIterator(); }
    ConstFilteredIterator end() const { return ConstFilteredIterator(); }
    ConstFilteredIterator cend() const { return ConstFilteredIterator(); }

    ReverseFilteredIterator rbegin() { return ReverseFilteredIterator(*roots_, automaton_); }

    ConstReverseFilteredIterator rbegin() const {
      return ConstReverseFilteredIterator(*roots_, automaton_);
    }

    ConstReverseFilteredIterator crbegin() const {
      return ConstReverseFilteredIterator(*roots_, automaton_);
    }

    ReverseFilteredIterator rend() { return ReverseFilteredIterator(); }
    ConstReverseFilteredIterator rend() const { return ConstReverseFilteredIterator(); }
    ConstReverseFilteredIterator crend() const { return ConstReverseFilteredIterator(); }

   private:
    friend class TrieNode;

    explicit FilteredView(NodeSet const& roots,
                          reffed_ptr<regexp_internal::AbstractAutomaton> automaton)
        : roots_(&roots), automaton_(std::move(automaton)) {}

    NodeSet const* roots_;
    reffed_ptr<regexp_internal::AbstractAutomaton> automaton_;
  };

  // Provides a view of the trie filtered by a regular expression, allowing the user to enumerate
  // only the elements whose key has a prefix matching the regular expression.
  //
  // Under the hood `PrefixFilteredView` uses efficient algorithms that can entirely skip
  // mismatching subtrees, so it's much more efficient than just iterating over all elements and
  // checking each one against the regular expression.
  //
  // NOTE: `FilteredView` uses full matching of the keys against the regular expression, while
  // `PrefixFilteredView` uses prefix matching. `PrefixFilteredView` is particularly useful to
  // search for arbitrary substrings of a large input text efficiently: you can use a trie to build
  // a suffix tree (i.e. store all possible suffixes) of the input text and associate the location
  // of each suffix, then you can search a substring by using it to create a `PrefixFilteredView`,
  // which will return all suffixes with that prefix and therefore all locations with the substring.
  //
  // You can get a `PrefixFilteredView` instance by calling `TrieNode::FilterPrefix`.
  //
  // NOTE: the `PrefixFilteredView` refers to the parent trie internally, so the trie must not be
  // moved or destroyed while one or more `PrefixFilteredView` instances exist. It is okay to move
  // and copy the `PrefixFilteredView` itself.
  class PrefixFilteredView {
   public:
    ~PrefixFilteredView() = default;
    PrefixFilteredView(PrefixFilteredView const&) = default;
    PrefixFilteredView& operator=(PrefixFilteredView const&) = default;
    PrefixFilteredView(PrefixFilteredView&&) noexcept = default;
    PrefixFilteredView& operator=(PrefixFilteredView&&) noexcept = default;

    void swap(PrefixFilteredView& other) noexcept {
      using std::swap;  // ensure ADL
      swap(roots_, other.roots_);
      swap(automaton_, other.automaton_);
    }

    friend void swap(PrefixFilteredView& lhs, PrefixFilteredView& rhs) noexcept { lhs.swap(rhs); }

    PrefixFilteredIterator begin() { return PrefixFilteredIterator(*roots_, automaton_); }

    ConstPrefixFilteredIterator begin() const {
      return ConstPrefixFilteredIterator(*roots_, automaton_);
    }

    ConstPrefixFilteredIterator cbegin() const {
      return ConstPrefixFilteredIterator(*roots_, automaton_);
    }

    PrefixFilteredIterator end() { return PrefixFilteredIterator(); }

    ConstPrefixFilteredIterator end() const { return ConstPrefixFilteredIterator(); }

    ConstPrefixFilteredIterator cend() const { return ConstPrefixFilteredIterator(); }

    ReversePrefixFilteredIterator rbegin() {
      return ReversePrefixFilteredIterator(*roots_, automaton_);
    }

    ConstReversePrefixFilteredIterator rbegin() const {
      return ConstReversePrefixFilteredIterator(*roots_, automaton_);
    }

    ConstReversePrefixFilteredIterator crbegin() const {
      return ConstReversePrefixFilteredIterator(*roots_, automaton_);
    }

    ReversePrefixFilteredIterator rend() { return ReversePrefixFilteredIterator(); }

    ConstReversePrefixFilteredIterator rend() const { return ConstReversePrefixFilteredIterator(); }

    ConstReversePrefixFilteredIterator crend() const {
      return ConstReversePrefixFilteredIterator();
    }

   private:
    friend class TrieNode;

    explicit PrefixFilteredView(NodeSet const& roots,
                                reffed_ptr<regexp_internal::AbstractAutomaton> automaton)
        : roots_(&roots), automaton_(std::move(automaton)) {}

    NodeSet const* roots_;
    reffed_ptr<regexp_internal::AbstractAutomaton> automaton_;
  };

  template <typename... Args>
  explicit TrieNode(EntryAllocator const& alloc, Args&&... args)
      : label_(std::forward<Args>(args)...), children_(alloc) {}

  ~TrieNode() = default;

  TrieNode(TrieNode const& other) = default;
  TrieNode& operator=(TrieNode const& other) = default;
  TrieNode(TrieNode&& other) noexcept = default;
  TrieNode& operator=(TrieNode&& other) noexcept = default;

  void swap(TrieNode& other) noexcept {
    using std::swap;  // ensure ADL
    swap(label_, other.label_);
    swap(children_, other.children_);
  }

  friend void swap(TrieNode& lhs, TrieNode& rhs) noexcept { lhs.swap(rhs); }

  EntryAllocator get_allocator() const noexcept {
    return EntryAllocatorTraits::select_on_container_copy_construction(children_.get_allocator());
  }

  template <typename H>
  friend H AbslHashValue(H h, TrieNode const& node) {
    return H::combine(std::move(h), node.label_, node.children_);
  }

  template <typename H>
  friend H Tsdb2FingerprintValue(H h, TrieNode const& node) {
    return H::Combine(std::move(h), node.label_, node.children_);
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

  // Creates a view of this trie filtered with the provided regular expression pattern. The returned
  // `FilteredView` allows efficiently enumerating only the elements whose key matches the regular
  // expression.
  static FilteredView Filter(NodeSet const& roots, RE re) {
    return FilteredView(roots, std::move(re.automaton_));
  }

  // Creates a view of this trie filtered with the provided regular expression pattern. The returned
  // `PrefixFilteredView` allows efficiently enumerating only the elements whose key has a prefix
  // matching the regular expression.
  static PrefixFilteredView FilterPrefix(NodeSet const& roots, RE re) {
    return PrefixFilteredView(roots, std::move(re.automaton_));
  }

  // Determines whether the trie rooted at this node contains the specified key.
  bool Contains(std::string_view key) const;

  // Determines whether the trie rooted at this node contains any strings matching the given regular
  // expression.
  bool Contains(std::string_view const prefix, RE const& re) const {
    return Contains(prefix, re.automaton_->MakeStepper(prefix.empty() ? 0 : prefix.back()));
  }

  // Determines whether the trie rooted at this node contains one or more strings with a prefix that
  // matches the given regular expression.
  bool ContainsPrefix(std::string_view const prefix, RE const& re) const {
    return ContainsPrefix(prefix, re.automaton_->MakeStepper(prefix.empty() ? 0 : prefix.back()));
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

  // NOTE: the `key` argument must be the key of *this* node.
  bool Contains(std::string_view key, Stepper const& stepper) const;

  // NOTE: the `key` argument must be the key of *this* node.
  bool ContainsPrefix(std::string_view key, Stepper const& stepper) const;

  template <typename... Args>
  std::pair<Iterator, bool> InsertChild(std::vector<DirectStateFrame> frames, std::string_view key,
                                        Args&&... args);

  Label label_;
  NodeSet children_;
};

template <typename Label, typename Allocator>
template <typename StateFrameType>
std::string TrieNode<Label, Allocator>::GetElementKey(std::vector<StateFrameType> const& frames) {
  size_t size = 0;
  for (auto const& frame : frames) {
    size += frame.key().size();
  }
  std::string key;
  key.reserve(size);
  for (auto const& frame : frames) {
    key += frame.key();
  }
  return key;
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
    auto& node = frames.back().node();
    auto const end = node.children_.end();
    auto const it = node.children_.lower_bound(key.substr(0, 1));
    if (it == end || !absl::ConsumePrefix(&key, it->first)) {
      return Iterator();
    }
    frames.emplace_back(it, end);
  }
  auto const& node = frames.back().node();
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
  for (size_t i = 1; i < needle.size() && i < prefix.size(); ++i) {
    if (needle[i] < prefix[i]) {
      return it;
    } else if (prefix[i] < needle[i]) {
      return ++it;
    }
  }
  return it;
}

template <typename Label, typename Allocator>
typename TrieNode<Label, Allocator>::Iterator TrieNode<Label, Allocator>::LowerBound(
    NodeSet const& roots, std::string_view key) {
  std::vector<DirectStateFrame> frames{DirectStateFrame(roots)};
  while (!key.empty()) {
    auto& node = frames.back().node();
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
  bool const terminal = !frame.at_end() && frame.node().TestLabel();
  Iterator result{std::move(frames)};
  if (!terminal) {
    result.Advance();
  }
  return result;
}

template <typename Label, typename Allocator>
typename TrieNode<Label, Allocator>::Iterator TrieNode<Label, Allocator>::UpperBound(
    NodeSet const& roots, std::string_view key) {
  std::vector<DirectStateFrame> frames{DirectStateFrame(roots)};
  while (!key.empty()) {
    auto& node = frames.back().node();
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
bool TrieNode<Label, Allocator>::Contains(std::string_view const key,
                                          Stepper const& stepper) const {
  if (!stepper->Step(key)) {
    return false;
  }
  if (TestLabel() && stepper->Finish()) {
    return true;
  }
  for (auto const& [key, child] : children_) {
    if (child.Contains(key, stepper->Clone())) {
      return true;
    }
  }
  return false;
}

template <typename Label, typename Allocator>
bool TrieNode<Label, Allocator>::ContainsPrefix(std::string_view const key,
                                                Stepper const& stepper) const {
  for (char const ch : key) {
    if (stepper->Finish(ch)) {
      return true;
    } else if (!stepper->Step(ch)) {
      return false;
    }
  }
  if (TestLabel() && stepper->Finish()) {
    return true;
  }
  for (auto const& [key, child] : children_) {
    if (child.ContainsPrefix(key, stepper->Clone())) {
      return true;
    }
  }
  return false;
}

template <typename Label, typename Allocator>
template <typename... Args>
std::pair<typename TrieNode<Label, Allocator>::Iterator, bool>
TrieNode<Label, Allocator>::InsertChild(std::vector<DirectStateFrame> frames,
                                        std::string_view const key, Args&&... args) {
  auto const [it, unused_inserted] = children_.try_emplace(
      std::string(key), children_.get_allocator(), std::forward<Args>(args)...);
  frames.emplace_back(it, children_.end());
  return std::make_pair(Iterator(std::move(frames)), true);
}

template <typename Label, typename Allocator>
template <typename... Args>
std::pair<typename TrieNode<Label, Allocator>::Iterator, bool> TrieNode<Label, Allocator>::Insert(
    NodeSet* const roots, std::string_view key, Args&&... args) {
  std::vector<DirectStateFrame> frames{DirectStateFrame(*roots)};
  while (!key.empty()) {
    auto& node = frames.back().node();
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
  auto& node = frames.back().node();
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
    nodes = &(frames[frames.size() - 2].node().children_);
  }
  auto& last_frame = frames.back();
  auto& node = last_frame.node();
  if (node.children_.size() > 1) {
    node.ResetLabel();
  } else if (node.children_.size() > 0) {
    auto child_node = node.children_.extract(node.children_.begin());
    last_frame.AppendToKey(child_node.key());
    node = std::move(child_node).mapped();
  } else {
    last_frame.EraseFrom(nodes);
    if (!last_frame.at_end() && last_frame.node().TestLabel()) {
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
    nodes = &(frames[frames.size() - 2].node().children_);
  }
  auto& last_frame = const_cast<DirectStateFrame&>(frames.back());
  auto& node = last_frame.node();
  if (node.children_.size() > 1) {
    node.ResetLabel();
  } else if (node.children_.size() > 0) {
    auto child_node = node.children_.extract(node.children_.begin());
    last_frame.AppendToKey(child_node.key());
    node = std::move(child_node).mapped();
  } else {
    std::move(last_frame).EraseFrom(nodes);
  }
}

}  // namespace internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RAW_TRIE_H__
