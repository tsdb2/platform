// This unit provides `tsz::EventMetric`, a cumulative distribution metric where you can record
// double precision numeric samples.
//
// Similarly to other tsz metrics, an `EventMetric` can be defined in many different ways. The
// following examples define a series of equivalent metrics with two string entity labels, one
// integer metric field, and one boolean metric field:
//
//   char constexpr kLoremName[] = "lorem";
//   char constexpr kIpsumName[] = "ipsum";
//   char constexpr kFooName[] = "foo";
//   char constexpr kBarName[] = "bar";
//
//   // with entity labels, metric fields, and type names:
//   tsz::NoDestructor<tsz::EventMetric<
//       tsz::EntityLabels<
//           tsz::Field<std::string, kLoremName>,   // 1st entity label
//           tsz::Field<std::string, kIpsumName>>,  // 2nd entity label
//       tsz::MetricFields<
//           tsz::Field<int, kFooName>,             // 1st metric field
//           tsz::Field<bool, kBarName>>>>          // 2nd metric field
//       event_metric1{
//           "/lorem/ipsum",                        // metric name
//       };
//
//   // with entity labels, metric fields, parameter names, and implicit Field tags:
//   tsz::NoDestructor<tsz::EventMetric<
//       tsz::EntityLabels<
//           std::string,     // 1st entity label
//           std::string>,    // 2nd entity label
//       tsz::MetricFields<
//           int,             // 1st metric field
//           bool>>>          // 2nd metric field
//       event_metric3{
//           "/lorem/ipsum",  // metric name
//           "lorem",         // name of the 1st entity label
//           "ipsum",         // name of the 2nd entity label
//           "foo",           // name of the 1st metric field
//           "bar",           // name of the 2nd metric field
//       };
//
//   // with metric fields only, implicit Field tags, and parameter names:
//   tsz::NoDestructor<tsz::EventMetric<
//       int,                 // 1st metric field
//       bool>>               // 2nd metric field
//       event_metric4{
//           "/lorem/ipsum",  // metric name
//           "foo",           // name of the 1st metric field
//           "bar",           // name of the 2nd metric field
//       };
//
//   // with metric fields only, type names, in the default entity:
//   tsz::NoDestructor<tsz::EventMetric<
//       tsz::Field<int, kFooName>,    // 1st metric field
//       tsz::Field<bool, kBarName>>>  // 2nd metric field
//       event_metric4{
//           "/lorem/ipsum",           // metric name
//       };
//
//   // with metric fields only, parameter names, in the default entity:
//   tsz::NoDestructor<tsz::EventMetric<
//       int,                 // 1st metric field
//       bool>>               // 2nd metric field
//       event_metric5{
//           "/lorem/ipsum",  // metric name
//           "foo",           // name of 1st metric field
//           "bar",           // name of 2nd metric field
//       };
//
//   tsz::Entity<
//       tsz::Field<std::string, kLoremName>,  // 1st entity label
//       tsz::Field<std::string, kIpsumName>>  // 2nd entity label
//       entity;
//
//   // with metric fields only, type names, referring to a specific entity:
//   tsz::NoDestructor<tsz::EventMetric<
//       tsz::Field<int, kFooName>,    // 1st metric field
//       tsz::Field<bool, kBarName>>>  // 2nd metric field
//       event_metric6{
//           entity,                   // refers to `entity`
//           "/lorem/ipsum",           // metric name
//       };
//
//   // with metric fields only, parameter names, referring to a specific entity:
//   tsz::NoDestructor<tsz::EventMetric<
//       int,                 // 1st metric field
//       bool>>               // 2nd metric field
//       event_metric7{
//           entity,          // refers to `entity`
//           "/lorem/ipsum",  // metric name
//           "foo",           // name of 1st metric field
//           "bar",           // name of 2nd metric field
//       };
//
// WARNING: in the last two forms the `entity` object MUST outlive all metrics associated to it.
#ifndef __TSDB2_TSZ_EVENT_METRIC_H__
#define __TSDB2_TSZ_EVENT_METRIC_H__

#include <cstddef>
#include <utility>

#include "tsz/base.h"
#include "tsz/base_metric.h"

namespace tsz {

template <typename... MetricFieldArgs>
class EventMetric : private BaseMetric<Distribution, MetricFieldArgs...> {
 private:
  using Base = BaseMetric<Distribution, MetricFieldArgs...>;

 public:
  using MetricFields = typename Base::MetricFields;

  template <typename... Args>
  explicit EventMetric(Args &&...args) : Base(std::forward<Args>(args)...) {}

  using Base::entity_labels;
  using Base::metric_field_names;
  using Base::metric_fields;
  using Base::name;
  using Base::options;

  void Record(double const sample, ParameterFieldTypeT<MetricFieldArgs> const... args) {
    RecordMany(sample, /*times=*/1, args...);
  }

  void RecordMany(double const sample, size_t const times,
                  ParameterFieldTypeT<MetricFieldArgs> const... args) {
    Base::proxy().AddToDistribution(metric_fields().MakeFieldMap(args...), sample, times);
  }

  void Delete(ParameterFieldTypeT<MetricFieldArgs> const... args) {
    Base::proxy().DeleteValue(metric_fields().MakeFieldMap(args...));
  }

  void Clear() { Base::proxy().Clear(); }
};

template <typename... EntityLabelArgs, typename... MetricFieldArgs>
class EventMetric<EntityLabels<EntityLabelArgs...>, MetricFields<MetricFieldArgs...>>
    : private BaseMetric<Distribution, EntityLabels<EntityLabelArgs...>,
                         MetricFields<MetricFieldArgs...>> {
 private:
  using Base =
      BaseMetric<Distribution, EntityLabels<EntityLabelArgs...>, MetricFields<MetricFieldArgs...>>;

 public:
  using EntityLabels = typename Base::EntityLabels;
  using MetricFields = typename Base::MetricFields;

  template <typename... Args>
  explicit EventMetric(Args &&...args) : Base(std::forward<Args>(args)...) {}

  using Base::entity_label_names;
  using Base::entity_labels;
  using Base::metric_field_names;
  using Base::metric_fields;
  using Base::name;
  using Base::options;

  void Record(double const sample,
              ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values,
              ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values) {
    RecordMany(sample, /*times=*/1, entity_label_values..., metric_field_values...);
  }

  void RecordMany(double const sample, size_t const times,
                  ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values,
                  ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values) {
    auto const shard = Base::shard();
    if (shard) {
      shard->AddToDistribution(entity_labels().MakeFieldMap(entity_label_values...), name(),
                               metric_fields().MakeFieldMap(metric_field_values...), sample, times);
    }
  }

  bool Delete(ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values,
              ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values) {
    auto const shard = Base::shard();
    if (shard) {
      return shard->DeleteValue(entity_labels().MakeFieldMap(entity_label_values...), name(),
                                metric_fields().MakeFieldMap(metric_field_values...));
    } else {
      return false;
    }
  }

  void DeleteEntity(ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values) {
    auto const shard = Base::shard();
    if (shard) {
      shard->DeleteMetric(entity_labels().MakeFieldMap(entity_label_values...), name());
    }
  }

  void Clear() {
    auto const shard = Base::shard();
    if (shard) {
      shard->DeleteMetric(name());
    }
  }
};

}  // namespace tsz

#endif  // __TSDB2_TSZ_EVENT_METRIC_H__
