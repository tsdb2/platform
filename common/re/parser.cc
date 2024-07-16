#include "common/re/parser.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

#include "absl/container/inlined_vector.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "common/re/automaton.h"
#include "common/re/capture_groups.h"
#include "common/re/temp.h"
#include "common/reffed_ptr.h"
#include "common/utilities.h"

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
  explicit Parser(std::string_view const pattern) : pattern_(pattern) {}

  // Parses the pattern provided at construction and returns a runnable automaton. The automaton is
  // initially an `NFA` but it's automatically converted to a `DFA` if it's found to be
  // deterministic. We do that because DFAs run faster.
  absl::StatusOr<reffed_ptr<AutomatonInterface>> Parse() &&;

 private:
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

  // If the pattern contains the provided `prefix` string at the current position then advance the
  // current position by the number of characters in `prefix` and return an OK status, otherwise do
  // nothing and return a syntax error with the provided `error_message`.
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

  // Parses a hex digit. Used by `ParseHexCode` to parse hex escape codes.
  absl::StatusOr<uint8_t> ParseHexDigit(char ch);

  // Parses the next two characters as a hex byte. Used to parse hex escape codes.
  absl::StatusOr<uint8_t> ParseHexCode();

  TempNFA MakeSingleCharacterNFA(int capture_group, char ch);
  TempNFA MakeCharacterClassNFA(int capture_group, std::string_view chars);
  TempNFA MakeNegatedCharacterClassNFA(int capture_group, std::string_view chars);

  static absl::Status UpdateCharacterClassEdge(bool negated, State* start_state, char ch,
                                               size_t stop_state_num);

  // Called by `ParseCharacterClass` to parse escape codes. `negated` indicates whether the
  // character class is negated (i.e. it begins with ^). `state` is the NFA state to update with the
  // characters resulting from the escape code. Returns an error status if the escape code is
  // invalid.
  //
  // REQUIRES: the backslash before the escape code must have already been consumed.
  absl::Status ParseCharacterClassEscapeCode(bool negated, State* start_state,
                                             size_t stop_state_num);

  // Called by `Parse0` to parse character classes (i.e. square brackets).
  absl::StatusOr<TempNFA> ParseCharacterClass(int capture_group);

  // Called by `Parse0` to parse escape codes (e.g. `\d`, `\w`, etc.).
  absl::StatusOr<TempNFA> ParseEscape(int capture_group);

  // Parses single character, escape code, dot, round brackets, square brackets, or end of input.
  absl::StatusOr<TempNFA> Parse0(int capture_group);

  // Parses the content of the curly braces in quantifiers.
  absl::StatusOr<std::pair<int, int>> ParseQuantifier();

  // Parses Kleene star, plus, question mark, or quantifier.
  absl::StatusOr<TempNFA> Parse1(int capture_group);

  // Parses sequences.
  absl::StatusOr<TempNFA> Parse2(int capture_group);

  // Parses the pipe operator.
  absl::StatusOr<TempNFA> Parse3(int capture_group);

  std::string_view const pattern_;
  size_t offset_ = 0;
  size_t next_state_ = 0;

  CaptureGroups capture_groups_;
  int next_capture_group_ = 0;
};

absl::StatusOr<reffed_ptr<AutomatonInterface>> Parser::Parse() && {
  ASSIGN_VAR_OR_RETURN(TempNFA, nfa, Parse3(-1));
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

TempNFA Parser::MakeSingleCharacterNFA(int const capture_group, char const ch) {
  size_t const start = next_state_++;
  size_t const stop = next_state_++;
  return TempNFA(
      {
          {start, State(capture_group, {{ch, {stop}}})},
          {stop, State(capture_group, {})},
      },
      start, stop);
}

TempNFA Parser::MakeCharacterClassNFA(int const capture_group, std::string_view const chars) {
  size_t const start = next_state_++;
  size_t const stop = next_state_++;
  State state{capture_group, {}};
  for (char const ch : chars) {
    state.edges[ch].emplace(stop);
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
  size_t const start = next_state_++;
  size_t const stop = next_state_++;
  State state{capture_group, {}};
  for (int ch = 1; ch < 256; ++ch) {
    state.edges[ch].emplace(stop);
  }
  for (char const ch : chars) {
    state.edges.erase(ch);
  }
  return TempNFA(
      {
          {start, std::move(state)},
          {stop, State(capture_group, {})},
      },
      start, stop);
}

absl::Status Parser::UpdateCharacterClassEdge(bool const negated, State* const start_state,
                                              char const ch, size_t const stop_state_num) {
  if (negated) {
    start_state->edges.erase(ch);
  } else {
    start_state->edges[ch].emplace(stop_state_num);
  }
  return absl::OkStatus();
}

absl::Status Parser::ParseCharacterClassEscapeCode(bool const negated, State* const start_state,
                                                   size_t const stop_state_num) {
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
      return UpdateCharacterClassEdge(negated, start_state, ch, stop_state_num);
    case 't':
      return UpdateCharacterClassEdge(negated, start_state, '\t', stop_state_num);
    case 'r':
      return UpdateCharacterClassEdge(negated, start_state, '\r', stop_state_num);
    case 'n':
      return UpdateCharacterClassEdge(negated, start_state, '\n', stop_state_num);
    case 'v':
      return UpdateCharacterClassEdge(negated, start_state, '\v', stop_state_num);
    case 'f':
      return UpdateCharacterClassEdge(negated, start_state, '\f', stop_state_num);
    case 'b':
      return UpdateCharacterClassEdge(negated, start_state, '\b', stop_state_num);
    case 'x': {
      ASSIGN_VAR_OR_RETURN(uint8_t, code, ParseHexCode());
      return UpdateCharacterClassEdge(negated, start_state, code, stop_state_num);
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

absl::StatusOr<TempNFA> Parser::ParseCharacterClass(int const capture_group) {
  RETURN_IF_ERROR(ExpectPrefix("[", "expected ["));
  size_t const start = next_state_++;
  size_t const stop = next_state_++;
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
    if (ConsumePrefix("\\")) {
      RETURN_IF_ERROR(ParseCharacterClassEscapeCode(negated, &state, stop));
    } else {
      char const ch = Advance();
      // TODO: ranges.
      if (negated) {
        state.edges.erase(ch);
      } else {
        state.edges[ch].emplace(stop);
      }
    }
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
      return MakeCharacterClassNFA(capture_group, "\f\n\r\t\v");
    case 'S':
      // TODO: add Unicode spaces.
      return MakeNegatedCharacterClassNFA(capture_group, "\f\n\r\t\v");
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
      // TODO: handle word boundary (`\b`).
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

absl::StatusOr<TempNFA> Parser::Parse0(int const capture_group) {
  size_t const start = next_state_++;
  if (at_end()) {
    return TempNFA({{start, State(capture_group, {})}}, start, start);
  }
  if (ConsumePrefix("(")) {
    capture_groups_.Add(next_capture_group_, capture_group);
    ASSIGN_VAR_OR_RETURN(TempNFA, result, Parse3(next_capture_group_++));
    RETURN_IF_ERROR(ExpectPrefix(")", "unmatched parens"));
    return result;
  }
  size_t const stop = next_state_++;
  if (ConsumePrefix(".")) {
    State state{capture_group, {}};
    for (int ch = 1; ch < 256; ++ch) {
      state.edges.try_emplace(ch, Transitions{stop});
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
    case '$':
      return SyntaxError("anchors are not allowed");
    default:
      Advance();
      return TempNFA(
          {
              {start, State(capture_group, {{ch, {stop}}})},
              {stop, State(capture_group, {})},
          },
          start, stop);
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

absl::StatusOr<TempNFA> Parser::Parse1(int const capture_group) {
  ASSIGN_VAR_OR_RETURN(TempNFA, nfa, Parse0(capture_group));
  if (at_end()) {
    return nfa;
  }
  if (ConsumePrefix("*")) {
    nfa.RenameState(nfa.initial_state(), nfa.final_state());
  } else if (ConsumePrefix("+")) {
    nfa.AddEpsilonEdge(nfa.final_state(), nfa.initial_state());
  } else if (ConsumePrefix("?")) {
    nfa.AddEpsilonEdge(nfa.initial_state(), nfa.final_state());
  } else if (ConsumePrefix("{")) {
    auto const status_or_quantifier = ParseQuantifier();
    if (!status_or_quantifier.ok()) {
      return std::move(status_or_quantifier).status();
    }
    auto const [min, max] = status_or_quantifier.value();
    if (min < 0) {
      if (max >= 0) {
        return SyntaxError("invalid quantifier");
      }
      nfa.RenameState(nfa.initial_state(), nfa.final_state());
    } else {
      auto piece = std::move(nfa);
      size_t const start = next_state_++;
      nfa = TempNFA({{start, State(capture_group, {})}}, start, start);
      for (int i = 0; i < min; ++i) {
        piece.RenameAllStates(&next_state_);
        nfa.Chain(piece);
      }
      if (max < 0) {
        piece.RenameState(piece.initial_state(), piece.final_state());
        piece.RenameAllStates(&next_state_);
        nfa.Chain(std::move(piece));
      } else {
        if (max < min) {
          return SyntaxError("invalid quantifier");
        }
        piece.AddEpsilonEdge(piece.initial_state(), piece.final_state());
        for (int i = min; i < max; ++i) {
          piece.RenameAllStates(&next_state_);
          nfa.Chain(piece);
        }
      }
    }
  }
  return nfa;
}

absl::StatusOr<TempNFA> Parser::Parse2(int const capture_group) {
  ASSIGN_VAR_OR_RETURN(TempNFA, nfa, Parse1(capture_group));
  while (!at_end() && front() != '|' && front() != ')') {
    ASSIGN_VAR_OR_RETURN(TempNFA, next, Parse1(capture_group));
    nfa.Chain(std::move(next));
  }
  return nfa;
}

absl::StatusOr<TempNFA> Parser::Parse3(int const capture_group) {
  ASSIGN_VAR_OR_RETURN(TempNFA, nfa, Parse2(capture_group));
  while (!at_end() && front() != ')') {
    RETURN_IF_ERROR(ExpectPrefix("|", "expected pipe operator"));
    ASSIGN_VAR_OR_RETURN(TempNFA, next, Parse2(capture_group));
    size_t const initial_state = next_state_++;
    size_t const final_state = next_state_++;
    nfa.Merge(std::move(next), capture_group, initial_state, final_state);
  }
  return nfa;
}

}  // namespace

absl::StatusOr<reffed_ptr<AutomatonInterface>> Parse(std::string_view const pattern) {
  return Parser(pattern).Parse();
}

}  // namespace regexp_internal
}  // namespace common
}  // namespace tsdb2
