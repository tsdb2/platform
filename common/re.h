#ifndef __TSDB2_COMMON_RE_H__
#define __TSDB2_COMMON_RE_H__

#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "common/re/automaton.h"
#include "common/reffed_ptr.h"
#include "common/utilities.h"

namespace tsdb2 {
namespace common {

// `RE` is the interface to TSDB2's own implementation of regular expressions.
//
// TSDB2 uses its own implementation rather than the one provided by STL or any other C++ regular
// expression library for at least two reasons:
//
//   1. We need an implementation that is guaranteed to be immune to ReDoS attacks. Our
//      implementation is immune because it doesn't provide any features that inherently require
//      backtracking algorithms (specifically we do not support backreferences).
//   2. We need an implementation that can be integrated with our tries so that we can run finite
//      state automata algorithms on tries, allowing for efficient retrieval of strings based on
//      regular expression patterns.
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

  // Checks if `input` matches `pattern` and returns an array of strings containing the strings
  // captured by capture groups. An error status is returned if the `pattern` fails to compile.
  static absl::StatusOr<std::vector<std::string>> Match(std::string_view input,
                                                        std::string_view pattern);

  // Compiles the provided `pattern` into a `RE` object that can be run efficiently multiple times.
  static absl::StatusOr<RE> Create(std::string_view pattern);

  RE(RE const &) = default;
  RE &operator=(RE const &) = default;
  RE(RE &&) noexcept = default;
  RE &operator=(RE &&) noexcept = default;

  // Checks if the provided `input` string matches this compiled regular expression.
  bool Test(std::string_view const input) const { return automaton_->Run(input); }

 private:
  explicit RE(reffed_ptr<regexp_internal::AutomatonInterface> automaton)
      : automaton_(std::move(automaton)) {}

  reffed_ptr<regexp_internal::AutomatonInterface> automaton_;
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_H__
