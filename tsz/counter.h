// This unit provides `tsz::Counter`, a cumulative integer metric.
//
// Similarly to other tsz metrics, a `Counter` can be defined in many different ways. The following
// examples define a series of equivalent metrics with two string entity labels, one integer metric
// field, and one boolean metric field:
//
//   char constexpr kLoremName[] = "lorem";
//   char constexpr kIpsumName[] = "ipsum";
//   char constexpr kFooName[] = "foo";
//   char constexpr kBarName[] = "bar";
//
//   // with entity labels, metric fields, and type names:
//   tsz::NoDestructor<tsz::Counter<
//       tsz::EntityLabels<
//           tsz::Field<std::string, kLoremName>,   // 1st entity label
//           tsz::Field<std::string, kIpsumName>>,  // 2nd entity label
//       tsz::MetricFields<
//           tsz::Field<int, kFooName>,             // 1st metric field
//           tsz::Field<bool, kBarName>>>>          // 2nd metric field
//       counter1{
//           "/lorem/ipsum",                        // metric name
//       };
//
//   // with entity labels, metric fields, parameter names, and implicit Field tags:
//   tsz::NoDestructor<tsz::Counter<
//       tsz::EntityLabels<
//           std::string,     // 1st entity label
//           std::string>,    // 2nd entity label
//       tsz::MetricFields<
//           int,             // 1st metric field
//           bool>>>          // 2nd metric field
//       counter3{
//           "/lorem/ipsum",  // metric name
//           "lorem",         // name of the 1st entity label
//           "ipsum",         // name of the 2nd entity label
//           "foo",           // name of the 1st metric field
//           "bar",           // name of the 2nd metric field
//       };
//
//   // with metric fields only, implicit Field tags, and parameter names:
//   tsz::NoDestructor<tsz::Counter<
//       int,                 // 1st metric field
//       bool>>               // 2nd metric field
//       counter4{
//           "/lorem/ipsum",  // metric name
//           "foo",           // name of the 1st metric field
//           "bar",           // name of the 2nd metric field
//       };
//
//   // with metric fields only, type names, in the default entity:
//   tsz::NoDestructor<tsz::Counter<
//       tsz::Field<int, kFooName>,    // 1st metric field
//       tsz::Field<bool, kBarName>>>  // 2nd metric field
//       counter4{
//           "/lorem/ipsum",           // metric name
//       };
//
//   // with metric fields only, parameter names, in the default entity:
//   tsz::NoDestructor<tsz::Counter<
//       int,                 // 1st metric field
//       bool>>               // 2nd metric field
//       counter5{
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
//   tsz::NoDestructor<tsz::Counter<
//       tsz::Field<int, kFooName>,    // 1st metric field
//       tsz::Field<bool, kBarName>>>  // 2nd metric field
//       counter6{
//           entity,                   // refers to `entity`
//           "/lorem/ipsum",           // metric name
//       };
//
//   // with metric fields only, parameter names, referring to a specific entity:
//   tsz::NoDestructor<tsz::Counter<
//       int,                 // 1st metric field
//       bool>>               // 2nd metric field
//       counter7{
//           entity,          // refers to `entity`
//           "/lorem/ipsum",  // metric name
//           "foo",           // name of 1st metric field
//           "bar",           // name of 2nd metric field
//       };
//
// WARNING: in the last two forms the `entity` object MUST outlive all metrics associated to it.
#ifndef __TSDB2_TSZ_COUNTER_H__
#define __TSDB2_TSZ_COUNTER_H__

#include <cstdint>
#include <utility>

#include "tsz/base.h"
#include "tsz/base_metric.h"

namespace tsz {

template <typename... MetricFieldArgs>
class Counter : private BaseMetric<int64_t, MetricFieldArgs...> {
 private:
  using Base = BaseMetric<int64_t, MetricFieldArgs...>;

 public:
  using MetricFields = typename Base::MetricFields;

  template <typename... Args>
  explicit Counter(Args &&...args) : Base(std::forward<Args>(args)...) {}

  using Base::entity_labels;
  using Base::metric_field_names;
  using Base::metric_fields;
  using Base::name;
  using Base::options;

  void IncrementBy(int64_t const delta, ParameterFieldTypeT<MetricFieldArgs> const... args) {
    Base::proxy().AddToInt(metric_fields().MakeFieldMap(args...), delta);
  }

  void Increment(ParameterFieldTypeT<MetricFieldArgs> const... args) {
    Base::proxy().AddToInt(metric_fields().MakeFieldMap(args...), 1);
  }

  void Delete(ParameterFieldTypeT<MetricFieldArgs> const... args) {
    Base::proxy().DeleteValue(metric_fields().MakeFieldMap(args...));
  }

  void Clear() { Base::proxy().Clear(); }
};

template <typename... EntityLabelArgs, typename... MetricFieldArgs>
class Counter<EntityLabels<EntityLabelArgs...>, MetricFields<MetricFieldArgs...>>
    : private BaseMetric<int64_t, EntityLabels<EntityLabelArgs...>,
                         MetricFields<MetricFieldArgs...>> {
 private:
  using Base =
      BaseMetric<int64_t, EntityLabels<EntityLabelArgs...>, MetricFields<MetricFieldArgs...>>;

 public:
  using EntityLabels = typename Base::EntityLabels;
  using MetricFields = typename Base::MetricFields;

  template <typename... Args>
  explicit Counter(Args &&...args) : Base(std::forward<Args>(args)...) {}

  using Base::entity_label_names;
  using Base::entity_labels;
  using Base::metric_field_names;
  using Base::metric_fields;
  using Base::name;
  using Base::options;

  void IncrementBy(int64_t const delta,
                   ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values,
                   ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values) {
    auto const shard = Base::shard();
    if (shard) {
      shard->AddToInt(entity_labels().MakeFieldMap(entity_label_values...), name(),
                      metric_fields().MakeFieldMap(metric_field_values...), delta);
    }
  }

  void Increment(ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values,
                 ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values) {
    auto const shard = Base::shard();
    if (shard) {
      shard->AddToInt(entity_labels().MakeFieldMap(entity_label_values...), name(),
                      metric_fields().MakeFieldMap(metric_field_values...), 1);
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

#endif  // __TSDB2_TSZ_COUNTER_H__
