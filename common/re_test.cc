#include "common/re.h"

#include <optional>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::tsdb2::common::RE;

TEST(RegexpTest, StaticTest) {
  EXPECT_TRUE(RE::Test("lore", "lo+rem?"));
  EXPECT_TRUE(RE::Test("looorem", "lo+rem?"));
  EXPECT_FALSE(RE::Test("lrem", "lo+rem?"));
}

TEST(RegexpTest, StaticPartialTest) {
  EXPECT_TRUE(RE::Contains("ipsum lore amet", "lo+rem?"));
  EXPECT_TRUE(RE::Contains("ipsum looorem amet", "lo+rem?"));
  EXPECT_FALSE(RE::Contains("ipsum lrem amet", "lo+rem?"));
}

TEST(RegexpTest, InvalidPattern) {
  EXPECT_THAT(RE::Create("?invalid"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(RegexpTest, IsDeterministic) {
  auto const status_or_re = RE::Create("lorem");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_TRUE(re.IsDeterministic());
}

TEST(RegexpTest, IsNotDeterministic) {
  auto const status_or_re = RE::Create("lorem(ipsum|dolor)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_FALSE(re.IsDeterministic());
}

TEST(RegexpTest, GetSize) {
  auto const status_or_re = RE::Create("(lorem)*");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.GetSize(), Pair(7, 7));
}

TEST(RegexpTest, GetNumCaptureGroups) {
  auto const status_or_re = RE::Create("lorem(ip(s)um)do(l)or");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_EQ(re.GetNumCaptureGroups(), 3);
}

TEST(RegexpTest, Test) {
  auto const status_or_re = RE::Create("lo+rem?");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_TRUE(re.Test("lore"));
  EXPECT_TRUE(re.Test("looorem"));
  EXPECT_FALSE(re.Test("lrem"));
}

TEST(RegexpTest, PartialTest) {
  auto const status_or_re = RE::Create("lo+rem?");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_TRUE(re.ContainedIn("ipsum lore amet"));
  EXPECT_TRUE(re.ContainedIn("ipsum looorem amet"));
  EXPECT_FALSE(re.ContainedIn("ipsum lrem amet"));
}

TEST(RegexpTest, CopyConstruct) {
  auto const status_or_re = RE::Create("lo+rem?");
  ASSERT_OK(status_or_re);
  auto const& re1 = status_or_re.value();
  RE const re2{re1};  // NOLINT(performance-unnecessary-copy-initialization)
  EXPECT_TRUE(re1.Test("lore"));
  EXPECT_TRUE(re1.Test("looorem"));
  EXPECT_FALSE(re1.Test("lrem"));
  EXPECT_TRUE(re2.Test("lore"));
  EXPECT_TRUE(re2.Test("looorem"));
  EXPECT_FALSE(re2.Test("lrem"));
}

TEST(RegexpTest, Copy) {
  auto status_or_re1 = RE::Create("lo+rem?");
  ASSERT_OK(status_or_re1);
  auto& re1 = status_or_re1.value();
  auto const status_or_re2 = RE::Create("ipsu*m");
  ASSERT_OK(status_or_re2);
  auto const& re2 = status_or_re2.value();
  re1 = re2;
  EXPECT_FALSE(re1.Test("lore"));
  EXPECT_FALSE(re1.Test("lorem"));
  EXPECT_FALSE(re1.Test("looorem"));
  EXPECT_TRUE(re1.Test("ipsm"));
  EXPECT_TRUE(re1.Test("ipsum"));
  EXPECT_TRUE(re1.Test("ipsuuum"));
  EXPECT_FALSE(re2.Test("lore"));
  EXPECT_FALSE(re2.Test("lorem"));
  EXPECT_FALSE(re2.Test("looorem"));
  EXPECT_TRUE(re2.Test("ipsm"));
  EXPECT_TRUE(re2.Test("ipsum"));
  EXPECT_TRUE(re2.Test("ipsuuum"));
}

TEST(RegexpTest, SelfCopy) {
  auto status_or_re = RE::Create("lo+rem?");
  ASSERT_OK(status_or_re);
  auto& re = status_or_re.value();
  re = re;  // NOLINT
  EXPECT_TRUE(re.Test("lore"));
  EXPECT_TRUE(re.Test("lorem"));
  EXPECT_TRUE(re.Test("looorem"));
}

TEST(RegexpTest, MoveConstruct) {
  auto status_or_re = RE::Create("lo+rem?");
  ASSERT_OK(status_or_re);
  auto& re1 = status_or_re.value();
  RE const re2{std::move(re1)};
  EXPECT_TRUE(re2.Test("lore"));
  EXPECT_TRUE(re2.Test("looorem"));
  EXPECT_FALSE(re2.Test("lrem"));
}

TEST(RegexpTest, Move) {
  auto status_or_re1 = RE::Create("lo+rem?");
  ASSERT_OK(status_or_re1);
  auto& re1 = status_or_re1.value();
  auto const status_or_re2 = RE::Create("ipsu*m");
  ASSERT_OK(status_or_re2);
  auto const& re2 = status_or_re2.value();
  re1 = std::move(re2);  // NOLINT(performance-move-const-arg)
  EXPECT_FALSE(re1.Test("lore"));
  EXPECT_FALSE(re1.Test("lorem"));
  EXPECT_FALSE(re1.Test("looorem"));
  EXPECT_TRUE(re1.Test("ipsm"));
  EXPECT_TRUE(re1.Test("ipsum"));
  EXPECT_TRUE(re1.Test("ipsuuum"));
}

TEST(RegexpTest, SelfMove) {
  auto status_or_re = RE::Create("ipsu*m");
  ASSERT_OK(status_or_re);
  auto& re = status_or_re.value();
  re = std::move(re);  // NOLINT
  EXPECT_TRUE(re.Test("ipsm"));
  EXPECT_TRUE(re.Test("ipsum"));
  EXPECT_TRUE(re.Test("ipsuuum"));
}

TEST(RegexpTest, Swap) {
  auto status_or_re1 = RE::Create("lo+rem?");
  ASSERT_OK(status_or_re1);
  auto& re1 = status_or_re1.value();
  auto status_or_re2 = RE::Create("ipsu*m");
  ASSERT_OK(status_or_re2);
  auto& re2 = status_or_re2.value();
  re1.swap(re2);
  EXPECT_FALSE(re1.Test("lore"));
  EXPECT_FALSE(re1.Test("lorem"));
  EXPECT_FALSE(re1.Test("looorem"));
  EXPECT_TRUE(re1.Test("ipsm"));
  EXPECT_TRUE(re1.Test("ipsum"));
  EXPECT_TRUE(re1.Test("ipsuuum"));
  EXPECT_TRUE(re2.Test("lore"));
  EXPECT_TRUE(re2.Test("lorem"));
  EXPECT_TRUE(re2.Test("looorem"));
  EXPECT_FALSE(re2.Test("ipsm"));
  EXPECT_FALSE(re2.Test("ipsum"));
  EXPECT_FALSE(re2.Test("ipsuuum"));
}

TEST(RegexpTest, SelfSwap) {
  auto status_or_re = RE::Create("lo+rem?");
  ASSERT_OK(status_or_re);
  auto& re = status_or_re.value();
  re.swap(re);
  EXPECT_TRUE(re.Test("lore"));
  EXPECT_TRUE(re.Test("lorem"));
  EXPECT_TRUE(re.Test("looorem"));
}

TEST(RegexpTest, AdlSwap) {
  auto status_or_re1 = RE::Create("lo+rem?");
  ASSERT_OK(status_or_re1);
  auto& re1 = status_or_re1.value();
  auto status_or_re2 = RE::Create("ipsu*m");
  ASSERT_OK(status_or_re2);
  auto& re2 = status_or_re2.value();
  swap(re1, re2);
  EXPECT_FALSE(re1.Test("lore"));
  EXPECT_FALSE(re1.Test("lorem"));
  EXPECT_FALSE(re1.Test("looorem"));
  EXPECT_TRUE(re1.Test("ipsm"));
  EXPECT_TRUE(re1.Test("ipsum"));
  EXPECT_TRUE(re1.Test("ipsuuum"));
  EXPECT_TRUE(re2.Test("lore"));
  EXPECT_TRUE(re2.Test("lorem"));
  EXPECT_TRUE(re2.Test("looorem"));
  EXPECT_FALSE(re2.Test("ipsm"));
  EXPECT_FALSE(re2.Test("ipsum"));
  EXPECT_FALSE(re2.Test("ipsuuum"));
}

TEST(RegexpTest, StaticMatch) {
  EXPECT_THAT(RE::Match("lore", "l(o+r)em?"), IsOkAndHolds(ElementsAre(ElementsAre("or"))));
  EXPECT_THAT(RE::Match("looorem", "l((o+r)em?)"),
              IsOkAndHolds(ElementsAre(ElementsAre("ooorem"), ElementsAre("ooor"))));
  EXPECT_THAT(RE::Match("lrem", "lo+rem?"), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(RE::Match("lore", "l(o+rem?"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(RegexpTest, Match) {
  auto const status_or_re = RE::Create("l((o+r)em?)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.Match("lore"), Optional(ElementsAre(ElementsAre("ore"), ElementsAre("or"))));
  EXPECT_THAT(re.Match("looorem"),
              Optional(ElementsAre(ElementsAre("ooorem"), ElementsAre("ooor"))));
  EXPECT_EQ(re.Match("lrem"), std::nullopt);
}

TEST(RegexpTest, StaticMatchArgs) {
  std::string_view sv1;
  std::string_view sv2;
  EXPECT_OK(RE::MatchArgs("lore", "l(o+r)em?", &sv1));
  EXPECT_EQ(sv1, "or");
  EXPECT_OK(RE::MatchArgs("looorem", "l((o+r)em?)", &sv1, &sv2));
  EXPECT_EQ(sv1, "ooorem");
  EXPECT_EQ(sv2, "ooor");
  EXPECT_THAT(RE::MatchArgs("lrem", "lo+rem?"), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(RE::MatchArgs("lore", "l(o+rem?"), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(RegexpTest, MatchArgs) {
  auto const status_or_re = RE::Create("l((o+r)em?)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  std::string_view sv1;
  std::string_view sv2;
  EXPECT_TRUE(re.MatchArgs("lore", &sv1, &sv2));
  EXPECT_EQ(sv1, "ore");
  EXPECT_EQ(sv2, "or");
  EXPECT_TRUE(re.MatchArgs("looorem", &sv1, &sv2));
  EXPECT_EQ(sv1, "ooorem");
  EXPECT_EQ(sv2, "ooor");
  EXPECT_FALSE(re.MatchArgs("lrem", &sv1, &sv2));
}

TEST(RegexpTest, InvalidPrefixPattern) {
  std::string_view input = "";
  EXPECT_THAT(RE::ConsumePrefix(&input, "foo("), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(RegexpTest, EmptyPrefixOfEmptyString) {
  std::string_view input = "";
  EXPECT_THAT(RE::ConsumePrefix(&input, ""), IsOkAndHolds(IsEmpty()));
  EXPECT_EQ(input, "");
}

TEST(RegexpTest, NonEmptyPrefixOfEmptyString) {
  std::string_view input = "";
  EXPECT_THAT(RE::ConsumePrefix(&input, "lorem"), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_EQ(input, "");
}

TEST(RegexpTest, ProperPrefix) {
  std::string_view input = "loremipsum";
  EXPECT_THAT(RE::ConsumePrefix(&input, "lorem"), IsOkAndHolds(IsEmpty()));
  EXPECT_EQ(input, "ipsum");
}

TEST(RegexpTest, ImproperPrefix) {
  std::string_view input = "lorem";
  EXPECT_THAT(RE::ConsumePrefix(&input, "lorem"), IsOkAndHolds(IsEmpty()));
  EXPECT_EQ(input, "");
}

TEST(RegexpTest, LongestPrefix) {
  std::string_view input = "loremipsum";
  EXPECT_THAT(RE::ConsumePrefix(&input, "lorem.*"), IsOkAndHolds(IsEmpty()));
  EXPECT_EQ(input, "");
}

TEST(RegexpTest, DeadPrefixBranch) {
  std::string_view input = "loremips";
  EXPECT_THAT(RE::ConsumePrefix(&input, "lorem(ipsum)?"), IsOkAndHolds(ElementsAre(IsEmpty())));
  EXPECT_EQ(input, "ips");
}

TEST(RegexpTest, PrefixPatternWithCapture) {
  std::string_view input = "lorem ipsum dolor";
  EXPECT_THAT(RE::ConsumePrefix(&input, "lorem (.*) "),
              IsOkAndHolds(ElementsAre(ElementsAre("ipsum"))));
  EXPECT_EQ(input, "dolor");
}

TEST(RegexpTest, PrefixArgs) {
  std::string_view input = "lorem ipsum dolor";
  std::string_view ipsum;
  EXPECT_OK(RE::ConsumePrefixArgs(&input, "lorem (.*) ", &ipsum));
  EXPECT_EQ(input, "dolor");
  EXPECT_EQ(ipsum, "ipsum");
}

TEST(RegexpTest, NoPrefixArgs) {
  std::string_view input = "lorem ipsum dolor";
  EXPECT_OK(RE::ConsumePrefixArgs(&input, "lorem (.*) "));
  EXPECT_EQ(input, "dolor");
}

TEST(RegexpTest, ExtraPrefixArgs) {
  std::string_view input = "lorem ipsum dolor";
  std::string_view ipsum;
  std::string_view foo;
  EXPECT_OK(RE::ConsumePrefixArgs(&input, "lorem (.*) ", &ipsum, &foo));
  EXPECT_EQ(input, "dolor");
  EXPECT_EQ(ipsum, "ipsum");
}

TEST(RegexpTest, InvalidPrefixPatternWithArgs) {
  std::string_view input = "";
  EXPECT_THAT(RE::ConsumePrefixArgs(&input, "foo("), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(RegexpTest, PrefixArgsNotFound) {
  std::string_view input = "dolor lorem ipsum";
  std::string_view ipsum;
  EXPECT_THAT(RE::ConsumePrefixArgs(&input, "lorem (.*) ", &ipsum),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST(RegexpTest, PartialMatch) {
  auto const status_or_re = RE::Create("(do+lor)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.PartialMatch("lorem ipsum dolor sic amat"),
              Optional(ElementsAre(ElementsAre("dolor"))));
  EXPECT_THAT(re.PartialMatch("lorem ipsum dooolor sic amat"),
              Optional(ElementsAre(ElementsAre("dooolor"))));
  EXPECT_EQ(re.PartialMatch("lorem ipsum color sic amat"), std::nullopt);
  EXPECT_EQ(re.PartialMatch("lorem ipsum dolet et amat"), std::nullopt);
}

TEST(RegexpTest, StaticPartialMatch) {
  EXPECT_THAT(RE::PartialMatch("lorem ipsum dooolor sic amat", "(do+lor)"),
              IsOkAndHolds(ElementsAre(ElementsAre("dooolor"))));
  EXPECT_THAT(RE::PartialMatch("lorem ipsum color sic amat", "(do+lor)"),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(RE::PartialMatch("lorem ipsum dolor sic amat", "(do+lor"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(RegexpTest, PartialMatchArgs) {
  auto const status_or_re = RE::Create("(do+lor)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  std::string_view sv;
  EXPECT_TRUE(re.PartialMatchArgs("lorem ipsum dolor sic amat", &sv));
  EXPECT_EQ(sv, "dolor");
  EXPECT_TRUE(re.PartialMatchArgs("lorem ipsum dooolor sic amat", &sv));
  EXPECT_EQ(sv, "dooolor");
  EXPECT_FALSE(re.PartialMatchArgs("lorem ipsum color sic amat"));
  EXPECT_FALSE(re.PartialMatchArgs("lorem ipsum dolet et amat"));
}

TEST(RegexpTest, StaticPartialMatchArgs) {
  std::string_view sv;
  EXPECT_OK(RE::PartialMatchArgs("lorem ipsum dooolor sic amat", "(do+lor)", &sv));
  EXPECT_EQ(sv, "dooolor");
  EXPECT_THAT(RE::PartialMatchArgs("lorem ipsum color sic amat", "(do+lor)", &sv),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(RE::PartialMatchArgs("lorem ipsum dolor sic amat", "(do+lor"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(RegexpTest, ReplaceFirstFullMatch) {
  auto const status_or_re = RE::Create("foo (bar) baz");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo bar baz", 0, "qux"), IsOkAndHolds("foo qux baz"));
}

TEST(RegexpTest, ReplaceFirstFullMatchCaptured) {
  auto const status_or_re = RE::Create("(foo bar baz)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo bar baz", 0, "qux"), IsOkAndHolds("qux"));
}

TEST(RegexpTest, ReplaceFirstPartialMatch) {
  auto const status_or_re = RE::Create("o (bar) b");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo bar baz", 0, "qux"), IsOkAndHolds("foo qux baz"));
}

TEST(RegexpTest, ReplaceFirstWrapped) {
  auto const status_or_re = RE::Create("(bar)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo bar baz", 0, "qux"), IsOkAndHolds("foo qux baz"));
}

TEST(RegexpTest, ReplaceFirstInvalidCaptureIndex) {
  auto const status_or_re = RE::Create("foo (bar) baz");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo bar baz", 1, "qux"), StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(RegexpTest, ReplaceFirstCaptureNotTriggered) {
  auto const status_or_re = RE::Create("foo (?:bar|(baz)) baz");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo bar baz", 0, "qux"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(RegexpTest, ReplaceFirstFirstGroup) {
  auto const status_or_re = RE::Create("(bar) (baz)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo bar baz qux", 0, "lorem"), IsOkAndHolds("foo lorem baz qux"));
}

TEST(RegexpTest, ReplaceFirstSecondGroup) {
  auto const status_or_re = RE::Create("(bar) (baz)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo bar baz qux", 1, "lorem"), IsOkAndHolds("foo bar lorem qux"));
}

TEST(RegexpTest, ReplaceFirstOuterGroup) {
  auto const status_or_re = RE::Create("o (b(a)r) b");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo bar baz qux", 0, "bear"), IsOkAndHolds("foo bear baz qux"));
}

TEST(RegexpTest, ReplaceFirstInnerGroup) {
  auto const status_or_re = RE::Create("o (b(a)r) b");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo bar baz qux", 1, "ea"), IsOkAndHolds("foo bear baz qux"));
}

TEST(RegexpTest, ReplaceFirstNotSecond) {
  auto const status_or_re = RE::Create("(bar)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo bar baz bar qux", 0, "lorem"),
              IsOkAndHolds("foo lorem baz bar qux"));
}

TEST(RegexpTest, ReplaceFirstWithOverlaps) {
  auto const status_or_re = RE::Create("(abab)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo ababababab baz", 0, "bar"),
              IsOkAndHolds("foo barababab baz"));
}

TEST(RegexpTest, ReplaceFirstWithRefs1) {
  auto const status_or_re = RE::Create("(lo(r)em ipsum (a)met)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo lorem ipsum amet baz", 0, "b\\2\\1"),
              IsOkAndHolds("foo bar baz"));
}

TEST(RegexpTest, ReplaceFirstWithRefs2) {
  auto const status_or_re = RE::Create("(the quick brown ([a-z]+) jumped over the lazy ([a-z]+))");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("the quick brown fox jumped over the lazy dog", 0,
                                 "jumper: \\1, jumpee: \\2"),
              IsOkAndHolds("jumper: fox, jumpee: dog"));
}

TEST(RegexpTest, ReplaceFirstWithInvalidRef) {
  auto const status_or_re = RE::Create("(lo(r)em ipsum (a)met)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo lorem ipsum amet baz", 0, "b\\a\\1"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(RegexpTest, ReplaceFirstWithOutOfRangeRef) {
  auto const status_or_re = RE::Create("(lo(r)em ipsum (a)met)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo lorem ipsum amet baz", 0, "b\\3\\1"),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(RegexpTest, ReplaceFirstWithRefNotTriggered) {
  auto const status_or_re = RE::Create("(lo(r)em ipsum (?:a|(b))met)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo lorem ipsum amet baz", 0, "b\\2\\1"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(RegexpTest, ReplaceFirstWithDefaultCaptureIndex) {
  auto const status_or_re = RE::Create("(bar) (baz)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceFirst("foo bar baz qux", "lorem"), IsOkAndHolds("foo lorem baz qux"));
}

TEST(RegexpTest, StaticReplaceFirst) {
  EXPECT_THAT(RE::StrReplaceFirst("foo bar baz bar qux", "bar", "lorem"),
              IsOkAndHolds("foo lorem baz bar qux"));
}

TEST(RegexpTest, StaticReplaceFirstWithRef) {
  EXPECT_THAT(RE::StrReplaceFirst("foo bar baz bar qux", "b(a)r", "\\1met"),
              IsOkAndHolds("foo amet baz bar qux"));
}

TEST(RegexpTest, ReplaceAllFullMatch) {
  auto const status_or_re = RE::Create("foo (bar) baz");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo bar baz", 0, "qux"), IsOkAndHolds("foo qux baz"));
}

TEST(RegexpTest, ReplaceAllFullMatchCaptured) {
  auto const status_or_re = RE::Create("(foo bar baz)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo bar baz", 0, "qux"), IsOkAndHolds("qux"));
}

TEST(RegexpTest, ReplaceAllPartialMatch) {
  auto const status_or_re = RE::Create("o (bar) b");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo bar baz", 0, "qux"), IsOkAndHolds("foo qux baz"));
}

TEST(RegexpTest, ReplaceAllWrapped) {
  auto const status_or_re = RE::Create("(bar)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo bar baz", 0, "qux"), IsOkAndHolds("foo qux baz"));
}

TEST(RegexpTest, ReplaceAllInvalidCaptureIndex) {
  auto const status_or_re = RE::Create("foo (bar) baz");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo bar baz", 1, "qux"), StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(RegexpTest, ReplaceAllCaptureNotTriggered) {
  auto const status_or_re = RE::Create("foo (?:bar|(baz)) baz");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo bar baz", 0, "qux"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(RegexpTest, ReplaceAllFirstGroup) {
  auto const status_or_re = RE::Create("(bar) (baz)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo bar baz qux", 0, "lorem"), IsOkAndHolds("foo lorem baz qux"));
}

TEST(RegexpTest, ReplaceAllSecondGroup) {
  auto const status_or_re = RE::Create("(bar) (baz)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo bar baz qux", 1, "lorem"), IsOkAndHolds("foo bar lorem qux"));
}

TEST(RegexpTest, ReplaceAllOuterGroup) {
  auto const status_or_re = RE::Create("o (b(a)r) b");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo bar baz qux", 0, "bear"), IsOkAndHolds("foo bear baz qux"));
}

TEST(RegexpTest, ReplaceAllInnerGroup) {
  auto const status_or_re = RE::Create("o (b(a)r) b");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo bar baz qux", 1, "ea"), IsOkAndHolds("foo bear baz qux"));
}

TEST(RegexpTest, ReplaceAllTwoMatches) {
  auto const status_or_re = RE::Create("(bar)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo bar baz bar qux", 0, "lorem"),
              IsOkAndHolds("foo lorem baz lorem qux"));
}

TEST(RegexpTest, ReplaceAllWithOverlaps) {
  auto const status_or_re = RE::Create("(abab)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo ababababab baz", 0, "bar"), IsOkAndHolds("foo barbarab baz"));
}

TEST(RegexpTest, ReplaceAllWithRefs1) {
  auto const status_or_re = RE::Create("(lo(r)em ipsum (a)met)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo lorem ipsum amet baz", 0, "b\\2\\1"),
              IsOkAndHolds("foo bar baz"));
}

TEST(RegexpTest, ReplaceAllWithRefs2) {
  auto const status_or_re = RE::Create("(the quick brown ([a-z]+) jumped over the lazy ([a-z]+))");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("the quick brown fox jumped over the lazy dog", 0,
                               "jumper: \\1, jumpee: \\2"),
              IsOkAndHolds("jumper: fox, jumpee: dog"));
}

TEST(RegexpTest, ReplaceAllWithInvalidRef) {
  auto const status_or_re = RE::Create("(lo(r)em ipsum (a)met)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo lorem ipsum amet baz", 0, "b\\a\\1"),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(RegexpTest, ReplaceAllWithOutOfRangeRef) {
  auto const status_or_re = RE::Create("(lo(r)em ipsum (a)met)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo lorem ipsum amet baz", 0, "b\\3\\1"),
              StatusIs(absl::StatusCode::kOutOfRange));
}

TEST(RegexpTest, ReplaceAllWithRefNotTriggered) {
  auto const status_or_re = RE::Create("(lo(r)em ipsum (?:a|(b))met)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo lorem ipsum amet baz", 0, "b\\2\\1"),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(RegexpTest, ReplaceAllWithDefaultCaptureIndex) {
  auto const status_or_re = RE::Create("(bar) (baz)");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_THAT(re.StrReplaceAll("foo bar baz qux", "lorem"), IsOkAndHolds("foo lorem baz qux"));
}

TEST(RegexpTest, StaticReplaceAll) {
  EXPECT_THAT(RE::StrReplaceAll("foo bar baz bar qux", "bar", "lorem"),
              IsOkAndHolds("foo lorem baz lorem qux"));
}

TEST(RegexpTest, StaticReplaceAllWithRef) {
  EXPECT_THAT(RE::StrReplaceAll("foo bar baz bar qux", "b(a)r", "\\1met"),
              IsOkAndHolds("foo amet baz amet qux"));
}

TEST(RegexpTest, CreateOrDie) {
  auto const re = RE::CreateOrDie("lo+rem?");
  EXPECT_TRUE(re.Test("lore"));
  EXPECT_TRUE(re.Test("looorem"));
  EXPECT_FALSE(re.Test("lrem"));
}

TEST(RegexpTest, CreateOrDieButInvalidPattern) { EXPECT_DEATH(RE::CreateOrDie("?invalid"), _); }

}  // namespace
