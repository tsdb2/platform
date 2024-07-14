#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
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
  bool use_runner;
};

class RegexpTest : public ::testing::TestWithParam<RegexpTestParams> {
 protected:
  explicit RegexpTest() { TempNFA::force_nfa_for_testing = GetParam().force_nfa; }
  ~RegexpTest() { TempNFA::force_nfa_for_testing = false; }

  bool CheckDeterministic(reffed_ptr<AutomatonInterface> const& automaton);
  bool CheckNotDeterministic(reffed_ptr<AutomatonInterface> const& automaton);

  bool Run(reffed_ptr<AutomatonInterface> const& automaton, std::string_view input);

  absl::StatusOr<std::optional<std::vector<std::string>>> Match(
      reffed_ptr<AutomatonInterface> const& automaton, std::string_view input);
};

bool RegexpTest::CheckDeterministic(reffed_ptr<AutomatonInterface> const& automaton) {
  return GetParam().force_nfa || automaton->IsDeterministic();
}

bool RegexpTest::CheckNotDeterministic(reffed_ptr<AutomatonInterface> const& automaton) {
  return GetParam().force_nfa || !automaton->IsDeterministic();
}

bool RegexpTest::Run(reffed_ptr<AutomatonInterface> const& automaton,
                     std::string_view const input) {
  if (GetParam().use_runner) {
    auto const runner = automaton->CreateRunner();
    return runner->Step(input) && runner->Finish();
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

TEST_P(RegexpTest, Empty) {
  auto const status_or_pattern = Parse("");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "hello"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
}

TEST_P(RegexpTest, SimpleCharacter) {
  auto const status_or_pattern = Parse("a");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
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
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(ElementsAre("b"))));
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
  EXPECT_THAT(Match(pattern, "0"), IsOkAndHolds(Optional(ElementsAre("0"))));
  EXPECT_THAT(Match(pattern, "1"), IsOkAndHolds(Optional(ElementsAre("1"))));
  EXPECT_THAT(Match(pattern, "2"), IsOkAndHolds(Optional(ElementsAre("2"))));
  EXPECT_THAT(Match(pattern, "3"), IsOkAndHolds(Optional(ElementsAre("3"))));
  EXPECT_THAT(Match(pattern, "4"), IsOkAndHolds(Optional(ElementsAre("4"))));
  EXPECT_THAT(Match(pattern, "5"), IsOkAndHolds(Optional(ElementsAre("5"))));
  EXPECT_THAT(Match(pattern, "6"), IsOkAndHolds(Optional(ElementsAre("6"))));
  EXPECT_THAT(Match(pattern, "7"), IsOkAndHolds(Optional(ElementsAre("7"))));
  EXPECT_THAT(Match(pattern, "8"), IsOkAndHolds(Optional(ElementsAre("8"))));
  EXPECT_THAT(Match(pattern, "9"), IsOkAndHolds(Optional(ElementsAre("9"))));
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
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(ElementsAre("b"))));
  EXPECT_THAT(Match(pattern, "D"), IsOkAndHolds(Optional(ElementsAre("D"))));
  EXPECT_THAT(Match(pattern, "\\D"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\0"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, WordCharacter) {
  auto const status_or_pattern = Parse("\\w");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "A"), IsOkAndHolds(Optional(ElementsAre("A"))));
  EXPECT_THAT(Match(pattern, "B"), IsOkAndHolds(Optional(ElementsAre("B"))));
  EXPECT_THAT(Match(pattern, "C"), IsOkAndHolds(Optional(ElementsAre("C"))));
  EXPECT_THAT(Match(pattern, "D"), IsOkAndHolds(Optional(ElementsAre("D"))));
  EXPECT_THAT(Match(pattern, "E"), IsOkAndHolds(Optional(ElementsAre("E"))));
  EXPECT_THAT(Match(pattern, "F"), IsOkAndHolds(Optional(ElementsAre("F"))));
  EXPECT_THAT(Match(pattern, "G"), IsOkAndHolds(Optional(ElementsAre("G"))));
  EXPECT_THAT(Match(pattern, "H"), IsOkAndHolds(Optional(ElementsAre("H"))));
  EXPECT_THAT(Match(pattern, "I"), IsOkAndHolds(Optional(ElementsAre("I"))));
  EXPECT_THAT(Match(pattern, "J"), IsOkAndHolds(Optional(ElementsAre("J"))));
  EXPECT_THAT(Match(pattern, "K"), IsOkAndHolds(Optional(ElementsAre("K"))));
  EXPECT_THAT(Match(pattern, "L"), IsOkAndHolds(Optional(ElementsAre("L"))));
  EXPECT_THAT(Match(pattern, "M"), IsOkAndHolds(Optional(ElementsAre("M"))));
  EXPECT_THAT(Match(pattern, "N"), IsOkAndHolds(Optional(ElementsAre("N"))));
  EXPECT_THAT(Match(pattern, "O"), IsOkAndHolds(Optional(ElementsAre("O"))));
  EXPECT_THAT(Match(pattern, "P"), IsOkAndHolds(Optional(ElementsAre("P"))));
  EXPECT_THAT(Match(pattern, "Q"), IsOkAndHolds(Optional(ElementsAre("Q"))));
  EXPECT_THAT(Match(pattern, "R"), IsOkAndHolds(Optional(ElementsAre("R"))));
  EXPECT_THAT(Match(pattern, "S"), IsOkAndHolds(Optional(ElementsAre("S"))));
  EXPECT_THAT(Match(pattern, "T"), IsOkAndHolds(Optional(ElementsAre("T"))));
  EXPECT_THAT(Match(pattern, "U"), IsOkAndHolds(Optional(ElementsAre("U"))));
  EXPECT_THAT(Match(pattern, "V"), IsOkAndHolds(Optional(ElementsAre("V"))));
  EXPECT_THAT(Match(pattern, "W"), IsOkAndHolds(Optional(ElementsAre("W"))));
  EXPECT_THAT(Match(pattern, "X"), IsOkAndHolds(Optional(ElementsAre("X"))));
  EXPECT_THAT(Match(pattern, "Y"), IsOkAndHolds(Optional(ElementsAre("Y"))));
  EXPECT_THAT(Match(pattern, "Z"), IsOkAndHolds(Optional(ElementsAre("Z"))));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(ElementsAre("b"))));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(Optional(ElementsAre("c"))));
  EXPECT_THAT(Match(pattern, "d"), IsOkAndHolds(Optional(ElementsAre("d"))));
  EXPECT_THAT(Match(pattern, "e"), IsOkAndHolds(Optional(ElementsAre("e"))));
  EXPECT_THAT(Match(pattern, "f"), IsOkAndHolds(Optional(ElementsAre("f"))));
  EXPECT_THAT(Match(pattern, "g"), IsOkAndHolds(Optional(ElementsAre("g"))));
  EXPECT_THAT(Match(pattern, "h"), IsOkAndHolds(Optional(ElementsAre("h"))));
  EXPECT_THAT(Match(pattern, "i"), IsOkAndHolds(Optional(ElementsAre("i"))));
  EXPECT_THAT(Match(pattern, "j"), IsOkAndHolds(Optional(ElementsAre("j"))));
  EXPECT_THAT(Match(pattern, "k"), IsOkAndHolds(Optional(ElementsAre("k"))));
  EXPECT_THAT(Match(pattern, "l"), IsOkAndHolds(Optional(ElementsAre("l"))));
  EXPECT_THAT(Match(pattern, "m"), IsOkAndHolds(Optional(ElementsAre("m"))));
  EXPECT_THAT(Match(pattern, "n"), IsOkAndHolds(Optional(ElementsAre("n"))));
  EXPECT_THAT(Match(pattern, "o"), IsOkAndHolds(Optional(ElementsAre("o"))));
  EXPECT_THAT(Match(pattern, "p"), IsOkAndHolds(Optional(ElementsAre("p"))));
  EXPECT_THAT(Match(pattern, "q"), IsOkAndHolds(Optional(ElementsAre("q"))));
  EXPECT_THAT(Match(pattern, "r"), IsOkAndHolds(Optional(ElementsAre("r"))));
  EXPECT_THAT(Match(pattern, "s"), IsOkAndHolds(Optional(ElementsAre("s"))));
  EXPECT_THAT(Match(pattern, "t"), IsOkAndHolds(Optional(ElementsAre("t"))));
  EXPECT_THAT(Match(pattern, "u"), IsOkAndHolds(Optional(ElementsAre("u"))));
  EXPECT_THAT(Match(pattern, "v"), IsOkAndHolds(Optional(ElementsAre("v"))));
  EXPECT_THAT(Match(pattern, "w"), IsOkAndHolds(Optional(ElementsAre("w"))));
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(Optional(ElementsAre("x"))));
  EXPECT_THAT(Match(pattern, "y"), IsOkAndHolds(Optional(ElementsAre("y"))));
  EXPECT_THAT(Match(pattern, "z"), IsOkAndHolds(Optional(ElementsAre("z"))));
  EXPECT_THAT(Match(pattern, "0"), IsOkAndHolds(Optional(ElementsAre("0"))));
  EXPECT_THAT(Match(pattern, "1"), IsOkAndHolds(Optional(ElementsAre("1"))));
  EXPECT_THAT(Match(pattern, "2"), IsOkAndHolds(Optional(ElementsAre("2"))));
  EXPECT_THAT(Match(pattern, "3"), IsOkAndHolds(Optional(ElementsAre("3"))));
  EXPECT_THAT(Match(pattern, "4"), IsOkAndHolds(Optional(ElementsAre("4"))));
  EXPECT_THAT(Match(pattern, "5"), IsOkAndHolds(Optional(ElementsAre("5"))));
  EXPECT_THAT(Match(pattern, "6"), IsOkAndHolds(Optional(ElementsAre("6"))));
  EXPECT_THAT(Match(pattern, "7"), IsOkAndHolds(Optional(ElementsAre("7"))));
  EXPECT_THAT(Match(pattern, "8"), IsOkAndHolds(Optional(ElementsAre("8"))));
  EXPECT_THAT(Match(pattern, "9"), IsOkAndHolds(Optional(ElementsAre("9"))));
  EXPECT_THAT(Match(pattern, "_"), IsOkAndHolds(Optional(ElementsAre("_"))));
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
  EXPECT_THAT(Match(pattern, "."), IsOkAndHolds(Optional(ElementsAre("."))));
  EXPECT_THAT(Match(pattern, "-"), IsOkAndHolds(Optional(ElementsAre("-"))));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(Optional(ElementsAre("\\"))));
  EXPECT_THAT(Match(pattern, "\\W"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, Spacing) {
  auto const status_or_pattern = Parse("\\s");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\f"), IsOkAndHolds(Optional(ElementsAre("\f"))));
  EXPECT_THAT(Match(pattern, "\n"), IsOkAndHolds(Optional(ElementsAre("\n"))));
  EXPECT_THAT(Match(pattern, "\r"), IsOkAndHolds(Optional(ElementsAre("\r"))));
  EXPECT_THAT(Match(pattern, "\t"), IsOkAndHolds(Optional(ElementsAre("\t"))));
  EXPECT_THAT(Match(pattern, "\v"), IsOkAndHolds(Optional(ElementsAre("\v"))));
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
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "s"), IsOkAndHolds(Optional(ElementsAre("s"))));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(Optional(ElementsAre("\\"))));
  EXPECT_THAT(Match(pattern, "\\S"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, HorizontalTab) {
  auto const status_or_pattern = Parse("\\t");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\t"), IsOkAndHolds(Optional(ElementsAre("\t"))));
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
  EXPECT_THAT(Match(pattern, "\r"), IsOkAndHolds(Optional(ElementsAre("\r"))));
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
  EXPECT_THAT(Match(pattern, "\n"), IsOkAndHolds(Optional(ElementsAre("\n"))));
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
  EXPECT_THAT(Match(pattern, "\v"), IsOkAndHolds(Optional(ElementsAre("\v"))));
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
  EXPECT_THAT(Match(pattern, "\f"), IsOkAndHolds(Optional(ElementsAre("\f"))));
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
  EXPECT_THAT(Match(pattern, "\x12"), IsOkAndHolds(Optional(ElementsAre("\x12"))));
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
  EXPECT_THAT(Match(pattern, "\xAF"), IsOkAndHolds(Optional(ElementsAre("\xAF"))));
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
  EXPECT_THAT(Match(pattern, "\xAF"), IsOkAndHolds(Optional(ElementsAre("\xAF"))));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\\xaf"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, AnyCharacter) {
  auto const status_or_pattern = Parse(".");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(ElementsAre("b"))));
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
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(ElementsAre("b"))));
  EXPECT_THAT(Match(pattern, "^"), IsOkAndHolds(Optional(ElementsAre("^"))));
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
  EXPECT_THAT(Match(pattern, "l"), IsOkAndHolds(Optional(ElementsAre("l"))));
  EXPECT_THAT(Match(pattern, "o"), IsOkAndHolds(Optional(ElementsAre("o"))));
  EXPECT_THAT(Match(pattern, "r"), IsOkAndHolds(Optional(ElementsAre("r"))));
  EXPECT_THAT(Match(pattern, "e"), IsOkAndHolds(Optional(ElementsAre("e"))));
  EXPECT_THAT(Match(pattern, "m"), IsOkAndHolds(Optional(ElementsAre("m"))));
  EXPECT_THAT(Match(pattern, "\xAF"), IsOkAndHolds(Optional(ElementsAre("\xAF"))));
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
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(ElementsAre("b"))));
  EXPECT_THAT(Match(pattern, "l"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "o"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "r"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "e"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "m"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\xAF"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\xBF"), IsOkAndHolds(Optional(ElementsAre("\xBF"))));
  EXPECT_THAT(Match(pattern, "^"), IsOkAndHolds(Optional(ElementsAre("^"))));
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
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(ElementsAre("b"))));
  EXPECT_THAT(Match(pattern, "^"), IsOkAndHolds(Optional(ElementsAre("^"))));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(Optional(ElementsAre("c"))));
  EXPECT_THAT(Match(pattern, "d"), IsOkAndHolds(Optional(ElementsAre("d"))));
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
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(Optional(ElementsAre("x"))));
  EXPECT_THAT(Match(pattern, "y"), IsOkAndHolds(Optional(ElementsAre("y"))));
  EXPECT_THAT(Match(pattern, "ab^cd"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "^ab^cd"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "[^ab^cd]"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, CharacterClassWithSpecialCharacters) {
  auto const status_or_pattern = Parse("[a^$.(){}|?*+b]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(ElementsAre("b"))));
  EXPECT_THAT(Match(pattern, "^"), IsOkAndHolds(Optional(ElementsAre("^"))));
  EXPECT_THAT(Match(pattern, "$"), IsOkAndHolds(Optional(ElementsAre("$"))));
  EXPECT_THAT(Match(pattern, "."), IsOkAndHolds(Optional(ElementsAre("."))));
  EXPECT_THAT(Match(pattern, "("), IsOkAndHolds(Optional(ElementsAre("("))));
  EXPECT_THAT(Match(pattern, ")"), IsOkAndHolds(Optional(ElementsAre(")"))));
  EXPECT_THAT(Match(pattern, "{"), IsOkAndHolds(Optional(ElementsAre("{"))));
  EXPECT_THAT(Match(pattern, "}"), IsOkAndHolds(Optional(ElementsAre("}"))));
  EXPECT_THAT(Match(pattern, "|"), IsOkAndHolds(Optional(ElementsAre("|"))));
  EXPECT_THAT(Match(pattern, "?"), IsOkAndHolds(Optional(ElementsAre("?"))));
  EXPECT_THAT(Match(pattern, "*"), IsOkAndHolds(Optional(ElementsAre("*"))));
  EXPECT_THAT(Match(pattern, "+"), IsOkAndHolds(Optional(ElementsAre("+"))));
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
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(Optional(ElementsAre("x"))));
  EXPECT_THAT(Match(pattern, "y"), IsOkAndHolds(Optional(ElementsAre("y"))));
}

TEST_P(RegexpTest, CharacterClassWithEscapes) {
  auto const status_or_pattern = Parse("[a\\\\\\^\\$\\.\\(\\)\\[\\]\\{\\}\\|\\?\\*\\+b]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(ElementsAre("b"))));
  EXPECT_THAT(Match(pattern, "\\"), IsOkAndHolds(Optional(ElementsAre("\\"))));
  EXPECT_THAT(Match(pattern, "^"), IsOkAndHolds(Optional(ElementsAre("^"))));
  EXPECT_THAT(Match(pattern, "$"), IsOkAndHolds(Optional(ElementsAre("$"))));
  EXPECT_THAT(Match(pattern, "."), IsOkAndHolds(Optional(ElementsAre("."))));
  EXPECT_THAT(Match(pattern, "("), IsOkAndHolds(Optional(ElementsAre("("))));
  EXPECT_THAT(Match(pattern, ")"), IsOkAndHolds(Optional(ElementsAre(")"))));
  EXPECT_THAT(Match(pattern, "["), IsOkAndHolds(Optional(ElementsAre("["))));
  EXPECT_THAT(Match(pattern, "]"), IsOkAndHolds(Optional(ElementsAre("]"))));
  EXPECT_THAT(Match(pattern, "{"), IsOkAndHolds(Optional(ElementsAre("{"))));
  EXPECT_THAT(Match(pattern, "}"), IsOkAndHolds(Optional(ElementsAre("}"))));
  EXPECT_THAT(Match(pattern, "|"), IsOkAndHolds(Optional(ElementsAre("|"))));
  EXPECT_THAT(Match(pattern, "?"), IsOkAndHolds(Optional(ElementsAre("?"))));
  EXPECT_THAT(Match(pattern, "*"), IsOkAndHolds(Optional(ElementsAre("*"))));
  EXPECT_THAT(Match(pattern, "+"), IsOkAndHolds(Optional(ElementsAre("+"))));
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
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(Optional(ElementsAre("x"))));
  EXPECT_THAT(Match(pattern, "y"), IsOkAndHolds(Optional(ElementsAre("y"))));
}

TEST_P(RegexpTest, CharacterClassWithMoreEscapes) {
  auto const status_or_pattern = Parse("[\\t\\r\\n\\v\\f\\b\\x12\\xAF]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\t"), IsOkAndHolds(Optional(ElementsAre("\t"))));
  EXPECT_THAT(Match(pattern, "\r"), IsOkAndHolds(Optional(ElementsAre("\r"))));
  EXPECT_THAT(Match(pattern, "\n"), IsOkAndHolds(Optional(ElementsAre("\n"))));
  EXPECT_THAT(Match(pattern, "\v"), IsOkAndHolds(Optional(ElementsAre("\v"))));
  EXPECT_THAT(Match(pattern, "\f"), IsOkAndHolds(Optional(ElementsAre("\f"))));
  EXPECT_THAT(Match(pattern, "\b"), IsOkAndHolds(Optional(ElementsAre("\b"))));
  EXPECT_THAT(Match(pattern, "\x12"), IsOkAndHolds(Optional(ElementsAre("\x12"))));
  EXPECT_THAT(Match(pattern, "\xAF"), IsOkAndHolds(Optional(ElementsAre("\xAF"))));
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "y"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, NegatedCharacterClassWithMoreEscapes) {
  auto const status_or_pattern = Parse("[^\\t\\r\\n\\v\\f\\b\\x12\\xAF]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(ElementsAre("b"))));
  EXPECT_THAT(Match(pattern, "\t"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\r"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\n"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\v"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\f"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\x12"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "\xAF"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "x"), IsOkAndHolds(Optional(ElementsAre("x"))));
  EXPECT_THAT(Match(pattern, "y"), IsOkAndHolds(Optional(ElementsAre("y"))));
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
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(Optional(ElementsAre("lorem"))));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolorloremipsum"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, CharacterSequenceWithDot) {
  auto const status_or_pattern = Parse("lo.em");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "l"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(Optional(ElementsAre("lorem"))));
  EXPECT_THAT(Match(pattern, "lo-em"), IsOkAndHolds(Optional(ElementsAre("lo-em"))));
  EXPECT_THAT(Match(pattern, "lovem"), IsOkAndHolds(Optional(ElementsAre("lovem"))));
  EXPECT_THAT(Match(pattern, "lodolorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolorloremipsum"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, KleeneStar) {
  auto const status_or_pattern = Parse("a*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(ElementsAre("aaa"))));
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
  EXPECT_THAT(Match(pattern, "lrem"), IsOkAndHolds(Optional(ElementsAre("lrem"))));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(Optional(ElementsAre("lorem"))));
  EXPECT_THAT(Match(pattern, "loorem"), IsOkAndHolds(Optional(ElementsAre("loorem"))));
  EXPECT_THAT(Match(pattern, "looorem"), IsOkAndHolds(Optional(ElementsAre("looorem"))));
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
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(ElementsAre("aaa"))));
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
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(Optional(ElementsAre("lorem"))));
  EXPECT_THAT(Match(pattern, "loorem"), IsOkAndHolds(Optional(ElementsAre("loorem"))));
  EXPECT_THAT(Match(pattern, "looorem"), IsOkAndHolds(Optional(ElementsAre("looorem"))));
  EXPECT_THAT(Match(pattern, "larem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremlorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "dolorloremipsum"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, Maybe) {
  auto const status_or_pattern = Parse("a?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, Many) {
  auto const status_or_pattern = Parse("a{}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(ElementsAre("aaa"))));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(Optional(ElementsAre("aaaa"))));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(Optional(ElementsAre("aaaaa"))));
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
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
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
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
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
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
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
              IsOkAndHolds(Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"))));
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
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(ElementsAre("aaa"))));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(Optional(ElementsAre("aaaa"))));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(Optional(ElementsAre("aaaaa"))));
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
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(ElementsAre("aaa"))));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(Optional(ElementsAre("aaaa"))));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(Optional(ElementsAre("aaaaa"))));
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
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(ElementsAre("aaa"))));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(Optional(ElementsAre("aaaa"))));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(Optional(ElementsAre("aaaaa"))));
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
              IsOkAndHolds(Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"))));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"))));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"))));
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
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
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
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
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
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
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
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
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
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
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
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
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
              IsOkAndHolds(Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"))));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"))));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"))));
  EXPECT_THAT(Match(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
              IsOkAndHolds(Optional(ElementsAre("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"))));
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
  EXPECT_THAT(Match(pattern, "lrem"), IsOkAndHolds(Optional(ElementsAre("lrem"))));
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(Optional(ElementsAre("lorem"))));
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
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(ElementsAre("aaa"))));
  EXPECT_THAT(Match(pattern, "aaaa"), IsOkAndHolds(Optional(ElementsAre("aaaa"))));
  EXPECT_THAT(Match(pattern, "aaaaa"), IsOkAndHolds(Optional(ElementsAre("aaaaa"))));
}

TEST_P(RegexpTest, EmptyOrEmpty) {
  auto const status_or_pattern = Parse("|");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, EmptyOrA) {
  auto const status_or_pattern = Parse("|a");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
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
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
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
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(ElementsAre("b"))));
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
  EXPECT_THAT(Match(pattern, "lorem"), IsOkAndHolds(Optional(ElementsAre("lorem"))));
  EXPECT_THAT(Match(pattern, "i"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsum"), IsOkAndHolds(Optional(ElementsAre("ipsum"))));
  EXPECT_THAT(Match(pattern, "loremipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "lorem|ipsum"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsumlorem"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ipsum|lorem"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, EmptyBrackets) {
  auto const status_or_pattern = Parse("()");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
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
  EXPECT_THAT(Match(pattern, "loremipsumdolor"),
              IsOkAndHolds(Optional(ElementsAre("loremipsumdolor"))));
}

TEST_P(RegexpTest, EpsilonLoop) {
  auto const status_or_pattern = Parse("(|a)+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(ElementsAre("aaa"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ba"), IsOkAndHolds(std::nullopt));
}

TEST_P(RegexpTest, ChainLoops) {
  auto const status_or_pattern = Parse("a*b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(ElementsAre("aaa"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(ElementsAre("b"))));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(ElementsAre("bb"))));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(Optional(ElementsAre("bbb"))));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(Optional(ElementsAre("ab"))));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(Optional(ElementsAre("aab"))));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(Optional(ElementsAre("abb"))));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(Optional(ElementsAre("aabb"))));
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
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(ElementsAre("b"))));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(ElementsAre("bb"))));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(Optional(ElementsAre("bbb"))));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(Optional(ElementsAre("ab"))));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(Optional(ElementsAre("aab"))));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(Optional(ElementsAre("abb"))));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(Optional(ElementsAre("aabb"))));
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
  EXPECT_THAT(Match(pattern, ""), IsOkAndHolds(Optional(ElementsAre())));
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(ElementsAre("aaa"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(Optional(ElementsAre("b"))));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(Optional(ElementsAre("ab"))));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(Optional(ElementsAre("aab"))));
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
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(Optional(ElementsAre("bb"))));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(Optional(ElementsAre("abb"))));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(Optional(ElementsAre("aabb"))));
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
  EXPECT_THAT(Match(pattern, "a"), IsOkAndHolds(Optional(ElementsAre("a"))));
  EXPECT_THAT(Match(pattern, "aa"), IsOkAndHolds(Optional(ElementsAre("aa"))));
  EXPECT_THAT(Match(pattern, "aaa"), IsOkAndHolds(Optional(ElementsAre("aaa"))));
  EXPECT_THAT(Match(pattern, "b"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "bbb"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "c"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "cc"), IsOkAndHolds(std::nullopt));
  EXPECT_THAT(Match(pattern, "ab"), IsOkAndHolds(Optional(ElementsAre("ab"))));
  EXPECT_THAT(Match(pattern, "aab"), IsOkAndHolds(Optional(ElementsAre("aab"))));
  EXPECT_THAT(Match(pattern, "abb"), IsOkAndHolds(Optional(ElementsAre("abb"))));
  EXPECT_THAT(Match(pattern, "aabb"), IsOkAndHolds(Optional(ElementsAre("aabb"))));
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
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "bbb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_TRUE(Run(pattern, "ab"));
  EXPECT_TRUE(Run(pattern, "aab"));
  EXPECT_TRUE(Run(pattern, "abb"));
  EXPECT_TRUE(Run(pattern, "aabb"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "bba"));
  EXPECT_FALSE(Run(pattern, "baa"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "bab"));
  EXPECT_FALSE(Run(pattern, "ac"));
  EXPECT_FALSE(Run(pattern, "ca"));
  EXPECT_FALSE(Run(pattern, "bc"));
  EXPECT_FALSE(Run(pattern, "cb"));
}

TEST_P(RegexpTest, ChainPlusAndMaybe) {
  auto const status_or_pattern = Parse("a+b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "bbb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_TRUE(Run(pattern, "ab"));
  EXPECT_TRUE(Run(pattern, "aab"));
  EXPECT_FALSE(Run(pattern, "abb"));
  EXPECT_FALSE(Run(pattern, "aabb"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "bba"));
  EXPECT_FALSE(Run(pattern, "baa"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "bab"));
  EXPECT_FALSE(Run(pattern, "ac"));
  EXPECT_FALSE(Run(pattern, "ca"));
  EXPECT_FALSE(Run(pattern, "bc"));
  EXPECT_FALSE(Run(pattern, "cb"));
}

TEST_P(RegexpTest, ChainPlusAndQuantifier) {
  auto const status_or_pattern = Parse("a+b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "bbb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "aab"));
  EXPECT_TRUE(Run(pattern, "abb"));
  EXPECT_TRUE(Run(pattern, "aabb"));
  EXPECT_FALSE(Run(pattern, "aabbb"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "bba"));
  EXPECT_FALSE(Run(pattern, "baa"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "bab"));
  EXPECT_FALSE(Run(pattern, "ac"));
  EXPECT_FALSE(Run(pattern, "ca"));
  EXPECT_FALSE(Run(pattern, "bc"));
  EXPECT_FALSE(Run(pattern, "cb"));
}

TEST_P(RegexpTest, ChainMaybeAndStar) {
  auto const status_or_pattern = Parse("a?b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "bb"));
  EXPECT_TRUE(Run(pattern, "bbb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_TRUE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "aab"));
  EXPECT_TRUE(Run(pattern, "abb"));
  EXPECT_FALSE(Run(pattern, "aabb"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "bba"));
  EXPECT_FALSE(Run(pattern, "baa"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "bab"));
  EXPECT_FALSE(Run(pattern, "ac"));
  EXPECT_FALSE(Run(pattern, "ca"));
  EXPECT_FALSE(Run(pattern, "bc"));
  EXPECT_FALSE(Run(pattern, "cb"));
}

TEST_P(RegexpTest, ChainMaybeAndPlus) {
  auto const status_or_pattern = Parse("a?b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "bb"));
  EXPECT_TRUE(Run(pattern, "bbb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_TRUE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "aab"));
  EXPECT_TRUE(Run(pattern, "abb"));
  EXPECT_FALSE(Run(pattern, "aabb"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "bba"));
  EXPECT_FALSE(Run(pattern, "baa"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "bab"));
  EXPECT_FALSE(Run(pattern, "ac"));
  EXPECT_FALSE(Run(pattern, "ca"));
  EXPECT_FALSE(Run(pattern, "bc"));
  EXPECT_FALSE(Run(pattern, "cb"));
}

TEST_P(RegexpTest, ChainMaybeAndMaybe) {
  auto const status_or_pattern = Parse("a?b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "bbb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_TRUE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "aab"));
  EXPECT_FALSE(Run(pattern, "abb"));
  EXPECT_FALSE(Run(pattern, "aabb"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "bba"));
  EXPECT_FALSE(Run(pattern, "baa"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "bab"));
  EXPECT_FALSE(Run(pattern, "ac"));
  EXPECT_FALSE(Run(pattern, "ca"));
  EXPECT_FALSE(Run(pattern, "bc"));
  EXPECT_FALSE(Run(pattern, "cb"));
}

TEST_P(RegexpTest, ChainMaybeAndQuantifier) {
  auto const status_or_pattern = Parse("a?b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "bbb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "aab"));
  EXPECT_TRUE(Run(pattern, "abb"));
  EXPECT_FALSE(Run(pattern, "aabb"));
  EXPECT_FALSE(Run(pattern, "aabbb"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "bba"));
  EXPECT_FALSE(Run(pattern, "baa"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "bab"));
  EXPECT_FALSE(Run(pattern, "ac"));
  EXPECT_FALSE(Run(pattern, "ca"));
  EXPECT_FALSE(Run(pattern, "bc"));
  EXPECT_FALSE(Run(pattern, "cb"));
}

TEST_P(RegexpTest, ChainQuantifierAndStar) {
  auto const status_or_pattern = Parse("a{2}b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "bbb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_TRUE(Run(pattern, "aab"));
  EXPECT_FALSE(Run(pattern, "abb"));
  EXPECT_TRUE(Run(pattern, "aabb"));
  EXPECT_FALSE(Run(pattern, "aaabb"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "bba"));
  EXPECT_FALSE(Run(pattern, "baa"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "bab"));
  EXPECT_FALSE(Run(pattern, "ac"));
  EXPECT_FALSE(Run(pattern, "ca"));
  EXPECT_FALSE(Run(pattern, "bc"));
  EXPECT_FALSE(Run(pattern, "cb"));
}

TEST_P(RegexpTest, ChainQuantifierAndPlus) {
  auto const status_or_pattern = Parse("a{2}b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "bbb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_TRUE(Run(pattern, "aab"));
  EXPECT_FALSE(Run(pattern, "abb"));
  EXPECT_TRUE(Run(pattern, "aabb"));
  EXPECT_FALSE(Run(pattern, "aaabb"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "bba"));
  EXPECT_FALSE(Run(pattern, "baa"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "bab"));
  EXPECT_FALSE(Run(pattern, "ac"));
  EXPECT_FALSE(Run(pattern, "ca"));
  EXPECT_FALSE(Run(pattern, "bc"));
  EXPECT_FALSE(Run(pattern, "cb"));
}

TEST_P(RegexpTest, ChainQuantifierAndMaybe) {
  auto const status_or_pattern = Parse("a{2}b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "bbb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_TRUE(Run(pattern, "aab"));
  EXPECT_FALSE(Run(pattern, "abb"));
  EXPECT_FALSE(Run(pattern, "aabb"));
  EXPECT_FALSE(Run(pattern, "aaabb"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "bba"));
  EXPECT_FALSE(Run(pattern, "baa"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "bab"));
  EXPECT_FALSE(Run(pattern, "ac"));
  EXPECT_FALSE(Run(pattern, "ca"));
  EXPECT_FALSE(Run(pattern, "bc"));
  EXPECT_FALSE(Run(pattern, "cb"));
}

TEST_P(RegexpTest, ChainQuantifiers) {
  auto const status_or_pattern = Parse("a{3}b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "bbb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "aab"));
  EXPECT_FALSE(Run(pattern, "abb"));
  EXPECT_FALSE(Run(pattern, "aabb"));
  EXPECT_TRUE(Run(pattern, "aaabb"));
  EXPECT_FALSE(Run(pattern, "aaabbb"));
  EXPECT_FALSE(Run(pattern, "aaaabbb"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "bba"));
  EXPECT_FALSE(Run(pattern, "baa"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "bab"));
  EXPECT_FALSE(Run(pattern, "ac"));
  EXPECT_FALSE(Run(pattern, "ca"));
  EXPECT_FALSE(Run(pattern, "bc"));
  EXPECT_FALSE(Run(pattern, "cb"));
}

TEST_P(RegexpTest, PipeLoops) {
  auto const status_or_pattern = Parse("a*|b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
}

TEST_P(RegexpTest, StarOrPlus) {
  auto const status_or_pattern = Parse("a*|b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
}

TEST_P(RegexpTest, StarOrMaybe) {
  auto const status_or_pattern = Parse("a*|b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
}

TEST_P(RegexpTest, StarOrQuantifier) {
  auto const status_or_pattern = Parse("a*|b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "bbb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "abb"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "bba"));
}

TEST_P(RegexpTest, PlusOrStar) {
  auto const status_or_pattern = Parse("a+|b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
}

TEST_P(RegexpTest, PlusOrPlus) {
  auto const status_or_pattern = Parse("a+|b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
}

TEST_P(RegexpTest, PlusOrMaybe) {
  auto const status_or_pattern = Parse("a+|b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
}

TEST_P(RegexpTest, PlusOrQuantifier) {
  auto const status_or_pattern = Parse("a+|b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "bbb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "abb"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "bba"));
}

TEST_P(RegexpTest, MaybeOrStar) {
  auto const status_or_pattern = Parse("a?|b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
}

TEST_P(RegexpTest, MaybeOrPlus) {
  auto const status_or_pattern = Parse("a?|b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
}

TEST_P(RegexpTest, MaybeOrMaybe) {
  auto const status_or_pattern = Parse("a?|b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
}

TEST_P(RegexpTest, MaybeOrQuantifier) {
  auto const status_or_pattern = Parse("a?|b{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "bbb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "abb"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "bba"));
}

TEST_P(RegexpTest, QuantifierOrStar) {
  auto const status_or_pattern = Parse("a{2}|b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aab"));
  EXPECT_FALSE(Run(pattern, "aabb"));
}

TEST_P(RegexpTest, QuantifierOrPlus) {
  auto const status_or_pattern = Parse("a{2}|b+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aab"));
  EXPECT_FALSE(Run(pattern, "aabb"));
}

TEST_P(RegexpTest, QuantifierOrMaybe) {
  auto const status_or_pattern = Parse("a{2}|b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aab"));
  EXPECT_FALSE(Run(pattern, "aabb"));
}

TEST_P(RegexpTest, QuantifierOrQuantifier) {
  auto const status_or_pattern = Parse("a{2}|b{3}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "bb"));
  EXPECT_TRUE(Run(pattern, "bbb"));
  EXPECT_FALSE(Run(pattern, "bbbb"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "cc"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aab"));
  EXPECT_FALSE(Run(pattern, "aabb"));
}

TEST_P(RegexpTest, Fork) {
  auto const status_or_pattern = Parse("lorem(ipsum|dolor)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  auto const runner1 = pattern->CreateRunner();
  EXPECT_TRUE(runner1->Step("lorem"));
  auto const runner2 = runner1->Clone();
  EXPECT_TRUE(runner1->Step("ipsum") && runner1->Finish());
  EXPECT_TRUE(runner2->Step("dolor") && runner2->Finish());
}

TEST_P(RegexpTest, Search) {
  auto const status_or_pattern = Parse(".*do+lor.*");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, "lorem ipsum dolor sic amat"));
  EXPECT_TRUE(Run(pattern, "lorem ipsum dooolor sic amat"));
  EXPECT_FALSE(Run(pattern, "lorem ipsum color sic amat"));
  EXPECT_FALSE(Run(pattern, "lorem ipsum dolet et amat"));
}

TEST_P(RegexpTest, HeavyBacktracker) {
  auto const status_or_pattern = Parse(
      "a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
}

INSTANTIATE_TEST_SUITE_P(RegexpTest, RegexpTest,
                         Values(RegexpTestParams{.force_nfa = false, .use_runner = false},
                                RegexpTestParams{.force_nfa = false, .use_runner = true},
                                RegexpTestParams{.force_nfa = true, .use_runner = false},
                                RegexpTestParams{.force_nfa = true, .use_runner = true}));

}  // namespace
