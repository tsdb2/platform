#ifndef __TSDB2_COMMON_RE_CAPTURE_GROUPS_H__
#define __TSDB2_COMMON_RE_CAPTURE_GROUPS_H__

#include <cstddef>
#include <utility>

#include "absl/container/node_hash_set.h"
#include "absl/hash/hash.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

// Manages the hierarchy of capture groups.
//
// This class allows enumerating the path to the root of a given capture group efficiently. For
// example, given the expression `()((())(()()))` and the capture group #5 (assuming capture group
// numbers are zero-based), this class allows enumerating the path 5, 4, 1.
class CaptureGroups {
 private:
  struct Node {
    static int ToCaptureGroup(Node const &node) { return node.capture_group; }
    static int ToCaptureGroup(int const capture_group) { return capture_group; }

    struct Hash {
      using is_transparent = void;
      template <typename Arg>
      size_t operator()(Arg &&arg) const {
        return absl::HashOf(ToCaptureGroup(std::forward<Arg>(arg)));
      }
    };

    struct Equal {
      using is_transparent = void;
      template <typename LHS, typename RHS>
      bool operator()(LHS &&lhs, RHS &&rhs) const {
        return ToCaptureGroup(std::forward<LHS>(lhs)) == ToCaptureGroup(std::forward<RHS>(rhs));
      }
    };

    explicit Node(int const capture_group, Node const *const parent)
        : capture_group(capture_group), parent(parent) {}

    int const capture_group;
    Node const *const parent;
  };

 public:
  // Allows iterating over the path to the root of a given capture group.
  class Iterator {
   public:
    Iterator(Iterator const &) = default;
    Iterator &operator=(Iterator const &) = default;
    Iterator(Iterator &&) noexcept = default;
    Iterator &operator=(Iterator &&) noexcept = default;

    int operator*() const { return node_->capture_group; }
    int const *operator->() const { return &(node_->capture_group); }

    friend bool operator==(Iterator const &lhs, Iterator const &rhs) {
      return lhs.node_ == rhs.node_;
    }

    friend bool operator!=(Iterator const &lhs, Iterator const &rhs) {
      return lhs.node_ != rhs.node_;
    }

    Iterator &operator++() {
      node_ = node_->parent;
      return *this;
    }

    Iterator &operator++(int) {
      Iterator result = *this;
      node_ = node_->parent;
      return result;
    }

   private:
    friend class CaptureGroups;

    // Constructs an iterator referring to the specified node.
    explicit Iterator(Node const &node) : node_(&node) {}

    // Constructs the end iterator;
    explicit Iterator() : node_(nullptr) {}

    Node const *node_;
  };

  explicit CaptureGroups() = default;

  CaptureGroups(CaptureGroups const &) = default;
  CaptureGroups &operator=(CaptureGroups const &) = default;
  CaptureGroups(CaptureGroups &&) noexcept = default;
  CaptureGroups &operator=(CaptureGroups &&) noexcept = default;

  size_t size() const { return nodes_.size(); }

  // Adds a `capture_group` to the hierarchy as a child of `parent_capture_group`.
  //
  // If `parent_capture_group` is negative, `capture_group` is added as a root.
  //
  // REQUIRES: if positive, `parent_capture_group` must already be in the hierarchy.
  // REQUIRES: `capture_group` must be positive.
  void Add(int const capture_group, int const parent_capture_group) {
    if (parent_capture_group < 0) {
      nodes_.emplace(capture_group, nullptr);
    } else {
      auto const &parent = *(nodes_.find(parent_capture_group));
      nodes_.emplace(capture_group, &parent);
    }
  }

  // Returns the end iterator, which can be used to test any other iterator. Example iteration:
  //
  //   for (auto it = cg.LookUp(123); it != cg.root(); ++it) {
  //     // ...
  //   }
  //
  Iterator root() const { return Iterator(); }

  // Looks up the specified capture group and returns an iterator that allows the caller to iterate
  // over the path to the root.
  //
  // If the specified capture group is not found or it's a negative number, the end iterator is
  // returned.
  Iterator LookUp(int const capture_group) const {
    if (capture_group < 0) {
      return Iterator();
    }
    auto const it = nodes_.find(capture_group);
    if (it != nodes_.end()) {
      return Iterator(*it);
    } else {
      return Iterator();
    }
  }

 private:
  absl::node_hash_set<Node, Node::Hash, Node::Equal> nodes_;
};

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_CAPTURE_GROUPS_H__
