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
using ::tsz::Field;
using ::tsz::FieldDescriptor;
using ::tsz::Name;

char constexpr kFooName[] = "foo";
char constexpr kBarName[] = "bar";

TEST(EmptyFieldDescriptorTest, Traits) {
  EXPECT_TRUE(FieldDescriptor<>::HasTypeNames::value);
  EXPECT_TRUE(FieldDescriptor<>::kHasTypeNames);
  EXPECT_TRUE(FieldDescriptor<>::HasParameterNames::value);
  EXPECT_TRUE(FieldDescriptor<>::kHasParameterNames);
}

TEST(EmptyFieldDescriptorTest, Names) {
  FieldDescriptor<> fd;
  EXPECT_THAT(fd.names(), ElementsAre());
}

TEST(EmptyFieldDescriptorTest, FieldMap) {
  FieldDescriptor<> fd;
  EXPECT_THAT(fd.MakeFieldMap(), ElementsAre());
}

TEST(EmptyFieldDescriptorTest, Copyable) {
  FieldDescriptor<> fd1;
  FieldDescriptor<> fd2{fd1};
  FieldDescriptor<> fd3;
  fd3 = fd1;
  EXPECT_THAT(fd2.names(), ElementsAre());
  EXPECT_THAT(fd3.names(), ElementsAre());
}

TEST(EmptyFieldDescriptorTest, Movable) {
  FieldDescriptor<> fd1;
  FieldDescriptor<> fd2{std::move(fd1)};
  FieldDescriptor<> fd3;
  FieldDescriptor<> fd4;
  fd4 = std::move(fd3);
  EXPECT_THAT(fd2.names(), ElementsAre());
  EXPECT_THAT(fd4.names(), ElementsAre());
}

TEST(SingleFieldDescriptorTest, Traits) {
  EXPECT_FALSE(FieldDescriptor<Field<int>>::HasTypeNames::value);
  EXPECT_FALSE(FieldDescriptor<Field<int>>::kHasTypeNames);
  EXPECT_TRUE((FieldDescriptor<Field<int, Name<kFooName>>>::HasTypeNames::value));
  EXPECT_TRUE((FieldDescriptor<Field<int, Name<kFooName>>>::kHasTypeNames));
  EXPECT_TRUE(FieldDescriptor<Field<int>>::HasParameterNames::value);
  EXPECT_TRUE(FieldDescriptor<Field<int>>::kHasParameterNames);
  EXPECT_FALSE((FieldDescriptor<Field<int, Name<kFooName>>>::HasParameterNames::value));
  EXPECT_FALSE((FieldDescriptor<Field<int, Name<kFooName>>>::kHasParameterNames));
}

TEST(SingleFieldDescriptorTest, TypeName) {
  FieldDescriptor<Field<int, Name<kFooName>>> fd1;
  EXPECT_THAT(fd1.names(), ElementsAre(kFooName));
  FieldDescriptor<Field<int, Name<kBarName>>> fd2;
  EXPECT_THAT(fd2.names(), ElementsAre(kBarName));
}

TEST(SingleFieldDescriptorTest, ParameterName) {
  FieldDescriptor<Field<int>> fd1{kFooName};
  EXPECT_THAT(fd1.names(), ElementsAre(kFooName));
  FieldDescriptor<Field<int>> fd2{kBarName};
  EXPECT_THAT(fd2.names(), ElementsAre(kBarName));
}

TEST(SingleFieldDescriptorTest, FieldMap) {
  FieldDescriptor<Field<int, Name<kFooName>>> fd1;
  EXPECT_THAT(fd1.MakeFieldMap(42), ElementsAre(Pair(kFooName, VariantWith<int64_t>(42))));
  FieldDescriptor<Field<int>> fd2{kBarName};
  EXPECT_THAT(fd2.MakeFieldMap(43), ElementsAre(Pair(kBarName, VariantWith<int64_t>(43))));
}

TEST(SingleFieldDescriptorTest, Copyable) {
  FieldDescriptor<Field<int, Name<kFooName>>> fd1;
  FieldDescriptor<Field<int, Name<kFooName>>> fd2{fd1};
  FieldDescriptor<Field<int, Name<kFooName>>> fd3;
  fd3 = fd1;
  EXPECT_THAT(fd2.names(), ElementsAre(kFooName));
  EXPECT_THAT(fd3.names(), ElementsAre(kFooName));
  FieldDescriptor<Field<int>> fd4{kBarName};
  FieldDescriptor<Field<int>> fd5{fd4};
  FieldDescriptor<Field<int>> fd6{kFooName};
  fd6 = fd4;
  EXPECT_THAT(fd5.names(), ElementsAre(kBarName));
  EXPECT_THAT(fd6.names(), ElementsAre(kBarName));
}

TEST(SingleFieldDescriptorTest, Movable) {
  FieldDescriptor<Field<int, Name<kFooName>>> fd1;
  FieldDescriptor<Field<int, Name<kFooName>>> fd2{std::move(fd1)};
  FieldDescriptor<Field<int, Name<kFooName>>> fd3;
  FieldDescriptor<Field<int, Name<kFooName>>> fd4;
  fd4 = std::move(fd3);
  EXPECT_THAT(fd2.names(), ElementsAre(kFooName));
  EXPECT_THAT(fd4.names(), ElementsAre(kFooName));
  FieldDescriptor<Field<int>> fd5{kBarName};
  FieldDescriptor<Field<int>> fd6{std::move(fd5)};
  FieldDescriptor<Field<int>> fd7{kFooName};
  FieldDescriptor<Field<int>> fd8{kBarName};
  fd8 = std::move(fd7);
  EXPECT_THAT(fd6.names(), ElementsAre(kBarName));
  EXPECT_THAT(fd8.names(), ElementsAre(kFooName));
}

TEST(TwoFieldDescriptorTest, Traits) {
  EXPECT_FALSE((FieldDescriptor<Field<bool>, Field<std::string>>::HasTypeNames::value));
  EXPECT_FALSE((FieldDescriptor<Field<bool>, Field<std::string>>::kHasTypeNames));
  EXPECT_TRUE((FieldDescriptor<Field<bool, Name<kFooName>>,
                               Field<std::string, Name<kBarName>>>::HasTypeNames::value));
  EXPECT_TRUE((FieldDescriptor<Field<bool, Name<kFooName>>,
                               Field<std::string, Name<kBarName>>>::kHasTypeNames));
  EXPECT_TRUE((FieldDescriptor<Field<bool>, Field<std::string>>::HasParameterNames::value));
  EXPECT_TRUE((FieldDescriptor<Field<bool>, Field<std::string>>::kHasParameterNames));
  EXPECT_FALSE((FieldDescriptor<Field<bool, Name<kFooName>>,
                                Field<std::string, Name<kBarName>>>::HasParameterNames::value));
  EXPECT_FALSE((FieldDescriptor<Field<bool, Name<kFooName>>,
                                Field<std::string, Name<kBarName>>>::kHasParameterNames));
}

TEST(TwoFieldDescriptorTest, TypeNames) {
  FieldDescriptor<Field<bool, Name<kFooName>>, Field<std::string, Name<kBarName>>> fd1;
  EXPECT_THAT(fd1.names(), ElementsAre(kFooName, kBarName));
  FieldDescriptor<Field<bool, Name<kBarName>>, Field<std::string, Name<kFooName>>> fd2;
  EXPECT_THAT(fd2.names(), ElementsAre(kBarName, kFooName));
}

TEST(TwoFieldDescriptorTest, ParameterNames) {
  FieldDescriptor<Field<bool>, Field<std::string>> fd1{kFooName, kBarName};
  EXPECT_THAT(fd1.names(), ElementsAre(kFooName, kBarName));
  FieldDescriptor<Field<bool>, Field<std::string>> fd2{kBarName, kFooName};
  EXPECT_THAT(fd2.names(), ElementsAre(kBarName, kFooName));
}

TEST(TwoFieldDescriptorTest, FieldMap) {
  FieldDescriptor<Field<bool, Name<kFooName>>, Field<std::string, Name<kBarName>>> fd1;
  EXPECT_THAT(fd1.MakeFieldMap(true, "lorem"),
              ElementsAre(Pair(kBarName, VariantWith<std::string>("lorem")),
                          Pair(kFooName, VariantWith<bool>(true))));
  FieldDescriptor<Field<bool>, Field<std::string>> fd2{kBarName, kFooName};
  EXPECT_THAT(fd2.MakeFieldMap(true, "lorem"),
              ElementsAre(Pair(kBarName, VariantWith<bool>(true)),
                          Pair(kFooName, VariantWith<std::string>("lorem"))));
}

}  // namespace
