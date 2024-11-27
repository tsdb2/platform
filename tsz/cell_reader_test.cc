#include "tsz/cell_reader.h"

#include <cstdint>
#include <string>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tsz/internal/exporter.h"
#include "tsz/internal/shard.h"
#include "tsz/types.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::DoubleNear;
using ::tsz::BoolValue;
using ::tsz::DoubleValue;
using ::tsz::FieldMap;
using ::tsz::IntValue;
using ::tsz::StringValue;
using ::tsz::internal::Shard;
using ::tsz::testing::CellReader;

using ::tsz::internal::exporter;

std::string_view constexpr kMetricName = "/lorem/ipsum";
std::string_view constexpr kUndefinedMetricName = "/lorem/dolor";

char constexpr kFooLabel[] = "foo";
char constexpr kBazLabel[] = "baz";
char constexpr kLoremField[] = "lorem";
char constexpr kIpsumField[] = "ipsum";

class CellReaderTest : public ::testing::Test {
 protected:
  static Shard* DefineMetric() {
    auto const status_or_shard = exporter->DefineMetricRedundant(kMetricName, /*options=*/{});
    if (status_or_shard.ok()) {
      return status_or_shard.value();
    } else {
      LOG(ERROR) << "Failed to define metric \"" << kMetricName
                 << "\": " << status_or_shard.status();
      return nullptr;
    }
  }

  explicit CellReaderTest() : shard_(DefineMetric()) {}

  ~CellReaderTest() {
    if (shard_) {
      shard_->DeleteMetric(entity_labels_, kMetricName);
    }
  }

  Shard* const shard_;

  FieldMap const entity_labels_{
      {"foo", StringValue("bar")},
      {"baz", IntValue(123)},
  };
};

TEST_F(CellReaderTest, Read) {
  ASSERT_NE(shard_, nullptr);
  shard_->SetValue(entity_labels_, kMetricName,
                   FieldMap{
                       {"lorem", IntValue(456)},
                       {"ipsum", BoolValue(true)},
                   },
                   DoubleValue(3.14));
  CellReader<double,
             tsz::EntityLabels<tsz::Field<std::string, kFooLabel>, tsz::Field<int, kBazLabel>>,
             tsz::MetricFields<tsz::Field<int, kLoremField>, tsz::Field<bool, kIpsumField>>>
      reader{kMetricName};
  EXPECT_THAT(reader.Read("bar", 123, 456, true), IsOkAndHolds(DoubleNear(3.14, 0.001)));
}

TEST_F(CellReaderTest, ReadMissing) {
  CellReader<double,
             tsz::EntityLabels<tsz::Field<std::string, kFooLabel>, tsz::Field<int, kBazLabel>>,
             tsz::MetricFields<tsz::Field<int, kLoremField>, tsz::Field<bool, kIpsumField>>>
      reader{kMetricName};
  EXPECT_THAT(reader.Read("bar", 123, 456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(CellReaderTest, ReadUndefinedMetric) {
  ASSERT_NE(shard_, nullptr);
  shard_->SetValue(entity_labels_, kUndefinedMetricName,
                   FieldMap{
                       {"lorem", IntValue(456)},
                       {"ipsum", BoolValue(true)},
                   },
                   DoubleValue(3.14));
  CellReader<double,
             tsz::EntityLabels<tsz::Field<std::string, kFooLabel>, tsz::Field<int, kBazLabel>>,
             tsz::MetricFields<tsz::Field<int, kLoremField>, tsz::Field<bool, kIpsumField>>>
      reader{kUndefinedMetricName};
  EXPECT_THAT(reader.Read("bar", 123, 456, true), StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(CellReaderTest, Delta) {
  ASSERT_NE(shard_, nullptr);
  shard_->AddToInt(entity_labels_, kMetricName,
                   FieldMap{
                       {"lorem", IntValue(456)},
                       {"ipsum", BoolValue(true)},
                   },
                   IntValue(42));
  CellReader<int64_t,
             tsz::EntityLabels<tsz::Field<std::string, kFooLabel>, tsz::Field<int, kBazLabel>>,
             tsz::MetricFields<tsz::Field<int, kLoremField>, tsz::Field<bool, kIpsumField>>>
      reader{kMetricName};
  EXPECT_THAT(reader.Delta("bar", 123, 456, true), IsOkAndHolds(42));
  EXPECT_THAT(reader.Delta("bar", 123, 456, true), IsOkAndHolds(0));
  EXPECT_THAT(reader.Delta("bar", 123, 456, true), IsOkAndHolds(0));
}

TEST_F(CellReaderTest, MissingDelta) {
  ASSERT_NE(shard_, nullptr);
  CellReader<int64_t,
             tsz::EntityLabels<tsz::Field<std::string, kFooLabel>, tsz::Field<int, kBazLabel>>,
             tsz::MetricFields<tsz::Field<int, kLoremField>, tsz::Field<bool, kIpsumField>>>
      reader{kMetricName};
  EXPECT_THAT(reader.Delta("bar", 123, 456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(CellReaderTest, SecondDelta) {
  ASSERT_NE(shard_, nullptr);
  FieldMap const metric_fields{
      {"lorem", IntValue(456)},
      {"ipsum", BoolValue(true)},
  };
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields, IntValue(42));
  CellReader<int64_t,
             tsz::EntityLabels<tsz::Field<std::string, kFooLabel>, tsz::Field<int, kBazLabel>>,
             tsz::MetricFields<tsz::Field<int, kLoremField>, tsz::Field<bool, kIpsumField>>>
      reader{kMetricName};
  ASSERT_THAT(reader.Delta("bar", 123, 456, true), IsOkAndHolds(42));
  ASSERT_THAT(reader.Delta("bar", 123, 456, true), IsOkAndHolds(0));
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields, IntValue(43));
  EXPECT_THAT(reader.Delta("bar", 123, 456, true), IsOkAndHolds(43));
  EXPECT_THAT(reader.Delta("bar", 123, 456, true), IsOkAndHolds(0));
}

TEST_F(CellReaderTest, DoubleDelta) {
  ASSERT_NE(shard_, nullptr);
  FieldMap const metric_fields{
      {"lorem", IntValue(456)},
      {"ipsum", BoolValue(true)},
  };
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields, IntValue(42));
  CellReader<int64_t,
             tsz::EntityLabels<tsz::Field<std::string, kFooLabel>, tsz::Field<int, kBazLabel>>,
             tsz::MetricFields<tsz::Field<int, kLoremField>, tsz::Field<bool, kIpsumField>>>
      reader{kMetricName};
  ASSERT_THAT(reader.Delta("bar", 123, 456, true), IsOkAndHolds(42));
  ASSERT_THAT(reader.Delta("bar", 123, 456, true), IsOkAndHolds(0));
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields, IntValue(43));
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields, IntValue(44));
  EXPECT_THAT(reader.Delta("bar", 123, 456, true), IsOkAndHolds(87));
  EXPECT_THAT(reader.Delta("bar", 123, 456, true), IsOkAndHolds(0));
}

TEST_F(CellReaderTest, DeletedDelta) {
  ASSERT_NE(shard_, nullptr);
  FieldMap const metric_fields{
      {"lorem", IntValue(456)},
      {"ipsum", BoolValue(true)},
  };
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields, IntValue(42));
  CellReader<int64_t,
             tsz::EntityLabels<tsz::Field<std::string, kFooLabel>, tsz::Field<int, kBazLabel>>,
             tsz::MetricFields<tsz::Field<int, kLoremField>, tsz::Field<bool, kIpsumField>>>
      reader{kMetricName};
  ASSERT_THAT(reader.Delta("bar", 123, 456, true), IsOkAndHolds(42));
  shard_->DeleteValue(entity_labels_, kMetricName, metric_fields);
  EXPECT_THAT(reader.Delta("bar", 123, 456, true), StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(CellReaderTest, DeltaTracking) {
  ASSERT_NE(shard_, nullptr);
  FieldMap const metric_fields1{
      {"lorem", IntValue(456)},
      {"ipsum", BoolValue(true)},
  };
  FieldMap const metric_fields2{
      {"lorem", IntValue(789)},
      {"ipsum", BoolValue(false)},
  };
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields1, IntValue(12));
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields2, IntValue(34));
  CellReader<int64_t,
             tsz::EntityLabels<tsz::Field<std::string, kFooLabel>, tsz::Field<int, kBazLabel>>,
             tsz::MetricFields<tsz::Field<int, kLoremField>, tsz::Field<bool, kIpsumField>>>
      reader{kMetricName};
  EXPECT_THAT(reader.Delta("bar", 123, 456, true), IsOkAndHolds(12));
  EXPECT_THAT(reader.Delta("bar", 123, 789, false), IsOkAndHolds(34));
  EXPECT_THAT(reader.Delta("bar", 123, 456, true), IsOkAndHolds(0));
  EXPECT_THAT(reader.Delta("bar", 123, 789, false), IsOkAndHolds(0));
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields1, IntValue(56));
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields1, IntValue(78));
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields2, IntValue(90));
  EXPECT_THAT(reader.Delta("bar", 123, 456, true), IsOkAndHolds(134));
  EXPECT_THAT(reader.Delta("bar", 123, 789, false), IsOkAndHolds(90));
  EXPECT_THAT(reader.Delta("bar", 123, 456, true), IsOkAndHolds(0));
  EXPECT_THAT(reader.Delta("bar", 123, 789, false), IsOkAndHolds(0));
}

TEST_F(CellReaderTest, DeltaOrZero) {
  ASSERT_NE(shard_, nullptr);
  shard_->AddToInt(entity_labels_, kMetricName,
                   FieldMap{
                       {"lorem", IntValue(456)},
                       {"ipsum", BoolValue(true)},
                   },
                   IntValue(42));
  CellReader<int64_t,
             tsz::EntityLabels<tsz::Field<std::string, kFooLabel>, tsz::Field<int, kBazLabel>>,
             tsz::MetricFields<tsz::Field<int, kLoremField>, tsz::Field<bool, kIpsumField>>>
      reader{kMetricName};
  EXPECT_EQ(reader.DeltaOrZero("bar", 123, 456, true), 42);
  EXPECT_EQ(reader.DeltaOrZero("bar", 123, 456, true), 0);
  EXPECT_EQ(reader.DeltaOrZero("bar", 123, 456, true), 0);
  EXPECT_EQ(reader.DeltaOrZero("bar", 123, 789, false), 0);
}

TEST_F(CellReaderTest, MissingDeltaOrZero) {
  ASSERT_NE(shard_, nullptr);
  CellReader<int64_t,
             tsz::EntityLabels<tsz::Field<std::string, kFooLabel>, tsz::Field<int, kBazLabel>>,
             tsz::MetricFields<tsz::Field<int, kLoremField>, tsz::Field<bool, kIpsumField>>>
      reader{kMetricName};
  EXPECT_EQ(reader.DeltaOrZero("bar", 123, 456, true), 0);
  EXPECT_EQ(reader.DeltaOrZero("bar", 123, 789, false), 0);
}

TEST_F(CellReaderTest, SecondDeltaOrZero) {
  ASSERT_NE(shard_, nullptr);
  FieldMap const metric_fields{
      {"lorem", IntValue(456)},
      {"ipsum", BoolValue(true)},
  };
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields, IntValue(42));
  CellReader<int64_t,
             tsz::EntityLabels<tsz::Field<std::string, kFooLabel>, tsz::Field<int, kBazLabel>>,
             tsz::MetricFields<tsz::Field<int, kLoremField>, tsz::Field<bool, kIpsumField>>>
      reader{kMetricName};
  ASSERT_EQ(reader.DeltaOrZero("bar", 123, 456, true), 42);
  ASSERT_EQ(reader.DeltaOrZero("bar", 123, 456, true), 0);
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields, IntValue(43));
  EXPECT_EQ(reader.DeltaOrZero("bar", 123, 456, true), 43);
  EXPECT_EQ(reader.DeltaOrZero("bar", 123, 456, true), 0);
}

TEST_F(CellReaderTest, DoubleDeltaOrZero) {
  ASSERT_NE(shard_, nullptr);
  FieldMap const metric_fields{
      {"lorem", IntValue(456)},
      {"ipsum", BoolValue(true)},
  };
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields, IntValue(42));
  CellReader<int64_t,
             tsz::EntityLabels<tsz::Field<std::string, kFooLabel>, tsz::Field<int, kBazLabel>>,
             tsz::MetricFields<tsz::Field<int, kLoremField>, tsz::Field<bool, kIpsumField>>>
      reader{kMetricName};
  ASSERT_EQ(reader.DeltaOrZero("bar", 123, 456, true), 42);
  ASSERT_EQ(reader.DeltaOrZero("bar", 123, 456, true), 0);
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields, IntValue(43));
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields, IntValue(44));
  EXPECT_EQ(reader.DeltaOrZero("bar", 123, 456, true), 87);
  EXPECT_EQ(reader.DeltaOrZero("bar", 123, 456, true), 0);
}

TEST_F(CellReaderTest, DeletedDeltaOrZero) {
  ASSERT_NE(shard_, nullptr);
  FieldMap const metric_fields{
      {"lorem", IntValue(456)},
      {"ipsum", BoolValue(true)},
  };
  shard_->AddToInt(entity_labels_, kMetricName, metric_fields, IntValue(42));
  CellReader<int64_t,
             tsz::EntityLabels<tsz::Field<std::string, kFooLabel>, tsz::Field<int, kBazLabel>>,
             tsz::MetricFields<tsz::Field<int, kLoremField>, tsz::Field<bool, kIpsumField>>>
      reader{kMetricName};
  ASSERT_EQ(reader.DeltaOrZero("bar", 123, 456, true), 42);
  shard_->DeleteValue(entity_labels_, kMetricName, metric_fields);
  EXPECT_EQ(reader.DeltaOrZero("bar", 123, 456, true), 0);
}

}  // namespace
