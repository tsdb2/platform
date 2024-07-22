#ifndef __TSDB2_COMMON_RE_H__
#define __TSDB2_COMMON_RE_H__

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/re/automaton.h"
#include "common/reffed_ptr.h"
#include "common/utilities.h"

namespace tsdb2 {
namespace common {

namespace internal {

// This forward declaration refers to the internal implementation of tries. We need it here so that
// we can declare `TrieNode` as a friend of `RE`, allowing integration between the two.
template <typename Label, typename Allocator>
class TrieNode;

}  // namespace internal

// `RE` is the interface to TSDB2's own implementation of regular expressions.
//
// TSDB2 uses its own implementation rather than the one provided by STL or any other C++ regular
// expression library for at least two reasons:
//
//   1. We need an implementation that is guaranteed to be immune to ReDoS attacks. Ours is immune
//      because it doesn't provide any NP-hard features (most notably we do not support
//      backreferences).
//   2. We need an implementation that can be integrated with our tries so that we can run finite
//      state automata algorithms on tries, allowing for efficient retrieval of strings based on
//      regular expression patterns.
//
// Our regular expressions are always anchored. If a user needs to do an unanchored match (i.e.
// search a substring) they need to surround their pattern with ".*". For example, ".*foo.*"
// searches the substring `foo` inside the provided string.
//
// TODO: describe the syntax.
//
class RE {
 public:
  // Checks if `input` matches `pattern`.
  //
  // This function doesn't allow the caller to tell if `pattern` fails to compile; in that case it
  // will simply return false. It's recommended to use this function only if the caller is 100% sure
  // the pattern compiles, e.g. it's hard-coded in the program rather than being provided by a
  // human.
  static bool Test(std::string_view input, std::string_view pattern);

  // Checks if `input` matches `pattern` and returns an array of the strings captured by the capture
  // groups. An error status is returned if `pattern` fails to compile or `input` doesn't match.
  static absl::StatusOr<std::vector<std::string>> Match(std::string_view input,
                                                        std::string_view pattern);

  // Strips the longest possible prefix matching `pattern` from the provided `input` string. Returns
  // true iff a prefix was matched and removed.
  static absl::StatusOr<std::vector<std::string>> ConsumePrefix(std::string_view *input,
                                                                std::string_view pattern);

  // Compiles the provided `pattern` into a `RE` object that can be run efficiently multiple times.
  static absl::StatusOr<RE> Create(std::string_view pattern);

  // Moves and copies are efficient because the inner automaton is immutable and is managed by means
  // of reference counting, so we never move or copy the whole automaton, just a pointer to it.
  //
  // For reference counting the automaton uses `SimpleRefCounted` which is lock-free and
  // thread-safe, so it's okay for different threads to keep `RE` objects referring to the same
  // automaton. It is however not okay for different threads to use the same `RE` object without any
  // synchronization. Only the internal automaton is thread-safe, `RE` is not.

  RE(RE const &) = default;
  RE &operator=(RE const &) = default;
  RE(RE &&) noexcept = default;
  RE &operator=(RE &&) noexcept = default;

  // Indicates whether the underlying automaton is deterministic.
  bool IsDeterministic() const { return automaton_->IsDeterministic(); }

  // Returns the number of capture groups in the regular expression. That is also the size of the
  // vector returned by `Match` for a matching input string.
  size_t GetNumCaptureGroups() const { return automaton_->GetNumCaptureGroups(); }

  // Checks if the provided `input` string matches this compiled regular expression.
  bool Test(std::string_view const input) const { return automaton_->Test(input); }

  // Checks if `input` matches this regular expression and returns an array of the strings captured
  // by the capture groups. An empty optional is returned if `input` doesn't match.
  std::optional<std::vector<std::string>> Match(std::string_view const input) const {
    return automaton_->Match(input);
  }

  // Matches the longest possible prefix of the provided string against this regular expression.
  // Returns the array of strings captured by any capture groups, or an empty optional if no
  // matching prefix is found.
  std::optional<std::vector<std::string>> MatchPrefix(std::string_view const input) const {
    return automaton_->MatchPrefix(input);
  }

 private:
  template <typename Label, typename Allocator>
  friend class internal::TrieNode;

  explicit RE(reffed_ptr<regexp_internal::AutomatonInterface> automaton)
      : automaton_(std::move(automaton)) {}

  reffed_ptr<regexp_internal::AutomatonInterface> automaton_;
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_H__
