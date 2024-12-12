#include "tsz/latency_recorder.h"

#include <string>
#include <string_view>

#include "absl/status/status_matchers.h"
#include "absl/time/time.h"
#include "common/mock_clock.h"
#include "common/no_destructor.h"
#include "common/scoped_override.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tsz/base.h"
#include "tsz/cell_reader.h"
#include "tsz/distribution_testing.h"
#include "tsz/entity.h"
#include "tsz/event_metric.h"

namespace tsz {

namespace {

using ::absl_testing::IsOkAndHolds;
using ::testing::DoubleNear;
using ::tsdb2::common::MockClock;
using ::tsdb2::common::NoDestructor;
using ::tsdb2::common::ScopedOverride;
using ::tsz::testing::CellReader;
using ::tsz::testing::DistributionSumAndCountAre;

std::string_view constexpr kMetricName = "/lorem/ipsum";

char constexpr kLoremLabel[] = "lorem";
char constexpr kIpsumLabel[] = "ipsum";
char constexpr kFooField[] = "foo";
char constexpr kBarField[] = "bar";

using TestEntityLabels =
    tsz::EntityLabels<tsz::Field<std::string, kLoremLabel>, tsz::Field<std::string, kIpsumLabel>>;

using TestMetricFields = tsz::MetricFields<tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>;

NoDestructor<tsz::Entity<tsz::Field<std::string, kLoremLabel>,
                         tsz::Field<std::string, kIpsumLabel>>> const entity{
    "lorem_value",
    "ipsum_value",
};

NoDestructor<tsz::EventMetric<TestEntityLabels, TestMetricFields>> event_metric1{
    kMetricName, tsz::Options{.time_unit = tsz::TimeUnit::kSecond}};

NoDestructor<tsz::EventMetric<tsz::Field<int, kFooField>, tsz::Field<bool, kBarField>>>
    event_metric2{*entity, kMetricName, tsz::Options{.time_unit = tsz::TimeUnit::kSecond}};

}  // namespace

class LatencyRecorderTest : public ::testing::Test {
 protected:
  MockClock clock_{absl::UnixEpoch() + absl::Seconds(123)};
  tsz::CellReader<tsz::Distribution, TestEntityLabels, TestMetricFields> metric_reader_{
      kMetricName};
};

TEST_F(LatencyRecorderTest, Getters) {
  using LatencyRecorder =
      tsz::LatencyRecorder<tsz::TimeUnit::kSecond, TestEntityLabels, TestMetricFields>;
  ScopedOverride override_clock{&LatencyRecorder::clock_, &clock_};
  {
    LatencyRecorder recorder{event_metric1.Get(), "lorem_value", "ipsum_value", 42, true};
    EXPECT_EQ(recorder.metric(), event_metric1.Get());
    EXPECT_EQ(recorder.start_time(), absl::UnixEpoch() + absl::Seconds(123));
  }
  clock_.AdvanceTime(absl::Seconds(456));
  {
    LatencyRecorder recorder{event_metric1.Get(), "lorem_value", "ipsum_value", 42, true};
    EXPECT_EQ(recorder.metric(), event_metric1.Get());
    EXPECT_EQ(recorder.start_time(), absl::UnixEpoch() + absl::Seconds(579));
  }
}

TEST_F(LatencyRecorderTest, RecordLatency) {
  using LatencyRecorder =
      tsz::LatencyRecorder<tsz::TimeUnit::kSecond, TestEntityLabels, TestMetricFields>;
  {
    ScopedOverride override_clock{&LatencyRecorder::clock_, &clock_};
    LatencyRecorder recorder{event_metric1.Get(), "lorem_value", "ipsum_value", 42, true};
    clock_.AdvanceTime(absl::Seconds(456));
  }
  EXPECT_THAT(metric_reader_.Read("lorem_value", "ipsum_value", 42, true),
              IsOkAndHolds(DistributionSumAndCountAre(DoubleNear(456, 0.001), 1)));
}

TEST_F(LatencyRecorderTest, RecordLatencies) {
  using LatencyRecorder =
      tsz::LatencyRecorder<tsz::TimeUnit::kSecond, TestEntityLabels, TestMetricFields>;
  {
    ScopedOverride override_clock{&LatencyRecorder::clock_, &clock_};
    {
      LatencyRecorder recorder{event_metric1.Get(), "lorem_value", "ipsum_value", 42, true};
      clock_.AdvanceTime(absl::Seconds(12));
    }
    {
      LatencyRecorder recorder{event_metric1.Get(), "ipsum_value", "lorem_value", 43, false};
      clock_.AdvanceTime(absl::Seconds(34));
    }
    {
      LatencyRecorder recorder{event_metric1.Get(), "lorem_value", "ipsum_value", 42, true};
      clock_.AdvanceTime(absl::Seconds(56));
    }
  }
  EXPECT_THAT(metric_reader_.Read("lorem_value", "ipsum_value", 42, true),
              IsOkAndHolds(DistributionSumAndCountAre(DoubleNear(68, 0.001), 2)));
  EXPECT_THAT(metric_reader_.Read("ipsum_value", "lorem_value", 43, false),
              IsOkAndHolds(DistributionSumAndCountAre(DoubleNear(34, 0.001), 1)));
}

TEST_F(LatencyRecorderTest, RecordNestedLatencies) {
  using LatencyRecorder =
      tsz::LatencyRecorder<tsz::TimeUnit::kSecond, TestEntityLabels, TestMetricFields>;
  {
    ScopedOverride override_clock{&LatencyRecorder::clock_, &clock_};
    LatencyRecorder recorder1{event_metric1.Get(), "lorem_value", "ipsum_value", 42, true};
    clock_.AdvanceTime(absl::Seconds(12));
    LatencyRecorder recorder2{event_metric1.Get(), "ipsum_value", "lorem_value", 43, false};
    clock_.AdvanceTime(absl::Seconds(34));
  }
  EXPECT_THAT(metric_reader_.Read("lorem_value", "ipsum_value", 42, true),
              IsOkAndHolds(DistributionSumAndCountAre(DoubleNear(46, 0.001), 1)));
  EXPECT_THAT(metric_reader_.Read("ipsum_value", "lorem_value", 43, false),
              IsOkAndHolds(DistributionSumAndCountAre(DoubleNear(34, 0.001), 1)));
}

TEST_F(LatencyRecorderTest, RecordNestedCollidingLatencies) {
  using LatencyRecorder =
      tsz::LatencyRecorder<tsz::TimeUnit::kSecond, TestEntityLabels, TestMetricFields>;
  {
    ScopedOverride override_clock{&LatencyRecorder::clock_, &clock_};
    LatencyRecorder recorder1{event_metric1.Get(), "lorem_value", "ipsum_value", 42, true};
    clock_.AdvanceTime(absl::Seconds(12));
    LatencyRecorder recorder2{event_metric1.Get(), "lorem_value", "ipsum_value", 42, true};
    clock_.AdvanceTime(absl::Seconds(34));
  }
  EXPECT_THAT(metric_reader_.Read("lorem_value", "ipsum_value", 42, true),
              IsOkAndHolds(DistributionSumAndCountAre(DoubleNear(80, 0.001), 2)));
}

TEST_F(LatencyRecorderTest, GettersInsideEntity) {
  using LatencyRecorder = tsz::LatencyRecorder<tsz::TimeUnit::kSecond, tsz::Field<int, kFooField>,
                                               tsz::Field<bool, kBarField>>;
  ScopedOverride override_clock{&LatencyRecorder::clock_, &clock_};
  {
    LatencyRecorder recorder{event_metric2.Get(), 42, true};
    EXPECT_EQ(recorder.metric(), event_metric2.Get());
    EXPECT_EQ(recorder.start_time(), absl::UnixEpoch() + absl::Seconds(123));
  }
  clock_.AdvanceTime(absl::Seconds(789));
  {
    LatencyRecorder recorder{event_metric2.Get(), 42, true};
    EXPECT_EQ(recorder.metric(), event_metric2.Get());
    EXPECT_EQ(recorder.start_time(), absl::UnixEpoch() + absl::Seconds(912));
  }
}

TEST_F(LatencyRecorderTest, RecordLatencyInsideEntity) {
  using LatencyRecorder = tsz::LatencyRecorder<tsz::TimeUnit::kSecond, tsz::Field<int, kFooField>,
                                               tsz::Field<bool, kBarField>>;
  {
    ScopedOverride override_clock{&LatencyRecorder::clock_, &clock_};
    LatencyRecorder recorder{event_metric2.Get(), 42, true};
    clock_.AdvanceTime(absl::Seconds(789));
  }
  EXPECT_THAT(metric_reader_.Read("lorem_value", "ipsum_value", 42, true),
              IsOkAndHolds(DistributionSumAndCountAre(DoubleNear(789, 0.001), 1)));
}

TEST_F(LatencyRecorderTest, RecordLatenciesInsideEntity) {
  using LatencyRecorder = tsz::LatencyRecorder<tsz::TimeUnit::kSecond, tsz::Field<int, kFooField>,
                                               tsz::Field<bool, kBarField>>;
  {
    ScopedOverride override_clock{&LatencyRecorder::clock_, &clock_};
    {
      LatencyRecorder recorder{event_metric2.Get(), 42, true};
      clock_.AdvanceTime(absl::Seconds(12));
    }
    {
      LatencyRecorder recorder{event_metric2.Get(), 43, false};
      clock_.AdvanceTime(absl::Seconds(34));
    }
    {
      LatencyRecorder recorder{event_metric2.Get(), 42, true};
      clock_.AdvanceTime(absl::Seconds(56));
    }
  }
  EXPECT_THAT(metric_reader_.Read("lorem_value", "ipsum_value", 42, true),
              IsOkAndHolds(DistributionSumAndCountAre(DoubleNear(68, 0.001), 2)));
  EXPECT_THAT(metric_reader_.Read("lorem_value", "ipsum_value", 43, false),
              IsOkAndHolds(DistributionSumAndCountAre(DoubleNear(34, 0.001), 1)));
}

TEST_F(LatencyRecorderTest, RecordNestedLatenciesInsideEntity) {
  using LatencyRecorder = tsz::LatencyRecorder<tsz::TimeUnit::kSecond, tsz::Field<int, kFooField>,
                                               tsz::Field<bool, kBarField>>;
  {
    ScopedOverride override_clock{&LatencyRecorder::clock_, &clock_};
    LatencyRecorder recorder1{event_metric2.Get(), 42, true};
    clock_.AdvanceTime(absl::Seconds(12));
    LatencyRecorder recorder2{event_metric2.Get(), 43, false};
    clock_.AdvanceTime(absl::Seconds(34));
  }
  EXPECT_THAT(metric_reader_.Read("lorem_value", "ipsum_value", 42, true),
              IsOkAndHolds(DistributionSumAndCountAre(DoubleNear(46, 0.001), 1)));
  EXPECT_THAT(metric_reader_.Read("lorem_value", "ipsum_value", 43, false),
              IsOkAndHolds(DistributionSumAndCountAre(DoubleNear(34, 0.001), 1)));
}

TEST_F(LatencyRecorderTest, RecordNestedCollidingLatenciesInsideEntity) {
  using LatencyRecorder = tsz::LatencyRecorder<tsz::TimeUnit::kSecond, tsz::Field<int, kFooField>,
                                               tsz::Field<bool, kBarField>>;
  {
    ScopedOverride override_clock{&LatencyRecorder::clock_, &clock_};
    LatencyRecorder recorder1{event_metric2.Get(), 42, true};
    clock_.AdvanceTime(absl::Seconds(12));
    LatencyRecorder recorder2{event_metric2.Get(), 42, true};
    clock_.AdvanceTime(absl::Seconds(34));
  }
  EXPECT_THAT(metric_reader_.Read("lorem_value", "ipsum_value", 42, true),
              IsOkAndHolds(DistributionSumAndCountAre(DoubleNear(80, 0.001), 2)));
}

}  // namespace tsz
