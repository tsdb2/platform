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

struct ParserTestParams {
  bool force_nfa;
  bool use_runner;
};

class ParserTest : public ::testing::TestWithParam<ParserTestParams> {
 protected:
  explicit ParserTest() { TempNFA::force_nfa_for_testing = GetParam().force_nfa; }
  ~ParserTest() { TempNFA::force_nfa_for_testing = false; }
  bool CheckDeterministic(reffed_ptr<AutomatonInterface> const& automaton);
  bool CheckNotDeterministic(reffed_ptr<AutomatonInterface> const& automaton);
  bool Run(reffed_ptr<AutomatonInterface> const& automaton, std::string_view input);
};

bool ParserTest::CheckDeterministic(reffed_ptr<AutomatonInterface> const& automaton) {
  return GetParam().force_nfa || automaton->IsDeterministic();
}

bool ParserTest::CheckNotDeterministic(reffed_ptr<AutomatonInterface> const& automaton) {
  return GetParam().force_nfa || !automaton->IsDeterministic();
}

bool ParserTest::Run(reffed_ptr<AutomatonInterface> const& automaton,
                     std::string_view const input) {
  if (GetParam().use_runner) {
    auto const runner = automaton->CreateRunner();
    return runner->Step(input) && runner->Finish();
  } else {
    return automaton->Run(input);
  }
}

TEST_P(ParserTest, EmptyIsDeterministic) {
  auto const status_or_pattern = Parse("");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(CheckDeterministic(pattern));
}

TEST_P(ParserTest, SimpleStringIsDeterministic) {
  auto const status_or_pattern = Parse("lorem");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(CheckDeterministic(pattern));
}

TEST_P(ParserTest, PipeIsNotDeterministic) {
  auto const status_or_pattern = Parse("lorem(ipsum|dolor)");
  ASSERT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(CheckNotDeterministic(pattern));
}

TEST_P(ParserTest, Empty) {
  auto const status_or_pattern = Parse("");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "hello"));
  EXPECT_TRUE(Run(pattern, ""));
}

TEST_P(ParserTest, SimpleCharacter) {
  auto const status_or_pattern = Parse("a");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "anchor"));
  EXPECT_FALSE(Run(pattern, "banana"));
  EXPECT_FALSE(Run(pattern, ""));
}

TEST_P(ParserTest, AnotherSimpleCharacter) {
  auto const status_or_pattern = Parse("b");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "anchor"));
  EXPECT_FALSE(Run(pattern, "banana"));
  EXPECT_FALSE(Run(pattern, ""));
}

TEST_P(ParserTest, InvalidEscapeCode) {
  EXPECT_THAT(Parse("\\a"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\T"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\R"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\N"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\V"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\F"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\X"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ParserTest, BlockBackrefs) {
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

TEST_P(ParserTest, Digit) {
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

TEST_P(ParserTest, NotDigit) {
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

TEST_P(ParserTest, WordCharacter) {
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

TEST_P(ParserTest, NotWordCharacter) {
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

TEST_P(ParserTest, Spacing) {
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

TEST_P(ParserTest, NotSpacing) {
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

TEST_P(ParserTest, HorizontalTab) {
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

TEST_P(ParserTest, CarriageReturn) {
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

TEST_P(ParserTest, LineFeed) {
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

TEST_P(ParserTest, VerticalTab) {
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

TEST_P(ParserTest, FormFeed) {
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

TEST_P(ParserTest, InvalidHexCode) {
  EXPECT_THAT(Parse("\\xZ0"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("\\x0Z"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ParserTest, HexCode1) {
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

TEST_P(ParserTest, HexCode2) {
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

TEST_P(ParserTest, HexCode3) {
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

TEST_P(ParserTest, AnyCharacter) {
  auto const status_or_pattern = Parse(".");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_TRUE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "anchor"));
  EXPECT_FALSE(Run(pattern, "banana"));
  EXPECT_FALSE(Run(pattern, ""));
}

TEST_P(ParserTest, EmptyCharacterClass) {
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

TEST_P(ParserTest, NegatedEmptyCharacterClass) {
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

TEST_P(ParserTest, CharacterClass) {
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

TEST_P(ParserTest, NegatedCharacterClass) {
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

TEST_P(ParserTest, CharacterClassWithCircumflex) {
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

TEST_P(ParserTest, NegatedCharacterClassWithCircumflex) {
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

TEST_P(ParserTest, CharacterClassWithEscapes) {
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

TEST_P(ParserTest, NegatedCharacterClassWithEscapes) {
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

TEST_P(ParserTest, CharacterClassWithMoreEscapes) {
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

TEST_P(ParserTest, NegatedCharacterClassWithMoreEscapes) {
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

TEST_P(ParserTest, InvalidEscapeCodesInCharacterClass) {
  EXPECT_THAT(Parse("[\\"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\x"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\x]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\x0Z]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\xZ0]"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("[\\a]"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ParserTest, BlockBackrefsInCharacterClass) {
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

TEST_P(ParserTest, InvalidSpecialCharacter) {
  EXPECT_THAT(Parse("*"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("+"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("?"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse(")"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("]"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ParserTest, CharacterSequence) {
  auto const status_or_pattern = Parse("lorem");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "l"));
  EXPECT_TRUE(Run(pattern, "lorem"));
  EXPECT_FALSE(Run(pattern, "loremipsum"));
  EXPECT_FALSE(Run(pattern, "dolorloremipsum"));
}

TEST_P(ParserTest, CharacterSequenceWithDot) {
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

TEST_P(ParserTest, KleeneStar) {
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

TEST_P(ParserTest, CharacterSequenceWithStar) {
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

TEST_P(ParserTest, KleenePlus) {
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

TEST_P(ParserTest, CharacterSequenceWithPlus) {
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

TEST_P(ParserTest, Maybe) {
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

TEST_P(ParserTest, Many) {
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

TEST_P(ParserTest, ExactlyZero) {
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

TEST_P(ParserTest, ExactlyOne) {
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

TEST_P(ParserTest, ExactlyTwo) {
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

TEST_P(ParserTest, ExactlyFourtyTwo) {
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

TEST_P(ParserTest, AtLeastZero) {
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

TEST_P(ParserTest, AtLeastOne) {
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

TEST_P(ParserTest, AtLeastTwo) {
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

TEST_P(ParserTest, AtLeastFourtyTwo) {
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

TEST_P(ParserTest, BetweenZeroAndZero) {
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

TEST_P(ParserTest, BetweenZeroAndOne) {
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

TEST_P(ParserTest, BetweenZeroAndTwo) {
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

TEST_P(ParserTest, BetweenOneAndOne) {
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

TEST_P(ParserTest, BetweenOneAndTwo) {
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

TEST_P(ParserTest, BetweenTwoAndTwo) {
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

TEST_P(ParserTest, BetweenFourtyTwoAndFourtyFive) {
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

TEST_P(ParserTest, CharacterSequenceWithMaybe) {
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

TEST_P(ParserTest, InvalidQuantifiers) {
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

TEST_P(ParserTest, MultipleQuantifiersDisallowed) {
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

TEST_P(ParserTest, MultipleQuantifiersWithBrackets) {
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

TEST_P(ParserTest, EmptyOrEmpty) {
  auto const status_or_pattern = Parse("|");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "b"));
}

TEST_P(ParserTest, EmptyOrA) {
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

TEST_P(ParserTest, AOrEmpty) {
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

TEST_P(ParserTest, AOrB) {
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

TEST_P(ParserTest, LoremOrIpsum) {
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

TEST_P(ParserTest, EmptyBrackets) {
  auto const status_or_pattern = Parse("()");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, ""));
  EXPECT_FALSE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "aa"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "ab"));
}

TEST_P(ParserTest, UnmatchedBrackets) {
  EXPECT_THAT(Parse("("), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse(")"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse(")("), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("(()"), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(Parse("())"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ParserTest, Brackets) {
  auto const status_or_pattern = Parse("(a)");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_FALSE(Run(pattern, ""));
  EXPECT_TRUE(Run(pattern, "a"));
  EXPECT_FALSE(Run(pattern, "b"));
  EXPECT_FALSE(Run(pattern, "anchor"));
  EXPECT_FALSE(Run(pattern, "banana"));
}

TEST_P(ParserTest, IpsumInBrackets) {
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

TEST_P(ParserTest, EpsilonLoop) {
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

TEST_P(ParserTest, ChainLoops) {
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

TEST_P(ParserTest, ChainStarAndPlus) {
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

TEST_P(ParserTest, ChainStarAndMaybe) {
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

TEST_P(ParserTest, ChainStarAndQuantifier) {
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

TEST_P(ParserTest, ChainPlusAndStar) {
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

TEST_P(ParserTest, ChainPlusAndPlus) {
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

TEST_P(ParserTest, ChainPlusAndMaybe) {
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

TEST_P(ParserTest, ChainPlusAndQuantifier) {
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

TEST_P(ParserTest, ChainMaybeAndStar) {
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

TEST_P(ParserTest, ChainMaybeAndPlus) {
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

TEST_P(ParserTest, ChainMaybeAndMaybe) {
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

TEST_P(ParserTest, ChainMaybeAndQuantifier) {
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

TEST_P(ParserTest, ChainQuantifierAndStar) {
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

TEST_P(ParserTest, ChainQuantifierAndPlus) {
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

TEST_P(ParserTest, ChainQuantifierAndMaybe) {
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

TEST_P(ParserTest, ChainQuantifiers) {
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

TEST_P(ParserTest, PipeLoops) {
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

TEST_P(ParserTest, StarOrPlus) {
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

TEST_P(ParserTest, StarOrMaybe) {
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

TEST_P(ParserTest, StarOrQuantifier) {
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

TEST_P(ParserTest, PlusOrStar) {
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

TEST_P(ParserTest, PlusOrPlus) {
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

TEST_P(ParserTest, PlusOrMaybe) {
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

TEST_P(ParserTest, PlusOrQuantifier) {
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

TEST_P(ParserTest, MaybeOrStar) {
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

TEST_P(ParserTest, MaybeOrPlus) {
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

TEST_P(ParserTest, MaybeOrMaybe) {
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

TEST_P(ParserTest, MaybeOrQuantifier) {
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

TEST_P(ParserTest, QuantifierOrStar) {
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

TEST_P(ParserTest, QuantifierOrPlus) {
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

TEST_P(ParserTest, QuantifierOrMaybe) {
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

TEST_P(ParserTest, QuantifierOrQuantifier) {
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

TEST_P(ParserTest, Search) {
  auto const status_or_pattern = Parse(".*do+lor.*");
  EXPECT_OK(status_or_pattern);
  auto const& pattern = status_or_pattern.value();
  EXPECT_TRUE(Run(pattern, "lorem ipsum dolor sic amat"));
  EXPECT_FALSE(Run(pattern, "lorem ipsum color sic amat"));
  EXPECT_FALSE(Run(pattern, "lorem ipsum dolet et amat"));
}

TEST_P(ParserTest, HeavyBacktracker) {
  auto const status_or_pattern = Parse(
      "a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?a?aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  EXPECT_OK(status_or_pattern);
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

INSTANTIATE_TEST_SUITE_P(ParserTest, ParserTest,
                         Values(ParserTestParams{.force_nfa = false, .use_runner = false},
                                ParserTestParams{.force_nfa = false, .use_runner = true},
                                ParserTestParams{.force_nfa = true, .use_runner = false},
                                ParserTestParams{.force_nfa = true, .use_runner = true}));

}  // namespace
