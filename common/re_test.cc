#include "common/re.h"

#include <utility>

#include "absl/status/status.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;
using ::tsdb2::common::RE;

// TODO: test `Match` methods.

TEST(RegexpTest, StaticTest) {
  EXPECT_TRUE(RE::Test("lore", "lo+rem?"));
  EXPECT_TRUE(RE::Test("looorem", "lo+rem?"));
  EXPECT_FALSE(RE::Test("lrem", "lo+rem?"));
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

TEST(RegexpTest, Test) {
  auto const status_or_re = RE::Create("lo+rem?");
  ASSERT_OK(status_or_re);
  auto const& re = status_or_re.value();
  EXPECT_TRUE(re.Test("lore"));
  EXPECT_TRUE(re.Test("looorem"));
  EXPECT_FALSE(re.Test("lrem"));
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

// TODO: test `Match` functions when they're available.

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

// TODO: this doesn't work at the moment.
//
// TEST(RegexpTest, LongestPrefix) {
//   std::string_view input = "loremipsum";
//   EXPECT_THAT(RE::ConsumePrefix(&input, "lorem.*"), IsOkAndHolds(IsEmpty()));
//   EXPECT_EQ(input, "");
// }

TEST(RegexpTest, DeadPrefixBranch) {
  std::string_view input = "loremips";
  EXPECT_THAT(RE::ConsumePrefix(&input, "lorem(ipsum)?"), IsOkAndHolds(ElementsAre("")));
  EXPECT_EQ(input, "ips");
}

TEST(RegexpTest, PrefixPatternWithCapture) {
  std::string_view input = "lorem ipsum dolor";
  EXPECT_THAT(RE::ConsumePrefix(&input, "lorem (.*) "), IsOkAndHolds(ElementsAre("ipsum")));
  EXPECT_EQ(input, "dolor");
}

}  // namespace
