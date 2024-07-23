#ifndef __TSDB2_COMMON_RE_PARSER_H__
#define __TSDB2_COMMON_RE_PARSER_H__

#include <string_view>

#include "absl/status/statusor.h"
#include "common/re/automaton.h"
#include "common/reffed_ptr.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

// Parses a regular expression and compiles it into a runnable automaton. The automaton is initially
// an `NFA` but it's automatically converted to a `DFA` if it's found to be deterministic. That is
// because DFAs run faster.
absl::StatusOr<reffed_ptr<AbstractAutomaton>> Parse(std::string_view pattern);

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_RE_PARSER_H__
