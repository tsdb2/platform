#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
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

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Values;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;
using ::tsdb2::common::reffed_ptr;
using ::tsdb2::common::regexp_internal::AutomatonInterface;
using ::tsdb2::common::regexp_internal::DFA;
using ::tsdb2::common::regexp_internal::NFA;
using ::tsdb2::common::regexp_internal::Parse;
using ::tsdb2::common::regexp_internal::TempNFA;

struct RegexpTestParams {
  bool force_nfa;
  bool use_stepper;
};

class RegexpTest : public ::testing::TestWithParam<RegexpTestParams> {
 protected:
  explicit RegexpTest() { TempNFA::force_nfa_for_testing = GetParam().force_nfa; }
  ~RegexpTest() { TempNFA::force_nfa_for_testing = false; }

  bool CheckDeterministic(reffed_ptr<AutomatonInterface> const& automaton);
  bool CheckNotDeterministic(reffed_ptr<AutomatonInterface> const& automaton);

  absl::StatusOr<std::optional<std::vector<std::string>>> Match(
      reffed_ptr<AutomatonInterface> const& automaton, std::string_view input);

 private:
  bool Run(reffed_ptr<AutomatonInterface> const& automaton, std::string_view input);
};

bool RegexpTest::CheckDeterministic(reffed_ptr<AutomatonInterface> const& automaton) {
  return GetParam().force_nfa || automaton->IsDeterministic();
}

bool RegexpTest::CheckNotDeterministic(reffed_ptr<AutomatonInterface> const& automaton) {
  return GetParam().force_nfa || !automaton->IsDeterministic();
}

bool RegexpTest::Run(reffed_ptr<AutomatonInterface> const& automaton,
                     std::string_view const input) {
  if (GetParam().use_stepper) {
    auto const stepper = automaton->MakeStepper();
    return stepper->Step(input) && stepper->Finish();
  } else {
    return automaton->Test(input);
  }
}

absl::StatusOr<std::optional<std::vector<std::string>>> RegexpTest::Match(
    reffed_ptr<AutomatonInterface> const& automaton, std::string_view const input) {
  bool const run_result = Run(automaton, input);
  auto match_results = automaton->Match(input);
  if (run_result != match_results.has_value()) {
    return absl::FailedPreconditionError("Test result differs from Match result");
  }
  return match_results;
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
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "hello"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
}

TEST_P(RegexpTest, SimpleCharacter) {
  auto const status_or_pattern = Parse("a");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "anchor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "banana"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, AnotherSimpleCharacter) {
  auto const status_or_pattern = Parse("b");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "anchor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "banana"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
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
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "0"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "1"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "2"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "3"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "4"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "5"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "6"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "7"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "8"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "9"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "d"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\d"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\0"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, NotDigit) {
  auto const status_or_pattern = Parse("\\D");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "0"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "1"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "2"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "3"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "4"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "5"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "6"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "7"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "8"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "9"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "D"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\\D"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\0"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, WordCharacter) {
  auto const status_or_pattern = Parse("\\w");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "A"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "B"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "C"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "D"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "E"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "F"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "G"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "H"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "I"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "J"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "K"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "L"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "M"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "N"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "O"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "P"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "Q"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "R"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "S"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "T"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "U"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "V"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "W"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "X"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "Y"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "Z"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "d"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "e"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "f"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "g"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "h"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "i"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "j"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "k"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "l"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "m"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "n"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "o"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "p"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "q"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "r"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "s"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "t"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "u"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "v"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "w"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "y"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "z"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "0"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "1"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "2"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "3"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "4"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "5"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "6"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "7"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "8"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "9"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "_"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "."), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "-"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\w"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, NotWordCharacter) {
  auto const status_or_pattern = Parse("\\W");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "A"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "B"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "C"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "D"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "E"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "F"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "G"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "H"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "I"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "J"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "K"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "L"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "M"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "N"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "O"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "P"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "Q"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "R"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "S"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "T"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "U"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "V"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "W"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "X"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "Y"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "Z"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "d"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "e"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "f"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "g"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "h"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "i"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "j"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "k"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "l"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "m"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "n"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "o"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "p"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "q"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "r"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "s"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "t"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "u"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "v"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "w"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "y"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "z"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "0"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "1"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "2"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "3"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "4"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "5"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "6"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "7"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "8"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "9"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "_"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "."), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "-"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\\W"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, Spacing) {
  auto const status_or_pattern = Parse("\\s");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\f"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\n"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\r"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\t"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\v"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "s"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\s"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, NotSpacing) {
  auto const status_or_pattern = Parse("\\S");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\f"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\n"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\r"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\t"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\v"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "s"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\\S"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, HorizontalTab) {
  auto const status_or_pattern = Parse("\\t");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\t"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\n"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "t"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\t"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, CarriageReturn) {
  auto const status_or_pattern = Parse("\\r");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\r"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\n"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "r"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\r"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, LineFeed) {
  auto const status_or_pattern = Parse("\\n");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\n"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\t"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "n"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\n"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, VerticalTab) {
  auto const status_or_pattern = Parse("\\v");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\v"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\n"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "v"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\v"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, FormFeed) {
  auto const status_or_pattern = Parse("\\f");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\f"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\n"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "f"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\f"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, InvalidHexCode) {
  EXPECT_THAT(Parse("\\xZ0"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\x0Z"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(RegexpTest, HexCode1) {
  auto const status_or_pattern = Parse("\\x12");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\x12"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\x12"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, HexCode2) {
  auto const status_or_pattern = Parse("\\xAF");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\xAF"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\xAF"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, HexCode3) {
  auto const status_or_pattern = Parse("\\xaf");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\xAF"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\xaf"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, AnyCharacter) {
  auto const status_or_pattern = Parse(".");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "anchor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "banana"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, EmptyCharacterClass) {
  auto const status_or_pattern = Parse("[]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "[]"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, NegatedEmptyCharacterClass) {
  auto const status_or_pattern = Parse("[^]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "^"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "[^]"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, CharacterClass) {
  auto const status_or_pattern = Parse("[lorem\xAF]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "l"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "o"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "r"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "e"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "m"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\xAF"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\xBF"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem\xAF"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "[lorem\xAF]"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, NegatedCharacterClass) {
  auto const status_or_pattern = Parse("[^lorem\xAF]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "l"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "o"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "r"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "e"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "m"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\xAF"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\xBF"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "^"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "^lorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "^lorem\xAF"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "[^lorem\xAF]"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, CharacterClassWithCircumflex) {
  auto const status_or_pattern = Parse("[ab^cd]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "^"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "d"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "ab^cd"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "[ab^cd]"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, NegatedCharacterClassWithCircumflex) {
  auto const status_or_pattern = Parse("[^ab^cd]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "^"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "d"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "y"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "ab^cd"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "^ab^cd"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "[^ab^cd]"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, CharacterRange) {
  auto const status_or_pattern = Parse("[2-4]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "0"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "1"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "2"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "3"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "4"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "5"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "6"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "-"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, NegatedCharacterRange) {
  auto const status_or_pattern = Parse("[^2-4]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "0"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "1"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "2"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "3"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "4"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "5"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "6"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "-"), IsOkAndHolds(Optional(IsEmpty())));
}

TEST_P(RegexpTest, CharacterRangeWithDash) {
  auto const status_or_pattern = Parse("[2-4-]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "0"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "1"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "2"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "3"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "4"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "5"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "6"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "-"), IsOkAndHolds(Optional(IsEmpty())));
}

TEST_P(RegexpTest, NegatedCharacterRangeWithDash) {
  auto const status_or_pattern = Parse("[^2-4-]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "0"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "1"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "2"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "3"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "4"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "5"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "6"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "-"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, CharacterClassWithCharactersAndRange) {
  auto const status_or_pattern = Parse("[ac2-4eg-]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "d"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "0"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "1"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "2"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "3"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "4"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "5"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "6"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "e"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "f"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "g"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "h"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "-"), IsOkAndHolds(Optional(IsEmpty())));
}

TEST_P(RegexpTest, NegatedCharacterClassWithCharactersAndRange) {
  auto const status_or_pattern = Parse("[^ac2-4eg-]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "d"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "0"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "1"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "2"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "3"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "4"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "5"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "6"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "e"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "f"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "g"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "h"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "-"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, CharacterClassWithSpecialCharacters) {
  auto const status_or_pattern = Parse("[a^$.(){}|?*+b]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "^"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "$"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "."), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "("), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, ")"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "{"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "}"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "|"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "?"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "*"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "+"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "y"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, NegatedCharacterClassWithSpecialCharacters) {
  auto const status_or_pattern = Parse("[^a^$.(){}|?*+b]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "^"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "$"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "."), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "("), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, ")"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "{"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "}"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "|"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "?"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "*"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "+"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "y"), IsOkAndHolds(Optional(IsEmpty())));
}

TEST_P(RegexpTest, CharacterClassWithEscapes) {
  auto const status_or_pattern = Parse("[a\\\\\\^\\$\\.\\(\\)\\[\\]\\{\\}\\|\\?\\*\\+b]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "^"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "$"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "."), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "("), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, ")"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "["), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "]"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "{"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "}"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "|"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "?"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "*"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "+"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "y"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, NegatedCharacterClassWithEscapes) {
  auto const status_or_pattern = Parse("[^a\\\\\\^\\$\\.\\(\\)\\[\\]\\{\\}\\|\\?\\*\\+b]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "^"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "$"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "."), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "("), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, ")"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "["), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "]"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "{"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "}"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "|"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "?"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "*"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "+"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "y"), IsOkAndHolds(Optional(IsEmpty())));
}

TEST_P(RegexpTest, CharacterClassWithMoreEscapes) {
  auto const status_or_pattern = Parse("[\\t\\r\\n\\v\\f\\b\\x12\\xAF]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\t"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\r"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\n"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\v"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\f"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\x12"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\xAF"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "y"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, NegatedCharacterClassWithMoreEscapes) {
  auto const status_or_pattern = Parse("[^\\t\\r\\n\\v\\f\\b\\x12\\xAF]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "\t"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\r"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\n"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\v"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\f"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\x12"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\xAF"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "y"), IsOkAndHolds(Optional(IsEmpty())));
}

TEST_P(RegexpTest, InvalidEscapeCodesInCharacterClass) {
  EXPECT_THAT(Parse("[\\"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\x"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\x]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\x0Z]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\xZ0]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\a]"), StatusIs(absl::StatusCode::kInvalidArgument));
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
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "l"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolorloremipsum"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, CharacterSequenceWithDot) {
  auto const status_or_pattern = Parse("lo.em");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "l"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "lo-em"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "lovem"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "lodolorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolorloremipsum"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, KleeneStar) {
  auto const status_or_pattern = Parse("a*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, CharacterSequenceWithStar) {
  auto const status_or_pattern = Parse("lo*rem");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "l"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lrem"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "loorem"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "looorem"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "larem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremlorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolorloremipsum"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, KleenePlus) {
  auto const status_or_pattern = Parse("a+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, CharacterSequenceWithPlus) {
  auto const status_or_pattern = Parse("lo+rem");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "l"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lrem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "loorem"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "looorem"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "larem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremlorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolorloremipsum"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, Maybe) {
  auto const status_or_pattern = Parse("a?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, Many) {
  auto const status_or_pattern = Parse("a{}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ExactlyZero) {
  auto const status_or_pattern = Parse("a{0}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ExactlyOne) {
  auto const status_or_pattern = Parse("a{1}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ExactlyTwo) {
  auto const status_or_pattern = Parse("a{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ExactlyFourtyTwo) {
  auto const status_or_pattern = Parse("a{42}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, AtLeastZero) {
  auto const status_or_pattern = Parse("a{0,}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, AtLeastOne) {
  auto const status_or_pattern = Parse("a{1,}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, AtLeastTwo) {
  auto const status_or_pattern = Parse("a{2,}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, AtLeastFourtyTwo) {
  auto const status_or_pattern = Parse("a{42,}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, BetweenZeroAndZero) {
  auto const status_or_pattern = Parse("a{0,0}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, BetweenZeroAndOne) {
  auto const status_or_pattern = Parse("a{0,1}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, BetweenZeroAndTwo) {
  auto const status_or_pattern = Parse("a{0,2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, BetweenOneAndOne) {
  auto const status_or_pattern = Parse("a{1,1}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, BetweenOneAndTwo) {
  auto const status_or_pattern = Parse("a{1,2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, BetweenTwoAndTwo) {
  auto const status_or_pattern = Parse("a{2,2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, BetweenFourtyTwoAndFourtyFive) {
  auto const status_or_pattern = Parse("a{42,45}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabaa"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, CharacterSequenceWithMaybe) {
  auto const status_or_pattern = Parse("lo?rem");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "l"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lrem"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "loorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "looorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "larem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremlorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolorloremipsum"), IsOkAndHolds(std::nullopt));
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
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre(""))));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(ElementsAre("aaa"))));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(Optional(ElementsAre("aaaa"))));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(Optional(ElementsAre("aaaaa"))));
}

TEST_P(RegexpTest, MultipleQuantifiersWithNonCapturingBrackets) {
  auto const status_or_pattern = Parse("(?:a+)*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(Optional(IsEmpty())));
}

TEST_P(RegexpTest, EmptyOrEmpty) {
  auto const status_or_pattern = Parse("|");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, EmptyOrA) {
  auto const status_or_pattern = Parse("|a");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, AOrEmpty) {
  auto const status_or_pattern = Parse("a|");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, AOrB) {
  auto const status_or_pattern = Parse("a|b");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a|b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, LoremOrIpsum) {
  auto const status_or_pattern = Parse("lorem|ipsum");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "l"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "i"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsum"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem|ipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsumlorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsum|lorem"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, EmptyBrackets) {
  auto const status_or_pattern = Parse("()");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre(""))));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, EmptyNonCapturingBrackets) {
  auto const status_or_pattern = Parse("(?:)");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
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
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "anchor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "banana"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, NonCapturingBrackets) {
  auto const status_or_pattern = Parse("(?:a)");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "anchor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "banana"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, IpsumInBrackets) {
  auto const status_or_pattern = Parse("lorem(ipsum)dolor");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremdolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremidolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolor"), IsOkAndHolds(Optional(ElementsAre("ipsum"))));
}

TEST_P(RegexpTest, IpsumInNonCapturingBrackets) {
  auto const status_or_pattern = Parse("lorem(?:ipsum)dolor");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremdolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremidolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolor"), IsOkAndHolds(Optional(IsEmpty())));
}

TEST_P(RegexpTest, NestedBrackets) {
  auto const status_or_pattern = Parse("lorem(ipsum(dolor)amet)adipisci");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdoloramet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "amet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "adipisci"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsumdoloramet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolorametadipisci"),
              IsOkAndHolds(Optional(ElementsAre("ipsumdoloramet", "dolor"))));
}

TEST_P(RegexpTest, CapturingBracketsNestedInNonCapturingBrackets) {
  auto const status_or_pattern = Parse("lorem(?:ipsum(dolor)amet)adipisci");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdoloramet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "amet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "adipisci"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsumdoloramet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolorametadipisci"),
              IsOkAndHolds(Optional(ElementsAre("dolor"))));
}

TEST_P(RegexpTest, NonCapturingBracketsNestedInCapturingBrackets) {
  auto const status_or_pattern = Parse("lorem(ipsum(?:dolor)amet)adipisci");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdoloramet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "amet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "adipisci"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsumdoloramet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolorametadipisci"),
              IsOkAndHolds(Optional(ElementsAre("ipsumdoloramet"))));
}

TEST_P(RegexpTest, NestedNonCapturingBrackets) {
  auto const status_or_pattern = Parse("lorem(?:ipsum(?:dolor)amet)adipisci");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdoloramet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "amet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "adipisci"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsumdoloramet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolorametadipisci"), IsOkAndHolds(Optional(IsEmpty())));
}

TEST_P(RegexpTest, PeeringBrackets) {
  auto const status_or_pattern = Parse("lorem(ipsum)dolor(amet)adipisci");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdoloramet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "amet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "adipisci"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolorametadipisci"),
              IsOkAndHolds(Optional(ElementsAre("ipsum", "amet"))));
}

TEST_P(RegexpTest, CapturingBracketsPeeringWithNonCapturingBrackets) {
  auto const status_or_pattern = Parse("lorem(ipsum)dolor(?:amet)adipisci");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdoloramet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "amet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "adipisci"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolorametadipisci"),
              IsOkAndHolds(Optional(ElementsAre("ipsum"))));
}

TEST_P(RegexpTest, NonCapturingBracketsPeeringWithCapturingBrackets) {
  auto const status_or_pattern = Parse("lorem(?:ipsum)dolor(amet)adipisci");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdoloramet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "amet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "adipisci"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolorametadipisci"),
              IsOkAndHolds(Optional(ElementsAre("amet"))));
}

TEST_P(RegexpTest, PeeringNonCapturingBrackets) {
  auto const status_or_pattern = Parse("lorem(?:ipsum)dolor(?:amet)adipisci");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdoloramet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolor"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "amet"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "adipisci"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsumdolorametadipisci"), IsOkAndHolds(Optional(IsEmpty())));
}

TEST_P(RegexpTest, InvalidNonCapturingBrackets) {
  EXPECT_THAT(Parse("lorem(?ipsum)dolor"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(RegexpTest, EpsilonLoop1) {
  auto const status_or_pattern = Parse("(|a)+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre(""))));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(ElementsAre("aaa"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, EpsilonLoop2) {
  auto const status_or_pattern = Parse("(a|)+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre(""))));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(ElementsAre("aaa"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, NonCapturingEpsilonLoop1) {
  auto const status_or_pattern = Parse("(?:|a)+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, NonCapturingEpsilonLoop2) {
  auto const status_or_pattern = Parse("(?:a|)+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainLoops) {
  auto const status_or_pattern = Parse("a*b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "baa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ac"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ca"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainStarAndPlus) {
  auto const status_or_pattern = Parse("a*b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "baa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ac"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ca"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainStarAndMaybe) {
  auto const status_or_pattern = Parse("a*b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "baa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ac"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ca"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainStarAndQuantifier) {
  auto const status_or_pattern = Parse("a*b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aabbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "baa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ac"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ca"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainPlusAndStar) {
  auto const status_or_pattern = Parse("a+b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "baa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ac"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ca"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainPlusAndPlus) {
  auto const status_or_pattern = Parse("a+b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "baa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ac"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ca"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainPlusAndMaybe) {
  auto const status_or_pattern = Parse("a+b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "baa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ac"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ca"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainPlusAndQuantifier) {
  auto const status_or_pattern = Parse("a+b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aabbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "baa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ac"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ca"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainMaybeAndStar) {
  auto const status_or_pattern = Parse("a?b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "baa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ac"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ca"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainMaybeAndPlus) {
  auto const status_or_pattern = Parse("a?b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "baa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ac"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ca"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainMaybeAndMaybe) {
  auto const status_or_pattern = Parse("a?b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "baa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ac"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ca"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainMaybeAndQuantifier) {
  auto const status_or_pattern = Parse("a?b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "baa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ac"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ca"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainQuantifierAndStar) {
  auto const status_or_pattern = Parse("a{2}b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaabb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "baa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ac"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ca"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainQuantifierAndPlus) {
  auto const status_or_pattern = Parse("a{2}b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaabb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "baa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ac"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ca"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainQuantifierAndMaybe) {
  auto const status_or_pattern = Parse("a{2}b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaabb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "baa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ac"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ca"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainQuantifiers) {
  auto const status_or_pattern = Parse("a{3}b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaabb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaabbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaabbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "baa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ac"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ca"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, PipeLoops) {
  auto const status_or_pattern = Parse("a*|b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, StarOrPlus) {
  auto const status_or_pattern = Parse("a*|b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, StarOrMaybe) {
  auto const status_or_pattern = Parse("a*|b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, StarOrQuantifier) {
  auto const status_or_pattern = Parse("a*|b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, PlusOrStar) {
  auto const status_or_pattern = Parse("a+|b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, PlusOrPlus) {
  auto const status_or_pattern = Parse("a+|b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, PlusOrMaybe) {
  auto const status_or_pattern = Parse("a+|b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, PlusOrQuantifier) {
  auto const status_or_pattern = Parse("a+|b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, MaybeOrStar) {
  auto const status_or_pattern = Parse("a?|b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, MaybeOrPlus) {
  auto const status_or_pattern = Parse("a?|b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, MaybeOrMaybe) {
  auto const status_or_pattern = Parse("a?|b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, MaybeOrQuantifier) {
  auto const status_or_pattern = Parse("a?|b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, QuantifierOrStar) {
  auto const status_or_pattern = Parse("a{2}|b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, QuantifierOrPlus) {
  auto const status_or_pattern = Parse("a{2}|b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, QuantifierOrMaybe) {
  auto const status_or_pattern = Parse("a{2}|b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, QuantifierOrQuantifier) {
  auto const status_or_pattern = Parse("a{2}|b{3}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "bbbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, CantMergeLoopEndpoints) {
  auto const status_or_pattern = Parse("(lore(m))*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre("", ""))));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(Optional(ElementsAre("lorem", "m"))));
  EXPECT_THAT(Match(pattern, "ipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremlorem"),
              IsOkAndHolds(Optional(ElementsAre("loremlorem", "mm"))));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsumlorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremloremlorem"),
              IsOkAndHolds(Optional(ElementsAre("loremloremlorem", "mmm"))));
}

TEST_P(RegexpTest, CantMergeLoopEndpointsOfPrefix) {
  auto const status_or_pattern = Parse("(lore(m))*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern->MatchPrefix(""), Optional(ElementsAre("", "")));
  EXPECT_THAT(pattern->MatchPrefix("lorem"), Optional(ElementsAre("lorem", "m")));
  EXPECT_THAT(pattern->MatchPrefix("ipsum"), Optional(ElementsAre("", "")));
  EXPECT_THAT(pattern->MatchPrefix("loremlorem"), Optional(ElementsAre("loremlorem", "mm")));
  EXPECT_THAT(pattern->MatchPrefix("loremipsum"), Optional(ElementsAre("lorem", "m")));
  EXPECT_THAT(pattern->MatchPrefix("ipsumlorem"), Optional(ElementsAre("", "")));
  EXPECT_THAT(pattern->MatchPrefix("loremloremlorem"),
              Optional(ElementsAre("loremloremlorem", "mmm")));
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

TEST_P(RegexpTest, Search) {
  auto const status_or_pattern = Parse(".*do+lor.*");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, "lorem ipsum dolor sic amat"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "lorem ipsum dooolor sic amat"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "lorem ipsum color sic amat"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem ipsum dolet et amat"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, NotAllCaptured) {
  auto const status_or_pattern = Parse("sator(arepo(tenet)|(opera)(rotas))");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, "satorarepotenet"),
              IsOkAndHolds(Optional(ElementsAre("arepotenet", "tenet", "", ""))));
  EXPECT_THAT(Match(pattern, "satoroperarotas"),
              IsOkAndHolds(Optional(ElementsAre("operarotas", "", "opera", "rotas"))));
}

TEST_P(RegexpTest, HeavyBacktracker) {
  auto const status_or_pattern = Parse(
      "a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(IsEmpty())));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, InvalidPrefixPattern) {
  EXPECT_THAT(Parse("foo("), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(RegexpTest, EmptyPrefixOfEmptyString) {
  auto const status_or_pattern = Parse("");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern->MatchPrefix(""), Optional(IsEmpty()));
}

TEST_P(RegexpTest, NonEmptyPrefixOfEmptyString) {
  auto const status_or_pattern = Parse("lorem");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_EQ(pattern->MatchPrefix(""), std::nullopt);
}

TEST_P(RegexpTest, ProperPrefix) {
  auto const status_or_pattern = Parse("(lorem)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern->MatchPrefix("loremipsum"), Optional(ElementsAre("lorem")));
}

TEST_P(RegexpTest, ImproperPrefix) {
  auto const status_or_pattern = Parse("(lorem)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern->MatchPrefix("lorem"), Optional(ElementsAre("lorem")));
}

TEST_P(RegexpTest, LongestPrefix) {
  auto const status_or_pattern = Parse("(lorem.*)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern->MatchPrefix("loremipsum"), Optional(ElementsAre("loremipsum")));
}

TEST_P(RegexpTest, DeadPrefixBranch1) {
  auto const status_or_pattern = Parse("(lorem(ipsum)?)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern->MatchPrefix("loremips"), Optional(ElementsAre("lorem", "")));
}

TEST_P(RegexpTest, DeadPrefixBranch2) {
  auto const status_or_pattern = Parse("(lorem)*");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern->MatchPrefix("loremlor"), Optional(ElementsAre("lorem")));
}

TEST_P(RegexpTest, PrefixPatternWithCapture) {
  auto const status_or_pattern = Parse("(lorem (.*) )");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(pattern->MatchPrefix("lorem ipsum dolor"),
              Optional(ElementsAre("lorem ipsum ", "ipsum")));
}

TEST_P(RegexpTest, HeavyPrefixBacktracker) {
  auto const status_or_pattern = Parse(
      "(a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_EQ(pattern->MatchPrefix(""), std::nullopt);
  EXPECT_EQ(pattern->MatchPrefix("b"), std::nullopt);
  EXPECT_EQ(pattern->MatchPrefix("ab"), std::nullopt);
  EXPECT_EQ(pattern->MatchPrefix("a"), std::nullopt);
  EXPECT_EQ(pattern->MatchPrefix("aa"), std::nullopt);
  EXPECT_EQ(pattern->MatchPrefix("aaa"), std::nullopt);
  EXPECT_EQ(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaa"), std::nullopt);
  EXPECT_EQ(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), std::nullopt);
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(
      pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
      Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(
      pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
      Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(
      pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
      Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  EXPECT_THAT(
      pattern->MatchPrefix("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
      Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
}

INSTANTIATE_TEST_SUITE_P(RegexpTest, RegexpTest,
                         Values(RegexpTestParams{.force_nfa = false, .use_stepper = false},
                                RegexpTestParams{.force_nfa = false, .use_stepper = true},
                                RegexpTestParams{.force_nfa = true, .use_stepper = false},
                                RegexpTestParams{.force_nfa = true, .use_stepper = true}));

}  // namespace
