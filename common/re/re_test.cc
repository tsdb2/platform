#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "common/flag_override.h"
#include "common/re/automaton.h"
#include "common/re/dfa.h"
#include "common/re/nfa.h"
#include "common/re/parser.h"
#include "common/re/temp.h"
#include "common/reffed_ptr.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Values;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;
using ::tsdb2::common::reffed_ptr;
using ::tsdb2::common::regexp_internal::AbstractAutomaton;
using ::tsdb2::common::regexp_internal::DFA;
using ::tsdb2::common::regexp_internal::NFA;
using ::tsdb2::common::regexp_internal::Parse;
using ::tsdb2::common::regexp_internal::TempNFA;
using ::tsdb2::common::testing::FlagOverride;

template <typename StringList>
std::string PrintStringList(StringList const& string_list) {
  std::vector<std::string> quoted_strings;
  quoted_strings.reserve(string_list.size());
  for (auto const& string : string_list) {
    quoted_strings.emplace_back(absl::StrCat("\"", string, "\""));
  }
  return absl::StrCat("{", absl::StrJoin(quoted_strings, ", "), "}");
}

template <typename CaptureSet>
std::string PrintCaptures(CaptureSet const& captures) {
  std::vector<std::string> entries;
  entries.reserve(captures.size());
  for (auto const& strings : captures) {
    entries.emplace_back(PrintStringList(strings));
  }
  return absl::StrCat("{", absl::StrJoin(entries, ", "), "}");
}

bool TestPrefixWithStepper(reffed_ptr<AbstractAutomaton> const& automaton,
                           std::string_view const input) {
  auto const stepper = automaton->MakeStepper();
  if (stepper->Finish()) {
    return true;
  }
  for (char const ch : input) {
    if (!stepper->Step(ch)) {
      return false;
    } else if (stepper->Finish()) {
      return true;
    }
  }
  return false;
}

bool CheckMatchResults(AbstractAutomaton::CaptureSet const& results,
                       std::vector<std::vector<std::string>> expected) {
  if (results.size() != expected.size()) {
    return false;
  }
  for (size_t i = 0; i < results.size(); ++i) {
    if (results[i].size() != expected[i].size()) {
      return false;
    }
    for (size_t j = 0; j < results[i].size(); ++j) {
      if (results[i][j] != expected[i][j]) {
        return false;
      }
    }
  }
  return true;
}

bool CheckMatchArgs(std::vector<std::string_view> const& args,
                    std::vector<std::vector<std::string>> expected) {
  for (size_t i = 0; i < expected.size(); ++i) {
    if (expected[i].empty()) {
      if (args[i] != "") {
        return false;
      }
    } else {
      if (args[i] != expected[i].back()) {
        return false;
      }
    }
  }
  return true;
}

class MatchesImpl {
 public:
  using is_gtest_matcher = void;

  explicit MatchesImpl(std::string_view const input, bool const use_stepper)
      : input_(input), captures_(std::nullopt), use_stepper_(use_stepper) {}

  MatchesImpl(MatchesImpl const&) = default;
  MatchesImpl& operator=(MatchesImpl const&) = default;
  MatchesImpl(MatchesImpl&&) noexcept = default;
  MatchesImpl& operator=(MatchesImpl&&) noexcept = default;

  MatchesImpl With(std::vector<std::vector<std::string>> captures) && {
    captures_ = std::move(captures);
    return *this;
  }

  void DescribeTo(std::ostream* const os) const {
    if (captures_) {
      *os << "matches \"" << absl::CEscape(input_) << "\" with " << PrintCaptures(*captures_);
    } else {
      *os << "matches \"" << absl::CEscape(input_) << "\"";
    }
  }

  void DescribeNegationTo(std::ostream* const os) const {
    if (captures_) {
      *os << "doesn't match \"" << absl::CEscape(input_) << "\" with " << PrintCaptures(*captures_);
    } else {
      *os << "doesn't match \"" << absl::CEscape(input_) << "\"";
    }
  }

  bool MatchAndExplain(reffed_ptr<AbstractAutomaton> const& automaton,
                       ::testing::MatchResultListener* const listener) const {
    bool const test_result = Run(automaton);
    if (test_result) {
      *listener << "matches";
    } else {
      *listener << "doesn't match";
    }
    auto const match_outcome = Match(automaton, listener);
    if ((match_outcome != Outcome::kNoMatch) != test_result) {
      *listener << ", Match() results differ from Test result";
      return !test_result;
    }
    auto const match_args_outcome = MatchArgs(automaton, listener);
    if ((match_args_outcome != Outcome::kNoMatch) != test_result) {
      *listener << ", MatchArgs() result differs from Test result";
      return !test_result;
    }
    return test_result;
  }

 private:
  enum class Outcome { kMatch, kNoMatch, kUnexpected };

  bool Run(reffed_ptr<AbstractAutomaton> const& automaton) const {
    if (use_stepper_) {
      auto const stepper = automaton->MakeStepper();
      return stepper->Step(input_) && stepper->Finish();
    } else {
      return automaton->Test(input_);
    }
  }

  Outcome Match(reffed_ptr<AbstractAutomaton> const& automaton,
                ::testing::MatchResultListener* const listener) const {
    auto maybe_results = automaton->Match(input_);
    if (!maybe_results.has_value()) {
      *listener << ", Match() doesn't match";
      return Outcome::kNoMatch;
    }
    auto const& results = maybe_results.value();
    if (!captures_ || CheckMatchResults(results, *captures_)) {
      *listener << ", Match() matches";
      return Outcome::kMatch;
    } else {
      *listener << ", Match() matches with unexpected captures: " << PrintCaptures(results);
      return Outcome::kUnexpected;
    }
  }

  Outcome MatchArgs(reffed_ptr<AbstractAutomaton> const& automaton,
                    ::testing::MatchResultListener* const listener) const {
    std::vector<std::string_view> args;
    std::vector<std::string_view*> arg_ptrs;
    if (captures_) {
      args = std::vector<std::string_view>{captures_->size(), std::string_view("")};
      arg_ptrs.reserve(args.size());
      for (auto& arg : args) {
        arg_ptrs.emplace_back(&arg);
      }
    }
    if (!automaton->MatchArgs(input_, arg_ptrs)) {
      *listener << ", MatchArgs() doesn't match";
      return Outcome::kNoMatch;
    }
    if (!captures_ || CheckMatchArgs(args, *captures_)) {
      *listener << ", MatchArgs() matches";
      return Outcome::kMatch;
    } else {
      *listener << ", MatchArgs() matches with unexpected captures: " << PrintStringList(args);
      return Outcome::kUnexpected;
    }
  }

  std::string input_;
  std::optional<std::vector<std::vector<std::string>>> captures_;
  bool use_stepper_;
};

class MatchesPrefixOfImpl {
 public:
  using is_gtest_matcher = void;

  explicit MatchesPrefixOfImpl(std::string_view const input,
                               std::vector<std::vector<std::string>> captures,
                               bool const use_stepper)
      : input_(input), captures_(std::move(captures)), use_stepper_(use_stepper) {}

  void DescribeTo(std::ostream* const os) const {
    *os << "matches some prefix of \"" << absl::CEscape(input_) << "\" with "
        << PrintCaptures(captures_);
  }

  void DescribeNegationTo(std::ostream* const os) const {
    *os << "doesn't match any prefix of \"" << absl::CEscape(input_) << "\" with "
        << PrintCaptures(captures_);
  }

  bool MatchAndExplain(reffed_ptr<AbstractAutomaton> const& automaton,
                       ::testing::MatchResultListener* const listener) const {
    if (!Run(automaton)) {
      *listener << "doesn't match";
      return false;
    }
    auto maybe_results = automaton->MatchPrefix(input_);
    if (!maybe_results.has_value()) {
      *listener << "MatchPrefix results differ from Test result";
      return false;
    }
    auto const& results = maybe_results.value();
    if (!CheckMatchResults(results, captures_)) {
      *listener << "unexpected MatchPrefix results: " << PrintCaptures(results);
      return false;
    }
    std::vector<std::string_view> args{captures_.size(), std::string_view("")};
    std::vector<std::string_view*> arg_ptrs;
    arg_ptrs.reserve(args.size());
    for (auto& arg : args) {
      arg_ptrs.emplace_back(&arg);
    }
    if (!automaton->MatchPrefixArgs(input_, arg_ptrs)) {
      *listener << "MatchPrefixArgs result differs from Test result";
      return false;
    }
    if (!CheckMatchArgs(args, captures_)) {
      *listener << "unexpected MatchPrefixArgs results: " << PrintStringList(args);
      return false;
    }
    *listener << "matches with " << PrintCaptures(captures_);
    return true;
  }

 private:
  bool Run(reffed_ptr<AbstractAutomaton> const& automaton) const {
    if (use_stepper_) {
      return TestPrefixWithStepper(automaton, input_);
    } else {
      return automaton->TestPrefix(input_);
    }
  }

  std::string const input_;
  std::vector<std::vector<std::string>> const captures_;
  bool const use_stepper_;
};

class DoesntMatchPrefixOfImpl {
 public:
  using is_gtest_matcher = void;

  explicit DoesntMatchPrefixOfImpl(std::string_view const input, bool const use_stepper)
      : input_(input), use_stepper_(use_stepper) {}

  void DescribeTo(std::ostream* const os) const {
    *os << "doesn't match any prefix of \"" << absl::CEscape(input_) << "\"";
  }

  void DescribeNegationTo(std::ostream* const os) const {
    *os << "matches some prefix of \"" << absl::CEscape(input_) << "\"";
  }

  bool MatchAndExplain(reffed_ptr<AbstractAutomaton> const& automaton,
                       ::testing::MatchResultListener* const listener) const {
    if (Run(automaton)) {
      *listener << "matches";
      return false;
    }
    if (automaton->MatchPrefix(input_).has_value()) {
      *listener << "MatchPrefix results differ from TestPrefix result";
      return false;
    }
    if (automaton->MatchPrefixArgs(input_, {})) {
      *listener << "MatchPrefixArgs result differs from TestPrefix result";
      return false;
    } else {
      *listener << "doesn't match";
      return true;
    }
  }

 private:
  bool Run(reffed_ptr<AbstractAutomaton> const& automaton) const {
    if (use_stepper_) {
      return TestPrefixWithStepper(automaton, input_);
    } else {
      return automaton->TestPrefix(input_);
    }
  }

  std::string const input_;
  bool const use_stepper_;
};

struct RegexpTestParams {
  bool force_nfa;
  bool use_stepper;
};

class RegexpTest : public ::testing::TestWithParam<RegexpTestParams> {
 protected:
  explicit RegexpTest() { TempNFA::force_nfa_for_testing = GetParam().force_nfa; }
  ~RegexpTest() { TempNFA::force_nfa_for_testing = false; }

  bool CheckDeterministic(reffed_ptr<AbstractAutomaton> const& automaton);
  bool CheckNotDeterministic(reffed_ptr<AbstractAutomaton> const& automaton);

  auto Matches(std::string_view const input, std::vector<std::vector<std::string>> captures) const {
    return MatchesImpl(input, GetParam().use_stepper).With(std::move(captures));
  }

  auto DoesntMatch(std::string_view const input) const {
    return Not(MatchesImpl(input, GetParam().use_stepper));
  }

  auto MatchesPrefixOf(std::string_view const input,
                       std::vector<std::vector<std::string>> captures) const {
    return MatchesPrefixOfImpl(input, std::move(captures), GetParam().use_stepper);
  }

  auto DoesntMatchPrefixOf(std::string_view const input) const {
    return DoesntMatchPrefixOfImpl(input, GetParam().use_stepper);
  }
};

bool RegexpTest::CheckDeterministic(reffed_ptr<AbstractAutomaton> const& automaton) {
  return GetParam().force_nfa || automaton->IsDeterministic();
}

bool RegexpTest::CheckNotDeterministic(reffed_ptr<AbstractAutomaton> const& automaton) {
  return GetParam().force_nfa || !automaton->IsDeterministic();
}

TEST_P(RegexpTest, MaxRecursionDepth) {
  FlagOverride fo{&FLAGS_re_max_recursion_depth, 20};
  EXPECT_OK(Parse("((()))"));
  EXPECT_THAT(Parse("(((((((((((((((((((()))))))))))))))))))"),
              StatusIs(absl::StatusCode::kResourceExhausted));
}

TEST_P(RegexpTest, EmptyIsDeterministic) {
  auto const status_or_pattern = Parse("");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(CheckDeterministic(pattern));
}

TEST_P(RegexpTest, SimpleStringIsDeterministic) {
  auto const status_or_pattern = Parse("lorem");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(CheckDeterministic(pattern));
}

TEST_P(RegexpTest, PipeIsNotDeterministic) {
  auto const status_or_pattern = Parse("lorem(ipsum|dolor)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(CheckNotDeterministic(pattern));
}

TEST_P(RegexpTest, Size) {
  auto const status_or_pattern = Parse("lorem");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern->GetSize(), Pair(6, 5));
}

TEST_P(RegexpTest, SizeWithLoopAndCaptureGroup) {
  auto const status_or_pattern = Parse("(lorem)*");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern->GetSize(), Pair(7, 7));
}

TEST_P(RegexpTest, NoCaptureGroups) {
  auto const status_or_pattern = Parse("lorem");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_EQ(pattern->GetNumCaptureGroups(), 0);
}

TEST_P(RegexpTest, OneCaptureGroup) {
  auto const status_or_pattern = Parse("lo(r)em");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_EQ(pattern->GetNumCaptureGroups(), 1);
}

TEST_P(RegexpTest, TwoPeeringCaptureGroups) {
  auto const status_or_pattern = Parse("l(o)r(e)m");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_EQ(pattern->GetNumCaptureGroups(), 2);
}

TEST_P(RegexpTest, TwoNestedCaptureGroups) {
  auto const status_or_pattern = Parse("l(o(r)e)m");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_EQ(pattern->GetNumCaptureGroups(), 2);
}

TEST_P(RegexpTest, ManyCaptureGroups) {
  auto const status_or_pattern = Parse("()((()())())");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_EQ(pattern->GetNumCaptureGroups(), 6);
}

TEST_P(RegexpTest, Empty) {
  auto const status_or_pattern = Parse("");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("hello"));
  EXPECT_THAT(pattern, Matches("", {}));
}

TEST_P(RegexpTest, SimpleCharacter) {
  auto const status_or_pattern = Parse("a");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("anchor"));
  EXPECT_THAT(pattern, DoesntMatch("banana"));
  EXPECT_THAT(pattern, DoesntMatch(""));
}

TEST_P(RegexpTest, AnotherSimpleCharacter) {
  auto const status_or_pattern = Parse("b");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, DoesntMatch("anchor"));
  EXPECT_THAT(pattern, DoesntMatch("banana"));
  EXPECT_THAT(pattern, DoesntMatch(""));
}

TEST_P(RegexpTest, InvalidEscapeCode) {
  EXPECT_THAT(Parse("\\a"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\T"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\R"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\N"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\V"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\F"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\X"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(RegexpTest, BlockBackrefs) {
  EXPECT_THAT(Parse("\\0"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\1"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\2"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\3"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\4"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\5"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\6"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\7"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\8"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\9"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(RegexpTest, Digit) {
  auto const status_or_pattern = Parse("\\d");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("0", {}));
  EXPECT_THAT(pattern, Matches("1", {}));
  EXPECT_THAT(pattern, Matches("2", {}));
  EXPECT_THAT(pattern, Matches("3", {}));
  EXPECT_THAT(pattern, Matches("4", {}));
  EXPECT_THAT(pattern, Matches("5", {}));
  EXPECT_THAT(pattern, Matches("6", {}));
  EXPECT_THAT(pattern, Matches("7", {}));
  EXPECT_THAT(pattern, Matches("8", {}));
  EXPECT_THAT(pattern, Matches("9", {}));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("d"));
  EXPECT_THAT(pattern, DoesntMatch("\\d"));
  EXPECT_THAT(pattern, DoesntMatch("\\0"));
}

TEST_P(RegexpTest, NotDigit) {
  auto const status_or_pattern = Parse("\\D");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("0"));
  EXPECT_THAT(pattern, DoesntMatch("1"));
  EXPECT_THAT(pattern, DoesntMatch("2"));
  EXPECT_THAT(pattern, DoesntMatch("3"));
  EXPECT_THAT(pattern, DoesntMatch("4"));
  EXPECT_THAT(pattern, DoesntMatch("5"));
  EXPECT_THAT(pattern, DoesntMatch("6"));
  EXPECT_THAT(pattern, DoesntMatch("7"));
  EXPECT_THAT(pattern, DoesntMatch("8"));
  EXPECT_THAT(pattern, DoesntMatch("9"));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("D", {}));
  EXPECT_THAT(pattern, DoesntMatch("\\D"));
  EXPECT_THAT(pattern, DoesntMatch("\\0"));
}

TEST_P(RegexpTest, WordCharacter) {
  auto const status_or_pattern = Parse("\\w");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("A", {}));
  EXPECT_THAT(pattern, Matches("B", {}));
  EXPECT_THAT(pattern, Matches("C", {}));
  EXPECT_THAT(pattern, Matches("D", {}));
  EXPECT_THAT(pattern, Matches("E", {}));
  EXPECT_THAT(pattern, Matches("F", {}));
  EXPECT_THAT(pattern, Matches("G", {}));
  EXPECT_THAT(pattern, Matches("H", {}));
  EXPECT_THAT(pattern, Matches("I", {}));
  EXPECT_THAT(pattern, Matches("J", {}));
  EXPECT_THAT(pattern, Matches("K", {}));
  EXPECT_THAT(pattern, Matches("L", {}));
  EXPECT_THAT(pattern, Matches("M", {}));
  EXPECT_THAT(pattern, Matches("N", {}));
  EXPECT_THAT(pattern, Matches("O", {}));
  EXPECT_THAT(pattern, Matches("P", {}));
  EXPECT_THAT(pattern, Matches("Q", {}));
  EXPECT_THAT(pattern, Matches("R", {}));
  EXPECT_THAT(pattern, Matches("S", {}));
  EXPECT_THAT(pattern, Matches("T", {}));
  EXPECT_THAT(pattern, Matches("U", {}));
  EXPECT_THAT(pattern, Matches("V", {}));
  EXPECT_THAT(pattern, Matches("W", {}));
  EXPECT_THAT(pattern, Matches("X", {}));
  EXPECT_THAT(pattern, Matches("Y", {}));
  EXPECT_THAT(pattern, Matches("Z", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("c", {}));
  EXPECT_THAT(pattern, Matches("d", {}));
  EXPECT_THAT(pattern, Matches("e", {}));
  EXPECT_THAT(pattern, Matches("f", {}));
  EXPECT_THAT(pattern, Matches("g", {}));
  EXPECT_THAT(pattern, Matches("h", {}));
  EXPECT_THAT(pattern, Matches("i", {}));
  EXPECT_THAT(pattern, Matches("j", {}));
  EXPECT_THAT(pattern, Matches("k", {}));
  EXPECT_THAT(pattern, Matches("l", {}));
  EXPECT_THAT(pattern, Matches("m", {}));
  EXPECT_THAT(pattern, Matches("n", {}));
  EXPECT_THAT(pattern, Matches("o", {}));
  EXPECT_THAT(pattern, Matches("p", {}));
  EXPECT_THAT(pattern, Matches("q", {}));
  EXPECT_THAT(pattern, Matches("r", {}));
  EXPECT_THAT(pattern, Matches("s", {}));
  EXPECT_THAT(pattern, Matches("t", {}));
  EXPECT_THAT(pattern, Matches("u", {}));
  EXPECT_THAT(pattern, Matches("v", {}));
  EXPECT_THAT(pattern, Matches("w", {}));
  EXPECT_THAT(pattern, Matches("x", {}));
  EXPECT_THAT(pattern, Matches("y", {}));
  EXPECT_THAT(pattern, Matches("z", {}));
  EXPECT_THAT(pattern, Matches("0", {}));
  EXPECT_THAT(pattern, Matches("1", {}));
  EXPECT_THAT(pattern, Matches("2", {}));
  EXPECT_THAT(pattern, Matches("3", {}));
  EXPECT_THAT(pattern, Matches("4", {}));
  EXPECT_THAT(pattern, Matches("5", {}));
  EXPECT_THAT(pattern, Matches("6", {}));
  EXPECT_THAT(pattern, Matches("7", {}));
  EXPECT_THAT(pattern, Matches("8", {}));
  EXPECT_THAT(pattern, Matches("9", {}));
  EXPECT_THAT(pattern, Matches("_", {}));
  EXPECT_THAT(pattern, DoesntMatch("."));
  EXPECT_THAT(pattern, DoesntMatch("-"));
  EXPECT_THAT(pattern, DoesntMatch("\\"));
  EXPECT_THAT(pattern, DoesntMatch("\\w"));
}

TEST_P(RegexpTest, NotWordCharacter) {
  auto const status_or_pattern = Parse("\\W");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("A"));
  EXPECT_THAT(pattern, DoesntMatch("B"));
  EXPECT_THAT(pattern, DoesntMatch("C"));
  EXPECT_THAT(pattern, DoesntMatch("D"));
  EXPECT_THAT(pattern, DoesntMatch("E"));
  EXPECT_THAT(pattern, DoesntMatch("F"));
  EXPECT_THAT(pattern, DoesntMatch("G"));
  EXPECT_THAT(pattern, DoesntMatch("H"));
  EXPECT_THAT(pattern, DoesntMatch("I"));
  EXPECT_THAT(pattern, DoesntMatch("J"));
  EXPECT_THAT(pattern, DoesntMatch("K"));
  EXPECT_THAT(pattern, DoesntMatch("L"));
  EXPECT_THAT(pattern, DoesntMatch("M"));
  EXPECT_THAT(pattern, DoesntMatch("N"));
  EXPECT_THAT(pattern, DoesntMatch("O"));
  EXPECT_THAT(pattern, DoesntMatch("P"));
  EXPECT_THAT(pattern, DoesntMatch("Q"));
  EXPECT_THAT(pattern, DoesntMatch("R"));
  EXPECT_THAT(pattern, DoesntMatch("S"));
  EXPECT_THAT(pattern, DoesntMatch("T"));
  EXPECT_THAT(pattern, DoesntMatch("U"));
  EXPECT_THAT(pattern, DoesntMatch("V"));
  EXPECT_THAT(pattern, DoesntMatch("W"));
  EXPECT_THAT(pattern, DoesntMatch("X"));
  EXPECT_THAT(pattern, DoesntMatch("Y"));
  EXPECT_THAT(pattern, DoesntMatch("Z"));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("d"));
  EXPECT_THAT(pattern, DoesntMatch("e"));
  EXPECT_THAT(pattern, DoesntMatch("f"));
  EXPECT_THAT(pattern, DoesntMatch("g"));
  EXPECT_THAT(pattern, DoesntMatch("h"));
  EXPECT_THAT(pattern, DoesntMatch("i"));
  EXPECT_THAT(pattern, DoesntMatch("j"));
  EXPECT_THAT(pattern, DoesntMatch("k"));
  EXPECT_THAT(pattern, DoesntMatch("l"));
  EXPECT_THAT(pattern, DoesntMatch("m"));
  EXPECT_THAT(pattern, DoesntMatch("n"));
  EXPECT_THAT(pattern, DoesntMatch("o"));
  EXPECT_THAT(pattern, DoesntMatch("p"));
  EXPECT_THAT(pattern, DoesntMatch("q"));
  EXPECT_THAT(pattern, DoesntMatch("r"));
  EXPECT_THAT(pattern, DoesntMatch("s"));
  EXPECT_THAT(pattern, DoesntMatch("t"));
  EXPECT_THAT(pattern, DoesntMatch("u"));
  EXPECT_THAT(pattern, DoesntMatch("v"));
  EXPECT_THAT(pattern, DoesntMatch("w"));
  EXPECT_THAT(pattern, DoesntMatch("x"));
  EXPECT_THAT(pattern, DoesntMatch("y"));
  EXPECT_THAT(pattern, DoesntMatch("z"));
  EXPECT_THAT(pattern, DoesntMatch("0"));
  EXPECT_THAT(pattern, DoesntMatch("1"));
  EXPECT_THAT(pattern, DoesntMatch("2"));
  EXPECT_THAT(pattern, DoesntMatch("3"));
  EXPECT_THAT(pattern, DoesntMatch("4"));
  EXPECT_THAT(pattern, DoesntMatch("5"));
  EXPECT_THAT(pattern, DoesntMatch("6"));
  EXPECT_THAT(pattern, DoesntMatch("7"));
  EXPECT_THAT(pattern, DoesntMatch("8"));
  EXPECT_THAT(pattern, DoesntMatch("9"));
  EXPECT_THAT(pattern, DoesntMatch("_"));
  EXPECT_THAT(pattern, Matches(".", {}));
  EXPECT_THAT(pattern, Matches("-", {}));
  EXPECT_THAT(pattern, Matches("\\", {}));
  EXPECT_THAT(pattern, DoesntMatch("\\W"));
}

TEST_P(RegexpTest, Spacing) {
  auto const status_or_pattern = Parse("\\s");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches(" ", {}));
  EXPECT_THAT(pattern, Matches("\f", {}));
  EXPECT_THAT(pattern, Matches("\n", {}));
  EXPECT_THAT(pattern, Matches("\r", {}));
  EXPECT_THAT(pattern, Matches("\t", {}));
  EXPECT_THAT(pattern, Matches("\v", {}));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("s"));
  EXPECT_THAT(pattern, DoesntMatch("\\"));
  EXPECT_THAT(pattern, DoesntMatch("\\s"));
}

TEST_P(RegexpTest, NotSpacing) {
  auto const status_or_pattern = Parse("\\S");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch(" "));
  EXPECT_THAT(pattern, DoesntMatch("\f"));
  EXPECT_THAT(pattern, DoesntMatch("\n"));
  EXPECT_THAT(pattern, DoesntMatch("\r"));
  EXPECT_THAT(pattern, DoesntMatch("\t"));
  EXPECT_THAT(pattern, DoesntMatch("\v"));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("s", {}));
  EXPECT_THAT(pattern, Matches("\\", {}));
  EXPECT_THAT(pattern, DoesntMatch("\\S"));
}

TEST_P(RegexpTest, HorizontalTab) {
  auto const status_or_pattern = Parse("\\t");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("\t", {}));
  EXPECT_THAT(pattern, DoesntMatch("\n"));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("t"));
  EXPECT_THAT(pattern, DoesntMatch("\\"));
  EXPECT_THAT(pattern, DoesntMatch("\\t"));
}

TEST_P(RegexpTest, CarriageReturn) {
  auto const status_or_pattern = Parse("\\r");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("\r", {}));
  EXPECT_THAT(pattern, DoesntMatch("\n"));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("r"));
  EXPECT_THAT(pattern, DoesntMatch("\\"));
  EXPECT_THAT(pattern, DoesntMatch("\\r"));
}

TEST_P(RegexpTest, LineFeed) {
  auto const status_or_pattern = Parse("\\n");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("\n", {}));
  EXPECT_THAT(pattern, DoesntMatch("\t"));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("n"));
  EXPECT_THAT(pattern, DoesntMatch("\\"));
  EXPECT_THAT(pattern, DoesntMatch("\\n"));
}

TEST_P(RegexpTest, VerticalTab) {
  auto const status_or_pattern = Parse("\\v");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("\v", {}));
  EXPECT_THAT(pattern, DoesntMatch("\n"));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("v"));
  EXPECT_THAT(pattern, DoesntMatch("\\"));
  EXPECT_THAT(pattern, DoesntMatch("\\v"));
}

TEST_P(RegexpTest, FormFeed) {
  auto const status_or_pattern = Parse("\\f");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("\f", {}));
  EXPECT_THAT(pattern, DoesntMatch("\n"));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("f"));
  EXPECT_THAT(pattern, DoesntMatch("\\"));
  EXPECT_THAT(pattern, DoesntMatch("\\f"));
}

TEST_P(RegexpTest, InvalidHexCode) {
  EXPECT_THAT(Parse("\\xZ0"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\x0Z"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(RegexpTest, HexCode1) {
  auto const status_or_pattern = Parse("\\x12");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("\x12", {}));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("x"));
  EXPECT_THAT(pattern, DoesntMatch("\\"));
  EXPECT_THAT(pattern, DoesntMatch("\\x12"));
}

TEST_P(RegexpTest, HexCode2) {
  auto const status_or_pattern = Parse("\\xAF");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("\xAF", {}));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("x"));
  EXPECT_THAT(pattern, DoesntMatch("\\"));
  EXPECT_THAT(pattern, DoesntMatch("\\xAF"));
}

TEST_P(RegexpTest, HexCode3) {
  auto const status_or_pattern = Parse("\\xaf");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("\xAF", {}));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("x"));
  EXPECT_THAT(pattern, DoesntMatch("\\"));
  EXPECT_THAT(pattern, DoesntMatch("\\xaf"));
}

TEST_P(RegexpTest, AnyCharacter) {
  auto const status_or_pattern = Parse(".");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, DoesntMatch("anchor"));
  EXPECT_THAT(pattern, DoesntMatch("banana"));
  EXPECT_THAT(pattern, DoesntMatch(""));
}

TEST_P(RegexpTest, EmptyCharacterClass) {
  auto const status_or_pattern = Parse("[]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, DoesntMatch("[]"));
}

TEST_P(RegexpTest, NegatedEmptyCharacterClass) {
  auto const status_or_pattern = Parse("[^]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("^", {}));
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, DoesntMatch("[^]"));
}

TEST_P(RegexpTest, CharacterClass) {
  auto const status_or_pattern = Parse("[lorem\xAF]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, Matches("l", {}));
  EXPECT_THAT(pattern, Matches("o", {}));
  EXPECT_THAT(pattern, Matches("r", {}));
  EXPECT_THAT(pattern, Matches("e", {}));
  EXPECT_THAT(pattern, Matches("m", {}));
  EXPECT_THAT(pattern, Matches("\xAF", {}));
  EXPECT_THAT(pattern, DoesntMatch("\xBF"));
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("lorem\xAF"));
  EXPECT_THAT(pattern, DoesntMatch("[lorem\xAF]"));
}

TEST_P(RegexpTest, NegatedCharacterClass) {
  auto const status_or_pattern = Parse("[^lorem\xAF]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, DoesntMatch("l"));
  EXPECT_THAT(pattern, DoesntMatch("o"));
  EXPECT_THAT(pattern, DoesntMatch("r"));
  EXPECT_THAT(pattern, DoesntMatch("e"));
  EXPECT_THAT(pattern, DoesntMatch("m"));
  EXPECT_THAT(pattern, DoesntMatch("\xAF"));
  EXPECT_THAT(pattern, Matches("\xBF", {}));
  EXPECT_THAT(pattern, Matches("^", {}));
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("^lorem"));
  EXPECT_THAT(pattern, DoesntMatch("^lorem\xAF"));
  EXPECT_THAT(pattern, DoesntMatch("[^lorem\xAF]"));
}

TEST_P(RegexpTest, CharacterClassWithCircumflex) {
  auto const status_or_pattern = Parse("[ab^cd]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("^", {}));
  EXPECT_THAT(pattern, Matches("c", {}));
  EXPECT_THAT(pattern, Matches("d", {}));
  EXPECT_THAT(pattern, DoesntMatch("ab^cd"));
  EXPECT_THAT(pattern, DoesntMatch("[ab^cd]"));
}

TEST_P(RegexpTest, NegatedCharacterClassWithCircumflex) {
  auto const status_or_pattern = Parse("[^ab^cd]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("^"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("d"));
  EXPECT_THAT(pattern, Matches("x", {}));
  EXPECT_THAT(pattern, Matches("y", {}));
  EXPECT_THAT(pattern, DoesntMatch("ab^cd"));
  EXPECT_THAT(pattern, DoesntMatch("^ab^cd"));
  EXPECT_THAT(pattern, DoesntMatch("[^ab^cd]"));
}

TEST_P(RegexpTest, CharacterRange) {
  auto const status_or_pattern = Parse("[2-4]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("0"));
  EXPECT_THAT(pattern, DoesntMatch("1"));
  EXPECT_THAT(pattern, Matches("2", {}));
  EXPECT_THAT(pattern, Matches("3", {}));
  EXPECT_THAT(pattern, Matches("4", {}));
  EXPECT_THAT(pattern, DoesntMatch("5"));
  EXPECT_THAT(pattern, DoesntMatch("6"));
  EXPECT_THAT(pattern, DoesntMatch("-"));
}

TEST_P(RegexpTest, NegatedCharacterRange) {
  auto const status_or_pattern = Parse("[^2-4]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("0", {}));
  EXPECT_THAT(pattern, Matches("1", {}));
  EXPECT_THAT(pattern, DoesntMatch("2"));
  EXPECT_THAT(pattern, DoesntMatch("3"));
  EXPECT_THAT(pattern, DoesntMatch("4"));
  EXPECT_THAT(pattern, Matches("5", {}));
  EXPECT_THAT(pattern, Matches("6", {}));
  EXPECT_THAT(pattern, Matches("-", {}));
}

TEST_P(RegexpTest, CharacterRangeWithDash) {
  auto const status_or_pattern = Parse("[2-4-]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("0"));
  EXPECT_THAT(pattern, DoesntMatch("1"));
  EXPECT_THAT(pattern, Matches("2", {}));
  EXPECT_THAT(pattern, Matches("3", {}));
  EXPECT_THAT(pattern, Matches("4", {}));
  EXPECT_THAT(pattern, DoesntMatch("5"));
  EXPECT_THAT(pattern, DoesntMatch("6"));
  EXPECT_THAT(pattern, Matches("-", {}));
}

TEST_P(RegexpTest, NegatedCharacterRangeWithDash) {
  auto const status_or_pattern = Parse("[^2-4-]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("0", {}));
  EXPECT_THAT(pattern, Matches("1", {}));
  EXPECT_THAT(pattern, DoesntMatch("2"));
  EXPECT_THAT(pattern, DoesntMatch("3"));
  EXPECT_THAT(pattern, DoesntMatch("4"));
  EXPECT_THAT(pattern, Matches("5", {}));
  EXPECT_THAT(pattern, Matches("6", {}));
  EXPECT_THAT(pattern, DoesntMatch("-"));
}

TEST_P(RegexpTest, CharacterClassWithCharactersAndRange) {
  auto const status_or_pattern = Parse("[ac2-4eg-]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, Matches("c", {}));
  EXPECT_THAT(pattern, DoesntMatch("d"));
  EXPECT_THAT(pattern, DoesntMatch("0"));
  EXPECT_THAT(pattern, DoesntMatch("1"));
  EXPECT_THAT(pattern, Matches("2", {}));
  EXPECT_THAT(pattern, Matches("3", {}));
  EXPECT_THAT(pattern, Matches("4", {}));
  EXPECT_THAT(pattern, DoesntMatch("5"));
  EXPECT_THAT(pattern, DoesntMatch("6"));
  EXPECT_THAT(pattern, Matches("e", {}));
  EXPECT_THAT(pattern, DoesntMatch("f"));
  EXPECT_THAT(pattern, Matches("g", {}));
  EXPECT_THAT(pattern, DoesntMatch("h"));
  EXPECT_THAT(pattern, Matches("-", {}));
}

TEST_P(RegexpTest, NegatedCharacterClassWithCharactersAndRange) {
  auto const status_or_pattern = Parse("[^ac2-4eg-]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, Matches("d", {}));
  EXPECT_THAT(pattern, Matches("0", {}));
  EXPECT_THAT(pattern, Matches("1", {}));
  EXPECT_THAT(pattern, DoesntMatch("2"));
  EXPECT_THAT(pattern, DoesntMatch("3"));
  EXPECT_THAT(pattern, DoesntMatch("4"));
  EXPECT_THAT(pattern, Matches("5", {}));
  EXPECT_THAT(pattern, Matches("6", {}));
  EXPECT_THAT(pattern, DoesntMatch("e"));
  EXPECT_THAT(pattern, Matches("f", {}));
  EXPECT_THAT(pattern, DoesntMatch("g"));
  EXPECT_THAT(pattern, Matches("h", {}));
  EXPECT_THAT(pattern, DoesntMatch("-"));
}

TEST_P(RegexpTest, CharacterClassWithSpecialCharacters) {
  auto const status_or_pattern = Parse("[a^$.(){}|?*+b]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("^", {}));
  EXPECT_THAT(pattern, Matches("$", {}));
  EXPECT_THAT(pattern, Matches(".", {}));
  EXPECT_THAT(pattern, Matches("(", {}));
  EXPECT_THAT(pattern, Matches(")", {}));
  EXPECT_THAT(pattern, Matches("{", {}));
  EXPECT_THAT(pattern, Matches("}", {}));
  EXPECT_THAT(pattern, Matches("|", {}));
  EXPECT_THAT(pattern, Matches("?", {}));
  EXPECT_THAT(pattern, Matches("*", {}));
  EXPECT_THAT(pattern, Matches("+", {}));
  EXPECT_THAT(pattern, DoesntMatch("x"));
  EXPECT_THAT(pattern, DoesntMatch("y"));
}

TEST_P(RegexpTest, NegatedCharacterClassWithSpecialCharacters) {
  auto const status_or_pattern = Parse("[^a^$.(){}|?*+b]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("^"));
  EXPECT_THAT(pattern, DoesntMatch("$"));
  EXPECT_THAT(pattern, DoesntMatch("."));
  EXPECT_THAT(pattern, DoesntMatch("("));
  EXPECT_THAT(pattern, DoesntMatch(")"));
  EXPECT_THAT(pattern, DoesntMatch("{"));
  EXPECT_THAT(pattern, DoesntMatch("}"));
  EXPECT_THAT(pattern, DoesntMatch("|"));
  EXPECT_THAT(pattern, DoesntMatch("?"));
  EXPECT_THAT(pattern, DoesntMatch("*"));
  EXPECT_THAT(pattern, DoesntMatch("+"));
  EXPECT_THAT(pattern, Matches("x", {}));
  EXPECT_THAT(pattern, Matches("y", {}));
}

TEST_P(RegexpTest, CharacterClassWithEscapes) {
  auto const status_or_pattern = Parse("[a\\\\\\^\\$\\.\\(\\)\\[\\]\\{\\}\\|\\?\\*\\+b]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("\\", {}));
  EXPECT_THAT(pattern, Matches("^", {}));
  EXPECT_THAT(pattern, Matches("$", {}));
  EXPECT_THAT(pattern, Matches(".", {}));
  EXPECT_THAT(pattern, Matches("(", {}));
  EXPECT_THAT(pattern, Matches(")", {}));
  EXPECT_THAT(pattern, Matches("[", {}));
  EXPECT_THAT(pattern, Matches("]", {}));
  EXPECT_THAT(pattern, Matches("{", {}));
  EXPECT_THAT(pattern, Matches("}", {}));
  EXPECT_THAT(pattern, Matches("|", {}));
  EXPECT_THAT(pattern, Matches("?", {}));
  EXPECT_THAT(pattern, Matches("*", {}));
  EXPECT_THAT(pattern, Matches("+", {}));
  EXPECT_THAT(pattern, DoesntMatch("x"));
  EXPECT_THAT(pattern, DoesntMatch("y"));
}

TEST_P(RegexpTest, NegatedCharacterClassWithEscapes) {
  auto const status_or_pattern = Parse("[^a\\\\\\^\\$\\.\\(\\)\\[\\]\\{\\}\\|\\?\\*\\+b]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("\\"));
  EXPECT_THAT(pattern, DoesntMatch("^"));
  EXPECT_THAT(pattern, DoesntMatch("$"));
  EXPECT_THAT(pattern, DoesntMatch("."));
  EXPECT_THAT(pattern, DoesntMatch("("));
  EXPECT_THAT(pattern, DoesntMatch(")"));
  EXPECT_THAT(pattern, DoesntMatch("["));
  EXPECT_THAT(pattern, DoesntMatch("]"));
  EXPECT_THAT(pattern, DoesntMatch("{"));
  EXPECT_THAT(pattern, DoesntMatch("}"));
  EXPECT_THAT(pattern, DoesntMatch("|"));
  EXPECT_THAT(pattern, DoesntMatch("?"));
  EXPECT_THAT(pattern, DoesntMatch("*"));
  EXPECT_THAT(pattern, DoesntMatch("+"));
  EXPECT_THAT(pattern, Matches("x", {}));
  EXPECT_THAT(pattern, Matches("y", {}));
}

TEST_P(RegexpTest, CharacterClassWithMoreEscapes) {
  auto const status_or_pattern = Parse("[\\t\\r\\n\\v\\f\\x12\\xAF]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, Matches("\t", {}));
  EXPECT_THAT(pattern, Matches("\r", {}));
  EXPECT_THAT(pattern, Matches("\n", {}));
  EXPECT_THAT(pattern, Matches("\v", {}));
  EXPECT_THAT(pattern, Matches("\f", {}));
  EXPECT_THAT(pattern, Matches("\x12", {}));
  EXPECT_THAT(pattern, Matches("\xAF", {}));
  EXPECT_THAT(pattern, DoesntMatch("x"));
  EXPECT_THAT(pattern, DoesntMatch("y"));
}

TEST_P(RegexpTest, NegatedCharacterClassWithMoreEscapes) {
  auto const status_or_pattern = Parse("[^\\t\\r\\n\\v\\f\\x12\\xAF]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, DoesntMatch("\t"));
  EXPECT_THAT(pattern, DoesntMatch("\r"));
  EXPECT_THAT(pattern, DoesntMatch("\n"));
  EXPECT_THAT(pattern, DoesntMatch("\v"));
  EXPECT_THAT(pattern, DoesntMatch("\f"));
  EXPECT_THAT(pattern, DoesntMatch("\x12"));
  EXPECT_THAT(pattern, DoesntMatch("\xAF"));
  EXPECT_THAT(pattern, Matches("x", {}));
  EXPECT_THAT(pattern, Matches("y", {}));
}

TEST_P(RegexpTest, InvalidEscapeCodesInCharacterClass) {
  EXPECT_THAT(Parse("[\\"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\x"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\x]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\x0Z]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\xZ0]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\a]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\b]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\B]"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(RegexpTest, BlockBackrefsInCharacterClass) {
  EXPECT_THAT(Parse("[\\0]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\1]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\2]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\3]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\4]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\5]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\6]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\7]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\8]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\9]"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(RegexpTest, InvalidSpecialCharacter) {
  EXPECT_THAT(Parse("*"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("+"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("?"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse(")"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("]"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(RegexpTest, CharacterSequence) {
  auto const status_or_pattern = Parse("lorem");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("l"));
  EXPECT_THAT(pattern, Matches("lorem", {}));
  EXPECT_THAT(pattern, DoesntMatch("loremipsum"));
  EXPECT_THAT(pattern, DoesntMatch("dolorloremipsum"));
}

TEST_P(RegexpTest, CharacterSequenceWithDot) {
  auto const status_or_pattern = Parse("lo.em");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("l"));
  EXPECT_THAT(pattern, Matches("lorem", {}));
  EXPECT_THAT(pattern, Matches("lo-em", {}));
  EXPECT_THAT(pattern, Matches("lovem", {}));
  EXPECT_THAT(pattern, DoesntMatch("lodolorem"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsum"));
  EXPECT_THAT(pattern, DoesntMatch("dolorloremipsum"));
}

TEST_P(RegexpTest, KleeneStar) {
  auto const status_or_pattern = Parse("a*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("aaa", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, CharacterSequenceWithStar) {
  auto const status_or_pattern = Parse("lo*rem");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("l"));
  EXPECT_THAT(pattern, Matches("lrem", {}));
  EXPECT_THAT(pattern, Matches("lorem", {}));
  EXPECT_THAT(pattern, Matches("loorem", {}));
  EXPECT_THAT(pattern, Matches("looorem", {}));
  EXPECT_THAT(pattern, DoesntMatch("larem"));
  EXPECT_THAT(pattern, DoesntMatch("loremlorem"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsum"));
  EXPECT_THAT(pattern, DoesntMatch("dolorloremipsum"));
}

TEST_P(RegexpTest, KleenePlus) {
  auto const status_or_pattern = Parse("a+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("aaa", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, CharacterSequenceWithPlus) {
  auto const status_or_pattern = Parse("lo+rem");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("l"));
  EXPECT_THAT(pattern, DoesntMatch("lrem"));
  EXPECT_THAT(pattern, Matches("lorem", {}));
  EXPECT_THAT(pattern, Matches("loorem", {}));
  EXPECT_THAT(pattern, Matches("looorem", {}));
  EXPECT_THAT(pattern, DoesntMatch("larem"));
  EXPECT_THAT(pattern, DoesntMatch("loremlorem"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsum"));
  EXPECT_THAT(pattern, DoesntMatch("dolorloremipsum"));
}

TEST_P(RegexpTest, Maybe) {
  auto const status_or_pattern = Parse("a?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
}

TEST_P(RegexpTest, Many) {
  auto const status_or_pattern = Parse("a{}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("aaa", {}));
  EXPECT_THAT(pattern, Matches("aaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaa", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, ExactlyZero) {
  auto const status_or_pattern = Parse("a{0}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, ExactlyOne) {
  auto const status_or_pattern = Parse("a{1}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, ExactlyTwo) {
  auto const status_or_pattern = Parse("a{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, ExactlyFourtyTwo) {
  auto const status_or_pattern = Parse("a{42}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, DoesntMatch("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, AtLeastZero) {
  auto const status_or_pattern = Parse("a{0,}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("aaa", {}));
  EXPECT_THAT(pattern, Matches("aaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaa", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, AtLeastOne) {
  auto const status_or_pattern = Parse("a{1,}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("aaa", {}));
  EXPECT_THAT(pattern, Matches("aaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaa", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, AtLeastTwo) {
  auto const status_or_pattern = Parse("a{2,}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("aaa", {}));
  EXPECT_THAT(pattern, Matches("aaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaa", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, AtLeastFourtyTwo) {
  auto const status_or_pattern = Parse("a{42,}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, BetweenZeroAndZero) {
  auto const status_or_pattern = Parse("a{0,0}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, BetweenZeroAndOne) {
  auto const status_or_pattern = Parse("a{0,1}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, BetweenZeroAndTwo) {
  auto const status_or_pattern = Parse("a{0,2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, BetweenOneAndOne) {
  auto const status_or_pattern = Parse("a{1,1}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, BetweenOneAndTwo) {
  auto const status_or_pattern = Parse("a{1,2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, BetweenTwoAndTwo) {
  auto const status_or_pattern = Parse("a{2,2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, BetweenFourtyTwoAndFourtyFive) {
  auto const status_or_pattern = Parse("a{42,45}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, DoesntMatch("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("aabaa"));
}

TEST_P(RegexpTest, CharacterSequenceWithMaybe) {
  auto const status_or_pattern = Parse("lo?rem");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("l"));
  EXPECT_THAT(pattern, Matches("lrem", {}));
  EXPECT_THAT(pattern, Matches("lorem", {}));
  EXPECT_THAT(pattern, DoesntMatch("loorem"));
  EXPECT_THAT(pattern, DoesntMatch("looorem"));
  EXPECT_THAT(pattern, DoesntMatch("larem"));
  EXPECT_THAT(pattern, DoesntMatch("loremlorem"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsum"));
  EXPECT_THAT(pattern, DoesntMatch("dolorloremipsum"));
}

TEST_P(RegexpTest, InvalidQuantifiers) {
  EXPECT_THAT(Parse("a{"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{ }"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{1"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{1,"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{1,2"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{2,1}"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{ 2,3}"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{2 ,3}"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{2, 3}"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{2,3 }"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{1001}"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{1002}"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{1001,}"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{10,1001}"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{10,1002}"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(RegexpTest, MultipleQuantifiersDisallowed) {
  EXPECT_THAT(Parse("a**"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a*+"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a*{}"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a+*"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a++"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a+{}"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a?*"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a?+"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a?{}"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{}*"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{}+"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("a{}{}"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(RegexpTest, MultipleQuantifiersWithBrackets) {
  auto const status_or_pattern = Parse("(a+)*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {{}}));
  EXPECT_THAT(pattern, Matches("a", {{"a"}}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, AnyOf(Matches("aa", {{"aa"}}), Matches("aa", {{"a", "a"}})));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, AnyOf(Matches("aaa", {{"aaa"}}), Matches("aaa", {{"aa", "a"}}),
                             Matches("aaa", {{"a", "aa"}}), Matches("aaa", {{"a", "a", "a"}})));
  EXPECT_THAT(pattern,
              AnyOf(Matches("aaaa", {{"aaaa"}}), Matches("aaaa", {{"aaa", "a"}}),
                    Matches("aaaa", {{"aa", "aa"}}), Matches("aaaa", {{"aa", "a", "a"}}),
                    Matches("aaaa", {{"a", "aaa"}}), Matches("aaaa", {{"a", "aa", "a"}}),
                    Matches("aaaa", {{"a", "a", "aa"}}), Matches("aaaa", {{"a", "a", "a", "a"}})));
}

TEST_P(RegexpTest, MultipleQuantifiersWithNonCapturingBrackets) {
  auto const status_or_pattern = Parse("(?:a+)*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, Matches("aaa", {}));
  EXPECT_THAT(pattern, Matches("aaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaa", {}));
}

TEST_P(RegexpTest, EmptyOrEmpty) {
  auto const status_or_pattern = Parse("|");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
}

TEST_P(RegexpTest, EmptyOrA) {
  auto const status_or_pattern = Parse("|a");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
}

TEST_P(RegexpTest, AOrEmpty) {
  auto const status_or_pattern = Parse("a|");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
}

TEST_P(RegexpTest, AOrB) {
  auto const status_or_pattern = Parse("a|b");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("a|b"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
}

TEST_P(RegexpTest, LoremOrIpsum) {
  auto const status_or_pattern = Parse("lorem|ipsum");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("l"));
  EXPECT_THAT(pattern, Matches("lorem", {}));
  EXPECT_THAT(pattern, DoesntMatch("i"));
  EXPECT_THAT(pattern, Matches("ipsum", {}));
  EXPECT_THAT(pattern, DoesntMatch("loremipsum"));
  EXPECT_THAT(pattern, DoesntMatch("lorem|ipsum"));
  EXPECT_THAT(pattern, DoesntMatch("ipsumlorem"));
  EXPECT_THAT(pattern, DoesntMatch("ipsum|lorem"));
}

TEST_P(RegexpTest, EmptyBrackets) {
  auto const status_or_pattern = Parse("()");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {{""}}));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
}

TEST_P(RegexpTest, EmptyNonCapturingBrackets) {
  auto const status_or_pattern = Parse("(?:)");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
}

TEST_P(RegexpTest, UnmatchedBrackets) {
  EXPECT_THAT(Parse("("), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse(")"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse(")("), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("(()"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("())"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(RegexpTest, Brackets) {
  auto const status_or_pattern = Parse("(a)");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {{"a"}}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("anchor"));
  EXPECT_THAT(pattern, DoesntMatch("banana"));
}

TEST_P(RegexpTest, NonCapturingBrackets) {
  auto const status_or_pattern = Parse("(?:a)");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("anchor"));
  EXPECT_THAT(pattern, DoesntMatch("banana"));
}

TEST_P(RegexpTest, IpsumInBrackets) {
  auto const status_or_pattern = Parse("lorem(ipsum)dolor");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, DoesntMatch("dolor"));
  EXPECT_THAT(pattern, DoesntMatch("loremdolor"));
  EXPECT_THAT(pattern, DoesntMatch("loremidolor"));
  EXPECT_THAT(pattern, Matches("loremipsumdolor", {{"ipsum"}}));
}

TEST_P(RegexpTest, IpsumInNonCapturingBrackets) {
  auto const status_or_pattern = Parse("lorem(?:ipsum)dolor");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, DoesntMatch("dolor"));
  EXPECT_THAT(pattern, DoesntMatch("loremdolor"));
  EXPECT_THAT(pattern, DoesntMatch("loremidolor"));
  EXPECT_THAT(pattern, Matches("loremipsumdolor", {}));
}

TEST_P(RegexpTest, NestedBrackets) {
  auto const status_or_pattern = Parse("lorem(ipsum(dolor)amet)adipisci");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsum"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsumdolor"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsumdoloramet"));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, DoesntMatch("dolor"));
  EXPECT_THAT(pattern, DoesntMatch("amet"));
  EXPECT_THAT(pattern, DoesntMatch("adipisci"));
  EXPECT_THAT(pattern, DoesntMatch("ipsumdoloramet"));
  EXPECT_THAT(pattern, Matches("loremipsumdolorametadipisci", {{"ipsumdoloramet"}, {"dolor"}}));
}

TEST_P(RegexpTest, CapturingBracketsNestedInNonCapturingBrackets) {
  auto const status_or_pattern = Parse("lorem(?:ipsum(dolor)amet)adipisci");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsum"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsumdolor"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsumdoloramet"));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, DoesntMatch("dolor"));
  EXPECT_THAT(pattern, DoesntMatch("amet"));
  EXPECT_THAT(pattern, DoesntMatch("adipisci"));
  EXPECT_THAT(pattern, DoesntMatch("ipsumdoloramet"));
  EXPECT_THAT(pattern, Matches("loremipsumdolorametadipisci", {{"dolor"}}));
}

TEST_P(RegexpTest, NonCapturingBracketsNestedInCapturingBrackets) {
  auto const status_or_pattern = Parse("lorem(ipsum(?:dolor)amet)adipisci");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsum"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsumdolor"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsumdoloramet"));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, DoesntMatch("dolor"));
  EXPECT_THAT(pattern, DoesntMatch("amet"));
  EXPECT_THAT(pattern, DoesntMatch("adipisci"));
  EXPECT_THAT(pattern, DoesntMatch("ipsumdoloramet"));
  EXPECT_THAT(pattern, Matches("loremipsumdolorametadipisci", {{"ipsumdoloramet"}}));
}

TEST_P(RegexpTest, NestedNonCapturingBrackets) {
  auto const status_or_pattern = Parse("lorem(?:ipsum(?:dolor)amet)adipisci");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsum"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsumdolor"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsumdoloramet"));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, DoesntMatch("dolor"));
  EXPECT_THAT(pattern, DoesntMatch("amet"));
  EXPECT_THAT(pattern, DoesntMatch("adipisci"));
  EXPECT_THAT(pattern, DoesntMatch("ipsumdoloramet"));
  EXPECT_THAT(pattern, Matches("loremipsumdolorametadipisci", {}));
}

TEST_P(RegexpTest, PeeringBrackets) {
  auto const status_or_pattern = Parse("lorem(ipsum)dolor(amet)adipisci");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsum"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsumdolor"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsumdoloramet"));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, DoesntMatch("dolor"));
  EXPECT_THAT(pattern, DoesntMatch("amet"));
  EXPECT_THAT(pattern, DoesntMatch("adipisci"));
  EXPECT_THAT(pattern, Matches("loremipsumdolorametadipisci", {{"ipsum"}, {"amet"}}));
}

TEST_P(RegexpTest, CapturingBracketsPeeringWithNonCapturingBrackets) {
  auto const status_or_pattern = Parse("lorem(ipsum)dolor(?:amet)adipisci");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsum"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsumdolor"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsumdoloramet"));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, DoesntMatch("dolor"));
  EXPECT_THAT(pattern, DoesntMatch("amet"));
  EXPECT_THAT(pattern, DoesntMatch("adipisci"));
  EXPECT_THAT(pattern, Matches("loremipsumdolorametadipisci", {{"ipsum"}}));
}

TEST_P(RegexpTest, NonCapturingBracketsPeeringWithCapturingBrackets) {
  auto const status_or_pattern = Parse("lorem(?:ipsum)dolor(amet)adipisci");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsum"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsumdolor"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsumdoloramet"));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, DoesntMatch("dolor"));
  EXPECT_THAT(pattern, DoesntMatch("amet"));
  EXPECT_THAT(pattern, DoesntMatch("adipisci"));
  EXPECT_THAT(pattern, Matches("loremipsumdolorametadipisci", {{"amet"}}));
}

TEST_P(RegexpTest, PeeringNonCapturingBrackets) {
  auto const status_or_pattern = Parse("lorem(?:ipsum)dolor(?:amet)adipisci");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsum"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsumdolor"));
  EXPECT_THAT(pattern, DoesntMatch("loremipsumdoloramet"));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, DoesntMatch("dolor"));
  EXPECT_THAT(pattern, DoesntMatch("amet"));
  EXPECT_THAT(pattern, DoesntMatch("adipisci"));
  EXPECT_THAT(pattern, Matches("loremipsumdolorametadipisci", {}));
}

TEST_P(RegexpTest, InvalidNonCapturingBrackets) {
  EXPECT_THAT(Parse("lorem(?ipsum)dolor"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(RegexpTest, EpsilonLoop1) {
  auto const status_or_pattern = Parse("(|a)+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {{""}}));
  EXPECT_THAT(pattern, Matches("a", {{"a"}}));
  EXPECT_THAT(pattern, AnyOf(Matches("aa", {{"aa"}}), Matches("aa", {{"a", "a"}})));
  EXPECT_THAT(pattern, AnyOf(Matches("aaa", {{"aaa"}}), Matches("aaa", {{"aa", "a"}}),
                             Matches("aaa", {{"a", "aa"}}), Matches("aaa", {{"a", "a", "a"}})));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
}

TEST_P(RegexpTest, EpsilonLoop2) {
  auto const status_or_pattern = Parse("(a|)+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {{""}}));
  EXPECT_THAT(pattern, Matches("a", {{"a"}}));
  EXPECT_THAT(pattern, AnyOf(Matches("aa", {{"aa"}}), Matches("aa", {{"a", "a"}})));
  EXPECT_THAT(pattern, AnyOf(Matches("aaa", {{"aaa"}}), Matches("aaa", {{"aa", "a"}}),
                             Matches("aaa", {{"a", "aa"}}), Matches("aaa", {{"a", "a", "a"}})));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
}

TEST_P(RegexpTest, NonCapturingEpsilonLoop1) {
  auto const status_or_pattern = Parse("(?:|a)+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("aaa", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
}

TEST_P(RegexpTest, NonCapturingEpsilonLoop2) {
  auto const status_or_pattern = Parse("(?:a|)+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("aaa", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
}

TEST_P(RegexpTest, ChainLoops) {
  auto const status_or_pattern = Parse("a*b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("aaa", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, Matches("bbb", {}));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, Matches("ab", {}));
  EXPECT_THAT(pattern, Matches("aab", {}));
  EXPECT_THAT(pattern, Matches("abb", {}));
  EXPECT_THAT(pattern, Matches("aabb", {}));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
  EXPECT_THAT(pattern, DoesntMatch("baa"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
  EXPECT_THAT(pattern, DoesntMatch("ac"));
  EXPECT_THAT(pattern, DoesntMatch("ca"));
  EXPECT_THAT(pattern, DoesntMatch("bc"));
  EXPECT_THAT(pattern, DoesntMatch("cb"));
}

TEST_P(RegexpTest, ChainStarAndPlus) {
  auto const status_or_pattern = Parse("a*b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, Matches("bbb", {}));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, Matches("ab", {}));
  EXPECT_THAT(pattern, Matches("aab", {}));
  EXPECT_THAT(pattern, Matches("abb", {}));
  EXPECT_THAT(pattern, Matches("aabb", {}));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
  EXPECT_THAT(pattern, DoesntMatch("baa"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
  EXPECT_THAT(pattern, DoesntMatch("ac"));
  EXPECT_THAT(pattern, DoesntMatch("ca"));
  EXPECT_THAT(pattern, DoesntMatch("bc"));
  EXPECT_THAT(pattern, DoesntMatch("cb"));
}

TEST_P(RegexpTest, ChainStarAndMaybe) {
  auto const status_or_pattern = Parse("a*b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("aaa", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("bbb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, Matches("ab", {}));
  EXPECT_THAT(pattern, Matches("aab", {}));
  EXPECT_THAT(pattern, DoesntMatch("abb"));
  EXPECT_THAT(pattern, DoesntMatch("aabb"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
  EXPECT_THAT(pattern, DoesntMatch("baa"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
  EXPECT_THAT(pattern, DoesntMatch("ac"));
  EXPECT_THAT(pattern, DoesntMatch("ca"));
  EXPECT_THAT(pattern, DoesntMatch("bc"));
  EXPECT_THAT(pattern, DoesntMatch("cb"));
}

TEST_P(RegexpTest, ChainStarAndQuantifier) {
  auto const status_or_pattern = Parse("a*b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, DoesntMatch("bbb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("aab"));
  EXPECT_THAT(pattern, Matches("abb", {}));
  EXPECT_THAT(pattern, Matches("aabb", {}));
  EXPECT_THAT(pattern, DoesntMatch("aabbb"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
  EXPECT_THAT(pattern, DoesntMatch("baa"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
  EXPECT_THAT(pattern, DoesntMatch("ac"));
  EXPECT_THAT(pattern, DoesntMatch("ca"));
  EXPECT_THAT(pattern, DoesntMatch("bc"));
  EXPECT_THAT(pattern, DoesntMatch("cb"));
}

TEST_P(RegexpTest, ChainPlusAndStar) {
  auto const status_or_pattern = Parse("a+b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("aaa", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("bbb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, Matches("ab", {}));
  EXPECT_THAT(pattern, Matches("aab", {}));
  EXPECT_THAT(pattern, Matches("abb", {}));
  EXPECT_THAT(pattern, Matches("aabb", {}));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
  EXPECT_THAT(pattern, DoesntMatch("baa"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
  EXPECT_THAT(pattern, DoesntMatch("ac"));
  EXPECT_THAT(pattern, DoesntMatch("ca"));
  EXPECT_THAT(pattern, DoesntMatch("bc"));
  EXPECT_THAT(pattern, DoesntMatch("cb"));
}

TEST_P(RegexpTest, ChainPlusAndPlus) {
  auto const status_or_pattern = Parse("a+b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("bbb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, Matches("ab", {}));
  EXPECT_THAT(pattern, Matches("aab", {}));
  EXPECT_THAT(pattern, Matches("abb", {}));
  EXPECT_THAT(pattern, Matches("aabb", {}));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
  EXPECT_THAT(pattern, DoesntMatch("baa"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
  EXPECT_THAT(pattern, DoesntMatch("ac"));
  EXPECT_THAT(pattern, DoesntMatch("ca"));
  EXPECT_THAT(pattern, DoesntMatch("bc"));
  EXPECT_THAT(pattern, DoesntMatch("cb"));
}

TEST_P(RegexpTest, ChainPlusAndMaybe) {
  auto const status_or_pattern = Parse("a+b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("aaa", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("bbb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, Matches("ab", {}));
  EXPECT_THAT(pattern, Matches("aab", {}));
  EXPECT_THAT(pattern, DoesntMatch("abb"));
  EXPECT_THAT(pattern, DoesntMatch("aabb"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
  EXPECT_THAT(pattern, DoesntMatch("baa"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
  EXPECT_THAT(pattern, DoesntMatch("ac"));
  EXPECT_THAT(pattern, DoesntMatch("ca"));
  EXPECT_THAT(pattern, DoesntMatch("bc"));
  EXPECT_THAT(pattern, DoesntMatch("cb"));
}

TEST_P(RegexpTest, ChainPlusAndQuantifier) {
  auto const status_or_pattern = Parse("a+b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("bbb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("aab"));
  EXPECT_THAT(pattern, Matches("abb", {}));
  EXPECT_THAT(pattern, Matches("aabb", {}));
  EXPECT_THAT(pattern, DoesntMatch("aabbb"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
  EXPECT_THAT(pattern, DoesntMatch("baa"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
  EXPECT_THAT(pattern, DoesntMatch("ac"));
  EXPECT_THAT(pattern, DoesntMatch("ca"));
  EXPECT_THAT(pattern, DoesntMatch("bc"));
  EXPECT_THAT(pattern, DoesntMatch("cb"));
}

TEST_P(RegexpTest, ChainMaybeAndStar) {
  auto const status_or_pattern = Parse("a?b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, Matches("bbb", {}));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, Matches("ab", {}));
  EXPECT_THAT(pattern, DoesntMatch("aab"));
  EXPECT_THAT(pattern, Matches("abb", {}));
  EXPECT_THAT(pattern, DoesntMatch("aabb"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
  EXPECT_THAT(pattern, DoesntMatch("baa"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
  EXPECT_THAT(pattern, DoesntMatch("ac"));
  EXPECT_THAT(pattern, DoesntMatch("ca"));
  EXPECT_THAT(pattern, DoesntMatch("bc"));
  EXPECT_THAT(pattern, DoesntMatch("cb"));
}

TEST_P(RegexpTest, ChainMaybeAndPlus) {
  auto const status_or_pattern = Parse("a?b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, Matches("bbb", {}));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, Matches("ab", {}));
  EXPECT_THAT(pattern, DoesntMatch("aab"));
  EXPECT_THAT(pattern, Matches("abb", {}));
  EXPECT_THAT(pattern, DoesntMatch("aabb"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
  EXPECT_THAT(pattern, DoesntMatch("baa"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
  EXPECT_THAT(pattern, DoesntMatch("ac"));
  EXPECT_THAT(pattern, DoesntMatch("ca"));
  EXPECT_THAT(pattern, DoesntMatch("bc"));
  EXPECT_THAT(pattern, DoesntMatch("cb"));
}

TEST_P(RegexpTest, ChainMaybeAndMaybe) {
  auto const status_or_pattern = Parse("a?b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("bbb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, Matches("ab", {}));
  EXPECT_THAT(pattern, DoesntMatch("aab"));
  EXPECT_THAT(pattern, DoesntMatch("abb"));
  EXPECT_THAT(pattern, DoesntMatch("aabb"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
  EXPECT_THAT(pattern, DoesntMatch("baa"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
  EXPECT_THAT(pattern, DoesntMatch("ac"));
  EXPECT_THAT(pattern, DoesntMatch("ca"));
  EXPECT_THAT(pattern, DoesntMatch("bc"));
  EXPECT_THAT(pattern, DoesntMatch("cb"));
}

TEST_P(RegexpTest, ChainMaybeAndQuantifier) {
  auto const status_or_pattern = Parse("a?b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, DoesntMatch("bbb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("aab"));
  EXPECT_THAT(pattern, Matches("abb", {}));
  EXPECT_THAT(pattern, DoesntMatch("aabb"));
  EXPECT_THAT(pattern, DoesntMatch("aabbb"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
  EXPECT_THAT(pattern, DoesntMatch("baa"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
  EXPECT_THAT(pattern, DoesntMatch("ac"));
  EXPECT_THAT(pattern, DoesntMatch("ca"));
  EXPECT_THAT(pattern, DoesntMatch("bc"));
  EXPECT_THAT(pattern, DoesntMatch("cb"));
}

TEST_P(RegexpTest, ChainQuantifierAndStar) {
  auto const status_or_pattern = Parse("a{2}b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("bbb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, Matches("aab", {}));
  EXPECT_THAT(pattern, DoesntMatch("abb"));
  EXPECT_THAT(pattern, Matches("aabb", {}));
  EXPECT_THAT(pattern, DoesntMatch("aaabb"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
  EXPECT_THAT(pattern, DoesntMatch("baa"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
  EXPECT_THAT(pattern, DoesntMatch("ac"));
  EXPECT_THAT(pattern, DoesntMatch("ca"));
  EXPECT_THAT(pattern, DoesntMatch("bc"));
  EXPECT_THAT(pattern, DoesntMatch("cb"));
}

TEST_P(RegexpTest, ChainQuantifierAndPlus) {
  auto const status_or_pattern = Parse("a{2}b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("bbb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, Matches("aab", {}));
  EXPECT_THAT(pattern, DoesntMatch("abb"));
  EXPECT_THAT(pattern, Matches("aabb", {}));
  EXPECT_THAT(pattern, DoesntMatch("aaabb"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
  EXPECT_THAT(pattern, DoesntMatch("baa"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
  EXPECT_THAT(pattern, DoesntMatch("ac"));
  EXPECT_THAT(pattern, DoesntMatch("ca"));
  EXPECT_THAT(pattern, DoesntMatch("bc"));
  EXPECT_THAT(pattern, DoesntMatch("cb"));
}

TEST_P(RegexpTest, ChainQuantifierAndMaybe) {
  auto const status_or_pattern = Parse("a{2}b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("bbb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, Matches("aab", {}));
  EXPECT_THAT(pattern, DoesntMatch("abb"));
  EXPECT_THAT(pattern, DoesntMatch("aabb"));
  EXPECT_THAT(pattern, DoesntMatch("aaabb"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
  EXPECT_THAT(pattern, DoesntMatch("baa"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
  EXPECT_THAT(pattern, DoesntMatch("ac"));
  EXPECT_THAT(pattern, DoesntMatch("ca"));
  EXPECT_THAT(pattern, DoesntMatch("bc"));
  EXPECT_THAT(pattern, DoesntMatch("cb"));
}

TEST_P(RegexpTest, ChainQuantifiers) {
  auto const status_or_pattern = Parse("a{3}b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("bbb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("aab"));
  EXPECT_THAT(pattern, DoesntMatch("abb"));
  EXPECT_THAT(pattern, DoesntMatch("aabb"));
  EXPECT_THAT(pattern, Matches("aaabb", {}));
  EXPECT_THAT(pattern, DoesntMatch("aaabbb"));
  EXPECT_THAT(pattern, DoesntMatch("aaaabbb"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
  EXPECT_THAT(pattern, DoesntMatch("baa"));
  EXPECT_THAT(pattern, DoesntMatch("aba"));
  EXPECT_THAT(pattern, DoesntMatch("bab"));
  EXPECT_THAT(pattern, DoesntMatch("ac"));
  EXPECT_THAT(pattern, DoesntMatch("ca"));
  EXPECT_THAT(pattern, DoesntMatch("bc"));
  EXPECT_THAT(pattern, DoesntMatch("cb"));
}

TEST_P(RegexpTest, PipeLoops) {
  auto const status_or_pattern = Parse("a*|b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
}

TEST_P(RegexpTest, StarOrPlus) {
  auto const status_or_pattern = Parse("a*|b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
}

TEST_P(RegexpTest, StarOrMaybe) {
  auto const status_or_pattern = Parse("a*|b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
}

TEST_P(RegexpTest, StarOrQuantifier) {
  auto const status_or_pattern = Parse("a*|b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, DoesntMatch("bbb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("abb"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
}

TEST_P(RegexpTest, PlusOrStar) {
  auto const status_or_pattern = Parse("a+|b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
}

TEST_P(RegexpTest, PlusOrPlus) {
  auto const status_or_pattern = Parse("a+|b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
}

TEST_P(RegexpTest, PlusOrMaybe) {
  auto const status_or_pattern = Parse("a+|b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
}

TEST_P(RegexpTest, PlusOrQuantifier) {
  auto const status_or_pattern = Parse("a+|b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, DoesntMatch("bbb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("abb"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
}

TEST_P(RegexpTest, MaybeOrStar) {
  auto const status_or_pattern = Parse("a?|b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
}

TEST_P(RegexpTest, MaybeOrPlus) {
  auto const status_or_pattern = Parse("a?|b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
}

TEST_P(RegexpTest, MaybeOrMaybe) {
  auto const status_or_pattern = Parse("a?|b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
}

TEST_P(RegexpTest, MaybeOrQuantifier) {
  auto const status_or_pattern = Parse("a?|b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, DoesntMatch("bbb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("abb"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("bba"));
}

TEST_P(RegexpTest, QuantifierOrStar) {
  auto const status_or_pattern = Parse("a{2}|b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aab"));
  EXPECT_THAT(pattern, DoesntMatch("aabb"));
}

TEST_P(RegexpTest, QuantifierOrPlus) {
  auto const status_or_pattern = Parse("a{2}|b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, Matches("bb", {}));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aab"));
  EXPECT_THAT(pattern, DoesntMatch("aabb"));
}

TEST_P(RegexpTest, QuantifierOrMaybe) {
  auto const status_or_pattern = Parse("a{2}|b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aab"));
  EXPECT_THAT(pattern, DoesntMatch("aabb"));
}

TEST_P(RegexpTest, QuantifierOrQuantifier) {
  auto const status_or_pattern = Parse("a{2}|b{3}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, Matches("aa", {}));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, Matches("bbb", {}));
  EXPECT_THAT(pattern, DoesntMatch("bbbb"));
  EXPECT_THAT(pattern, DoesntMatch("c"));
  EXPECT_THAT(pattern, DoesntMatch("cc"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("ba"));
  EXPECT_THAT(pattern, DoesntMatch("aab"));
  EXPECT_THAT(pattern, DoesntMatch("aabb"));
}

TEST_P(RegexpTest, CaptureMultipleTimes) {
  auto const status_or_pattern = Parse("((ab.)*)");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {{""}, {}}));
  EXPECT_THAT(pattern, Matches("abcabdabe", {{"abcabdabe"}, {"abc", "abd", "abe"}}));
}

TEST_P(RegexpTest, CantMergeLoopEndpoints) {
  auto const status_or_pattern = Parse("(lore(m))*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {{}, {}}));
  EXPECT_THAT(pattern, Matches("lorem", {{"lorem"}, {"m"}}));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, Matches("loremlorem", {{"lorem", "lorem"}, {"m", "m"}}));
  EXPECT_THAT(pattern, DoesntMatch("loremipsum"));
  EXPECT_THAT(pattern, DoesntMatch("ipsumlorem"));
  EXPECT_THAT(pattern, Matches("loremloremlorem", {{"lorem", "lorem", "lorem"}, {"m", "m", "m"}}));
}

TEST_P(RegexpTest, CantMergeLoopEndpointsOfPrefix) {
  auto const status_or_pattern = Parse("(lore(m))*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, MatchesPrefixOf("", {{}, {}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("lorem", {{"lorem"}, {"m"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("ipsum", {{}, {}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("loremlorem", {{"lorem", "lorem"}, {"m", "m"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("loremipsum", {{"lorem"}, {"m"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("ipsumlorem", {{}, {}}));
  EXPECT_THAT(pattern,
              MatchesPrefixOf("loremloremlorem", {{"lorem", "lorem", "lorem"}, {"m", "m", "m"}}));
}

TEST_P(RegexpTest, Fork) {
  auto const status_or_pattern = Parse("lorem(ipsum|dolor)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  auto const stepper1 = pattern->MakeStepper();
  EXPECT_TRUE(stepper1->Step("lorem"));
  auto const stepper2 = stepper1->Clone();
  EXPECT_TRUE(stepper1->Step("ipsum") && stepper1->Finish());
  EXPECT_TRUE(stepper2->Step("dolor") && stepper2->Finish());
}

TEST_P(RegexpTest, SearchWithKleeneStars) {
  auto const status_or_pattern = Parse(".*do+lor.*");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("lorem ipsum dolor sic amat", {}));
  EXPECT_THAT(pattern, Matches("lorem ipsum dooolor sic amat", {}));
  EXPECT_THAT(pattern, DoesntMatch("lorem ipsum color sic amat"));
  EXPECT_THAT(pattern, DoesntMatch("lorem ipsum dolet et amat"));
}

TEST_P(RegexpTest, SearchWithPartialMatch) {
  auto const status_or_pattern = Parse("do+lor");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern->PartialMatch("lorem ipsum dolor sic amat"), Optional(IsEmpty()));
  EXPECT_THAT(pattern->PartialMatch("lorem ipsum dooolor sic amat"), Optional(IsEmpty()));
  EXPECT_EQ(pattern->PartialMatch("lorem ipsum color sic amat"), std::nullopt);
  EXPECT_EQ(pattern->PartialMatch("lorem ipsum dolet et amat"), std::nullopt);
}

TEST_P(RegexpTest, SearchWithCapturingPartialMatch) {
  auto const status_or_pattern = Parse("(do+lor)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern->PartialMatch("lorem ipsum dolor sic amat"),
              Optional(ElementsAre(ElementsAre("dolor"))));
  EXPECT_THAT(pattern->PartialMatch("lorem ipsum dooolor sic amat"),
              Optional(ElementsAre(ElementsAre("dooolor"))));
  EXPECT_EQ(pattern->PartialMatch("lorem ipsum color sic amat"), std::nullopt);
  EXPECT_EQ(pattern->PartialMatch("lorem ipsum dolet et amat"), std::nullopt);
}

TEST_P(RegexpTest, AmbiguousMatch) {
  auto const status_or_pattern = Parse("(.*) (.*) (.*)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("lorem ipsum"));
  EXPECT_THAT(pattern, Matches("lorem ipsum dolor", {{"lorem"}, {"ipsum"}, {"dolor"}}));
  std::string_view input = "lorem ipsum dolor amet";
  EXPECT_THAT(pattern, AnyOf(Matches(input, {{"lorem"}, {"ipsum"}, {"dolor amet"}}),
                             Matches(input, {{"lorem"}, {"ipsum dolor"}, {"amet"}}),
                             Matches(input, {{"lorem ipsum"}, {"dolor"}, {"amet"}})));
  input = "lorem ipsum dolor amet consectetur";
  EXPECT_THAT(pattern, AnyOf(Matches(input, {{"lorem"}, {"ipsum"}, {"dolor amet consectetur"}}),
                             Matches(input, {{"lorem"}, {"ipsum dolor"}, {"amet consectetur"}}),
                             Matches(input, {{"lorem"}, {"ipsum dolor amet"}, {"consectetur"}}),
                             Matches(input, {{"lorem ipsum"}, {"dolor"}, {"amet consectetur"}}),
                             Matches(input, {{"lorem ipsum"}, {"dolor amet"}, {"consectetur"}}),
                             Matches(input, {{"lorem ipsum dolor"}, {"amet"}, {"consectetur"}})));
}

TEST_P(RegexpTest, AmbiguousPrefixMatch) {
  auto const status_or_pattern = Parse("([^END-]*) ([^END-]*) ([^END-]*)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatchPrefixOf("lorem"));
  EXPECT_THAT(pattern, DoesntMatchPrefixOf("lorem-END"));
  EXPECT_THAT(pattern, DoesntMatchPrefixOf("lorem ipsum"));
  EXPECT_THAT(pattern, DoesntMatchPrefixOf("lorem ipsum-END"));
  EXPECT_THAT(pattern, MatchesPrefixOf("lorem ipsum dolor", {{"lorem"}, {"ipsum"}, {"dolor"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("lorem ipsum dolor-END", {{"lorem"}, {"ipsum"}, {"dolor"}}));
  std::string_view input = "lorem ipsum dolor amet";
  EXPECT_THAT(pattern, AnyOf(MatchesPrefixOf(input, {{"lorem"}, {"ipsum"}, {"dolor amet"}}),
                             MatchesPrefixOf(input, {{"lorem"}, {"ipsum dolor"}, {"amet"}}),
                             MatchesPrefixOf(input, {{"lorem ipsum"}, {"dolor"}, {"amet"}})));
  input = "lorem ipsum dolor amet-END";
  EXPECT_THAT(pattern, AnyOf(MatchesPrefixOf(input, {{"lorem"}, {"ipsum"}, {"dolor amet"}}),
                             MatchesPrefixOf(input, {{"lorem"}, {"ipsum dolor"}, {"amet"}}),
                             MatchesPrefixOf(input, {{"lorem ipsum"}, {"dolor"}, {"amet"}})));
  input = "lorem ipsum dolor amet consectetur";
  EXPECT_THAT(pattern,
              AnyOf(MatchesPrefixOf(input, {{"lorem"}, {"ipsum"}, {"dolor amet consectetur"}}),
                    MatchesPrefixOf(input, {{"lorem"}, {"ipsum dolor"}, {"amet consectetur"}}),
                    MatchesPrefixOf(input, {{"lorem"}, {"ipsum dolor amet"}, {"consectetur"}}),
                    MatchesPrefixOf(input, {{"lorem ipsum"}, {"dolor"}, {"amet consectetur"}}),
                    MatchesPrefixOf(input, {{"lorem ipsum"}, {"dolor amet"}, {"consectetur"}}),
                    MatchesPrefixOf(input, {{"lorem ipsum dolor"}, {"amet"}, {"consectetur"}})));
  input = "lorem ipsum dolor amet consectetur-END";
  EXPECT_THAT(pattern,
              AnyOf(MatchesPrefixOf(input, {{"lorem"}, {"ipsum"}, {"dolor amet consectetur"}}),
                    MatchesPrefixOf(input, {{"lorem"}, {"ipsum dolor"}, {"amet consectetur"}}),
                    MatchesPrefixOf(input, {{"lorem"}, {"ipsum dolor amet"}, {"consectetur"}}),
                    MatchesPrefixOf(input, {{"lorem ipsum"}, {"dolor"}, {"amet consectetur"}}),
                    MatchesPrefixOf(input, {{"lorem ipsum"}, {"dolor amet"}, {"consectetur"}}),
                    MatchesPrefixOf(input, {{"lorem ipsum dolor"}, {"amet"}, {"consectetur"}})));
}

TEST_P(RegexpTest, NotAllCaptured) {
  auto const status_or_pattern = Parse("sator(arepo(tenet)|(opera)(rotas))");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("satorarepotenet", {{"arepotenet"}, {"tenet"}, {}, {}}));
  EXPECT_THAT(pattern, Matches("satoroperarotas", {{"operarotas"}, {}, {"opera"}, {"rotas"}}));
}

TEST_P(RegexpTest, HeavyBacktracker) {
  auto const status_or_pattern = Parse(
      "a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("aaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_THAT(pattern, DoesntMatch("aaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern, Matches("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", {}));
  EXPECT_THAT(pattern,
              DoesntMatch("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_THAT(pattern,
              DoesntMatch("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
}

TEST_P(RegexpTest, InvalidPrefixPattern) {
  EXPECT_THAT(Parse("foo("), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(RegexpTest, EmptyPrefixOfEmptyString) {
  auto const status_or_pattern = Parse("");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, MatchesPrefixOf("", {}));
}

TEST_P(RegexpTest, NonEmptyPrefixOfEmptyString) {
  auto const status_or_pattern = Parse("lorem");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatchPrefixOf(""));
}

TEST_P(RegexpTest, ProperPrefix) {
  auto const status_or_pattern = Parse("(lorem)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, MatchesPrefixOf("loremipsum", {{"lorem"}}));
}

TEST_P(RegexpTest, ImproperPrefix) {
  auto const status_or_pattern = Parse("(lorem)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, MatchesPrefixOf("lorem", {{"lorem"}}));
}

TEST_P(RegexpTest, LongestPrefix) {
  auto const status_or_pattern = Parse("(lorem.*)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, MatchesPrefixOf("loremipsum", {{"loremipsum"}}));
}

TEST_P(RegexpTest, DeadPrefixBranch1) {
  auto const status_or_pattern = Parse("(lorem(ipsum)?)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, MatchesPrefixOf("loremips", {{"lorem"}, {}}));
}

TEST_P(RegexpTest, DeadPrefixBranch2) {
  auto const status_or_pattern = Parse("(lorem)*");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, MatchesPrefixOf("loremlor", {{"lorem"}}));
}

TEST_P(RegexpTest, PrefixPatternWithCapture) {
  auto const status_or_pattern = Parse("(lorem (.*) )");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, MatchesPrefixOf("lorem ipsum dolor", {{"lorem ipsum "}, {"ipsum"}}));
}

TEST_P(RegexpTest, HeavyPrefixBacktracker) {
  auto const status_or_pattern = Parse(
      "(a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatchPrefixOf(""));
  EXPECT_THAT(pattern, DoesntMatchPrefixOf("b"));
  EXPECT_THAT(pattern, DoesntMatchPrefixOf("ab"));
  EXPECT_THAT(pattern, DoesntMatchPrefixOf("a"));
  EXPECT_THAT(pattern, DoesntMatchPrefixOf("aa"));
  EXPECT_THAT(pattern, DoesntMatchPrefixOf("aaa"));
  EXPECT_THAT(pattern, DoesntMatchPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_THAT(pattern, DoesntMatchPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                       {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern,
              MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                              {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern,
              MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                              {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern,
              MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                              {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern,
              MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                              {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern,
              MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                              {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern,
              MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                              {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern,
              MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                              {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern,
              MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                              {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern,
              MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                              {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern,
              MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                              {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
  EXPECT_THAT(pattern,
              MatchesPrefixOf("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                              {{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}}));
}

INSTANTIATE_TEST_SUITE_P(RegexpTest, RegexpTest,
                         Values(RegexpTestParams{.force_nfa = false, .use_stepper = false},
                                RegexpTestParams{.force_nfa = false, .use_stepper = true},
                                RegexpTestParams{.force_nfa = true, .use_stepper = false},
                                RegexpTestParams{.force_nfa = true, .use_stepper = true}));

// Steppers do not currently support assertions, so these tests are executed aside without steppers.
//
// TODO: update when steppers support assertions.
class AssertedRegexpTest : public RegexpTest {};

TEST_P(AssertedRegexpTest, StartAnchor) {
  auto const status_or_pattern = Parse("(^lorem)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_EQ(pattern->PartialMatch("ipsum lorem"), std::nullopt);
  EXPECT_THAT(pattern->PartialMatch("lorem ipsum"), Optional(ElementsAre(ElementsAre("lorem"))));
}

TEST_P(AssertedRegexpTest, EndAnchor) {
  auto const status_or_pattern = Parse("(lorem$)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_EQ(pattern->PartialMatch("lorem ipsum"), std::nullopt);
  EXPECT_THAT(pattern->PartialMatch("ipsum lorem"), Optional(ElementsAre(ElementsAre("lorem"))));
}

TEST_P(AssertedRegexpTest, AnchoredPartialMatch) {
  auto const status_or_pattern = Parse("(^ipsum$)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_EQ(pattern->PartialMatch("lorem ipsum dolor"), std::nullopt);
  EXPECT_THAT(pattern->PartialMatch("ipsum"), Optional(ElementsAre(ElementsAre("ipsum"))));
}

TEST_P(AssertedRegexpTest, WordBoundary) {
  auto const status_or_pattern = Parse(".\\b.");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("A ", {}));
  EXPECT_THAT(pattern, Matches(" B", {}));
  EXPECT_THAT(pattern, Matches("c ", {}));
  EXPECT_THAT(pattern, Matches(" d", {}));
  EXPECT_THAT(pattern, Matches("0 ", {}));
  EXPECT_THAT(pattern, Matches(" 1", {}));
  EXPECT_THAT(pattern, Matches("_ ", {}));
  EXPECT_THAT(pattern, Matches(" _", {}));
  EXPECT_THAT(pattern, DoesntMatch("Ab"));
  EXPECT_THAT(pattern, DoesntMatch("cD"));
  EXPECT_THAT(pattern, DoesntMatch("2e"));
  EXPECT_THAT(pattern, DoesntMatch("f3"));
  EXPECT_THAT(pattern, DoesntMatch("_4"));
  EXPECT_THAT(pattern, DoesntMatch("5_"));
  EXPECT_THAT(pattern, DoesntMatch(". "));
  EXPECT_THAT(pattern, DoesntMatch(" ."));
}

TEST_P(AssertedRegexpTest, NotWordBoundary) {
  auto const status_or_pattern = Parse(".\\B.");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch("A "));
  EXPECT_THAT(pattern, DoesntMatch(" B"));
  EXPECT_THAT(pattern, DoesntMatch("c "));
  EXPECT_THAT(pattern, DoesntMatch(" d"));
  EXPECT_THAT(pattern, DoesntMatch("0 "));
  EXPECT_THAT(pattern, DoesntMatch(" 1"));
  EXPECT_THAT(pattern, DoesntMatch("_ "));
  EXPECT_THAT(pattern, DoesntMatch(" _"));
  EXPECT_THAT(pattern, Matches("Ab", {}));
  EXPECT_THAT(pattern, Matches("cD", {}));
  EXPECT_THAT(pattern, Matches("2e", {}));
  EXPECT_THAT(pattern, Matches("f3", {}));
  EXPECT_THAT(pattern, Matches("_4", {}));
  EXPECT_THAT(pattern, Matches("5_", {}));
  EXPECT_THAT(pattern, Matches(". ", {}));
  EXPECT_THAT(pattern, Matches(" .", {}));
}

TEST_P(AssertedRegexpTest, WordBoundaries) {
  auto const status_or_pattern = Parse(".*(\\blorem\\b).*");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("dolorem ipsum lorem loremipsum", {{"lorem"}}));
}

TEST_P(AssertedRegexpTest, NotWordBoundaries) {
  auto const status_or_pattern = Parse(".*(..(\\Blorem\\B)..).*");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("ipsum lorem doloremdo lorem ipsum", {{"doloremdo"}, {"lorem"}}));
}

TEST_P(AssertedRegexpTest, WordBoundariesInPrefix) {
  auto const status_or_pattern = Parse(".*(\\blorem\\b)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, MatchesPrefixOf("dolorem ipsum lorem loremipsum", {{"lorem"}}));
}

TEST_P(AssertedRegexpTest, NotWordBoundariesInPrefix) {
  auto const status_or_pattern = Parse(".*(..(\\Blorem\\B)..)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern,
              MatchesPrefixOf("ipsum lorem doloremdo lorem ipsum", {{"doloremdo"}, {"lorem"}}));
}

TEST_P(AssertedRegexpTest, WordBoundariesAtStringBoundaries) {
  auto const status_or_pattern = Parse("(\\blorem\\b)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("lorem", {{"lorem"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("lorem", {{"lorem"}}));
}

TEST_P(AssertedRegexpTest, NotWordBoundariesNotAtStringBoundaries) {
  auto const status_or_pattern = Parse("(\\Blorem\\B)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatchPrefixOf("lorem"));
}

TEST_P(AssertedRegexpTest, SearchWordWithBoundaries) {
  auto const status_or_pattern = Parse("(\\blo?rem\\b)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern->PartialMatch("dolrem lorem lremipsum"),
              Optional(ElementsAre(ElementsAre("lorem"))));
}

TEST_P(AssertedRegexpTest, SearchWordWithoutBoundaries) {
  auto const status_or_pattern = Parse("(\\Blo?rem\\B)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern->PartialMatch("ipsum lremdo doloremdo dolrem"),
              Optional(ElementsAre(ElementsAre("lorem"))));
}

INSTANTIATE_TEST_SUITE_P(AssertedRegexpTest, AssertedRegexpTest,
                         Values(RegexpTestParams{.force_nfa = false, .use_stepper = false},
                                RegexpTestParams{.force_nfa = true, .use_stepper = false}));

}  // namespace
