// This unit provides `LatencyRecorder`, a scoped object that records the time difference between
// its construction and destruction in a provided `EventMetric`.
//
// The first template parameter of the `LatencyRecorder` is the `TimeUnit` of the measured
// latencies, which must correspond to the `time_unit` option specified in the `tsz::Options` of the
// `EventMetric`.
//
// Other template arguments are the entity labels and metric fields types as usual, and must
// correspond to those of the `EventMetric`.
//
// The arguments of the `LatencyRecorder` constructor are a pointer to the `EventMetric` and the
// *values* of the entity labels and metric fields where the measured latency will be stored. Note
// that if the entity label and metric field descriptors have parametric names, the
// `LatencyRecorder` constructor doesn't need to know those names. The typing of the descriptors as
// provided to `EventMetric` is sufficient even if it doesn't include the names.
//
// WARNING: the `EventMetric` MUST outlive all associated `LatencyRecorder`s.
//
// Example usage:
//
//   char constexpr kLoremLabel[] = "lorem";
//   char constexpr kIpsumLabel[] = "ipsum";
//   char constexpr kFooField[] = "foo";
//   char constexpr kBarField[] = "bar";
//
//   using MyEntityLabels = tsz::EntityLabels<
//       tsz::Field<std::string, kLoremLabel>,
//       tsz::Field<std::string, kIpsumLabel>>;
//
//   using MyMetricFields = tsz::MetricFields<
//       tsz::Field<int, kFooField>,
//       tsz::Field<bool, kBarField>>;
//
//   tsz::NoDestructor<tsz::EventMetric<MyEntityLabels, MyMetricFields>> latency_metric{
//       "/foo/latency", tsz::Options{
//           .time_unit = tsz::TimeUnit::kMillisecond,
//       }};
//
//   void Foo() {
//     LatencyRecorder<tsz::TimeUnit::kMillisecond, MyEntityLabels, MyMetricFields> recorder{
//         latency_metric.Get(),
//         "lorem_value",  // value of entity label "lorem"
//         "ipsum_value",  // value of entity label "ipsum"
//         42,             // value of metric field "foo"
//         true            // value of metric field "bar"
//     };
//     DoThis();
//     DoThat();
//   }
//
// The above example runs lengthy operations (`DoThis` and `DoThat`) in the `Foo` function, which
// automatically records the duration of the run in milliseconds and adds it to the `latency_metric`
// before exiting.
#ifndef __TSDB2_TSZ_LATENCY_RECORDER_H__
#define __TSDB2_TSZ_LATENCY_RECORDER_H__

#include <tuple>
#include <utility>

#include "absl/time/time.h"
#include "common/clock.h"
#include "common/singleton.h"
#include "gtest/gtest_prod.h"
#include "tsz/base.h"
#include "tsz/event_metric.h"

namespace tsz {

namespace internal {

template <TimeUnit unit = TimeUnit::kMillisecond>
double LatencyToDouble(absl::Duration latency);

template <>
inline double LatencyToDouble<TimeUnit::kNanosecond>(absl::Duration const latency) {
  return absl::ToDoubleNanoseconds(latency);
}

template <>
inline double LatencyToDouble<TimeUnit::kMicrosecond>(absl::Duration const latency) {
  return absl::ToDoubleMicroseconds(latency);
}

template <>
inline double LatencyToDouble<TimeUnit::kMillisecond>(absl::Duration const latency) {
  return absl::ToDoubleMilliseconds(latency);
}

template <>
inline double LatencyToDouble<TimeUnit::kSecond>(absl::Duration const latency) {
  return absl::ToDoubleSeconds(latency);
}

}  // namespace internal

template <TimeUnit unit, typename... MetricFieldArgs>
class LatencyRecorder final {
 public:
  using EntityLabels = EntityLabels<>;
  using MetricFields = MetricFields<MetricFieldArgs...>;
  using EventMetric = EventMetric<MetricFieldArgs...>;

  explicit LatencyRecorder(EventMetric* const metric, absl::Time const start_time,
                           ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values)
      : metric_(metric), start_time_(start_time), metric_field_values_(metric_field_values...) {}

  explicit LatencyRecorder(EventMetric* const metric,
                           ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values)
      : LatencyRecorder(metric, clock_->TimeNow(), metric_field_values...) {}

  ~LatencyRecorder() {
    if (metric_) {
      double const latency = internal::LatencyToDouble<unit>(clock_->TimeNow() - start_time_);
      auto prefix_tuple = std::make_tuple(metric_, ParameterTypeT<double>{latency});
      auto metric_field_values =
          std::make_from_tuple<std::tuple<ParameterFieldTypeT<MetricFieldArgs>...>>(
              metric_field_values_);
      std::apply(&EventMetric::Record,
                 std::tuple_cat(std::move(prefix_tuple), std::move(metric_field_values)));
    }
  }

  LatencyRecorder(LatencyRecorder&& other) noexcept
      : metric_(other.metric_),
        start_time_(other.start_time_),
        metric_field_values_(std::move(other.metric_field_values_)) {
    other.metric_ = nullptr;
  }

  EventMetric* metric() const { return metric_; }
  absl::Time start_time() const { return start_time_; }

 private:
  LatencyRecorder(LatencyRecorder const&) = delete;
  LatencyRecorder& operator=(LatencyRecorder const&) = delete;
  LatencyRecorder& operator=(LatencyRecorder&&) = delete;

  static tsdb2::common::Singleton<tsdb2::common::Clock const> clock_;

  EventMetric* metric_;
  absl::Time start_time_;
  typename MetricFields::Tuple metric_field_values_;

  FRIEND_TEST(LatencyRecorderTest, GettersInsideEntity);
  FRIEND_TEST(LatencyRecorderTest, RecordLatencyInsideEntity);
  FRIEND_TEST(LatencyRecorderTest, RecordLatenciesInsideEntity);
  FRIEND_TEST(LatencyRecorderTest, RecordNestedLatenciesInsideEntity);
  FRIEND_TEST(LatencyRecorderTest, RecordNestedCollidingLatenciesInsideEntity);
};

template <TimeUnit unit, typename... MetricFieldArgs>
tsdb2::common::Singleton<tsdb2::common::Clock const>
    LatencyRecorder<unit, MetricFieldArgs...>::clock_{
        +[] { return tsdb2::common::RealClock::GetInstance(); }};

template <TimeUnit unit, typename... EntityLabelArgs, typename... MetricFieldArgs>
class LatencyRecorder<unit, EntityLabels<EntityLabelArgs...>, MetricFields<MetricFieldArgs...>>
    final {
 public:
  using EntityLabels = EntityLabels<EntityLabelArgs...>;
  using MetricFields = MetricFields<MetricFieldArgs...>;
  using EventMetric = EventMetric<EntityLabels, MetricFields>;

  explicit LatencyRecorder(EventMetric* const metric, absl::Time const start_time,
                           ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values,
                           ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values)
      : metric_(metric),
        start_time_(start_time),
        entity_label_values_(entity_label_values...),
        metric_field_values_(metric_field_values...) {}

  explicit LatencyRecorder(EventMetric* const metric,
                           ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values,
                           ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values)
      : LatencyRecorder(metric, clock_->TimeNow(), entity_label_values..., metric_field_values...) {
  }

  ~LatencyRecorder() {
    if (metric_) {
      double const latency = internal::LatencyToDouble<unit>(clock_->TimeNow() - start_time_);
      auto prefix_tuple = std::make_tuple(metric_, ParameterTypeT<double>{latency});
      auto entity_label_values =
          std::make_from_tuple<std::tuple<ParameterFieldTypeT<EntityLabelArgs>...>>(
              entity_label_values_);
      auto metric_field_values =
          std::make_from_tuple<std::tuple<ParameterFieldTypeT<MetricFieldArgs>...>>(
              metric_field_values_);
      std::apply(&EventMetric::Record,
                 std::tuple_cat(std::move(prefix_tuple), std::move(entity_label_values),
                                std::move(metric_field_values)));
    }
  }

  LatencyRecorder(LatencyRecorder&& other) noexcept
      : metric_(other.metric_),
        start_time_(other.start_time_),
        entity_label_values_(std::move(other.entity_label_values_)),
        metric_field_values_(std::move(other.metric_field_values_)) {
    other.metric_ = nullptr;
  }

  EventMetric* metric() const { return metric_; }
  absl::Time start_time() const { return start_time_; }

 private:
  LatencyRecorder(LatencyRecorder const&) = delete;
  LatencyRecorder& operator=(LatencyRecorder const&) = delete;
  LatencyRecorder& operator=(LatencyRecorder&&) = delete;

  static tsdb2::common::Singleton<tsdb2::common::Clock const> clock_;

  EventMetric* metric_;
  absl::Time start_time_;

  typename EntityLabels::Tuple entity_label_values_;
  typename MetricFields::Tuple metric_field_values_;

  FRIEND_TEST(LatencyRecorderTest, Getters);
  FRIEND_TEST(LatencyRecorderTest, RecordLatency);
  FRIEND_TEST(LatencyRecorderTest, RecordLatencies);
  FRIEND_TEST(LatencyRecorderTest, RecordNestedLatencies);
  FRIEND_TEST(LatencyRecorderTest, RecordNestedCollidingLatencies);
};

template <TimeUnit unit, typename... EntityLabelArgs, typename... MetricFieldArgs>
tsdb2::common::Singleton<tsdb2::common::Clock const> LatencyRecorder<
    unit, EntityLabels<EntityLabelArgs...>, MetricFields<MetricFieldArgs...>>::clock_{
    +[] { return tsdb2::common::RealClock::GetInstance(); }};

template <typename... Args>
using LatencyRecorderNs = LatencyRecorder<TimeUnit::kNanosecond, Args...>;

template <typename... Args>
using LatencyRecorderUs = LatencyRecorder<TimeUnit::kMicrosecond, Args...>;

template <typename... Args>
using LatencyRecorderMs = LatencyRecorder<TimeUnit::kMillisecond, Args...>;

template <typename... Args>
using LatencyRecorderS = LatencyRecorder<TimeUnit::kSecond, Args...>;

}  // namespace tsz

#endif  // __TSDB2_TSZ_LATENCY_RECORDER_H__
