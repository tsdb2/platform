#include "common/re/parser.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/flat_set.h"
#include "common/re/automaton.h"
#include "common/re/capture_groups.h"
#include "common/re/temp.h"
#include "common/reffed_ptr.h"
#include "common/utilities.h"

ABSL_FLAG(size_t, re_max_recursion_depth, 1000,
          "Maximum recursion depth of the regular expression parser.");

namespace tsdb2 {
namespace common {
namespace regexp_internal {

namespace {

// Parses a regular expression and compiles it into a runnable automaton.
class Parser final {
 public:
  // Maximum number of repetitions for quantified expression, e.g. `(abc){42}`. We need to prevent
  // patterns coming from untrusted sources from creating large automata with small inputs, e.g.
  // `(abc){1000000000}`, as that would expose us to DoS attacks. The Kleene star is not affected by
  // this limit, so a sub-expression can repeat either an infinite number of times or a number of
  // times that's no larger than 1000.
  static inline int constexpr kMaxNumericQuantifier = 1000;

  // Constructs a parser to parse the provided regular expression `pattern`.
  explicit Parser(std::string_view const pattern, Options const& options)
      // TODO: when module initializers are available, add one to check if the
      // --re_max_recursion_depth flag is available (we might be running before command line flags
      // have been parsed). If not, manually fall back to the default value.
      : options_(options),
        max_recursion_depth_(absl::GetFlag(FLAGS_re_max_recursion_depth)),
        pattern_(pattern) {}

  // Parses the pattern provided at construction and returns a runnable automaton. The automaton is
  // initially an `NFA` but it's automatically converted to a `DFA` if it's found to be
  // deterministic. We do that because DFAs are faster.
  absl::StatusOr<reffed_ptr<AbstractAutomaton>> Parse() &&;

 private:
  static absl::Status MaxRecursionDepthExceededError() {
    return absl::ResourceExhaustedError("max recursion depth exceeded");
  }

  // Returns the number of pattern characters not yet consumed by `Advance` or `ConsumePrefix`.
  size_t characters_left() const { return pattern_.size() - offset_; }

  // Indicates whether the parser has reached the end of the input pattern. Equivalent to
  // `characters_left() == 0`.
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

  // If the pattern contains the specified `prefix` at the current position, advance the current
  // position by consuming the characters of `prefix` and return true; otherwise do nothing and
  // return false.
  bool ConsumePrefix(std::string_view const prefix) {
    if (absl::StartsWith(pattern_.substr(offset_), prefix)) {
      offset_ += prefix.size();
      return true;
    } else {
      return false;
    }
  }

  // If the pattern contains the specified `prefix` at the current position, advance the current
  // position by consuming the characters of `prefix` and return an OK status; otherwise do nothing
  // and return a syntax error with the provided `error_message`.
  absl::Status ExpectPrefix(std::string_view const prefix, std::string_view const error_message) {
    if (absl::StartsWith(pattern_.substr(offset_), prefix)) {
      offset_ += prefix.size();
      return absl::OkStatus();
    } else {
      return SyntaxError(error_message);
    }
  }

  // Builds an `absl::Status` for a syntax error found at the current position of the input pattern.
  absl::Status SyntaxError(std::string_view message) const {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid syntax in regular expression pattern \"", absl::CEscape(pattern_),
                     "\" at position ", offset_, ": ", message));
  }

  absl::Status NoAnchorsError() const { return SyntaxError("anchors are not allowed here"); }

  // Parses a hex digit. Used by `ParseHexCode` to parse hex escape codes.
  absl::StatusOr<uint8_t> ParseHexDigit(char ch);

  // Parses the next two characters as a hex byte. Used to parse hex escape codes.
  absl::StatusOr<uint8_t> ParseHexCode();

  // Adds an edge from `state` to `destination` labeled with `ch`, subject to case sensitivity: if
  // case-insensitive mode is enabled it adds two edges, one labeled with `std::toupper(ch)` and one
  // with `std::tolower(ch)` (which may be the same); otherwise it adds only one.
  void AddEdgeWithCase(State* state, char ch, uint32_t destination) const;

  // Erases all edges labeled with `ch` (subject to case sensitivity) from `state` to `destination`.
  // If case-insensitive mode is enabled it erases both the edges labeled with `std::toupper(ch)`
  // and the ones with `std::tolower(ch)` (which may be the same); otherwise it erases only those
  // labeled with `ch`.
  void EraseEdgeWithCase(State* state, char ch) const;

  TempNFA MakeSingleCharacterNFA(int capture_group, char ch);
  TempNFA MakeCharacterClassNFA(int capture_group, std::string_view chars);
  TempNFA MakeNegatedCharacterClassNFA(int capture_group, std::string_view chars);
  TempNFA MakeAssertionState(int capture_group, Assertions assertions);

  // Called by `ParseCharacterClass` to parse a single, possibly escaped character.
  absl::StatusOr<char> ParseCharacterClassElement();

  absl::Status ParseCharacterOrRange(bool negated, State* start_state, uint32_t stop_state_num);

  // Called by `Parse0` to parse character classes (i.e. square brackets).
  absl::StatusOr<TempNFA> ParseCharacterClass(int capture_group);

  // Called by `Parse0` to parse escape codes (e.g. `\d`, `\w`, etc.).
  absl::StatusOr<TempNFA> ParseEscape(int capture_group);

  // Parses single character, escape code, dot, round brackets, square brackets, or end of input.
  absl::StatusOr<TempNFA> Parse0(size_t recursion_depth, int capture_group);

  // Parses the content of the curly braces in quantifiers.
  absl::StatusOr<std::pair<int, int>> ParseQuantifier();

  // Parses Kleene star, plus, question mark, or quantifier.
  absl::StatusOr<TempNFA> Parse1(size_t recursion_depth, int capture_group);

  // Parses sequences.
  absl::StatusOr<TempNFA> Parse2(size_t recursion_depth, int capture_group);

  // Parses the pipe operator.
  absl::StatusOr<TempNFA> Parse3(size_t recursion_depth, int capture_group);

  Options const options_;
  size_t const max_recursion_depth_;

  std::string_view const pattern_;
  size_t offset_ = 0;
  uint32_t next_state_ = 0;

  CaptureGroups capture_groups_;
  int next_capture_group_ = 0;
};

absl::StatusOr<reffed_ptr<AbstractAutomaton>> Parser::Parse() && {
  ASSIGN_VAR_OR_RETURN(TempNFA, nfa, Parse3(1, -1));
  if (!at_end()) {
    return SyntaxError("expected end of string");
  }
  return std::move(nfa).Finalize(std::move(capture_groups_));
}

absl::StatusOr<uint8_t> Parser::ParseHexDigit(char const ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  } else if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  } else if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  } else {
    return SyntaxError("invalid hex digit");
  }
}

absl::StatusOr<uint8_t> Parser::ParseHexCode() {
  if (characters_left() < 2) {
    return SyntaxError("invalid escape code");
  }
  ASSIGN_VAR_OR_RETURN(int, digit1, ParseHexDigit(Advance()));
  ASSIGN_VAR_OR_RETURN(int, digit2, ParseHexDigit(Advance()));
  return digit1 * 16 + digit2;
}

void Parser::AddEdgeWithCase(State* const state, char const ch, uint32_t const destination) const {
  if (options_.case_sensitive) {
    state->edges[ch].emplace(destination);
  } else {
    state->edges[std::toupper(ch)].emplace(destination);
    state->edges[std::tolower(ch)].emplace(destination);
  }
}

void Parser::EraseEdgeWithCase(State* const state, char const ch) const {
  if (options_.case_sensitive) {
    state->edges.erase(ch);
  } else {
    state->edges.erase(std::toupper(ch));
    state->edges.erase(std::tolower(ch));
  }
}

TempNFA Parser::MakeSingleCharacterNFA(int const capture_group, char const ch) {
  uint32_t const start = next_state_++;
  uint32_t const stop = next_state_++;
  State state{capture_group, {}};
  AddEdgeWithCase(&state, ch, stop);
  return TempNFA(
      {
          {start, std::move(state)},
          {stop, State(capture_group, {})},
      },
      start, stop);
}

TempNFA Parser::MakeCharacterClassNFA(int const capture_group, std::string_view const chars) {
  uint32_t const start = next_state_++;
  uint32_t const stop = next_state_++;
  State state{capture_group, {}};
  for (char const ch : chars) {
    AddEdgeWithCase(&state, ch, stop);
  }
  return TempNFA(
      {
          {start, std::move(state)},
          {stop, State(capture_group, {})},
      },
      start, stop);
}

TempNFA Parser::MakeNegatedCharacterClassNFA(int const capture_group,
                                             std::string_view const chars) {
  uint32_t const start = next_state_++;
  uint32_t const stop = next_state_++;
  State state{capture_group, {}};
  for (int ch = 1; ch < 256; ++ch) {
    state.edges[ch].emplace(stop);
  }
  for (char const ch : chars) {
    EraseEdgeWithCase(&state, ch);
  }
  return TempNFA(
      {
          {start, std::move(state)},
          {stop, State(capture_group, {})},
      },
      start, stop);
}

TempNFA Parser::MakeAssertionState(int const capture_group, Assertions const assertions) {
  uint32_t const state = next_state_++;
  return TempNFA({{state, State(capture_group, assertions, {})}}, state, state);
}

absl::StatusOr<char> Parser::ParseCharacterClassElement() {
  if (!ConsumePrefix("\\")) {
    return Advance();
  }
  if (at_end()) {
    return SyntaxError("invalid escape code");
  }
  char const ch = Advance();
  switch (ch) {
    case '\\':
    case '^':
    case '$':
    case '.':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '|':
    case '?':
    case '*':
    case '+':
      return ch;
    case 't':
      return '\t';
    case 'r':
      return '\r';
    case 'n':
      return '\n';
    case 'v':
      return '\v';
    case 'f':
      return '\f';
    case 'x': {
      ASSIGN_VAR_OR_RETURN(uint8_t, code, ParseHexCode());
      return code;
    }
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return SyntaxError("backreferences are not supported");
    default:
      return SyntaxError("invalid escape code");
  }
}

absl::Status Parser::ParseCharacterOrRange(bool const negated, State* const start_state,
                                           uint32_t const stop_state_num) {
  DEFINE_CONST_OR_RETURN(ch1, ParseCharacterClassElement());
  if (ConsumePrefix("-")) {
    if (at_end()) {
      return SyntaxError("unmatched square bracket");
    }
    if (front() != ']') {
      DEFINE_CONST_OR_RETURN(ch2, ParseCharacterClassElement());
      uint8_t const uch1 = ch1;
      uint8_t const uch2 = ch2;
      if (uch2 <= uch1) {
        return SyntaxError(
            "the right-hand side of a character range must be greater than the left-hand side");
      }
      for (int ch = uch1; ch <= uch2; ++ch) {
        if (negated) {
          EraseEdgeWithCase(start_state, ch);
        } else {
          AddEdgeWithCase(start_state, ch, stop_state_num);
        }
      }
    } else {
      if (negated) {
        EraseEdgeWithCase(start_state, ch1);
        start_state->edges.erase('-');
      } else {
        AddEdgeWithCase(start_state, ch1, stop_state_num);
        start_state->edges['-'].emplace(stop_state_num);
      }
    }
  } else {
    if (negated) {
      EraseEdgeWithCase(start_state, ch1);
    } else {
      AddEdgeWithCase(start_state, ch1, stop_state_num);
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<TempNFA> Parser::ParseCharacterClass(int const capture_group) {
  RETURN_IF_ERROR(ExpectPrefix("[", "expected ["));
  uint32_t const start = next_state_++;
  uint32_t const stop = next_state_++;
  State state{capture_group, {}};
  bool const negated = ConsumePrefix("^");
  if (negated) {
    for (int ch = 1; ch < 256; ++ch) {
      state.edges[ch].emplace(stop);
    }
  }
  while (!ConsumePrefix("]")) {
    if (at_end()) {
      return SyntaxError("unmatched square bracket");
    }
    RETURN_IF_ERROR(ParseCharacterOrRange(negated, &state, stop));
  }
  return TempNFA(
      {
          {start, std::move(state)},
          {stop, State(capture_group, {})},
      },
      start, stop);
}

absl::StatusOr<TempNFA> Parser::ParseEscape(int const capture_group) {
  RETURN_IF_ERROR(ExpectPrefix("\\", "expected \\"));
  if (at_end()) {
    return SyntaxError("invalid escape code");
  }
  char const ch = Advance();
  switch (ch) {
    case '\\':
    case '^':
    case '$':
    case '.':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '|':
    case '?':
    case '*':
    case '+':
      return MakeSingleCharacterNFA(capture_group, ch);
    case 'd':
      return MakeCharacterClassNFA(capture_group, "0123456789");
    case 'D':
      return MakeNegatedCharacterClassNFA(capture_group, "0123456789");
    case 'w':
      return MakeCharacterClassNFA(
          capture_group, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
    case 'W':
      return MakeNegatedCharacterClassNFA(
          capture_group, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
    case 's':
      // TODO: add Unicode spaces.
      return MakeCharacterClassNFA(capture_group, " \f\n\r\t\v");
    case 'S':
      // TODO: add Unicode spaces.
      return MakeNegatedCharacterClassNFA(capture_group, " \f\n\r\t\v");
    case 't':
      return MakeSingleCharacterNFA(capture_group, '\t');
    case 'r':
      return MakeSingleCharacterNFA(capture_group, '\r');
    case 'n':
      return MakeSingleCharacterNFA(capture_group, '\n');
    case 'v':
      return MakeSingleCharacterNFA(capture_group, '\v');
    case 'f':
      return MakeSingleCharacterNFA(capture_group, '\f');
    case 'b':
      return MakeAssertionState(capture_group, Assertions::kWordBoundary);
    case 'B':
      return MakeAssertionState(capture_group, Assertions::kNotWordBoundary);
    case 'x': {
      ASSIGN_VAR_OR_RETURN(uint8_t, code, ParseHexCode());
      return MakeSingleCharacterNFA(capture_group, code);
    }
    // TODO: handle Unicode escape codes.
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return SyntaxError("backreferences are not supported");
    default:
      return SyntaxError("invalid escape code");
  }
}

absl::StatusOr<TempNFA> Parser::Parse0(size_t const recursion_depth, int const capture_group) {
  if (recursion_depth > max_recursion_depth_) {
    return MaxRecursionDepthExceededError();
  }
  uint32_t const start = next_state_++;
  if (at_end()) {
    return TempNFA({{start, State(capture_group, {})}}, start, start);
  }
  if (ConsumePrefix("(")) {
    if (ConsumePrefix("?")) {
      if (!ConsumePrefix(":")) {
        return SyntaxError("invalid non-capturing brackets");
      }
      ASSIGN_VAR_OR_RETURN(TempNFA, result, Parse3(recursion_depth + 1, capture_group));
      RETURN_IF_ERROR(ExpectPrefix(")", "unmatched parens"));
      return result;
    } else {
      capture_groups_.Add(next_capture_group_, capture_group);
      ASSIGN_VAR_OR_RETURN(TempNFA, nfa, Parse3(recursion_depth + 1, next_capture_group_++));
      RETURN_IF_ERROR(ExpectPrefix(")", "unmatched parens"));
      uint32_t const start = next_state_++;
      uint32_t const stop = next_state_++;
      return TempNFA({{start, State(capture_group, {})}}, start, start)
          .Chain(std::move(nfa))
          .Chain(TempNFA({{stop, State(capture_group, {})}}, stop, stop));
    }
  }
  uint32_t const stop = next_state_++;
  if (ConsumePrefix(".")) {
    State state{capture_group, {}};
    for (int ch = 1; ch < 256; ++ch) {
      state.edges.try_emplace(ch, StateSet{stop});
    }
    return TempNFA(
        {
            {start, State(std::move(state))},
            {stop, State(capture_group, {})},
        },
        start, stop);
  }
  char const ch = front();
  switch (ch) {
    case ')':
    case '|':
      return TempNFA({{start, State(capture_group, {})}}, start, start);
    case '[':
      return ParseCharacterClass(capture_group);
    case ']':
      return SyntaxError("unmatched square bracket");
    case '{':
    case '}':
      return SyntaxError("curly brackets in invalid position");
    case '\\':
      return ParseEscape(capture_group);
    case '*':
    case '+':
      return SyntaxError("Kleene operator in invalid position");
    case '?':
      return SyntaxError("question mark operator in invalid position");
    case '^':
      if (options_.no_anchors) {
        return NoAnchorsError();
      } else {
        Advance();
        return MakeAssertionState(capture_group, Assertions::kBegin);
      }
    case '$':
      if (options_.no_anchors) {
        return NoAnchorsError();
      } else {
        Advance();
        return MakeAssertionState(capture_group, Assertions::kEnd);
      }
    default:
      Advance();
      return MakeSingleCharacterNFA(capture_group, ch);
  }
}

absl::StatusOr<std::pair<int, int>> Parser::ParseQuantifier() {
  if (ConsumePrefix("}")) {
    return std::make_pair(-1, -1);
  }
  if (at_end() || !absl::ascii_isdigit(front())) {
    return SyntaxError("invalid quantifier");
  }
  char ch = Advance();
  int min = ch - '0';
  if (ch != 0) {
    while (!at_end() && absl::ascii_isdigit(front())) {
      min = min * 10 + (Advance() - '0');
      if (min > kMaxNumericQuantifier) {
        return SyntaxError("numeric quantifiers greater than 1000 are not supported");
      }
    }
  }
  if (ConsumePrefix("}")) {
    return std::make_pair(min, min);
  }
  RETURN_IF_ERROR(ExpectPrefix(",", "invalid quantifier"));
  if (ConsumePrefix("}")) {
    return std::make_pair(min, -1);
  }
  if (at_end() || !absl::ascii_isdigit(front())) {
    return absl::InvalidArgumentError("invalid quantifier");
  }
  ch = Advance();
  int max = ch - '0';
  if (ch != 0) {
    while (!at_end() && absl::ascii_isdigit(front())) {
      max = max * 10 + (Advance() - '0');
      if (max > kMaxNumericQuantifier) {
        return SyntaxError("numeric quantifiers greater than 1000 are not supported");
      }
    }
  }
  if (ConsumePrefix("}")) {
    return std::make_pair(min, max);
  } else {
    return SyntaxError("invalid quantifier");
  }
}

absl::StatusOr<TempNFA> Parser::Parse1(size_t const recursion_depth, int const capture_group) {
  if (recursion_depth > max_recursion_depth_) {
    return MaxRecursionDepthExceededError();
  }
  ASSIGN_VAR_OR_RETURN(TempNFA, nfa, Parse0(recursion_depth + 1, capture_group));
  if (at_end()) {
    return nfa;
  }
  if (ConsumePrefix("*")) {
    if (!nfa.RenameState(nfa.initial_state(), nfa.final_state())) {
      nfa.MaybeAddEpsilonEdge(nfa.initial_state(), nfa.final_state());
      nfa.MaybeAddEpsilonEdge(nfa.final_state(), nfa.initial_state());
    }
  } else if (ConsumePrefix("+")) {
    nfa.MaybeAddEpsilonEdge(nfa.final_state(), nfa.initial_state());
  } else if (ConsumePrefix("?")) {
    nfa.MaybeAddEpsilonEdge(nfa.initial_state(), nfa.final_state());
  } else if (ConsumePrefix("{")) {
    auto status_or_quantifier = ParseQuantifier();
    if (!status_or_quantifier.ok()) {
      return std::move(status_or_quantifier).status();
    }
    auto const [min, max] = status_or_quantifier.value();
    if (min < 0) {
      if (max >= 0) {
        return SyntaxError("invalid quantifier");
      }
      if (!nfa.RenameState(nfa.initial_state(), nfa.final_state())) {
        nfa.MaybeAddEpsilonEdge(nfa.final_state(), nfa.initial_state());
      }
    } else {
      auto piece = std::move(nfa);
      uint32_t const start = next_state_++;
      nfa = TempNFA({{start, State(capture_group, {})}}, start, start);
      for (int i = 0; i < min; ++i) {
        piece.RenameAllStates(&next_state_);
        nfa.Chain(piece);
      }
      if (max < 0) {
        if (!piece.RenameState(piece.initial_state(), piece.final_state())) {
          piece.MaybeAddEpsilonEdge(piece.final_state(), piece.initial_state());
        }
        piece.RenameAllStates(&next_state_);
        nfa.Chain(std::move(piece));
      } else {
        if (max < min) {
          return SyntaxError("invalid quantifier");
        }
        piece.MaybeAddEpsilonEdge(piece.initial_state(), piece.final_state());
        for (int i = min; i < max; ++i) {
          piece.RenameAllStates(&next_state_);
          nfa.Chain(piece);
        }
      }
    }
  }
  return nfa;
}

absl::StatusOr<TempNFA> Parser::Parse2(size_t const recursion_depth, int const capture_group) {
  if (recursion_depth > max_recursion_depth_) {
    return MaxRecursionDepthExceededError();
  }
  ASSIGN_VAR_OR_RETURN(TempNFA, nfa, Parse1(recursion_depth + 1, capture_group));
  while (!at_end() && front() != '|' && front() != ')') {
    ASSIGN_VAR_OR_RETURN(TempNFA, next, Parse1(recursion_depth + 1, capture_group));
    nfa.Chain(std::move(next));
  }
  return nfa;
}

absl::StatusOr<TempNFA> Parser::Parse3(size_t const recursion_depth, int const capture_group) {
  if (recursion_depth > max_recursion_depth_) {
    return MaxRecursionDepthExceededError();
  }
  ASSIGN_VAR_OR_RETURN(TempNFA, nfa, Parse2(recursion_depth + 1, capture_group));
  while (!at_end() && front() != ')') {
    RETURN_IF_ERROR(ExpectPrefix("|", "expected pipe operator"));
    ASSIGN_VAR_OR_RETURN(TempNFA, next, Parse2(recursion_depth + 1, capture_group));
    uint32_t const initial_state = next_state_++;
    uint32_t const final_state = next_state_++;
    nfa.Merge(std::move(next), capture_group, initial_state, final_state);
  }
  return nfa;
}

}  // namespace

absl::StatusOr<reffed_ptr<AbstractAutomaton>> Parse(std::string_view const pattern,
                                                    Options const& options) {
  return Parser(pattern, options).Parse();
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
