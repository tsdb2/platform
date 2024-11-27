#include "tsz/counter.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tsz/base.h"
#include "tsz/cell_reader.h"
#include "tsz/entity.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;
using ::tsz::NoDestructor;
using ::tsz::testing::CellReader;

std::string_view constexpr kMetricName = "/lorem/ipsum";
std::string_view constexpr kMetricDescription = "Lorem ipsum dolor sit amet.";

char constexpr kLoremLabel[] = "lorem";
char constexpr kIpsumLabel[] = "ipsum";
char constexpr kFooField[] = "foo";
char constexpr kBarField[] = "bar";

NoDestructor<tsz::Entity<tsz::Field<std::string, kLoremLabel>,
                         tsz::Field<std::string, kIpsumLabel>>> const entity{
    "lorem_value",
    "ipsum_value",
};

NoDestructor<tsz::Counter<
    tsz::EntityLabels<tsz::Field<std::string, kLoremLabel>, tsz::Field<std::string, kIpsumLabel>>,
    tsz::MetricFields<tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>>>
    counter1{kMetricName, tsz::Options{
                              .description = std::string(kMetricDescription),
                          }};

NoDestructor<tsz::Counter<tsz::EntityLabels<tsz::Field<std::string>, tsz::Field<std::string>>,
                          tsz::MetricFields<tsz::Field<int>, tsz::Field<bool>>>>
    counter2{kMetricName,
             kLoremLabel,
             kIpsumLabel,
             kFooField,
             kBarField,
             tsz::Options{
                 .description = std::string(kMetricDescription),
             }};

NoDestructor<
    tsz::Counter<tsz::EntityLabels<std::string, std::string>, tsz::MetricFields<int, bool>>>
    counter3{kMetricName,
             kLoremLabel,
             kIpsumLabel,
             kFooField,
             kBarField,
             tsz::Options{
                 .description = std::string(kMetricDescription),
             }};

NoDestructor<tsz::Counter<tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>> counter4{
    *entity, kMetricName,
    tsz::Options{
        .description = std::string(kMetricDescription),
    }};

NoDestructor<tsz::Counter<int, bool>> counter5{*entity, kMetricName, kFooField, kBarField,
                                               tsz::Options{
                                                   .description = std::string(kMetricDescription),
                                               }};

NoDestructor<tsz::Counter<tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>> counter6{
    kMetricName, tsz::Options{
                     .description = std::string(kMetricDescription),
                 }};

NoDestructor<tsz::Counter<int, bool>> counter7{kMetricName, kFooField, kBarField,
                                               tsz::Options{
                                                   .description = std::string(kMetricDescription),
                                               }};

class CounterTest : public ::testing::Test {
 protected:
  CellReader<
      int64_t,
      tsz::EntityLabels<tsz::Field<std::string, kLoremLabel>, tsz::Field<std::string, kIpsumLabel>>,
      tsz::MetricFields<tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>>
      reader1_{kMetricName};

  CellReader<int64_t, tsz::EntityLabels<>,
             tsz::MetricFields<tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>>
      reader2_{kMetricName};
};

TEST_F(CounterTest, MetricName) {
  EXPECT_EQ(counter1->name(), kMetricName);
  EXPECT_EQ(counter2->name(), kMetricName);
  EXPECT_EQ(counter3->name(), kMetricName);
  EXPECT_EQ(counter4->name(), kMetricName);
  EXPECT_EQ(counter5->name(), kMetricName);
  EXPECT_EQ(counter6->name(), kMetricName);
  EXPECT_EQ(counter7->name(), kMetricName);
}

TEST_F(CounterTest, Options) {
  EXPECT_THAT(counter1->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(counter2->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(counter3->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(counter4->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(counter5->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(counter6->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(counter7->options(), Field(&tsz::Options::description, kMetricDescription));
}

TEST_F(CounterTest, EntityLabels) {
  EXPECT_THAT(counter1->entity_labels().names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(counter1->entity_label_names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(counter2->entity_labels().names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(counter2->entity_label_names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(counter3->entity_labels().names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(counter3->entity_label_names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(counter4->entity_labels(),
              UnorderedElementsAre(Pair(kLoremLabel, VariantWith<std::string>("lorem_value")),
                                   Pair(kIpsumLabel, VariantWith<std::string>("ipsum_value"))));
  EXPECT_THAT(counter5->entity_labels(),
              UnorderedElementsAre(Pair(kLoremLabel, VariantWith<std::string>("lorem_value")),
                                   Pair(kIpsumLabel, VariantWith<std::string>("ipsum_value"))));
  EXPECT_THAT(counter6->entity_labels(), UnorderedElementsAre());
  EXPECT_THAT(counter7->entity_labels(), UnorderedElementsAre());
}

TEST_F(CounterTest, MetricFields) {
  EXPECT_THAT(counter1->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(counter1->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(counter2->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(counter2->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(counter3->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(counter3->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(counter4->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(counter4->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(counter5->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(counter5->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(counter6->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(counter6->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(counter7->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(counter7->metric_field_names(), ElementsAre(kFooField, kBarField));
}

TEST_F(CounterTest, Increment1) {
  counter1->IncrementBy(12, "12", "34", 123, false);
  counter1->IncrementBy(34, "12", "34", 123, false);
  counter1->IncrementBy(56, "56", "78", 456, true);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), IsOkAndHolds(46));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(56));
}

TEST_F(CounterTest, Increment2) {
  counter2->IncrementBy(12, "12", "34", 123, false);
  counter2->IncrementBy(34, "12", "34", 123, false);
  counter2->IncrementBy(56, "56", "78", 456, true);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), IsOkAndHolds(46));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(56));
}

TEST_F(CounterTest, Increment3) {
  counter3->IncrementBy(12, "12", "34", 123, false);
  counter3->IncrementBy(34, "12", "34", 123, false);
  counter3->IncrementBy(56, "56", "78", 456, true);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), IsOkAndHolds(46));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(56));
}

TEST_F(CounterTest, Increment4) {
  counter4->IncrementBy(12, 123, false);
  counter4->IncrementBy(34, 123, false);
  counter4->IncrementBy(56, 456, true);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false), IsOkAndHolds(46));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true), IsOkAndHolds(56));
}

TEST_F(CounterTest, Increment5) {
  counter5->IncrementBy(12, 123, false);
  counter5->IncrementBy(34, 123, false);
  counter5->IncrementBy(56, 456, true);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false), IsOkAndHolds(46));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true), IsOkAndHolds(56));
}

TEST_F(CounterTest, Increment6) {
  counter6->IncrementBy(12, 123, false);
  counter6->IncrementBy(34, 123, false);
  counter6->IncrementBy(56, 456, true);
  EXPECT_THAT(reader2_.Read(123, false), IsOkAndHolds(46));
  EXPECT_THAT(reader2_.Read(456, true), IsOkAndHolds(56));
}

TEST_F(CounterTest, Increment7) {
  counter7->IncrementBy(12, 123, false);
  counter7->IncrementBy(34, 123, false);
  counter7->IncrementBy(56, 456, true);
  EXPECT_THAT(reader2_.Read(123, false), IsOkAndHolds(46));
  EXPECT_THAT(reader2_.Read(456, true), IsOkAndHolds(56));
}

TEST_F(CounterTest, IncrementByOne1) {
  counter1->Increment("12", "34", 123, false);
  counter1->Increment("56", "78", 456, true);
  counter1->Increment("56", "78", 456, true);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), IsOkAndHolds(1));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(2));
}

TEST_F(CounterTest, IncrementByOne2) {
  counter2->Increment("12", "34", 123, false);
  counter2->Increment("56", "78", 456, true);
  counter2->Increment("56", "78", 456, true);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), IsOkAndHolds(1));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(2));
}

TEST_F(CounterTest, IncrementByOne3) {
  counter3->Increment("12", "34", 123, false);
  counter3->Increment("56", "78", 456, true);
  counter3->Increment("56", "78", 456, true);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), IsOkAndHolds(1));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(2));
}

TEST_F(CounterTest, IncrementByOne4) {
  counter4->Increment(123, false);
  counter4->Increment(456, true);
  counter4->Increment(456, true);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false), IsOkAndHolds(1));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true), IsOkAndHolds(2));
}

TEST_F(CounterTest, IncrementByOne5) {
  counter5->Increment(123, false);
  counter5->Increment(456, true);
  counter5->Increment(456, true);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false), IsOkAndHolds(1));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true), IsOkAndHolds(2));
}

TEST_F(CounterTest, IncrementByOne6) {
  counter6->Increment(123, false);
  counter6->Increment(456, true);
  counter6->Increment(456, true);
  EXPECT_THAT(reader2_.Read(123, false), IsOkAndHolds(1));
  EXPECT_THAT(reader2_.Read(456, true), IsOkAndHolds(2));
}

TEST_F(CounterTest, IncrementByOne7) {
  counter7->Increment(123, false);
  counter7->Increment(456, true);
  counter7->Increment(456, true);
  EXPECT_THAT(reader2_.Read(123, false), IsOkAndHolds(1));
  EXPECT_THAT(reader2_.Read(456, true), IsOkAndHolds(2));
}

TEST_F(CounterTest, Delete1) {
  counter1->IncrementBy(12, "12", "34", 123, false);
  counter1->IncrementBy(34, "56", "78", 456, true);
  counter1->Delete("12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(34));
}

TEST_F(CounterTest, Delete2) {
  counter2->IncrementBy(12, "12", "34", 123, false);
  counter2->IncrementBy(34, "56", "78", 456, true);
  counter2->Delete("12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(34));
}

TEST_F(CounterTest, Delete3) {
  counter3->IncrementBy(12, "12", "34", 123, false);
  counter3->IncrementBy(34, "56", "78", 456, true);
  counter3->Delete("12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(34));
}

TEST_F(CounterTest, Delete4) {
  counter4->IncrementBy(12, 123, false);
  counter4->IncrementBy(34, 456, true);
  counter4->Delete(123, false);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true), IsOkAndHolds(34));
}

TEST_F(CounterTest, Delete5) {
  counter5->IncrementBy(12, 123, false);
  counter5->IncrementBy(34, 456, true);
  counter5->Delete(123, false);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true), IsOkAndHolds(34));
}

TEST_F(CounterTest, Delete6) {
  counter6->IncrementBy(12, 123, false);
  counter6->IncrementBy(34, 456, true);
  counter6->Delete(123, false);
  EXPECT_THAT(reader2_.Read(123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader2_.Read(456, true), IsOkAndHolds(34));
}

TEST_F(CounterTest, Delete7) {
  counter7->IncrementBy(12, 123, false);
  counter7->IncrementBy(34, 456, true);
  counter7->Delete(123, false);
  EXPECT_THAT(reader2_.Read(123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader2_.Read(456, true), IsOkAndHolds(34));
}

TEST_F(CounterTest, IncrementAgain1) {
  counter1->IncrementBy(12, "12", "34", 123, false);
  counter1->IncrementBy(34, "56", "78", 456, true);
  counter1->Delete("12", "34", 123, false);
  counter1->IncrementBy(56, "12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), IsOkAndHolds(56));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(34));
}

TEST_F(CounterTest, IncrementAgain2) {
  counter2->IncrementBy(12, "12", "34", 123, false);
  counter2->IncrementBy(34, "56", "78", 456, true);
  counter2->Delete("12", "34", 123, false);
  counter2->IncrementBy(56, "12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), IsOkAndHolds(56));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(34));
}

TEST_F(CounterTest, IncrementAgain3) {
  counter3->IncrementBy(12, "12", "34", 123, false);
  counter3->IncrementBy(34, "56", "78", 456, true);
  counter3->Delete("12", "34", 123, false);
  counter3->IncrementBy(56, "12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), IsOkAndHolds(56));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(34));
}

TEST_F(CounterTest, IncrementAgain4) {
  counter4->IncrementBy(12, 123, false);
  counter4->IncrementBy(34, 456, true);
  counter4->Delete(123, false);
  counter4->IncrementBy(56, 123, false);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false), IsOkAndHolds(56));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true), IsOkAndHolds(34));
}

TEST_F(CounterTest, IncrementAgain5) {
  counter5->IncrementBy(12, 123, false);
  counter5->IncrementBy(34, 456, true);
  counter5->Delete(123, false);
  counter5->IncrementBy(56, 123, false);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false), IsOkAndHolds(56));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true), IsOkAndHolds(34));
}

TEST_F(CounterTest, IncrementAgain6) {
  counter6->IncrementBy(12, 123, false);
  counter6->IncrementBy(34, 456, true);
  counter6->Delete(123, false);
  counter6->IncrementBy(56, 123, false);
  EXPECT_THAT(reader2_.Read(123, false), IsOkAndHolds(56));
  EXPECT_THAT(reader2_.Read(456, true), IsOkAndHolds(34));
}

TEST_F(CounterTest, IncrementAgain7) {
  counter7->IncrementBy(12, 123, false);
  counter7->IncrementBy(34, 456, true);
  counter7->Delete(123, false);
  counter7->IncrementBy(56, 123, false);
  EXPECT_THAT(reader2_.Read(123, false), IsOkAndHolds(56));
  EXPECT_THAT(reader2_.Read(456, true), IsOkAndHolds(34));
}

TEST_F(CounterTest, Clear1) {
  counter1->IncrementBy(12, "12", "34", 123, false);
  counter1->IncrementBy(34, "56", "78", 456, true);
  counter1->Clear();
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(CounterTest, Clear2) {
  counter2->IncrementBy(12, "12", "34", 123, false);
  counter2->IncrementBy(34, "56", "78", 456, true);
  counter2->Clear();
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(CounterTest, Clear3) {
  counter3->IncrementBy(12, "12", "34", 123, false);
  counter3->IncrementBy(34, "56", "78", 456, true);
  counter3->Clear();
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(CounterTest, Clear4) {
  counter4->IncrementBy(12, 123, false);
  counter4->IncrementBy(34, 456, true);
  counter4->Clear();
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(CounterTest, Clear5) {
  counter5->IncrementBy(12, 123, false);
  counter5->IncrementBy(34, 456, true);
  counter5->Clear();
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(CounterTest, Clear6) {
  counter6->IncrementBy(12, 123, false);
  counter6->IncrementBy(34, 456, true);
  counter6->Clear();
  EXPECT_THAT(reader2_.Read(123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader2_.Read(456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(CounterTest, Clear7) {
  counter7->IncrementBy(12, 123, false);
  counter7->IncrementBy(34, 456, true);
  counter7->Clear();
  EXPECT_THAT(reader2_.Read(123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader2_.Read(456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(CounterTest, DeleteEntity1) {
  counter1->IncrementBy(12, "12", "34", 123, false);
  counter1->IncrementBy(34, "12", "34", 456, true);
  counter1->IncrementBy(56, "56", "78", 789, true);
  counter1->DeleteEntity("12", "34");
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("12", "34", 456, true), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 789, true), IsOkAndHolds(56));
}

TEST_F(CounterTest, DeleteEntity2) {
  counter2->IncrementBy(12, "12", "34", 123, false);
  counter2->IncrementBy(34, "12", "34", 456, true);
  counter2->IncrementBy(56, "56", "78", 789, true);
  counter2->DeleteEntity("12", "34");
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("12", "34", 456, true), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 789, true), IsOkAndHolds(56));
}

TEST_F(CounterTest, DeleteEntity3) {
  counter3->IncrementBy(12, "12", "34", 123, false);
  counter3->IncrementBy(34, "12", "34", 456, true);
  counter3->IncrementBy(56, "56", "78", 789, true);
  counter3->DeleteEntity("12", "34");
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("12", "34", 456, true), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 789, true), IsOkAndHolds(56));
}

}  // namespace
