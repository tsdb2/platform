#include "tsz/metric.h"

#include <string>
#include <string_view>

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

NoDestructor<tsz::Metric<
    int,
    tsz::EntityLabels<tsz::Field<std::string, kLoremLabel>, tsz::Field<std::string, kIpsumLabel>>,
    tsz::MetricFields<tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>>>
    metric1{kMetricName, tsz::Options{
                             .description = std::string(kMetricDescription),
                         }};

NoDestructor<tsz::Metric<int, tsz::EntityLabels<tsz::Field<std::string>, tsz::Field<std::string>>,
                         tsz::MetricFields<tsz::Field<int>, tsz::Field<bool>>>>
    metric2{kMetricName,
            kLoremLabel,
            kIpsumLabel,
            kFooField,
            kBarField,
            tsz::Options{
                .description = std::string(kMetricDescription),
            }};

NoDestructor<
    tsz::Metric<int, tsz::EntityLabels<std::string, std::string>, tsz::MetricFields<int, bool>>>
    metric3{kMetricName,
            kLoremLabel,
            kIpsumLabel,
            kFooField,
            kBarField,
            tsz::Options{
                .description = std::string(kMetricDescription),
            }};

NoDestructor<tsz::Metric<int, tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>> metric4{
    *entity, kMetricName,
    tsz::Options{
        .description = std::string(kMetricDescription),
    }};

NoDestructor<tsz::Metric<int, int, bool>> metric5{
    *entity, kMetricName, kFooField, kBarField,
    tsz::Options{
        .description = std::string(kMetricDescription),
    }};

NoDestructor<tsz::Metric<int, tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>> metric6{
    kMetricName, tsz::Options{
                     .description = std::string(kMetricDescription),
                 }};

NoDestructor<tsz::Metric<int, int, bool>> metric7{
    kMetricName, kFooField, kBarField,
    tsz::Options{
        .description = std::string(kMetricDescription),
    }};

class MetricTest : public ::testing::Test {
 protected:
  CellReader<
      int,
      tsz::EntityLabels<tsz::Field<std::string, kLoremLabel>, tsz::Field<std::string, kIpsumLabel>>,
      tsz::MetricFields<tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>>
      reader1_{kMetricName};

  CellReader<int, tsz::EntityLabels<>,
             tsz::MetricFields<tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>>
      reader2_{kMetricName};
};

TEST_F(MetricTest, MetricName) {
  EXPECT_EQ(metric1->name(), kMetricName);
  EXPECT_EQ(metric2->name(), kMetricName);
  EXPECT_EQ(metric3->name(), kMetricName);
  EXPECT_EQ(metric4->name(), kMetricName);
  EXPECT_EQ(metric5->name(), kMetricName);
  EXPECT_EQ(metric6->name(), kMetricName);
  EXPECT_EQ(metric7->name(), kMetricName);
}

TEST_F(MetricTest, Options) {
  EXPECT_THAT(metric1->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(metric2->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(metric3->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(metric4->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(metric5->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(metric6->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(metric7->options(), Field(&tsz::Options::description, kMetricDescription));
}

TEST_F(MetricTest, EntityLabels) {
  EXPECT_THAT(metric1->entity_labels().names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(metric1->entity_label_names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(metric2->entity_labels().names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(metric2->entity_label_names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(metric3->entity_labels().names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(metric3->entity_label_names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(metric4->entity_labels(),
              UnorderedElementsAre(Pair(kLoremLabel, VariantWith<std::string>("lorem_value")),
                                   Pair(kIpsumLabel, VariantWith<std::string>("ipsum_value"))));
  EXPECT_THAT(metric5->entity_labels(),
              UnorderedElementsAre(Pair(kLoremLabel, VariantWith<std::string>("lorem_value")),
                                   Pair(kIpsumLabel, VariantWith<std::string>("ipsum_value"))));
  EXPECT_THAT(metric6->entity_labels(), UnorderedElementsAre());
  EXPECT_THAT(metric7->entity_labels(), UnorderedElementsAre());
}

TEST_F(MetricTest, MetricFields) {
  EXPECT_THAT(metric1->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(metric1->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(metric2->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(metric2->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(metric3->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(metric3->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(metric4->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(metric4->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(metric5->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(metric5->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(metric6->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(metric6->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(metric7->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(metric7->metric_field_names(), ElementsAre(kFooField, kBarField));
}

TEST_F(MetricTest, Set1) {
  metric1->Set(12, "12", "34", 123, false);
  metric1->Set(34, "12", "34", 123, false);
  metric1->Set(56, "56", "78", 456, true);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), IsOkAndHolds(34));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(56));
}

TEST_F(MetricTest, Set2) {
  metric2->Set(12, "12", "34", 123, false);
  metric2->Set(34, "12", "34", 123, false);
  metric2->Set(56, "56", "78", 456, true);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), IsOkAndHolds(34));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(56));
}

TEST_F(MetricTest, Set3) {
  metric3->Set(12, "12", "34", 123, false);
  metric3->Set(34, "12", "34", 123, false);
  metric3->Set(56, "56", "78", 456, true);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), IsOkAndHolds(34));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(56));
}

TEST_F(MetricTest, Set4) {
  metric4->Set(12, 123, false);
  metric4->Set(34, 123, false);
  metric4->Set(56, 456, true);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false), IsOkAndHolds(34));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true), IsOkAndHolds(56));
}

TEST_F(MetricTest, Set5) {
  metric5->Set(12, 123, false);
  metric5->Set(34, 123, false);
  metric5->Set(56, 456, true);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false), IsOkAndHolds(34));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true), IsOkAndHolds(56));
}

TEST_F(MetricTest, Set6) {
  metric6->Set(12, 123, false);
  metric6->Set(34, 123, false);
  metric6->Set(56, 456, true);
  EXPECT_THAT(reader2_.Read(123, false), IsOkAndHolds(34));
  EXPECT_THAT(reader2_.Read(456, true), IsOkAndHolds(56));
}

TEST_F(MetricTest, Set7) {
  metric7->Set(12, 123, false);
  metric7->Set(34, 123, false);
  metric7->Set(56, 456, true);
  EXPECT_THAT(reader2_.Read(123, false), IsOkAndHolds(34));
  EXPECT_THAT(reader2_.Read(456, true), IsOkAndHolds(56));
}

TEST_F(MetricTest, Get1) {
  metric1->Set(12, "12", "34", 123, false);
  metric1->Set(34, "12", "34", 123, false);
  metric1->Set(56, "56", "78", 456, true);
  EXPECT_THAT(metric1->Get("12", "34", 123, false), IsOkAndHolds(34));
  EXPECT_THAT(metric1->Get("56", "78", 456, true), IsOkAndHolds(56));
}

TEST_F(MetricTest, Get2) {
  metric2->Set(12, "12", "34", 123, false);
  metric2->Set(34, "12", "34", 123, false);
  metric2->Set(56, "56", "78", 456, true);
  EXPECT_THAT(metric2->Get("12", "34", 123, false), IsOkAndHolds(34));
  EXPECT_THAT(metric2->Get("56", "78", 456, true), IsOkAndHolds(56));
}

TEST_F(MetricTest, Get3) {
  metric3->Set(12, "12", "34", 123, false);
  metric3->Set(34, "12", "34", 123, false);
  metric3->Set(56, "56", "78", 456, true);
  EXPECT_THAT(metric3->Get("12", "34", 123, false), IsOkAndHolds(34));
  EXPECT_THAT(metric3->Get("56", "78", 456, true), IsOkAndHolds(56));
}

TEST_F(MetricTest, Get4) {
  metric4->Set(12, 123, false);
  metric4->Set(34, 123, false);
  metric4->Set(56, 456, true);
  EXPECT_THAT(metric4->Get(123, false), IsOkAndHolds(34));
  EXPECT_THAT(metric4->Get(456, true), IsOkAndHolds(56));
}

TEST_F(MetricTest, Get5) {
  metric5->Set(12, 123, false);
  metric5->Set(34, 123, false);
  metric5->Set(56, 456, true);
  EXPECT_THAT(metric5->Get(123, false), IsOkAndHolds(34));
  EXPECT_THAT(metric5->Get(456, true), IsOkAndHolds(56));
}

TEST_F(MetricTest, Get6) {
  metric6->Set(12, 123, false);
  metric6->Set(34, 123, false);
  metric6->Set(56, 456, true);
  EXPECT_THAT(metric6->Get(123, false), IsOkAndHolds(34));
  EXPECT_THAT(metric6->Get(456, true), IsOkAndHolds(56));
}

TEST_F(MetricTest, Get7) {
  metric7->Set(12, 123, false);
  metric7->Set(34, 123, false);
  metric7->Set(56, 456, true);
  EXPECT_THAT(metric7->Get(123, false), IsOkAndHolds(34));
  EXPECT_THAT(metric7->Get(456, true), IsOkAndHolds(56));
}

TEST_F(MetricTest, Delete1) {
  metric1->Set(12, "12", "34", 123, false);
  metric1->Set(34, "56", "78", 456, true);
  metric1->Delete("12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(34));
}

TEST_F(MetricTest, Delete2) {
  metric2->Set(12, "12", "34", 123, false);
  metric2->Set(34, "56", "78", 456, true);
  metric2->Delete("12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(34));
}

TEST_F(MetricTest, Delete3) {
  metric3->Set(12, "12", "34", 123, false);
  metric3->Set(34, "56", "78", 456, true);
  metric3->Delete("12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(34));
}

TEST_F(MetricTest, Delete4) {
  metric4->Set(12, 123, false);
  metric4->Set(34, 456, true);
  metric4->Delete(123, false);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true), IsOkAndHolds(34));
}

TEST_F(MetricTest, Delete5) {
  metric5->Set(12, 123, false);
  metric5->Set(34, 456, true);
  metric5->Delete(123, false);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true), IsOkAndHolds(34));
}

TEST_F(MetricTest, Delete6) {
  metric6->Set(12, 123, false);
  metric6->Set(34, 456, true);
  metric6->Delete(123, false);
  EXPECT_THAT(reader2_.Read(123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader2_.Read(456, true), IsOkAndHolds(34));
}

TEST_F(MetricTest, Delete7) {
  metric7->Set(12, 123, false);
  metric7->Set(34, 456, true);
  metric7->Delete(123, false);
  EXPECT_THAT(reader2_.Read(123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader2_.Read(456, true), IsOkAndHolds(34));
}

TEST_F(MetricTest, SetAgain1) {
  metric1->Set(12, "12", "34", 123, false);
  metric1->Set(34, "56", "78", 456, true);
  metric1->Delete("12", "34", 123, false);
  metric1->Set(56, "12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), IsOkAndHolds(56));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(34));
}

TEST_F(MetricTest, SetAgain2) {
  metric2->Set(12, "12", "34", 123, false);
  metric2->Set(34, "56", "78", 456, true);
  metric2->Delete("12", "34", 123, false);
  metric2->Set(56, "12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), IsOkAndHolds(56));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(34));
}

TEST_F(MetricTest, SetAgain3) {
  metric3->Set(12, "12", "34", 123, false);
  metric3->Set(34, "56", "78", 456, true);
  metric3->Delete("12", "34", 123, false);
  metric3->Set(56, "12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), IsOkAndHolds(56));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), IsOkAndHolds(34));
}

TEST_F(MetricTest, SetAgain4) {
  metric4->Set(12, 123, false);
  metric4->Set(34, 456, true);
  metric4->Delete(123, false);
  metric4->Set(56, 123, false);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false), IsOkAndHolds(56));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true), IsOkAndHolds(34));
}

TEST_F(MetricTest, SetAgain5) {
  metric5->Set(12, 123, false);
  metric5->Set(34, 456, true);
  metric5->Delete(123, false);
  metric5->Set(56, 123, false);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false), IsOkAndHolds(56));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true), IsOkAndHolds(34));
}

TEST_F(MetricTest, SetAgain6) {
  metric6->Set(12, 123, false);
  metric6->Set(34, 456, true);
  metric6->Delete(123, false);
  metric6->Set(56, 123, false);
  EXPECT_THAT(reader2_.Read(123, false), IsOkAndHolds(56));
  EXPECT_THAT(reader2_.Read(456, true), IsOkAndHolds(34));
}

TEST_F(MetricTest, SetAgain7) {
  metric7->Set(12, 123, false);
  metric7->Set(34, 456, true);
  metric7->Delete(123, false);
  metric7->Set(56, 123, false);
  EXPECT_THAT(reader2_.Read(123, false), IsOkAndHolds(56));
  EXPECT_THAT(reader2_.Read(456, true), IsOkAndHolds(34));
}

TEST_F(MetricTest, Clear1) {
  metric1->Set(12, "12", "34", 123, false);
  metric1->Set(34, "56", "78", 456, true);
  metric1->Clear();
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(MetricTest, Clear2) {
  metric2->Set(12, "12", "34", 123, false);
  metric2->Set(34, "56", "78", 456, true);
  metric2->Clear();
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(MetricTest, Clear3) {
  metric3->Set(12, "12", "34", 123, false);
  metric3->Set(34, "56", "78", 456, true);
  metric3->Clear();
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(MetricTest, Clear4) {
  metric4->Set(12, 123, false);
  metric4->Set(34, 456, true);
  metric4->Clear();
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(MetricTest, Clear5) {
  metric5->Set(12, 123, false);
  metric5->Set(34, 456, true);
  metric5->Clear();
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(MetricTest, Clear6) {
  metric6->Set(12, 123, false);
  metric6->Set(34, 456, true);
  metric6->Clear();
  EXPECT_THAT(reader2_.Read(123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader2_.Read(456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(MetricTest, Clear7) {
  metric7->Set(12, 123, false);
  metric7->Set(34, 456, true);
  metric7->Clear();
  EXPECT_THAT(reader2_.Read(123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader2_.Read(456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(MetricTest, DeleteEntity1) {
  metric1->Set(12, "12", "34", 123, false);
  metric1->Set(34, "12", "34", 456, true);
  metric1->Set(56, "56", "78", 789, true);
  metric1->DeleteEntity("12", "34");
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("12", "34", 456, true), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 789, true), IsOkAndHolds(56));
}

TEST_F(MetricTest, DeleteEntity2) {
  metric2->Set(12, "12", "34", 123, false);
  metric2->Set(34, "12", "34", 456, true);
  metric2->Set(56, "56", "78", 789, true);
  metric2->DeleteEntity("12", "34");
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("12", "34", 456, true), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 789, true), IsOkAndHolds(56));
}

TEST_F(MetricTest, DeleteEntity3) {
  metric3->Set(12, "12", "34", 123, false);
  metric3->Set(34, "12", "34", 456, true);
  metric3->Set(56, "56", "78", 789, true);
  metric3->DeleteEntity("12", "34");
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("12", "34", 456, true), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 789, true), IsOkAndHolds(56));
}

}  // namespace
