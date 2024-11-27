#include "tsz/event_metric.h"

#include <string>
#include <string_view>

#include "absl/status/status_matchers.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tsz/base.h"
#include "tsz/cell_reader.h"
#include "tsz/distribution_testing.h"
#include "tsz/entity.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;
using ::tsz::Bucketer;
using ::tsz::Distribution;
using ::tsz::NoDestructor;
using ::tsz::testing::CellReader;
using ::tsz::testing::DistributionBucketerIs;
using ::tsz::testing::DistributionSumAndCountAre;

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

NoDestructor<tsz::EventMetric<
    tsz::EntityLabels<tsz::Field<std::string, kLoremLabel>, tsz::Field<std::string, kIpsumLabel>>,
    tsz::MetricFields<tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>>>
    event_metric1{kMetricName, tsz::Options{
                                   .description = std::string(kMetricDescription),
                               }};

NoDestructor<tsz::EventMetric<tsz::EntityLabels<tsz::Field<std::string>, tsz::Field<std::string>>,
                              tsz::MetricFields<tsz::Field<int>, tsz::Field<bool>>>>
    event_metric2{kMetricName,
                  kLoremLabel,
                  kIpsumLabel,
                  kFooField,
                  kBarField,
                  tsz::Options{
                      .description = std::string(kMetricDescription),
                  }};

NoDestructor<
    tsz::EventMetric<tsz::EntityLabels<std::string, std::string>, tsz::MetricFields<int, bool>>>
    event_metric3{kMetricName,
                  kLoremLabel,
                  kIpsumLabel,
                  kFooField,
                  kBarField,
                  tsz::Options{
                      .description = std::string(kMetricDescription),
                  }};

NoDestructor<tsz::EventMetric<tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>>
    event_metric4{*entity, kMetricName,
                  tsz::Options{
                      .description = std::string(kMetricDescription),
                  }};

NoDestructor<tsz::EventMetric<int, bool>> event_metric5{
    *entity, kMetricName, kFooField, kBarField,
    tsz::Options{
        .description = std::string(kMetricDescription),
    }};

NoDestructor<tsz::EventMetric<tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>>
    event_metric6{kMetricName, tsz::Options{
                                   .description = std::string(kMetricDescription),
                               }};

NoDestructor<tsz::EventMetric<int, bool>> event_metric7{
    kMetricName, kFooField, kBarField,
    tsz::Options{
        .description = std::string(kMetricDescription),
    }};

class EventMetricTest : public ::testing::Test {
 protected:
  CellReader<
      Distribution,
      tsz::EntityLabels<tsz::Field<std::string, kLoremLabel>, tsz::Field<std::string, kIpsumLabel>>,
      tsz::MetricFields<tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>>
      reader1_{kMetricName};

  CellReader<Distribution, tsz::EntityLabels<>,
             tsz::MetricFields<tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>>
      reader2_{kMetricName};
};

TEST_F(EventMetricTest, MetricName) {
  EXPECT_EQ(event_metric1->name(), kMetricName);
  EXPECT_EQ(event_metric2->name(), kMetricName);
  EXPECT_EQ(event_metric3->name(), kMetricName);
  EXPECT_EQ(event_metric4->name(), kMetricName);
  EXPECT_EQ(event_metric5->name(), kMetricName);
  EXPECT_EQ(event_metric6->name(), kMetricName);
  EXPECT_EQ(event_metric7->name(), kMetricName);
}

TEST_F(EventMetricTest, Options) {
  EXPECT_THAT(event_metric1->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(event_metric2->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(event_metric3->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(event_metric4->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(event_metric5->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(event_metric6->options(), Field(&tsz::Options::description, kMetricDescription));
  EXPECT_THAT(event_metric7->options(), Field(&tsz::Options::description, kMetricDescription));
}

TEST_F(EventMetricTest, EntityLabels) {
  EXPECT_THAT(event_metric1->entity_labels().names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(event_metric1->entity_label_names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(event_metric2->entity_labels().names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(event_metric2->entity_label_names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(event_metric3->entity_labels().names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(event_metric3->entity_label_names(), ElementsAre(kLoremLabel, kIpsumLabel));
  EXPECT_THAT(event_metric4->entity_labels(),
              UnorderedElementsAre(Pair(kLoremLabel, VariantWith<std::string>("lorem_value")),
                                   Pair(kIpsumLabel, VariantWith<std::string>("ipsum_value"))));
  EXPECT_THAT(event_metric5->entity_labels(),
              UnorderedElementsAre(Pair(kLoremLabel, VariantWith<std::string>("lorem_value")),
                                   Pair(kIpsumLabel, VariantWith<std::string>("ipsum_value"))));
  EXPECT_THAT(event_metric6->entity_labels(), UnorderedElementsAre());
  EXPECT_THAT(event_metric7->entity_labels(), UnorderedElementsAre());
}

TEST_F(EventMetricTest, MetricFields) {
  EXPECT_THAT(event_metric1->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(event_metric1->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(event_metric2->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(event_metric2->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(event_metric3->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(event_metric3->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(event_metric4->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(event_metric4->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(event_metric5->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(event_metric5->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(event_metric6->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(event_metric6->metric_field_names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(event_metric7->metric_fields().names(), ElementsAre(kFooField, kBarField));
  EXPECT_THAT(event_metric7->metric_field_names(), ElementsAre(kFooField, kBarField));
}

TEST_F(EventMetricTest, Record1) {
  event_metric1->Record(12, "12", "34", 123, false);
  event_metric1->Record(34, "12", "34", 123, false);
  event_metric1->Record(56, "56", "78", 456, true);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(46, 2))));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
}

TEST_F(EventMetricTest, Record2) {
  event_metric2->Record(12, "12", "34", 123, false);
  event_metric2->Record(34, "12", "34", 123, false);
  event_metric2->Record(56, "56", "78", 456, true);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(46, 2))));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
}

TEST_F(EventMetricTest, Record3) {
  event_metric3->Record(12, "12", "34", 123, false);
  event_metric3->Record(34, "12", "34", 123, false);
  event_metric3->Record(56, "56", "78", 456, true);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(46, 2))));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
}

TEST_F(EventMetricTest, Record4) {
  event_metric4->Record(12, 123, false);
  event_metric4->Record(34, 123, false);
  event_metric4->Record(56, 456, true);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(46, 2))));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
}

TEST_F(EventMetricTest, Record5) {
  event_metric5->Record(12, 123, false);
  event_metric5->Record(34, 123, false);
  event_metric5->Record(56, 456, true);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(46, 2))));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
}

TEST_F(EventMetricTest, Record6) {
  event_metric6->Record(12, 123, false);
  event_metric6->Record(34, 123, false);
  event_metric6->Record(56, 456, true);
  EXPECT_THAT(reader2_.Read(123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(46, 2))));
  EXPECT_THAT(reader2_.Read(456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
}

TEST_F(EventMetricTest, Record7) {
  event_metric7->Record(12, 123, false);
  event_metric7->Record(34, 123, false);
  event_metric7->Record(56, 456, true);
  EXPECT_THAT(reader2_.Read(123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(46, 2))));
  EXPECT_THAT(reader2_.Read(456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
}

TEST_F(EventMetricTest, RecordMany1) {
  event_metric1->RecordMany(12, 2, "12", "34", 123, false);
  event_metric1->RecordMany(34, 1, "12", "34", 123, false);
  event_metric1->RecordMany(56, 2, "56", "78", 456, true);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(58, 3))));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(112, 2))));
}

TEST_F(EventMetricTest, RecordMany2) {
  event_metric2->RecordMany(12, 2, "12", "34", 123, false);
  event_metric2->RecordMany(34, 1, "12", "34", 123, false);
  event_metric2->RecordMany(56, 2, "56", "78", 456, true);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(58, 3))));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(112, 2))));
}

TEST_F(EventMetricTest, RecordMany3) {
  event_metric3->RecordMany(12, 2, "12", "34", 123, false);
  event_metric3->RecordMany(34, 1, "12", "34", 123, false);
  event_metric3->RecordMany(56, 2, "56", "78", 456, true);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(58, 3))));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(112, 2))));
}

TEST_F(EventMetricTest, RecordMany4) {
  event_metric4->RecordMany(12, 2, 123, false);
  event_metric4->RecordMany(34, 1, 123, false);
  event_metric4->RecordMany(56, 2, 456, true);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(58, 3))));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(112, 2))));
}

TEST_F(EventMetricTest, RecordMany5) {
  event_metric5->RecordMany(12, 2, 123, false);
  event_metric5->RecordMany(34, 1, 123, false);
  event_metric5->RecordMany(56, 2, 456, true);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(58, 3))));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(112, 2))));
}

TEST_F(EventMetricTest, RecordMany6) {
  event_metric6->RecordMany(12, 2, 123, false);
  event_metric6->RecordMany(34, 1, 123, false);
  event_metric6->RecordMany(56, 2, 456, true);
  EXPECT_THAT(reader2_.Read(123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(58, 3))));
  EXPECT_THAT(reader2_.Read(456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(112, 2))));
}

TEST_F(EventMetricTest, RecordMany7) {
  event_metric7->RecordMany(12, 2, 123, false);
  event_metric7->RecordMany(34, 1, 123, false);
  event_metric7->RecordMany(56, 2, 456, true);
  EXPECT_THAT(reader2_.Read(123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(58, 3))));
  EXPECT_THAT(reader2_.Read(456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(112, 2))));
}

TEST_F(EventMetricTest, Delete1) {
  event_metric1->Record(12, "12", "34", 123, false);
  event_metric1->Record(34, "56", "78", 456, true);
  event_metric1->Delete("12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(34, 1))));
}

TEST_F(EventMetricTest, Delete2) {
  event_metric2->Record(12, "12", "34", 123, false);
  event_metric2->Record(34, "56", "78", 456, true);
  event_metric2->Delete("12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(34, 1))));
}

TEST_F(EventMetricTest, Delete3) {
  event_metric3->Record(12, "12", "34", 123, false);
  event_metric3->Record(34, "56", "78", 456, true);
  event_metric3->Delete("12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(34, 1))));
}

TEST_F(EventMetricTest, Delete4) {
  event_metric4->Record(12, 123, false);
  event_metric4->Record(34, 456, true);
  event_metric4->Delete(123, false);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(34, 1))));
}

TEST_F(EventMetricTest, Delete5) {
  event_metric5->Record(12, 123, false);
  event_metric5->Record(34, 456, true);
  event_metric5->Delete(123, false);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(34, 1))));
}

TEST_F(EventMetricTest, Delete6) {
  event_metric6->Record(12, 123, false);
  event_metric6->Record(34, 456, true);
  event_metric6->Delete(123, false);
  EXPECT_THAT(reader2_.Read(123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader2_.Read(456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(34, 1))));
}

TEST_F(EventMetricTest, Delete7) {
  event_metric7->Record(12, 123, false);
  event_metric7->Record(34, 456, true);
  event_metric7->Delete(123, false);
  EXPECT_THAT(reader2_.Read(123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader2_.Read(456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(34, 1))));
}

TEST_F(EventMetricTest, RecordAgain1) {
  event_metric1->Record(12, "12", "34", 123, false);
  event_metric1->Record(34, "56", "78", 456, true);
  event_metric1->Delete("12", "34", 123, false);
  event_metric1->Record(56, "12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(34, 1))));
}

TEST_F(EventMetricTest, RecordAgain2) {
  event_metric2->Record(12, "12", "34", 123, false);
  event_metric2->Record(34, "56", "78", 456, true);
  event_metric2->Delete("12", "34", 123, false);
  event_metric2->Record(56, "12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(34, 1))));
}

TEST_F(EventMetricTest, RecordAgain3) {
  event_metric3->Record(12, "12", "34", 123, false);
  event_metric3->Record(34, "56", "78", 456, true);
  event_metric3->Delete("12", "34", 123, false);
  event_metric3->Record(56, "12", "34", 123, false);
  EXPECT_THAT(reader1_.Read("12", "34", 123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(34, 1))));
}

TEST_F(EventMetricTest, RecordAgain4) {
  event_metric4->Record(12, 123, false);
  event_metric4->Record(34, 456, true);
  event_metric4->Delete(123, false);
  event_metric4->Record(56, 123, false);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(34, 1))));
}

TEST_F(EventMetricTest, RecordAgain5) {
  event_metric5->Record(12, 123, false);
  event_metric5->Record(34, 456, true);
  event_metric5->Delete(123, false);
  event_metric5->Record(56, 123, false);
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(34, 1))));
}

TEST_F(EventMetricTest, RecordAgain6) {
  event_metric6->Record(12, 123, false);
  event_metric6->Record(34, 456, true);
  event_metric6->Delete(123, false);
  event_metric6->Record(56, 123, false);
  EXPECT_THAT(reader2_.Read(123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
  EXPECT_THAT(reader2_.Read(456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(34, 1))));
}

TEST_F(EventMetricTest, RecordAgain7) {
  event_metric7->Record(12, 123, false);
  event_metric7->Record(34, 456, true);
  event_metric7->Delete(123, false);
  event_metric7->Record(56, 123, false);
  EXPECT_THAT(reader2_.Read(123, false),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
  EXPECT_THAT(reader2_.Read(456, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(34, 1))));
}

TEST_F(EventMetricTest, Clear1) {
  event_metric1->Record(12, "12", "34", 123, false);
  event_metric1->Record(34, "56", "78", 456, true);
  event_metric1->Clear();
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EventMetricTest, Clear2) {
  event_metric2->Record(12, "12", "34", 123, false);
  event_metric2->Record(34, "56", "78", 456, true);
  event_metric2->Clear();
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EventMetricTest, Clear3) {
  event_metric3->Record(12, "12", "34", 123, false);
  event_metric3->Record(34, "56", "78", 456, true);
  event_metric3->Clear();
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EventMetricTest, Clear4) {
  event_metric4->Record(12, 123, false);
  event_metric4->Record(34, 456, true);
  event_metric4->Clear();
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EventMetricTest, Clear5) {
  event_metric5->Record(12, 123, false);
  event_metric5->Record(34, 456, true);
  event_metric5->Clear();
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 123, false),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("lorem_value", "ipsum_value", 456, true),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EventMetricTest, Clear6) {
  event_metric6->Record(12, 123, false);
  event_metric6->Record(34, 456, true);
  event_metric6->Clear();
  EXPECT_THAT(reader2_.Read(123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader2_.Read(456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EventMetricTest, Clear7) {
  event_metric7->Record(12, 123, false);
  event_metric7->Record(34, 456, true);
  event_metric7->Clear();
  EXPECT_THAT(reader2_.Read(123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader2_.Read(456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(EventMetricTest, DeleteEntity1) {
  event_metric1->Record(12, "12", "34", 123, false);
  event_metric1->Record(34, "12", "34", 456, true);
  event_metric1->Record(56, "56", "78", 789, true);
  event_metric1->DeleteEntity("12", "34");
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("12", "34", 456, true), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 789, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
}

TEST_F(EventMetricTest, DeleteEntity2) {
  event_metric2->Record(12, "12", "34", 123, false);
  event_metric2->Record(34, "12", "34", 456, true);
  event_metric2->Record(56, "56", "78", 789, true);
  event_metric2->DeleteEntity("12", "34");
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("12", "34", 456, true), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 789, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
}

TEST_F(EventMetricTest, DeleteEntity3) {
  event_metric3->Record(12, "12", "34", 123, false);
  event_metric3->Record(34, "12", "34", 456, true);
  event_metric3->Record(56, "56", "78", 789, true);
  event_metric3->DeleteEntity("12", "34");
  EXPECT_THAT(reader1_.Read("12", "34", 123, false), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("12", "34", 456, true), StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(reader1_.Read("56", "78", 789, true),
              IsOkAndHolds(AllOf(DistributionBucketerIs(Bucketer::Default()),
                                 DistributionSumAndCountAre(56, 1))));
}

}  // namespace
