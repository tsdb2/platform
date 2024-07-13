#include <string_view>

#include "absl/status/status.h"
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

using ::testing::Values;
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
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "hello"));
  EXPECT_TRUE(Run(pattern, ""));
}

TEST_P(RegexpTest, SimpleCharacter) {
  auto const status_or_pattern = Parse("a");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "anchor"));
  EXPECT_FALSE(Run(pattern, "banana"));
  EXPECT_FALSE(Run(pattern, ""));
}

TEST_P(RegexpTest, AnotherSimpleCharacter) {
  auto const status_or_pattern = Parse("b");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "anchor"));
  EXPECT_FALSE(Run(pattern, "banana"));
  EXPECT_FALSE(Run(pattern, ""));
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
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "0"));
  EXPECT_TRUE(Run(pattern, "1"));
  EXPECT_TRUE(Run(pattern, "2"));
  EXPECT_TRUE(Run(pattern, "3"));
  EXPECT_TRUE(Run(pattern, "4"));
  EXPECT_TRUE(Run(pattern, "5"));
  EXPECT_TRUE(Run(pattern, "6"));
  EXPECT_TRUE(Run(pattern, "7"));
  EXPECT_TRUE(Run(pattern, "8"));
  EXPECT_TRUE(Run(pattern, "9"));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "d"));
  EXPECT_FALSE(Run(pattern, "\\d"));
  EXPECT_FALSE(Run(pattern, "\\0"));
}

TEST_P(RegexpTest, NotDigit) {
  auto const status_or_pattern = Parse("\\D");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "0"));
  EXPECT_FALSE(Run(pattern, "1"));
  EXPECT_FALSE(Run(pattern, "2"));
  EXPECT_FALSE(Run(pattern, "3"));
  EXPECT_FALSE(Run(pattern, "4"));
  EXPECT_FALSE(Run(pattern, "5"));
  EXPECT_FALSE(Run(pattern, "6"));
  EXPECT_FALSE(Run(pattern, "7"));
  EXPECT_FALSE(Run(pattern, "8"));
  EXPECT_FALSE(Run(pattern, "9"));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "D"));
  EXPECT_FALSE(Run(pattern, "\\D"));
  EXPECT_FALSE(Run(pattern, "\\0"));
}

TEST_P(RegexpTest, WordCharacter) {
  auto const status_or_pattern = Parse("\\w");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "A"));
  EXPECT_TRUE(Run(pattern, "B"));
  EXPECT_TRUE(Run(pattern, "C"));
  EXPECT_TRUE(Run(pattern, "D"));
  EXPECT_TRUE(Run(pattern, "E"));
  EXPECT_TRUE(Run(pattern, "F"));
  EXPECT_TRUE(Run(pattern, "G"));
  EXPECT_TRUE(Run(pattern, "H"));
  EXPECT_TRUE(Run(pattern, "I"));
  EXPECT_TRUE(Run(pattern, "J"));
  EXPECT_TRUE(Run(pattern, "K"));
  EXPECT_TRUE(Run(pattern, "L"));
  EXPECT_TRUE(Run(pattern, "M"));
  EXPECT_TRUE(Run(pattern, "N"));
  EXPECT_TRUE(Run(pattern, "O"));
  EXPECT_TRUE(Run(pattern, "P"));
  EXPECT_TRUE(Run(pattern, "Q"));
  EXPECT_TRUE(Run(pattern, "R"));
  EXPECT_TRUE(Run(pattern, "S"));
  EXPECT_TRUE(Run(pattern, "T"));
  EXPECT_TRUE(Run(pattern, "U"));
  EXPECT_TRUE(Run(pattern, "V"));
  EXPECT_TRUE(Run(pattern, "W"));
  EXPECT_TRUE(Run(pattern, "X"));
  EXPECT_TRUE(Run(pattern, "Y"));
  EXPECT_TRUE(Run(pattern, "Z"));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "c"));
  EXPECT_TRUE(Run(pattern, "d"));
  EXPECT_TRUE(Run(pattern, "e"));
  EXPECT_TRUE(Run(pattern, "f"));
  EXPECT_TRUE(Run(pattern, "g"));
  EXPECT_TRUE(Run(pattern, "h"));
  EXPECT_TRUE(Run(pattern, "i"));
  EXPECT_TRUE(Run(pattern, "j"));
  EXPECT_TRUE(Run(pattern, "k"));
  EXPECT_TRUE(Run(pattern, "l"));
  EXPECT_TRUE(Run(pattern, "m"));
  EXPECT_TRUE(Run(pattern, "n"));
  EXPECT_TRUE(Run(pattern, "o"));
  EXPECT_TRUE(Run(pattern, "p"));
  EXPECT_TRUE(Run(pattern, "q"));
  EXPECT_TRUE(Run(pattern, "r"));
  EXPECT_TRUE(Run(pattern, "s"));
  EXPECT_TRUE(Run(pattern, "t"));
  EXPECT_TRUE(Run(pattern, "u"));
  EXPECT_TRUE(Run(pattern, "v"));
  EXPECT_TRUE(Run(pattern, "w"));
  EXPECT_TRUE(Run(pattern, "x"));
  EXPECT_TRUE(Run(pattern, "y"));
  EXPECT_TRUE(Run(pattern, "z"));
  EXPECT_TRUE(Run(pattern, "0"));
  EXPECT_TRUE(Run(pattern, "1"));
  EXPECT_TRUE(Run(pattern, "2"));
  EXPECT_TRUE(Run(pattern, "3"));
  EXPECT_TRUE(Run(pattern, "4"));
  EXPECT_TRUE(Run(pattern, "5"));
  EXPECT_TRUE(Run(pattern, "6"));
  EXPECT_TRUE(Run(pattern, "7"));
  EXPECT_TRUE(Run(pattern, "8"));
  EXPECT_TRUE(Run(pattern, "9"));
  EXPECT_TRUE(Run(pattern, "_"));
  EXPECT_FALSE(Run(pattern, "."));
  EXPECT_FALSE(Run(pattern, "-"));
  EXPECT_FALSE(Run(pattern, "\\"));
  EXPECT_FALSE(Run(pattern, "\\w"));
}

TEST_P(RegexpTest, NotWordCharacter) {
  auto const status_or_pattern = Parse("\\W");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "A"));
  EXPECT_FALSE(Run(pattern, "B"));
  EXPECT_FALSE(Run(pattern, "C"));
  EXPECT_FALSE(Run(pattern, "D"));
  EXPECT_FALSE(Run(pattern, "E"));
  EXPECT_FALSE(Run(pattern, "F"));
  EXPECT_FALSE(Run(pattern, "G"));
  EXPECT_FALSE(Run(pattern, "H"));
  EXPECT_FALSE(Run(pattern, "I"));
  EXPECT_FALSE(Run(pattern, "J"));
  EXPECT_FALSE(Run(pattern, "K"));
  EXPECT_FALSE(Run(pattern, "L"));
  EXPECT_FALSE(Run(pattern, "M"));
  EXPECT_FALSE(Run(pattern, "N"));
  EXPECT_FALSE(Run(pattern, "O"));
  EXPECT_FALSE(Run(pattern, "P"));
  EXPECT_FALSE(Run(pattern, "Q"));
  EXPECT_FALSE(Run(pattern, "R"));
  EXPECT_FALSE(Run(pattern, "S"));
  EXPECT_FALSE(Run(pattern, "T"));
  EXPECT_FALSE(Run(pattern, "U"));
  EXPECT_FALSE(Run(pattern, "V"));
  EXPECT_FALSE(Run(pattern, "W"));
  EXPECT_FALSE(Run(pattern, "X"));
  EXPECT_FALSE(Run(pattern, "Y"));
  EXPECT_FALSE(Run(pattern, "Z"));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "d"));
  EXPECT_FALSE(Run(pattern, "e"));
  EXPECT_FALSE(Run(pattern, "f"));
  EXPECT_FALSE(Run(pattern, "g"));
  EXPECT_FALSE(Run(pattern, "h"));
  EXPECT_FALSE(Run(pattern, "i"));
  EXPECT_FALSE(Run(pattern, "j"));
  EXPECT_FALSE(Run(pattern, "k"));
  EXPECT_FALSE(Run(pattern, "l"));
  EXPECT_FALSE(Run(pattern, "m"));
  EXPECT_FALSE(Run(pattern, "n"));
  EXPECT_FALSE(Run(pattern, "o"));
  EXPECT_FALSE(Run(pattern, "p"));
  EXPECT_FALSE(Run(pattern, "q"));
  EXPECT_FALSE(Run(pattern, "r"));
  EXPECT_FALSE(Run(pattern, "s"));
  EXPECT_FALSE(Run(pattern, "t"));
  EXPECT_FALSE(Run(pattern, "u"));
  EXPECT_FALSE(Run(pattern, "v"));
  EXPECT_FALSE(Run(pattern, "w"));
  EXPECT_FALSE(Run(pattern, "x"));
  EXPECT_FALSE(Run(pattern, "y"));
  EXPECT_FALSE(Run(pattern, "z"));
  EXPECT_FALSE(Run(pattern, "0"));
  EXPECT_FALSE(Run(pattern, "1"));
  EXPECT_FALSE(Run(pattern, "2"));
  EXPECT_FALSE(Run(pattern, "3"));
  EXPECT_FALSE(Run(pattern, "4"));
  EXPECT_FALSE(Run(pattern, "5"));
  EXPECT_FALSE(Run(pattern, "6"));
  EXPECT_FALSE(Run(pattern, "7"));
  EXPECT_FALSE(Run(pattern, "8"));
  EXPECT_FALSE(Run(pattern, "9"));
  EXPECT_FALSE(Run(pattern, "_"));
  EXPECT_TRUE(Run(pattern, "."));
  EXPECT_TRUE(Run(pattern, "-"));
  EXPECT_TRUE(Run(pattern, "\\"));
  EXPECT_FALSE(Run(pattern, "\\w"));
}

TEST_P(RegexpTest, Spacing) {
  auto const status_or_pattern = Parse("\\s");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "\f"));
  EXPECT_TRUE(Run(pattern, "\n"));
  EXPECT_TRUE(Run(pattern, "\r"));
  EXPECT_TRUE(Run(pattern, "\t"));
  EXPECT_TRUE(Run(pattern, "\v"));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "s"));
  EXPECT_FALSE(Run(pattern, "\\"));
  EXPECT_FALSE(Run(pattern, "\\s"));
}

TEST_P(RegexpTest, NotSpacing) {
  auto const status_or_pattern = Parse("\\S");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "\f"));
  EXPECT_FALSE(Run(pattern, "\n"));
  EXPECT_FALSE(Run(pattern, "\r"));
  EXPECT_FALSE(Run(pattern, "\t"));
  EXPECT_FALSE(Run(pattern, "\v"));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "s"));
  EXPECT_TRUE(Run(pattern, "\\"));
  EXPECT_FALSE(Run(pattern, "\\s"));
}

TEST_P(RegexpTest, HorizontalTab) {
  auto const status_or_pattern = Parse("\\t");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "\t"));
  EXPECT_FALSE(Run(pattern, "\n"));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "t"));
  EXPECT_FALSE(Run(pattern, "\\"));
  EXPECT_FALSE(Run(pattern, "\\t"));
}

TEST_P(RegexpTest, CarriageReturn) {
  auto const status_or_pattern = Parse("\\r");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "\r"));
  EXPECT_FALSE(Run(pattern, "\n"));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "r"));
  EXPECT_FALSE(Run(pattern, "\\"));
  EXPECT_FALSE(Run(pattern, "\\r"));
}

TEST_P(RegexpTest, LineFeed) {
  auto const status_or_pattern = Parse("\\n");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "\n"));
  EXPECT_FALSE(Run(pattern, "\t"));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "n"));
  EXPECT_FALSE(Run(pattern, "\\"));
  EXPECT_FALSE(Run(pattern, "\\n"));
}

TEST_P(RegexpTest, VerticalTab) {
  auto const status_or_pattern = Parse("\\v");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "\v"));
  EXPECT_FALSE(Run(pattern, "\n"));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "v"));
  EXPECT_FALSE(Run(pattern, "\\"));
  EXPECT_FALSE(Run(pattern, "\\v"));
}

TEST_P(RegexpTest, FormFeed) {
  auto const status_or_pattern = Parse("\\f");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "\f"));
  EXPECT_FALSE(Run(pattern, "\n"));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "f"));
  EXPECT_FALSE(Run(pattern, "\\"));
  EXPECT_FALSE(Run(pattern, "\\f"));
}

TEST_P(RegexpTest, InvalidHexCode) {
  EXPECT_THAT(Parse("\\xZ0"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\x0Z"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(RegexpTest, HexCode1) {
  auto const status_or_pattern = Parse("\\x12");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "\x12"));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "x"));
  EXPECT_FALSE(Run(pattern, "\\"));
  EXPECT_FALSE(Run(pattern, "\\x12"));
}

TEST_P(RegexpTest, HexCode2) {
  auto const status_or_pattern = Parse("\\xAF");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "\xAF"));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "x"));
  EXPECT_FALSE(Run(pattern, "\\"));
  EXPECT_FALSE(Run(pattern, "\\xAF"));
}

TEST_P(RegexpTest, HexCode3) {
  auto const status_or_pattern = Parse("\\xaf");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "\xAF"));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "x"));
  EXPECT_FALSE(Run(pattern, "\\"));
  EXPECT_FALSE(Run(pattern, "\\xaf"));
}

TEST_P(RegexpTest, AnyCharacter) {
  auto const status_or_pattern = Parse(".");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "anchor"));
  EXPECT_FALSE(Run(pattern, "banana"));
  EXPECT_FALSE(Run(pattern, ""));
}

TEST_P(RegexpTest, EmptyCharacterClass) {
  auto const status_or_pattern = Parse("[]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "lorem"));
  EXPECT_FALSE(Run(pattern, "ipsum"));
  EXPECT_FALSE(Run(pattern, "[]"));
}

TEST_P(RegexpTest, NegatedEmptyCharacterClass) {
  auto const status_or_pattern = Parse("[^]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "^"));
  EXPECT_FALSE(Run(pattern, "lorem"));
  EXPECT_FALSE(Run(pattern, "ipsum"));
  EXPECT_FALSE(Run(pattern, "[^]"));
}

TEST_P(RegexpTest, CharacterClass) {
  auto const status_or_pattern = Parse("[lorem\xAF]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "l"));
  EXPECT_TRUE(Run(pattern, "o"));
  EXPECT_TRUE(Run(pattern, "r"));
  EXPECT_TRUE(Run(pattern, "e"));
  EXPECT_TRUE(Run(pattern, "m"));
  EXPECT_TRUE(Run(pattern, "\xAF"));
  EXPECT_FALSE(Run(pattern, "\xBF"));
  EXPECT_FALSE(Run(pattern, "lorem"));
  EXPECT_FALSE(Run(pattern, "[lorem]"));
}

TEST_P(RegexpTest, NegatedCharacterClass) {
  auto const status_or_pattern = Parse("[^lorem\xAF]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "l"));
  EXPECT_FALSE(Run(pattern, "o"));
  EXPECT_FALSE(Run(pattern, "r"));
  EXPECT_FALSE(Run(pattern, "e"));
  EXPECT_FALSE(Run(pattern, "m"));
  EXPECT_FALSE(Run(pattern, "\xAF"));
  EXPECT_TRUE(Run(pattern, "\xBF"));
  EXPECT_TRUE(Run(pattern, "^"));
  EXPECT_FALSE(Run(pattern, "lorem"));
  EXPECT_FALSE(Run(pattern, "^lorem"));
  EXPECT_FALSE(Run(pattern, "[^lorem]"));
}

TEST_P(RegexpTest, CharacterClassWithCircumflex) {
  auto const status_or_pattern = Parse("[ab^cd]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "^"));
  EXPECT_TRUE(Run(pattern, "c"));
  EXPECT_TRUE(Run(pattern, "d"));
  EXPECT_FALSE(Run(pattern, "ab^cd"));
}

TEST_P(RegexpTest, NegatedCharacterClassWithCircumflex) {
  auto const status_or_pattern = Parse("[^ab^cd]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "^"));
  EXPECT_FALSE(Run(pattern, "c"));
  EXPECT_FALSE(Run(pattern, "d"));
  EXPECT_TRUE(Run(pattern, "x"));
  EXPECT_TRUE(Run(pattern, "y"));
  EXPECT_FALSE(Run(pattern, "ab^cd"));
}

TEST_P(RegexpTest, CharacterClassWithSpecialCharacters) {
  auto const status_or_pattern = Parse("[a^$.(){}|?*+b]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "^"));
  EXPECT_TRUE(Run(pattern, "$"));
  EXPECT_TRUE(Run(pattern, "."));
  EXPECT_TRUE(Run(pattern, "("));
  EXPECT_TRUE(Run(pattern, ")"));
  EXPECT_TRUE(Run(pattern, "{"));
  EXPECT_TRUE(Run(pattern, "}"));
  EXPECT_TRUE(Run(pattern, "|"));
  EXPECT_TRUE(Run(pattern, "?"));
  EXPECT_TRUE(Run(pattern, "*"));
  EXPECT_TRUE(Run(pattern, "+"));
  EXPECT_FALSE(Run(pattern, "x"));
  EXPECT_FALSE(Run(pattern, "y"));
}

TEST_P(RegexpTest, NegatedCharacterClassWithSpecialCharacters) {
  auto const status_or_pattern = Parse("[^a^$.(){}|?*+b]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "^"));
  EXPECT_FALSE(Run(pattern, "$"));
  EXPECT_FALSE(Run(pattern, "."));
  EXPECT_FALSE(Run(pattern, "("));
  EXPECT_FALSE(Run(pattern, ")"));
  EXPECT_FALSE(Run(pattern, "{"));
  EXPECT_FALSE(Run(pattern, "}"));
  EXPECT_FALSE(Run(pattern, "|"));
  EXPECT_FALSE(Run(pattern, "?"));
  EXPECT_FALSE(Run(pattern, "*"));
  EXPECT_FALSE(Run(pattern, "+"));
  EXPECT_TRUE(Run(pattern, "x"));
  EXPECT_TRUE(Run(pattern, "y"));
}

TEST_P(RegexpTest, CharacterClassWithEscapes) {
  auto const status_or_pattern = Parse("[a\\\\\\^\\$\\.\\(\\)\\[\\]\\{\\}\\|\\?\\*\\+b]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "\\"));
  EXPECT_TRUE(Run(pattern, "^"));
  EXPECT_TRUE(Run(pattern, "$"));
  EXPECT_TRUE(Run(pattern, "."));
  EXPECT_TRUE(Run(pattern, "("));
  EXPECT_TRUE(Run(pattern, ")"));
  EXPECT_TRUE(Run(pattern, "["));
  EXPECT_TRUE(Run(pattern, "]"));
  EXPECT_TRUE(Run(pattern, "{"));
  EXPECT_TRUE(Run(pattern, "}"));
  EXPECT_TRUE(Run(pattern, "|"));
  EXPECT_TRUE(Run(pattern, "?"));
  EXPECT_TRUE(Run(pattern, "*"));
  EXPECT_TRUE(Run(pattern, "+"));
  EXPECT_FALSE(Run(pattern, "x"));
  EXPECT_FALSE(Run(pattern, "y"));
}

TEST_P(RegexpTest, NegatedCharacterClassWithEscapes) {
  auto const status_or_pattern = Parse("[^a\\\\\\^\\$\\.\\(\\)\\[\\]\\{\\}\\|\\?\\*\\+b]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "\\"));
  EXPECT_FALSE(Run(pattern, "^"));
  EXPECT_FALSE(Run(pattern, "$"));
  EXPECT_FALSE(Run(pattern, "."));
  EXPECT_FALSE(Run(pattern, "("));
  EXPECT_FALSE(Run(pattern, ")"));
  EXPECT_FALSE(Run(pattern, "["));
  EXPECT_FALSE(Run(pattern, "]"));
  EXPECT_FALSE(Run(pattern, "{"));
  EXPECT_FALSE(Run(pattern, "}"));
  EXPECT_FALSE(Run(pattern, "|"));
  EXPECT_FALSE(Run(pattern, "?"));
  EXPECT_FALSE(Run(pattern, "*"));
  EXPECT_FALSE(Run(pattern, "+"));
  EXPECT_TRUE(Run(pattern, "x"));
  EXPECT_TRUE(Run(pattern, "y"));
}

TEST_P(RegexpTest, CharacterClassWithMoreEscapes) {
  auto const status_or_pattern = Parse("[\\t\\r\\n\\v\\f\\b\\x12\\xAF]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "\t"));
  EXPECT_TRUE(Run(pattern, "\r"));
  EXPECT_TRUE(Run(pattern, "\n"));
  EXPECT_TRUE(Run(pattern, "\v"));
  EXPECT_TRUE(Run(pattern, "\f"));
  EXPECT_TRUE(Run(pattern, "\b"));
  EXPECT_TRUE(Run(pattern, "\x12"));
  EXPECT_TRUE(Run(pattern, "\xAF"));
  EXPECT_FALSE(Run(pattern, "x"));
  EXPECT_FALSE(Run(pattern, "y"));
}

TEST_P(RegexpTest, NegatedCharacterClassWithMoreEscapes) {
  auto const status_or_pattern = Parse("[^\\t\\r\\n\\v\\f\\b\\x12\\xAF]");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "\t"));
  EXPECT_FALSE(Run(pattern, "\r"));
  EXPECT_FALSE(Run(pattern, "\n"));
  EXPECT_FALSE(Run(pattern, "\v"));
  EXPECT_FALSE(Run(pattern, "\f"));
  EXPECT_FALSE(Run(pattern, "\b"));
  EXPECT_FALSE(Run(pattern, "\x12"));
  EXPECT_FALSE(Run(pattern, "\xAF"));
  EXPECT_TRUE(Run(pattern, "x"));
  EXPECT_TRUE(Run(pattern, "y"));
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
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "l"));
  EXPECT_TRUE(Run(pattern, "lorem"));
  EXPECT_FALSE(Run(pattern, "loremipsum"));
  EXPECT_FALSE(Run(pattern, "dolorloremipsum"));
}

TEST_P(RegexpTest, CharacterSequenceWithDot) {
  auto const status_or_pattern = Parse("lo.em");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "l"));
  EXPECT_TRUE(Run(pattern, "lorem"));
  EXPECT_TRUE(Run(pattern, "lo-em"));
  EXPECT_TRUE(Run(pattern, "lovem"));
  EXPECT_FALSE(Run(pattern, "lodolorem"));
  EXPECT_FALSE(Run(pattern, "loremipsum"));
  EXPECT_FALSE(Run(pattern, "dolorloremipsum"));
}

TEST_P(RegexpTest, KleeneStar) {
  auto const status_or_pattern = Parse("a*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, CharacterSequenceWithStar) {
  auto const status_or_pattern = Parse("lo*rem");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "l"));
  EXPECT_TRUE(Run(pattern, "lrem"));
  EXPECT_TRUE(Run(pattern, "lorem"));
  EXPECT_TRUE(Run(pattern, "loorem"));
  EXPECT_TRUE(Run(pattern, "looorem"));
  EXPECT_FALSE(Run(pattern, "larem"));
  EXPECT_FALSE(Run(pattern, "loremlorem"));
  EXPECT_FALSE(Run(pattern, "loremipsum"));
  EXPECT_FALSE(Run(pattern, "dolorloremipsum"));
}

TEST_P(RegexpTest, KleenePlus) {
  auto const status_or_pattern = Parse("a+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, CharacterSequenceWithPlus) {
  auto const status_or_pattern = Parse("lo+rem");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "l"));
  EXPECT_FALSE(Run(pattern, "lrem"));
  EXPECT_TRUE(Run(pattern, "lorem"));
  EXPECT_TRUE(Run(pattern, "loorem"));
  EXPECT_TRUE(Run(pattern, "looorem"));
  EXPECT_FALSE(Run(pattern, "larem"));
  EXPECT_FALSE(Run(pattern, "loremlorem"));
  EXPECT_FALSE(Run(pattern, "loremipsum"));
  EXPECT_FALSE(Run(pattern, "dolorloremipsum"));
}

TEST_P(RegexpTest, Maybe) {
  auto const status_or_pattern = Parse("a?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
}

TEST_P(RegexpTest, Many) {
  auto const status_or_pattern = Parse("a{}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "aaa"));
  EXPECT_TRUE(Run(pattern, "aaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, ExactlyZero) {
  auto const status_or_pattern = Parse("a{0}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, ExactlyOne) {
  auto const status_or_pattern = Parse("a{1}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, ExactlyTwo) {
  auto const status_or_pattern = Parse("a{2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, ExactlyFourtyTwo) {
  auto const status_or_pattern = Parse("a{42}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, AtLeastZero) {
  auto const status_or_pattern = Parse("a{0,}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "aaa"));
  EXPECT_TRUE(Run(pattern, "aaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, AtLeastOne) {
  auto const status_or_pattern = Parse("a{1,}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "aaa"));
  EXPECT_TRUE(Run(pattern, "aaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, AtLeastTwo) {
  auto const status_or_pattern = Parse("a{2,}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "aaa"));
  EXPECT_TRUE(Run(pattern, "aaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, AtLeastFourtyTwo) {
  auto const status_or_pattern = Parse("a{42,}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, BetweenZeroAndZero) {
  auto const status_or_pattern = Parse("a{0,0}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "aaaa"));
  EXPECT_FALSE(Run(pattern, "aaaaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, BetweenZeroAndOne) {
  auto const status_or_pattern = Parse("a{0,1}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "aaaa"));
  EXPECT_FALSE(Run(pattern, "aaaaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, BetweenZeroAndTwo) {
  auto const status_or_pattern = Parse("a{0,2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "aaaa"));
  EXPECT_FALSE(Run(pattern, "aaaaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, BetweenOneAndOne) {
  auto const status_or_pattern = Parse("a{1,1}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "aaaa"));
  EXPECT_FALSE(Run(pattern, "aaaaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, BetweenOneAndTwo) {
  auto const status_or_pattern = Parse("a{1,2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "aaaa"));
  EXPECT_FALSE(Run(pattern, "aaaaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, BetweenTwoAndTwo) {
  auto const status_or_pattern = Parse("a{2,2}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "aaaa"));
  EXPECT_FALSE(Run(pattern, "aaaaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, BetweenFourtyTwoAndFourtyFive) {
  auto const status_or_pattern = Parse("a{42,45}");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(Run(pattern, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "aabaa"));
}

TEST_P(RegexpTest, CharacterSequenceWithMaybe) {
  auto const status_or_pattern = Parse("lo?rem");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "l"));
  EXPECT_TRUE(Run(pattern, "lrem"));
  EXPECT_TRUE(Run(pattern, "lorem"));
  EXPECT_FALSE(Run(pattern, "loorem"));
  EXPECT_FALSE(Run(pattern, "looorem"));
  EXPECT_FALSE(Run(pattern, "larem"));
  EXPECT_FALSE(Run(pattern, "loremlorem"));
  EXPECT_FALSE(Run(pattern, "loremipsum"));
  EXPECT_FALSE(Run(pattern, "dolorloremipsum"));
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
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_TRUE(Run(pattern, "aaa"));
  EXPECT_TRUE(Run(pattern, "aaaa"));
  EXPECT_TRUE(Run(pattern, "aaaaa"));
}

TEST_P(RegexpTest, EmptyOrEmpty) {
  auto const status_or_pattern = Parse("|");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "b"));
}

TEST_P(RegexpTest, EmptyOrA) {
  auto const status_or_pattern = Parse("|a");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
}

TEST_P(RegexpTest, AOrEmpty) {
  auto const status_or_pattern = Parse("a|");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
}

TEST_P(RegexpTest, AOrB) {
  auto const status_or_pattern = Parse("a|b");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "aaa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "a|b"));
  EXPECT_FALSE(Run(pattern, "ba"));
  EXPECT_FALSE(Run(pattern, "aba"));
  EXPECT_FALSE(Run(pattern, "bab"));
}

TEST_P(RegexpTest, LoremOrIpsum) {
  auto const status_or_pattern = Parse("lorem|ipsum");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "l"));
  EXPECT_TRUE(Run(pattern, "lorem"));
  EXPECT_FALSE(Run(pattern, "i"));
  EXPECT_TRUE(Run(pattern, "ipsum"));
  EXPECT_FALSE(Run(pattern, "loremipsum"));
  EXPECT_FALSE(Run(pattern, "lorem|ipsum"));
  EXPECT_FALSE(Run(pattern, "ipsumlorem"));
  EXPECT_FALSE(Run(pattern, "ipsum|lorem"));
}

TEST_P(RegexpTest, EmptyBrackets) {
  auto const status_or_pattern = Parse("()");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
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
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "anchor"));
  EXPECT_FALSE(Run(pattern, "banana"));
}

TEST_P(RegexpTest, IpsumInBrackets) {
  auto const status_or_pattern = Parse("lorem(ipsum)dolor");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "lorem"));
  EXPECT_FALSE(Run(pattern, "ipsum"));
  EXPECT_FALSE(Run(pattern, "dolor"));
  EXPECT_FALSE(Run(pattern, "loremdolor"));
  EXPECT_FALSE(Run(pattern, "loremidolor"));
  EXPECT_TRUE(Run(pattern, "loremipsumdolor"));
}

TEST_P(RegexpTest, EpsilonLoop) {
  auto const status_or_pattern = Parse("(|a)+");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "aaa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "bb"));
  EXPECT_FALSE(Run(pattern, "ab"));
  EXPECT_FALSE(Run(pattern, "ba"));
}

TEST_P(RegexpTest, ChainLoops) {
  auto const status_or_pattern = Parse("a*b*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "aaa"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_TRUE(Run(pattern, "bb"));
  EXPECT_TRUE(Run(pattern, "bbb"));
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

TEST_P(RegexpTest, ChainStarAndPlus) {
  auto const status_or_pattern = Parse("a*b+");
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

TEST_P(RegexpTest, ChainStarAndMaybe) {
  auto const status_or_pattern = Parse("a*b?");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "aa"));
  EXPECT_TRUE(Run(pattern, "aaa"));
  EXPECT_TRUE(Run(pattern, "b"));
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

TEST_P(RegexpTest, ChainStarAndQuantifier) {
  auto const status_or_pattern = Parse("a*b{2}");
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

TEST_P(RegexpTest, ChainPlusAndStar) {
  auto const status_or_pattern = Parse("a+b*");
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
