#ifndef __TSDB2_COMMON_RE_CAPTURE_GROUPS_H__
#define __TSDB2_COMMON_RE_CAPTURE_GROUPS_H__

#include <cstddef>
#include <utility>

#include "common/flat_map.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

// Manages the hierarchy of capture groups.
//
// This class allows enumerating the path to the root of a given capture group efficiently. For
// example, given the expression `()((())(()()))` and the capture group #5 (assuming capture group
// numbers are zero-based), this class allows enumerating the path 5, 4, 1.
class CaptureGroups {
 public:
  // Allows iterating over the path to the root of a given capture group.
  class Iterator {
   private:
    auto Tie() const { return std::tie(parent_, capture_group_); }

   public:
    // Constructs an empty iterator.
    explicit Iterator() : parent_(nullptr), capture_group_(-1) {}

    Iterator(Iterator const &) = default;
    Iterator &operator=(Iterator const &) = default;
    Iterator(Iterator &&) noexcept = default;
    Iterator &operator=(Iterator &&) noexcept = default;

    int operator*() const { return capture_group_; }
    int const *operator->() const { return &capture_group_; }

    friend bool operator==(Iterator const &lhs, Iterator const &rhs) {
      return lhs.Tie() == rhs.Tie();
    }

    friend bool operator!=(Iterator const &lhs, Iterator const &rhs) {
      return lhs.Tie() != rhs.Tie();
    }

    Iterator &operator++() {
      capture_group_ = parent_->map_.at(capture_group_);
      return *this;
    }

    Iterator operator++(int) {
      Iterator result = *this;
      capture_group_ = parent_->map_.at(capture_group_);
      return result;
    }

   private:
    friend class CaptureGroups;

    // Constructs an iterator referring to the specified node.
    explicit Iterator(CaptureGroups const &parent, int const capture_group)
        : parent_(&parent), capture_group_(capture_group) {}

    // Constructs the end iterator.
    explicit Iterator(CaptureGroups const &parent) : Iterator(parent, -1) {}

    CaptureGroups const *parent_;
    int capture_group_;
  };

  explicit CaptureGroups() = default;

  CaptureGroups(CaptureGroups const &) = default;
  CaptureGroups &operator=(CaptureGroups const &) = default;
  CaptureGroups(CaptureGroups &&) noexcept = default;
  CaptureGroups &operator=(CaptureGroups &&) noexcept = default;

  size_t size() const { return map_.size(); }

  // Adds a `capture_group` to the hierarchy as a child of `parent_capture_group`.
  //
  // If `parent_capture_group` is negative, `capture_group` is added as a root.
  //
  // REQUIRES: if positive, `parent_capture_group` must already be in the hierarchy.
  // REQUIRES: `capture_group` must be positive.
  void Add(int const capture_group, int const parent_capture_group) {
    map_.try_emplace(capture_group, parent_capture_group);
  }

  // Returns the end iterator, which can be used to test any other iterator. Example iteration:
  //
  //   for (auto it = cg.LookUp(123); it != cg.root(); ++it) {
  //     // ...
  //   }
  //
  Iterator root() const { return Iterator(*this); }

  // Looks up the specified capture group and returns an iterator that allows the caller to iterate
  // over the path to the root.
  //
  // If the specified capture group is not found or it's a negative number, the end iterator is
  // returned.
  Iterator LookUp(int const capture_group) const {
    if (capture_group < 0) {
      return Iterator(*this);
    }
    auto const it = map_.find(capture_group);
    if (it != map_.end()) {
      return Iterator(*this, it->first);
    } else {
      return Iterator(*this);
    }
  }

 private:
  // Keys are capture group numbers, values are their respective parent capture group numbers.
  tsdb2::common::flat_map<int, int> map_;
};

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_CAPTURE_GROUPS_H__
