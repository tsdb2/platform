#include "common/re/parser.h"

#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "common/re/automaton.h"
#include "common/re/temp.h"

namespace tsdb2 {
namespace common {
namespace regexp_internal {

namespace {

// Parses a regular expression and compiles it into a runnable automaton.
class Parser final {
 public:
  // Maximum number of repetitions for quantified expression, e.g. `(abc){42}`. We need to prevent
  // patterns coming from untrusted sources from creating large automata with small inputs, e.g.
  // `(abc){1000000000}`, as that would exposes us to DoS attacks. The Kleene star is not affected
  // by this limit, so a sub-expression can repeat either an infinite number of times or a number of
  // times that's no larger than 1000.
  static inline int constexpr kMaxNumericQuantifier = 1000;

  // Constructs a parser to parse the provided regular expression `pattern`.
  explicit Parser(std::string_view const pattern) : pattern_(pattern) {}

  // Parses the pattern provided at construction and returns a runnable automaton. The automaton is
  // initially an `NFA` but it's automatically converted to a `DFA` if it's found to be
  // deterministic. We do that because DFAs run faster.
  absl::StatusOr<std::unique_ptr<AutomatonInterface>> Parse();

 private:
  // Indicates whether the parser has reached the end of the input pattern.
  bool at_end() const { return offset_ >= pattern_.size(); }

  // Returns the next character of the input pattern. Undefined behavior if `at_end` returns true.
  char front() const { return pattern_[offset_]; }

  // Reads the next character, advances the current position by 1, and returns the character.
  // Undefined behavior if `at_end` returns true.
  char Advance() {
    char const ch = front();
    ++offset_;
    return ch;
  }

  // If the pattern contains the provided `prefix` string at the current position then advance the
  // current position by the number of characters in `prefix` and return true, otherwise do nothing
  // and return false.
  bool ConsumePrefix(std::string_view const prefix) {
    if (absl::StartsWith(pattern_.substr(offset_), prefix)) {
      offset_ += prefix.size();
      return true;
    } else {
      return false;
    }
  }

  // Builds an `absl::Status` for a syntax error found at the current position of the input pattern.
  absl::Status InvalidSyntaxError(std::string_view message) const {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid syntax in regular expression pattern \"", absl::CEscape(pattern_),
                     "\" at position ", offset_, ": ", message));
  }

  // Parses single character, escape code, dot, round brackets, square brackets, or end of input.
  absl::StatusOr<TempNFA> Parse0();

  // Parses Kleene star, plus, question mark, or quantifier.
  absl::StatusOr<TempNFA> Parse1();

  // Parses sequences.
  absl::StatusOr<TempNFA> Parse2();

  // Parses the pipe operator.
  absl::StatusOr<TempNFA> Parse3();

  std::string_view const pattern_;
  size_t offset_ = 0;
  size_t next_state_ = 0;
};

absl::StatusOr<std::unique_ptr<AutomatonInterface>> Parser::Parse() {
  auto status_or_nfa = Parse3();
  if (!status_or_nfa.ok()) {
    return std::move(status_or_nfa).status();
  }
  if (!at_end()) {
    return InvalidSyntaxError("expected end of string");
  }
  return std::move(status_or_nfa).value().Finalize();
}

absl::StatusOr<TempNFA> Parser::Parse3() {
  auto status_or_nfa = Parse2();
  if (!status_or_nfa.ok()) {
    return status_or_nfa;
  }
  auto nfa = std::move(status_or_nfa).value();
  while (!at_end() && front() != ')') {
    if (!ConsumePrefix("|")) {
      return InvalidSyntaxError("expected pipe operator");
    }
    status_or_nfa = Parse2();
    if (!status_or_nfa.ok()) {
      return status_or_nfa;
    }
    size_t const initial_state = next_state_++;
    size_t const final_state = next_state_++;
    nfa.Merge(std::move(status_or_nfa).value(), initial_state, final_state);
  }
  return nfa;
}

}  // namespace

absl::StatusOr<std::unique_ptr<AutomatonInterface>> Parse(std::string_view const pattern) {
  return Parser(pattern).Parse();
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
