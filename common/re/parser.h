#ifndef __TSDB2_COMMON_RE_PARSER_H__
#define __TSDB2_COMMON_RE_PARSER_H__

#include <cstddef>
#include <string_view>

#include "absl/flags/declare.h"
#include "absl/status/statusor.h"
#include "common/re/automaton.h"
#include "common/reffed_ptr.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

// Regular expression options. This is part of the public API in re.h.
struct Options {
  // Disallows anchors (^ and $). Typically used in full matches, i.e. if you plan to always call
  // `Match(Args)` rather than `PartialMatch(Args)`.
  bool no_anchors = false;

  // Case-sensitive mode, e.g. `hello` only matches `hello` and not `HELLO`, `Hello`, `HeLlO`, etc.
  bool case_sensitive = true;
};

// Parses a regular expression and compiles it into a runnable automaton. The automaton is initially
// an `NFA` but it's automatically converted to a `DFA` if it's found to be deterministic. We do
// that because DFAs are faster.
absl::StatusOr<reffed_ptr<AbstractAutomaton>> Parse(std::string_view pattern,
                                                    Options const& options = {});

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

// Exposed for testing only.
ABSL_DECLARE_FLAG(size_t, re_max_recursion_depth);

#endif  // __TSDB2_COMMON_RE_PARSER_H__
