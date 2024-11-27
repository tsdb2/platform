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
  RE const re2{re1};
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
  re1 = std::move(re2);
  EXPECT_FALSE(re1.Test("lore"));
  EXPECT_FALSE(re1.Test("lorem"));
  EXPECT_FALSE(re1.Test("looorem"));
  EXPECT_TRUE(re1.Test("ipsm"));
  EXPECT_TRUE(re1.Test("ipsum"));
  EXPECT_TRUE(re1.Test("ipsuuum"));
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
  std::string_view sv1, sv2;
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
  std::string_view sv1, sv2;
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

TEST(RegexpTest, CreateOrDie) {
  auto const re = RE::CreateOrDie("lo+rem?");
  EXPECT_TRUE(re.Test("lore"));
  EXPECT_TRUE(re.Test("looorem"));
  EXPECT_FALSE(re.Test("lrem"));
}

TEST(RegexpTest, CreateOrDieButInvalidPattern) { EXPECT_DEATH(RE::CreateOrDie("?invalid"), _); }

}  // namespace
