#include "tsz/field_descriptor.h"

#include <cstdint>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::VariantWith;
using ::tsz::EntityLabels;
using ::tsz::Field;
using ::tsz::MetricFields;
using ::tsz::Name;

char constexpr kFooName[] = "foo";
char constexpr kBarName[] = "bar";

TEST(EmptyFieldDescriptorTest, Traits) {
  EXPECT_TRUE(EntityLabels<>::HasTypeNames::value);
  EXPECT_TRUE(EntityLabels<>::kHasTypeNames);
  EXPECT_TRUE(EntityLabels<>::HasParameterNames::value);
  EXPECT_TRUE(EntityLabels<>::kHasParameterNames);
  EXPECT_TRUE(MetricFields<>::HasTypeNames::value);
  EXPECT_TRUE(MetricFields<>::kHasTypeNames);
  EXPECT_TRUE(MetricFields<>::HasParameterNames::value);
  EXPECT_TRUE(MetricFields<>::kHasParameterNames);
}

TEST(EmptyFieldDescriptorTest, Names) {
  EntityLabels<> el;
  EXPECT_THAT(el.names(), ElementsAre());
  MetricFields<> mf;
  EXPECT_THAT(mf.names(), ElementsAre());
}

TEST(EmptyFieldDescriptorTest, FieldMap) {
  EntityLabels<> el;
  EXPECT_THAT(el.MakeFieldMap(), ElementsAre());
  MetricFields<> mf;
  EXPECT_THAT(mf.MakeFieldMap(), ElementsAre());
}

TEST(EmptyEntityLabelsTest, Copyable) {
  EntityLabels<> fd1;
  EntityLabels<> fd2{fd1};
  EntityLabels<> fd3;
  fd3 = fd1;
  EXPECT_THAT(fd2.names(), ElementsAre());
  EXPECT_THAT(fd3.names(), ElementsAre());
}

TEST(EmptyMetricFieldsTest, Copyable) {
  MetricFields<> fd1;
  MetricFields<> fd2{fd1};
  MetricFields<> fd3;
  fd3 = fd1;
  EXPECT_THAT(fd2.names(), ElementsAre());
  EXPECT_THAT(fd3.names(), ElementsAre());
}

TEST(EmptyEntityLabelsTest, Movable) {
  EntityLabels<> fd1;
  EntityLabels<> fd2{std::move(fd1)};
  EntityLabels<> fd3;
  EntityLabels<> fd4;
  fd4 = std::move(fd3);
  EXPECT_THAT(fd2.names(), ElementsAre());
  EXPECT_THAT(fd4.names(), ElementsAre());
}

TEST(EmptyMetricFieldsTest, Movable) {
  MetricFields<> fd1;
  MetricFields<> fd2{std::move(fd1)};
  MetricFields<> fd3;
  MetricFields<> fd4;
  fd4 = std::move(fd3);
  EXPECT_THAT(fd2.names(), ElementsAre());
  EXPECT_THAT(fd4.names(), ElementsAre());
}

TEST(SingleFieldDescriptorTest, Traits) {
  EXPECT_FALSE(EntityLabels<Field<int>>::HasTypeNames::value);
  EXPECT_FALSE(EntityLabels<Field<int>>::kHasTypeNames);
  EXPECT_TRUE((EntityLabels<Field<int, Name<kFooName>>>::HasTypeNames::value));
  EXPECT_TRUE((EntityLabels<Field<int, Name<kFooName>>>::kHasTypeNames));
  EXPECT_TRUE(EntityLabels<Field<int>>::HasParameterNames::value);
  EXPECT_TRUE(EntityLabels<Field<int>>::kHasParameterNames);
  EXPECT_FALSE((EntityLabels<Field<int, Name<kFooName>>>::HasParameterNames::value));
  EXPECT_FALSE((EntityLabels<Field<int, Name<kFooName>>>::kHasParameterNames));
  EXPECT_FALSE(MetricFields<Field<int>>::HasTypeNames::value);
  EXPECT_FALSE(MetricFields<Field<int>>::kHasTypeNames);
  EXPECT_TRUE((MetricFields<Field<int, Name<kFooName>>>::HasTypeNames::value));
  EXPECT_TRUE((MetricFields<Field<int, Name<kFooName>>>::kHasTypeNames));
  EXPECT_TRUE(MetricFields<Field<int>>::HasParameterNames::value);
  EXPECT_TRUE(MetricFields<Field<int>>::kHasParameterNames);
  EXPECT_FALSE((MetricFields<Field<int, Name<kFooName>>>::HasParameterNames::value));
  EXPECT_FALSE((MetricFields<Field<int, Name<kFooName>>>::kHasParameterNames));
}

TEST(SingleFieldDescriptorTest, TypeName) {
  EntityLabels<Field<int, Name<kFooName>>> el;
  EXPECT_THAT(el.names(), ElementsAre(kFooName));
  MetricFields<Field<int, Name<kBarName>>> mf;
  EXPECT_THAT(mf.names(), ElementsAre(kBarName));
}

TEST(SingleFieldDescriptorTest, ParameterName) {
  EntityLabels<Field<int>> el{kFooName};
  EXPECT_THAT(el.names(), ElementsAre(kFooName));
  MetricFields<Field<int>> mf{kBarName};
  EXPECT_THAT(mf.names(), ElementsAre(kBarName));
}

TEST(SingleFieldDescriptorTest, FieldMap) {
  EntityLabels<Field<int, Name<kFooName>>> el;
  EXPECT_THAT(el.MakeFieldMap(42), ElementsAre(Pair(kFooName, VariantWith<int64_t>(42))));
  MetricFields<Field<int>> mf{kBarName};
  EXPECT_THAT(mf.MakeFieldMap(43), ElementsAre(Pair(kBarName, VariantWith<int64_t>(43))));
}

TEST(SingleEntityLabelTest, Copyable) {
  EntityLabels<Field<int, Name<kFooName>>> fd1;
  EntityLabels<Field<int, Name<kFooName>>> fd2{fd1};
  EntityLabels<Field<int, Name<kFooName>>> fd3;
  fd3 = fd1;
  EXPECT_THAT(fd2.names(), ElementsAre(kFooName));
  EXPECT_THAT(fd3.names(), ElementsAre(kFooName));
  EntityLabels<Field<int>> fd4{kBarName};
  EntityLabels<Field<int>> fd5{fd4};
  EntityLabels<Field<int>> fd6{kFooName};
  fd6 = fd4;
  EXPECT_THAT(fd5.names(), ElementsAre(kBarName));
  EXPECT_THAT(fd6.names(), ElementsAre(kBarName));
}

TEST(SingleMetricFieldTest, Copyable) {
  MetricFields<Field<int, Name<kFooName>>> fd1;
  MetricFields<Field<int, Name<kFooName>>> fd2{fd1};
  MetricFields<Field<int, Name<kFooName>>> fd3;
  fd3 = fd1;
  EXPECT_THAT(fd2.names(), ElementsAre(kFooName));
  EXPECT_THAT(fd3.names(), ElementsAre(kFooName));
  MetricFields<Field<int>> fd4{kBarName};
  MetricFields<Field<int>> fd5{fd4};
  MetricFields<Field<int>> fd6{kFooName};
  fd6 = fd4;
  EXPECT_THAT(fd5.names(), ElementsAre(kBarName));
  EXPECT_THAT(fd6.names(), ElementsAre(kBarName));
}

TEST(SingleEntityLabelTest, Movable) {
  EntityLabels<Field<int, Name<kFooName>>> fd1;
  EntityLabels<Field<int, Name<kFooName>>> fd2{std::move(fd1)};
  EntityLabels<Field<int, Name<kFooName>>> fd3;
  EntityLabels<Field<int, Name<kFooName>>> fd4;
  fd4 = std::move(fd3);
  EXPECT_THAT(fd2.names(), ElementsAre(kFooName));
  EXPECT_THAT(fd4.names(), ElementsAre(kFooName));
  EntityLabels<Field<int>> fd5{kBarName};
  EntityLabels<Field<int>> fd6{std::move(fd5)};
  EntityLabels<Field<int>> fd7{kFooName};
  EntityLabels<Field<int>> fd8{kBarName};
  fd8 = std::move(fd7);
  EXPECT_THAT(fd6.names(), ElementsAre(kBarName));
  EXPECT_THAT(fd8.names(), ElementsAre(kFooName));
}

TEST(SingleMetricFieldTest, Movable) {
  MetricFields<Field<int, Name<kFooName>>> fd1;
  MetricFields<Field<int, Name<kFooName>>> fd2{std::move(fd1)};
  MetricFields<Field<int, Name<kFooName>>> fd3;
  MetricFields<Field<int, Name<kFooName>>> fd4;
  fd4 = std::move(fd3);
  EXPECT_THAT(fd2.names(), ElementsAre(kFooName));
  EXPECT_THAT(fd4.names(), ElementsAre(kFooName));
  MetricFields<Field<int>> fd5{kBarName};
  MetricFields<Field<int>> fd6{std::move(fd5)};
  MetricFields<Field<int>> fd7{kFooName};
  MetricFields<Field<int>> fd8{kBarName};
  fd8 = std::move(fd7);
  EXPECT_THAT(fd6.names(), ElementsAre(kBarName));
  EXPECT_THAT(fd8.names(), ElementsAre(kFooName));
}

TEST(TwoFieldDescriptorTest, Traits) {
  EXPECT_FALSE((EntityLabels<Field<bool>, Field<std::string>>::HasTypeNames::value));
  EXPECT_FALSE((EntityLabels<Field<bool>, Field<std::string>>::kHasTypeNames));
  EXPECT_TRUE((EntityLabels<Field<bool, Name<kFooName>>,
                            Field<std::string, Name<kBarName>>>::HasTypeNames::value));
  EXPECT_TRUE((EntityLabels<Field<bool, Name<kFooName>>,
                            Field<std::string, Name<kBarName>>>::kHasTypeNames));
  EXPECT_TRUE((EntityLabels<Field<bool>, Field<std::string>>::HasParameterNames::value));
  EXPECT_TRUE((EntityLabels<Field<bool>, Field<std::string>>::kHasParameterNames));
  EXPECT_FALSE((EntityLabels<Field<bool, Name<kFooName>>,
                             Field<std::string, Name<kBarName>>>::HasParameterNames::value));
  EXPECT_FALSE((EntityLabels<Field<bool, Name<kFooName>>,
                             Field<std::string, Name<kBarName>>>::kHasParameterNames));
  EXPECT_FALSE((MetricFields<Field<bool>, Field<std::string>>::HasTypeNames::value));
  EXPECT_FALSE((MetricFields<Field<bool>, Field<std::string>>::kHasTypeNames));
  EXPECT_TRUE((MetricFields<Field<bool, Name<kFooName>>,
                            Field<std::string, Name<kBarName>>>::HasTypeNames::value));
  EXPECT_TRUE((MetricFields<Field<bool, Name<kFooName>>,
                            Field<std::string, Name<kBarName>>>::kHasTypeNames));
  EXPECT_TRUE((MetricFields<Field<bool>, Field<std::string>>::HasParameterNames::value));
  EXPECT_TRUE((MetricFields<Field<bool>, Field<std::string>>::kHasParameterNames));
  EXPECT_FALSE((MetricFields<Field<bool, Name<kFooName>>,
                             Field<std::string, Name<kBarName>>>::HasParameterNames::value));
  EXPECT_FALSE((MetricFields<Field<bool, Name<kFooName>>,
                             Field<std::string, Name<kBarName>>>::kHasParameterNames));
}

TEST(TwoFieldDescriptorTest, TypeNames) {
  EntityLabels<Field<bool, Name<kFooName>>, Field<std::string, Name<kBarName>>> el;
  EXPECT_THAT(el.names(), ElementsAre(kFooName, kBarName));
  MetricFields<Field<bool, Name<kBarName>>, Field<std::string, Name<kFooName>>> mf;
  EXPECT_THAT(mf.names(), ElementsAre(kBarName, kFooName));
}

TEST(TwoFieldDescriptorTest, ParameterNames) {
  EntityLabels<Field<bool>, Field<std::string>> el{kFooName, kBarName};
  EXPECT_THAT(el.names(), ElementsAre(kFooName, kBarName));
  MetricFields<Field<bool>, Field<std::string>> mf{kBarName, kFooName};
  EXPECT_THAT(mf.names(), ElementsAre(kBarName, kFooName));
}

TEST(TwoFieldDescriptorTest, FieldMap) {
  EntityLabels<Field<bool, Name<kFooName>>, Field<std::string, Name<kBarName>>> el;
  EXPECT_THAT(el.MakeFieldMap(true, "lorem"),
              ElementsAre(Pair(kBarName, VariantWith<std::string>("lorem")),
                          Pair(kFooName, VariantWith<bool>(true))));
  MetricFields<Field<bool>, Field<std::string>> mf{kBarName, kFooName};
  EXPECT_THAT(mf.MakeFieldMap(true, "lorem"),
              ElementsAre(Pair(kBarName, VariantWith<bool>(true)),
                          Pair(kFooName, VariantWith<std::string>("lorem"))));
}

}  // namespace
