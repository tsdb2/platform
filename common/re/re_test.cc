#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
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

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::_;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Values;
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

bool TestWithStepper(reffed_ptr<AbstractAutomaton> const& automaton, std::string_view const input) {
  auto const stepper = automaton->MakeStepper();
  return stepper->Step(input) && stepper->Finish();
}

bool TestSubstringWithStepper(reffed_ptr<AbstractAutomaton> const& automaton,
                              std::string_view const input, size_t const offset) {
  char const previous_character = offset > 0 ? input[offset - 1] : 0;
  auto const stepper = automaton->MakeStepper(previous_character);
  for (char const ch : input.substr(offset)) {
    if (stepper->Finish(ch)) {
      return true;
    } else if (!stepper->Step(ch)) {
      return false;
    }
  }
  return stepper->Finish();
}

bool TestPrefixWithStepper(reffed_ptr<AbstractAutomaton> const& automaton,
                           std::string_view const input) {
  return TestSubstringWithStepper(automaton, input, 0);
}

bool PartialTestWithStepper(reffed_ptr<AbstractAutomaton> const& automaton,
                            std::string_view const input) {
  if (TestPrefixWithStepper(automaton, input)) {
    return true;
  }
  if (automaton->AssertsBeginOfInput()) {
    return false;
  }
  for (size_t i = 1; i < input.size(); ++i) {
    if (TestSubstringWithStepper(automaton, input, i)) {
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

using TestMethod = bool (AbstractAutomaton::*)(std::string_view) const;
using StepperTestFunction = bool (*)(reffed_ptr<AbstractAutomaton> const&, std::string_view);

using MatchMethod =
    std::optional<AbstractAutomaton::CaptureSet> (AbstractAutomaton::*)(std::string_view) const;

using MatchArgsMethod = bool (AbstractAutomaton::*)(std::string_view,
                                                    absl::Span<std::string_view* const>) const;

template <TestMethod test_method, char const test_method_name[],
          StepperTestFunction stepper_test_function, MatchMethod match_method,
          char const match_method_name[], MatchArgsMethod match_args_method,
          char const match_args_method_name[]>
class GenericMatcher {
 public:
  using is_gtest_matcher = void;

  explicit GenericMatcher(std::string_view const input, bool const use_stepper, bool const negated)
      : input_(input), captures_(std::nullopt), use_stepper_(use_stepper), negated_(negated) {}

  ~GenericMatcher() = default;

  GenericMatcher(GenericMatcher const&) = default;
  GenericMatcher& operator=(GenericMatcher const&) = default;
  GenericMatcher(GenericMatcher&&) noexcept = default;
  GenericMatcher& operator=(GenericMatcher&&) noexcept = default;

  GenericMatcher With(std::vector<std::vector<std::string>> captures) && {
    captures_ = std::move(captures);
    return *this;
  }

  void DescribeTo(std::ostream* const os) const {
    *os << "matches \"" << absl::CEscape(input_) << "\"";
    if (captures_) {
      *os << " with " << PrintCaptures(*captures_);
    }
  }

  void DescribeNegationTo(std::ostream* const os) const {
    *os << "doesn't match \"" << absl::CEscape(input_) << "\"";
    if (captures_) {
      *os << " with " << PrintCaptures(*captures_);
    }
  }

  bool MatchAndExplain(reffed_ptr<AbstractAutomaton> const& automaton,
                       ::testing::MatchResultListener* const listener) const {
    bool const test_result = Test(automaton);
    if (test_result) {
      *listener << "matches";
    } else {
      *listener << "doesn't match";
    }
    auto const match_outcome = Match(automaton, listener);
    if ((match_outcome != Outcome::kNoMatch) != test_result) {
      *listener << ", " << match_method_name << "() results differ from " << test_method_name
                << "() result";
      return negated_;
    }
    if (test_result && match_outcome == Outcome::kUnexpected) {
      return false;
    }
    auto const match_args_outcome = MatchArgs(automaton, listener);
    if ((match_args_outcome != Outcome::kNoMatch) != test_result) {
      *listener << ", " << match_args_method_name << "() result differs from " << test_method_name
                << "() result";
      return negated_;
    }
    if (test_result && match_args_outcome == Outcome::kUnexpected) {
      return false;
    }
    return test_result;
  }

 private:
  enum class Outcome { kMatch, kNoMatch, kUnexpected };

  bool Test(reffed_ptr<AbstractAutomaton> const& automaton) const {
    if (use_stepper_) {
      return stepper_test_function(automaton, input_);
    } else {
      return (*automaton.*test_method)(input_);
    }
  }

  Outcome Match(reffed_ptr<AbstractAutomaton> const& automaton,
                ::testing::MatchResultListener* const listener) const {
    auto maybe_results = (*automaton.*match_method)(input_);
    if (!maybe_results.has_value()) {
      *listener << ", " << match_method_name << "() doesn't match";
      return Outcome::kNoMatch;
    }
    auto const& results = maybe_results.value();
    if (!captures_ || CheckMatchResults(results, *captures_)) {
      *listener << ", " << match_method_name << "() matches";
      return Outcome::kMatch;
    } else {
      *listener << ", " << match_method_name
                << "() matches with unexpected captures: " << PrintCaptures(results);
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
    if (!(*automaton.*match_args_method)(input_, arg_ptrs)) {
      *listener << ", " << match_args_method_name << "() doesn't match";
      return Outcome::kNoMatch;
    }
    if (!captures_ || CheckMatchArgs(args, *captures_)) {
      *listener << ", " << match_args_method_name << "() matches";
      return Outcome::kMatch;
    } else {
      *listener << ", " << match_args_method_name
                << "() matches with unexpected captures: " << PrintStringList(args);
      return Outcome::kUnexpected;
    }
  }

  std::string input_;
  std::optional<std::vector<std::vector<std::string>>> captures_;
  bool use_stepper_;
  bool negated_;
};

char constexpr kTestMethodName[] = "Test";
char constexpr kTestPrefixMethodName[] = "TestPrefix";
char constexpr kPartialTestMethodName[] = "PartialTest";

char constexpr kMatchMethodName[] = "Match";
char constexpr kMatchPrefixMethodName[] = "MatchPrefix";
char constexpr kPartialMatchMethodName[] = "PartialMatch";

char constexpr kMatchArgsMethodName[] = "MatchArgs";
char constexpr kMatchPrefixArgsMethodName[] = "MatchPrefixArgs";
char constexpr kPartialMatchArgsMethodName[] = "PartialMatchArgs";

using MatchesImpl = GenericMatcher<&AbstractAutomaton::Test, kTestMethodName, TestWithStepper,
                                   &AbstractAutomaton::Match, kMatchMethodName,
                                   &AbstractAutomaton::MatchArgs, kMatchArgsMethodName>;

using MatchesPrefixOfImpl =
    GenericMatcher<&AbstractAutomaton::TestPrefix, kTestPrefixMethodName, TestPrefixWithStepper,
                   &AbstractAutomaton::MatchPrefix, kMatchPrefixMethodName,
                   &AbstractAutomaton::MatchPrefixArgs, kMatchPrefixArgsMethodName>;

using PartiallyMatchesImpl =
    GenericMatcher<&AbstractAutomaton::PartialTest, kPartialTestMethodName, PartialTestWithStepper,
                   &AbstractAutomaton::PartialMatch, kPartialMatchMethodName,
                   &AbstractAutomaton::PartialMatchArgs, kPartialMatchArgsMethodName>;

struct RegexpTestParams {
  bool force_nfa;
  bool use_stepper;
};

class RegexpTest : public ::testing::TestWithParam<RegexpTestParams> {
 public:
  explicit RegexpTest() { TempNFA::force_nfa_for_testing = GetParam().force_nfa; }
  ~RegexpTest() override { TempNFA::force_nfa_for_testing = false; }

 protected:
  static bool CheckDeterministic(reffed_ptr<AbstractAutomaton> const& automaton);
  static bool CheckNotDeterministic(reffed_ptr<AbstractAutomaton> const& automaton);

  static auto Matches(std::string_view const input,
                      std::vector<std::vector<std::string>> captures) {
    return MatchesImpl(input, GetParam().use_stepper, /*negated=*/false).With(std::move(captures));
  }

  static auto DoesntMatch(std::string_view const input) {
    return Not(MatchesImpl(input, GetParam().use_stepper, /*negated=*/true));
  }

  static auto MatchesPrefixOf(std::string_view const input,
                              std::vector<std::vector<std::string>> captures) {
    return MatchesPrefixOfImpl(input, GetParam().use_stepper, /*negated=*/false)
        .With(std::move(captures));
  }

  static auto DoesntMatchPrefixOf(std::string_view const input) {
    return Not(MatchesPrefixOfImpl(input, GetParam().use_stepper, /*negated=*/true));
  }

  static auto PartiallyMatches(std::string_view const input,
                               std::vector<std::vector<std::string>> captures) {
    return PartiallyMatchesImpl(input, GetParam().use_stepper, /*negated=*/false)
        .With(std::move(captures));
  }

  static auto DoesntPartiallyMatch(std::string_view const input) {
    return Not(PartiallyMatchesImpl(input, GetParam().use_stepper, /*negated=*/true));
  }

 private:
  RegexpTest(RegexpTest const&) = delete;
  RegexpTest& operator=(RegexpTest const&) = delete;
  RegexpTest(RegexpTest&&) = delete;
  RegexpTest& operator=(RegexpTest&&) = delete;
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
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("hello"));
  EXPECT_THAT(pattern, Matches("", {}));
}

TEST_P(RegexpTest, SimpleCharacter) {
  auto const status_or_pattern = Parse("a");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("anchor"));
  EXPECT_THAT(pattern, DoesntMatch("banana"));
  EXPECT_THAT(pattern, DoesntMatch(""));
}

TEST_P(RegexpTest, AnotherSimpleCharacter) {
  auto const status_or_pattern = Parse("b");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, DoesntMatch("anchor"));
  EXPECT_THAT(pattern, DoesntMatch("banana"));
  EXPECT_THAT(pattern, DoesntMatch(""));
}

TEST_P(RegexpTest, InvalidEscapeCode) {
  EXPECT_THAT(Parse("\\x00"), StatusIs(absl::StatusCode::kInvalidArgument));
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, DoesntMatch("anchor"));
  EXPECT_THAT(pattern, DoesntMatch("banana"));
  EXPECT_THAT(pattern, DoesntMatch(""));
}

TEST_P(RegexpTest, EmptyCharacterClass) {
  auto const status_or_pattern = Parse("[]");
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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

TEST_P(RegexpTest, CharacterRangeBeginsWithEscape) {
  auto const status_or_pattern = Parse("[\\x12-4]");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("\x11"));
  EXPECT_THAT(pattern, Matches("\x12", {}));
  EXPECT_THAT(pattern, Matches("\x13", {}));
  EXPECT_THAT(pattern, Matches("3", {}));
  EXPECT_THAT(pattern, Matches("4", {}));
  EXPECT_THAT(pattern, DoesntMatch("5"));
}

TEST_P(RegexpTest, CharacterRangeEndsWithEscape) {
  auto const status_or_pattern = Parse("[0-\\x34]");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("/"));
  EXPECT_THAT(pattern, Matches("0", {}));
  EXPECT_THAT(pattern, Matches("1", {}));
  EXPECT_THAT(pattern, Matches("2", {}));
  EXPECT_THAT(pattern, Matches("3", {}));
  EXPECT_THAT(pattern, Matches("4", {}));
  EXPECT_THAT(pattern, DoesntMatch("5"));
}

TEST_P(RegexpTest, CharacterRangeWithEscapes) {
  auto const status_or_pattern = Parse("[\\x12-\\x34]");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("\x11"));
  EXPECT_THAT(pattern, Matches("\x12", {}));
  EXPECT_THAT(pattern, Matches("\x13", {}));
  EXPECT_THAT(pattern, Matches("3", {}));
  EXPECT_THAT(pattern, Matches("4", {}));
  EXPECT_THAT(pattern, DoesntMatch("5"));
}

TEST_P(RegexpTest, NegatedCharacterRangeWithEscapes) {
  auto const status_or_pattern = Parse("[^\\x12-\\x34]");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, Matches("\x11", {}));
  EXPECT_THAT(pattern, DoesntMatch("\x12"));
  EXPECT_THAT(pattern, DoesntMatch("\x13"));
  EXPECT_THAT(pattern, DoesntMatch("3"));
  EXPECT_THAT(pattern, DoesntMatch("4"));
  EXPECT_THAT(pattern, Matches("5", {}));
}

TEST_P(RegexpTest, CharacterRangeCrossingSignBoundary) {
  auto const status_or_pattern = Parse("[\\x42-\\xDB]");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("A"));
  EXPECT_THAT(pattern, Matches("\x42", {}));
  EXPECT_THAT(pattern, Matches("\x43", {}));
  EXPECT_THAT(pattern, Matches("\xDA", {}));
  EXPECT_THAT(pattern, Matches("\xDB", {}));
  EXPECT_THAT(pattern, DoesntMatch("\xDC"));
}

TEST_P(RegexpTest, FullCharacterRange) {
  auto const status_or_pattern = Parse("[\\x01-\\xFF]");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("\x01", {}));
  EXPECT_THAT(pattern, Matches("\x02", {}));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, Matches("b", {}));
  EXPECT_THAT(pattern, DoesntMatch("bb"));
  EXPECT_THAT(pattern, Matches("\xFE", {}));
  EXPECT_THAT(pattern, Matches("\xFF", {}));
}

TEST_P(RegexpTest, EmptyCharacterRange) {
  auto const status_or_pattern = Parse("[^\\x01-\\xFF]");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("\x01"));
  EXPECT_THAT(pattern, DoesntMatch("\x02"));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("\xFE"));
  EXPECT_THAT(pattern, DoesntMatch("\xFF"));
}

TEST_P(RegexpTest, InvalidEscapeCodesInCharacterClass) {
  EXPECT_THAT(Parse("[\\"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\x"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\x]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\x00]"), StatusIs(absl::StatusCode::kInvalidArgument));
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
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, DoesntMatch("l"));
  EXPECT_THAT(pattern, Matches("lorem", {}));
  EXPECT_THAT(pattern, DoesntMatch("loremipsum"));
  EXPECT_THAT(pattern, DoesntMatch("dolorloremipsum"));
}

TEST_P(RegexpTest, CharacterSequenceWithDot) {
  auto const status_or_pattern = Parse("lo.em");
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
}

TEST_P(RegexpTest, EmptyOrA) {
  auto const status_or_pattern = Parse("|a");
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {{""}}));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("aa"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("ab"));
}

TEST_P(RegexpTest, EmptyNonCapturingBrackets) {
  auto const status_or_pattern = Parse("(?:)");
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {{"a"}}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("anchor"));
  EXPECT_THAT(pattern, DoesntMatch("banana"));
}

TEST_P(RegexpTest, NonCapturingBrackets) {
  auto const status_or_pattern = Parse("(?:a)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch(""));
  EXPECT_THAT(pattern, Matches("a", {}));
  EXPECT_THAT(pattern, DoesntMatch("b"));
  EXPECT_THAT(pattern, DoesntMatch("anchor"));
  EXPECT_THAT(pattern, DoesntMatch("banana"));
}

TEST_P(RegexpTest, IpsumInBrackets) {
  auto const status_or_pattern = Parse("lorem(ipsum)dolor");
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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

TEST_P(RegexpTest, EpsilonLoop) {
  EXPECT_THAT(Parse("()*"), StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse("(|a)+"), StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse("(a|)+"), StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse("(?:|a)+"), StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(Parse("(?:a|)+"), StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(RegexpTest, CollapsedEpsilonLoop1) {
  auto const status_or_pattern = Parse("(?:)*");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
}

TEST_P(RegexpTest, CollapsedEpsilonLoop2) {
  auto const status_or_pattern = Parse("[^\\x01-\\xFF]*");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {}));
  EXPECT_THAT(pattern, DoesntMatch("a"));
  EXPECT_THAT(pattern, DoesntMatch("b"));
}

TEST_P(RegexpTest, ChainLoops) {
  auto const status_or_pattern = Parse("a*b*");
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("", {{""}, {}}));
  EXPECT_THAT(pattern, Matches("abcabdabe", {{"abcabdabe"}, {"abc", "abd", "abe"}}));
}

TEST_P(RegexpTest, CantMergeLoopEndpoints) {
  auto const status_or_pattern = Parse("(lore(m))*");
  ASSERT_OK(status_or_pattern);
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
  ASSERT_OK(status_or_pattern);
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
  EXPECT_THAT(pattern, PartiallyMatches("lorem ipsum dolor sic amat", {}));
  EXPECT_THAT(pattern, PartiallyMatches("lorem ipsum dooolor sic amat", {}));
  EXPECT_THAT(pattern, DoesntPartiallyMatch("lorem ipsum color sic amat"));
  EXPECT_THAT(pattern, DoesntPartiallyMatch("lorem ipsum dolet et amat"));
}

TEST_P(RegexpTest, SearchWithCapturingPartialMatch) {
  auto const status_or_pattern = Parse("(do+lor)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, PartiallyMatches("lorem ipsum dolor sic amat", {{"dolor"}}));
  EXPECT_THAT(pattern, PartiallyMatches("lorem ipsum dooolor sic amat", {{"dooolor"}}));
  EXPECT_THAT(pattern, DoesntPartiallyMatch("lorem ipsum color sic amat"));
  EXPECT_THAT(pattern, DoesntPartiallyMatch("lorem ipsum dolet et amat"));
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

TEST_P(RegexpTest, MatchArgCount) {
  auto const status_or_pattern = Parse("sator(arepo(tenet)|(opera)(rotas))");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(pattern->MatchArgs("satorarepotenet", {}));
  std::string_view sv1, sv2, sv3, sv4, sv5, sv6;  // NOLINT(readability-isolate-declaration)
  auto const tie = [&] { return std::tie(sv1, sv2, sv3, sv4, sv5, sv6); };
  auto const reset = [&] { tie() = std::tie("sv1", "sv2", "sv3", "sv4", "sv5", "sv6"); };
  reset();
  EXPECT_TRUE(pattern->MatchArgs("satorarepotenet", {&sv1}));
  EXPECT_THAT(tie(), FieldsAre("arepotenet", _, _, _, _, _));
  reset();
  EXPECT_TRUE(pattern->MatchArgs("satorarepotenet", {&sv1, &sv2}));
  EXPECT_THAT(tie(), FieldsAre("arepotenet", "tenet", _, _, _, _));
  reset();
  EXPECT_TRUE(pattern->MatchArgs("satorarepotenet", {&sv1, &sv2, &sv3}));
  EXPECT_THAT(tie(), FieldsAre("arepotenet", "tenet", "", _, _, _));
  reset();
  EXPECT_TRUE(pattern->MatchArgs("satorarepotenet", {&sv1, &sv2, &sv3, &sv4}));
  EXPECT_THAT(tie(), FieldsAre("arepotenet", "tenet", "", "", _, _));
  reset();
  EXPECT_TRUE(pattern->MatchArgs("satorarepotenet", {&sv1, &sv2, &sv3, &sv4, &sv5}));
  EXPECT_THAT(tie(), FieldsAre("arepotenet", "tenet", "", "", _, _));
  reset();
  EXPECT_TRUE(pattern->MatchArgs("satorarepotenet", {&sv1, &sv2, &sv3, &sv4, &sv5, &sv6}));
  EXPECT_THAT(tie(), FieldsAre("arepotenet", "tenet", "", "", _, _));
}

TEST_P(RegexpTest, PartialMatchArgCount) {
  auto const status_or_pattern = Parse("sator(arepo(tenet)|(opera)(rotas))");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(pattern->PartialMatchArgs("satorarepotenet", {}));
  std::string_view sv1, sv2, sv3, sv4, sv5, sv6;  // NOLINT(readability-isolate-declaration)
  auto const tie = [&] { return std::tie(sv1, sv2, sv3, sv4, sv5, sv6); };
  auto const reset = [&] { tie() = std::tie("sv1", "sv2", "sv3", "sv4", "sv5", "sv6"); };
  reset();
  EXPECT_TRUE(pattern->PartialMatchArgs("satorarepotenet", {&sv1}));
  EXPECT_THAT(tie(), FieldsAre("arepotenet", _, _, _, _, _));
  reset();
  EXPECT_TRUE(pattern->PartialMatchArgs("satorarepotenet", {&sv1, &sv2}));
  EXPECT_THAT(tie(), FieldsAre("arepotenet", "tenet", _, _, _, _));
  reset();
  EXPECT_TRUE(pattern->PartialMatchArgs("satorarepotenet", {&sv1, &sv2, &sv3}));
  EXPECT_THAT(tie(), FieldsAre("arepotenet", "tenet", "", _, _, _));
  reset();
  EXPECT_TRUE(pattern->PartialMatchArgs("satorarepotenet", {&sv1, &sv2, &sv3, &sv4}));
  EXPECT_THAT(tie(), FieldsAre("arepotenet", "tenet", "", "", _, _));
  reset();
  EXPECT_TRUE(pattern->PartialMatchArgs("satorarepotenet", {&sv1, &sv2, &sv3, &sv4, &sv5}));
  EXPECT_THAT(tie(), FieldsAre("arepotenet", "tenet", "", "", _, _));
  reset();
  EXPECT_TRUE(pattern->PartialMatchArgs("satorarepotenet", {&sv1, &sv2, &sv3, &sv4, &sv5, &sv6}));
  EXPECT_THAT(tie(), FieldsAre("arepotenet", "tenet", "", "", _, _));
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

TEST_P(RegexpTest, WordBoundary) {
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

TEST_P(RegexpTest, NotWordBoundary) {
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

TEST_P(RegexpTest, WordBoundaries) {
  auto const status_or_pattern = Parse(".*(\\blorem\\b).*");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("dolorem ipsum lorem loremipsum", {{"lorem"}}));
}

TEST_P(RegexpTest, NotWordBoundaries) {
  auto const status_or_pattern = Parse(".*(..(\\Blorem\\B)..).*");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("ipsum lorem doloremdo lorem ipsum", {{"doloremdo"}, {"lorem"}}));
}

TEST_P(RegexpTest, WordBoundariesInPrefix) {
  auto const status_or_pattern = Parse(".*(\\blorem\\b)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, MatchesPrefixOf("dolorem ipsum lorem loremipsum", {{"lorem"}}));
}

TEST_P(RegexpTest, NotWordBoundariesInPrefix) {
  auto const status_or_pattern = Parse(".*(..(\\Blorem\\B)..)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern,
              MatchesPrefixOf("ipsum lorem doloremdo lorem ipsum", {{"doloremdo"}, {"lorem"}}));
}

TEST_P(RegexpTest, WordBoundariesAtStringBoundaries) {
  auto const status_or_pattern = Parse("(\\blorem\\b)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("lorem", {{"lorem"}}));
  EXPECT_THAT(pattern, MatchesPrefixOf("lorem", {{"lorem"}}));
}

TEST_P(RegexpTest, NotWordBoundariesNotAtStringBoundaries) {
  auto const status_or_pattern = Parse("(\\Blorem\\B)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatchPrefixOf("lorem"));
}

TEST_P(RegexpTest, SearchWordWithBoundaries) {
  auto const status_or_pattern = Parse("(\\blo?rem\\b)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, PartiallyMatches("dolrem lorem lremipsum", {{"lorem"}}));
}

TEST_P(RegexpTest, SearchWordWithoutBoundaries) {
  auto const status_or_pattern = Parse("(\\Blo?rem\\B)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, PartiallyMatches("ipsum lremdo doloremdo dolrem", {{"lorem"}}));
}

TEST_P(RegexpTest, NoAnchors) {
  EXPECT_OK(Parse("sator(arepo(tenet)|(?:opera)(rotas)+)", {.no_anchors = true}));
  EXPECT_THAT(Parse("^lorem", {.no_anchors = true}), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("ipsum$", {.no_anchors = true}), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("^dolor$", {.no_anchors = true}), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(RegexpTest, StartAnchor) {
  auto const status_or_pattern = Parse("(^lorem)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntPartiallyMatch("ipsum lorem"));
  EXPECT_THAT(pattern, PartiallyMatches("lorem ipsum", {{"lorem"}}));
}

TEST_P(RegexpTest, EndAnchor) {
  auto const status_or_pattern = Parse("(lorem$)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntPartiallyMatch("lorem ipsum"));
  EXPECT_THAT(pattern, PartiallyMatches("ipsum lorem", {{"lorem"}}));
}

TEST_P(RegexpTest, AnchoredPartialMatch) {
  auto const status_or_pattern = Parse("(^ipsum$)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntPartiallyMatch("lorem ipsum dolor"));
  EXPECT_THAT(pattern, PartiallyMatches("ipsum", {{"ipsum"}}));
}

TEST_P(RegexpTest, DoesntAssertBeginOfInput) {
  auto const status_or_pattern = Parse("lorem");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(pattern->AssertsBeginOfInput());
  EXPECT_THAT(pattern, PartiallyMatches("dolor lorem amet", {}));
}

TEST_P(RegexpTest, AssertsBeginOfInput) {
  auto const status_or_pattern = Parse("^lorem");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(pattern->AssertsBeginOfInput());
  EXPECT_THAT(pattern, PartiallyMatches("lorem ipsum", {}));
  EXPECT_THAT(pattern, DoesntPartiallyMatch("dolor lorem amet"));
}

TEST_P(RegexpTest, NoBranchAssertsBeginOfInput) {
  auto const status_or_pattern = Parse("lorem|ipsum");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(pattern->AssertsBeginOfInput());
  EXPECT_THAT(pattern, PartiallyMatches("dolor lorem amet", {}));
  EXPECT_THAT(pattern, PartiallyMatches("dolor ipsum amet", {}));
}

TEST_P(RegexpTest, FirstBranchAssertsBeginOfInput) {
  auto const status_or_pattern = Parse("^lorem|ipsum");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(pattern->AssertsBeginOfInput());
  EXPECT_THAT(pattern, PartiallyMatches("lorem dolor amet", {}));
  EXPECT_THAT(pattern, DoesntPartiallyMatch("dolor lorem amet"));
  EXPECT_THAT(pattern, PartiallyMatches("dolor ipsum amet", {}));
}

TEST_P(RegexpTest, SecondBranchAssertsBeginOfInput) {
  auto const status_or_pattern = Parse("lorem|^ipsum");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(pattern->AssertsBeginOfInput());
  EXPECT_THAT(pattern, PartiallyMatches("ipsum dolor amet", {}));
  EXPECT_THAT(pattern, DoesntPartiallyMatch("dolor ipsum amet"));
  EXPECT_THAT(pattern, PartiallyMatches("dolor lorem amet", {}));
}

TEST_P(RegexpTest, BothBranchesAssertBeginOfInput) {
  auto const status_or_pattern = Parse("^lorem|^ipsum");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(pattern->AssertsBeginOfInput());
  EXPECT_THAT(pattern, PartiallyMatches("lorem dolor amet", {}));
  EXPECT_THAT(pattern, DoesntPartiallyMatch("dolor lorem amet"));
  EXPECT_THAT(pattern, PartiallyMatches("ipsum dolor amet", {}));
  EXPECT_THAT(pattern, DoesntPartiallyMatch("dolor ipsum amet"));
}

TEST_P(RegexpTest, CaseSensitive) {
  auto const status_or_pattern = Parse("(lorem)", {.case_sensitive = true});
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("lorem", {{"lorem"}}));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, DoesntMatch("LOREM"));
  EXPECT_THAT(pattern, DoesntMatch("Lorem"));
  EXPECT_THAT(pattern, DoesntMatch("LoReM"));
}

TEST_P(RegexpTest, CaseInsensitive) {
  auto const status_or_pattern = Parse("(lorem)", {.case_sensitive = false});
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("lorem", {{"lorem"}}));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, Matches("LOREM", {{"LOREM"}}));
  EXPECT_THAT(pattern, Matches("Lorem", {{"Lorem"}}));
  EXPECT_THAT(pattern, Matches("LoReM", {{"LoReM"}}));
}

TEST_P(RegexpTest, CaseSensitiveCharacterClass) {
  auto const status_or_pattern = Parse("([lorem]+)", {.case_sensitive = true});
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("lorem", {{"lorem"}}));
  EXPECT_THAT(pattern, Matches("merol", {{"merol"}}));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, DoesntMatch("IPSUM"));
  EXPECT_THAT(pattern, DoesntMatch("LOREM"));
  EXPECT_THAT(pattern, DoesntMatch("MEROL"));
  EXPECT_THAT(pattern, DoesntMatch("Lorem"));
  EXPECT_THAT(pattern, DoesntMatch("Merol"));
  EXPECT_THAT(pattern, DoesntMatch("LoReM"));
  EXPECT_THAT(pattern, DoesntMatch("mErOl"));
}

TEST_P(RegexpTest, CaseInsensitiveCharacterClass) {
  auto const status_or_pattern = Parse("([lorem]+)", {.case_sensitive = false});
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("lorem", {{"lorem"}}));
  EXPECT_THAT(pattern, Matches("merol", {{"merol"}}));
  EXPECT_THAT(pattern, DoesntMatch("ipsum"));
  EXPECT_THAT(pattern, DoesntMatch("IPSUM"));
  EXPECT_THAT(pattern, Matches("LOREM", {{"LOREM"}}));
  EXPECT_THAT(pattern, Matches("MEROL", {{"MEROL"}}));
  EXPECT_THAT(pattern, Matches("Lorem", {{"Lorem"}}));
  EXPECT_THAT(pattern, Matches("Merol", {{"Merol"}}));
  EXPECT_THAT(pattern, Matches("LoReM", {{"LoReM"}}));
  EXPECT_THAT(pattern, Matches("mErOl", {{"mErOl"}}));
}

TEST_P(RegexpTest, CaseSensitiveNegatedCharacterClass) {
  auto const status_or_pattern = Parse("([^lorem]+)", {.case_sensitive = true});
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("merol"));
  EXPECT_THAT(pattern, Matches("adipisci", {{"adipisci"}}));
  EXPECT_THAT(pattern, Matches("ADIPISCI", {{"ADIPISCI"}}));
  EXPECT_THAT(pattern, Matches("LOREM", {{"LOREM"}}));
  EXPECT_THAT(pattern, Matches("MEROL", {{"MEROL"}}));
  EXPECT_THAT(pattern, DoesntMatch("Lorem"));
  EXPECT_THAT(pattern, DoesntMatch("Merol"));
  EXPECT_THAT(pattern, DoesntMatch("LoReM"));
  EXPECT_THAT(pattern, DoesntMatch("mErOl"));
}

TEST_P(RegexpTest, CaseInsensitiveNegatedCharacterClass) {
  auto const status_or_pattern = Parse("([^lorem]+)", {.case_sensitive = false});
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch("lorem"));
  EXPECT_THAT(pattern, DoesntMatch("merol"));
  EXPECT_THAT(pattern, Matches("adipisci", {{"adipisci"}}));
  EXPECT_THAT(pattern, Matches("ADIPISCI", {{"ADIPISCI"}}));
  EXPECT_THAT(pattern, DoesntMatch("LOREM"));
  EXPECT_THAT(pattern, DoesntMatch("MEROL"));
  EXPECT_THAT(pattern, DoesntMatch("Lorem"));
  EXPECT_THAT(pattern, DoesntMatch("Merol"));
  EXPECT_THAT(pattern, DoesntMatch("LoReM"));
  EXPECT_THAT(pattern, DoesntMatch("mErOl"));
}

TEST_P(RegexpTest, CaseSensitiveCharacterRange) {
  auto const status_or_pattern = Parse("([T-s]+)", {.case_sensitive = true});
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch("ABCDEFGHIJKLMNOPQRS"));
  EXPECT_THAT(pattern, Matches("TUVWXYZ[\\]^_`abcdefghijklmnopqrs",
                               {{"TUVWXYZ[\\]^_`abcdefghijklmnopqrs"}}));
  EXPECT_THAT(pattern, DoesntMatch("tuvwxyz{|}~"));
}

TEST_P(RegexpTest, CaseInsensitiveCharacterRange) {
  auto const status_or_pattern = Parse("([T-s]+)", {.case_sensitive = false});
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("ABCDEFGHIJKLMNOPQRSTUVWXYZ", {{"ABCDEFGHIJKLMNOPQRSTUVWXYZ"}}));
  EXPECT_THAT(pattern, Matches("[\\]^_`", {{"[\\]^_`"}}));
  EXPECT_THAT(pattern, Matches("abcdefghijklmnopqrstuvwxyz", {{"abcdefghijklmnopqrstuvwxyz"}}));
  EXPECT_THAT(pattern, DoesntMatch("{|}~"));
}

TEST_P(RegexpTest, CaseSensitiveNegatedCharacterRange) {
  auto const status_or_pattern = Parse("([^T-s]+)", {.case_sensitive = true});
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, Matches("ABCDEFGHIJKLMNOPQRS", {{"ABCDEFGHIJKLMNOPQRS"}}));
  EXPECT_THAT(pattern, DoesntMatch("TUVWXYZ[\\]^_`abcdefghijklmnopqrs"));
  EXPECT_THAT(pattern, Matches("tuvwxyz{|}~", {{"tuvwxyz{|}~"}}));
}

TEST_P(RegexpTest, CaseInsensitiveNegatedCharacterRange) {
  auto const status_or_pattern = Parse("([^T-s]+)", {.case_sensitive = false});
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern, DoesntMatch("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
  EXPECT_THAT(pattern, DoesntMatch("[\\]^_`"));
  EXPECT_THAT(pattern, DoesntMatch("abcdefghijklmnopqrstuvwxyz"));
  EXPECT_THAT(pattern, Matches("{|}~", {{"{|}~"}}));
}

INSTANTIATE_TEST_SUITE_P(RegexpTest, RegexpTest,
                         Values(RegexpTestParams{.force_nfa = false, .use_stepper = false},
                                RegexpTestParams{.force_nfa = false, .use_stepper = true},
                                RegexpTestParams{.force_nfa = true, .use_stepper = false},
                                RegexpTestParams{.force_nfa = true, .use_stepper = true}));

}  // namespace
