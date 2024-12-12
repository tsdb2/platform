// This unit provides `tsz::Metric`, a non-cumulative gauge metric with support for many different
// data types.
//
// Similarly to other tsz metrics, a `Metric` can be defined in many different ways. The following
// examples define a series of equivalent metrics with two string entity labels, one integer metric
// field, and one boolean metric field:
//
//   char constexpr kLoremName[] = "lorem";
//   char constexpr kIpsumName[] = "ipsum";
//   char constexpr kFooName[] = "foo";
//   char constexpr kBarName[] = "bar";
//
//   // with entity labels, metric fields, and type names:
//   tsz::NoDestructor<tsz::Metric<
//       int,                                       // value type
//       tsz::EntityLabels<
//           tsz::Field<std::string, kLoremName>,   // 1st entity label
//           tsz::Field<std::string, kIpsumName>>,  // 2nd entity label
//       tsz::MetricFields<
//           tsz::Field<int, kFooName>,             // 1st metric field
//           tsz::Field<bool, kBarName>>>>          // 2nd metric field
//       metric1{
//           "/lorem/ipsum",                        // metric name
//       };
//
//   // with entity labels, metric fields, parameter names, and implicit Field tags:
//   tsz::NoDestructor<tsz::Metric<
//       int,                 // value type
//       tsz::EntityLabels<
//           std::string,     // 1st entity label
//           std::string>,    // 2nd entity label
//       tsz::MetricFields<
//           int,             // 1st metric field
//           bool>>>          // 2nd metric field
//       metric3{
//           "/lorem/ipsum",  // metric name
//           "lorem",         // name of the 1st entity label
//           "ipsum",         // name of the 2nd entity label
//           "foo",           // name of the 1st metric field
//           "bar",           // name of the 2nd metric field
//       };
//
//   // with metric fields only, implicit Field tags, and parameter names:
//   tsz::NoDestructor<tsz::Metric<
//       int,                 // value type
//       int,                 // 1st metric field
//       bool>>               // 2nd metric field
//       metric4{
//           "/lorem/ipsum",  // metric name
//           "foo",           // name of the 1st metric field
//           "bar",           // name of the 2nd metric field
//       };
//
//   // with metric fields only, type names, in the default entity:
//   tsz::NoDestructor<tsz::Metric<
//       int,                          // value type
//       tsz::Field<int, kFooName>,    // 1st metric field
//       tsz::Field<bool, kBarName>>>  // 2nd metric field
//       metric4{
//           "/lorem/ipsum",           // metric name
//       };
//
//   // with metric fields only, parameter names, in the default entity:
//   tsz::NoDestructor<tsz::Metric<
//       int,                 // value type
//       int,                 // 1st metric field
//       bool>>               // 2nd metric field
//       metric5{
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
//   tsz::NoDestructor<tsz::Metric<
//       int,                          // value type
//       tsz::Field<int, kFooName>,    // 1st metric field
//       tsz::Field<bool, kBarName>>>  // 2nd metric field
//       metric6{
//           entity,                   // refers to `entity`
//           "/lorem/ipsum",           // metric name
//       };
//
//   // with metric fields only, parameter names, referring to a specific entity:
//   tsz::NoDestructor<tsz::Metric<
//       int,                 // value type
//       int,                 // 1st metric field
//       bool>>               // 2nd metric field
//       metric7{
//           entity,          // refers to `entity`
//           "/lorem/ipsum",  // metric name
//           "foo",           // name of 1st metric field
//           "bar",           // name of 2nd metric field
//       };
//
// WARNING: in the last two forms the `entity` object MUST outlive all metrics associated to it.
//
// In all the above forms, the first template argument of `tsz::Metric` is the value type of the
// metric. The examples use `int`, but `Metric` supports all of the following:
//
//   * all integral types,
//   * all floating point types,
//   * booleans,
//   * strings,
//   * `Distribution`s.
//
#ifndef __TSDB2_TSZ_METRIC_H__
#define __TSDB2_TSZ_METRIC_H__

#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "tsz/base.h"
#include "tsz/base_metric.h"

namespace tsz {

template <typename Value, typename... MetricFieldArgs>
class Metric : private BaseMetric<Value, MetricFieldArgs...> {
 private:
  using Base = BaseMetric<Value, MetricFieldArgs...>;

 public:
  using MetricFields = typename Base::MetricFields;

  template <typename... Args>
  explicit Metric(Args &&...args) : Base(std::forward<Args>(args)...) {}

  using Base::entity_labels;
  using Base::metric_field_names;
  using Base::metric_fields;
  using Base::name;
  using Base::options;

  void Set(ParameterTypeT<Value> value, ParameterFieldTypeT<MetricFieldArgs> const... args) {
    Base::proxy().SetValue(metric_fields().MakeFieldMap(args...),
                           CanonicalTypeT<Value>(std::move(value)));
  }

  absl::StatusOr<Value> Get(ParameterFieldTypeT<MetricFieldArgs> const... args) const {
    auto status_or_value = Base::proxy().GetValue(metric_fields().MakeFieldMap(args...));
    if (status_or_value.ok()) {
      return std::get<CanonicalTypeT<Value>>(std::move(status_or_value).value());
    } else {
      return std::move(status_or_value).status();
    }
  }

  void Delete(ParameterFieldTypeT<MetricFieldArgs> const... args) {
    Base::proxy().DeleteValue(metric_fields().MakeFieldMap(args...));
  }

  void Clear() { Base::proxy().Clear(); }
};

template <typename Value, typename... EntityLabelArgs, typename... MetricFieldArgs>
class Metric<Value, EntityLabels<EntityLabelArgs...>, MetricFields<MetricFieldArgs...>>
    : private BaseMetric<Value, EntityLabels<EntityLabelArgs...>,
                         MetricFields<MetricFieldArgs...>> {
 private:
  using Base =
      BaseMetric<Value, EntityLabels<EntityLabelArgs...>, MetricFields<MetricFieldArgs...>>;

 public:
  using EntityLabels = typename Base::EntityLabels;
  using MetricFields = typename Base::MetricFields;

  template <typename... Args>
  explicit Metric(Args &&...args) : Base(std::forward<Args>(args)...) {}

  using Base::entity_label_names;
  using Base::entity_labels;
  using Base::metric_field_names;
  using Base::metric_fields;
  using Base::name;
  using Base::options;

  void Set(ParameterTypeT<Value> value,
           ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values,
           ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values) {
    auto const shard = Base::shard();
    if (shard) {
      shard->SetValue(entity_labels().MakeFieldMap(entity_label_values...), name(),
                      metric_fields().MakeFieldMap(metric_field_values...),
                      CanonicalTypeT<Value>(std::move(value)));
    }
  }

  absl::StatusOr<Value> Get(
      ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values,
      ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values) const {
    auto const shard = Base::shard();
    if (!shard) {
      return absl::FailedPreconditionError(absl::StrCat(
          "failed to define metric \"", absl::CEscape(name()), "\" in the tsz exporter"));
    }
    auto status_or_value =
        shard->GetValue(entity_labels().MakeFieldMap(entity_label_values...), name(),
                        metric_fields().MakeFieldMap(metric_field_values...));
    if (status_or_value.ok()) {
      return std::get<CanonicalTypeT<Value>>(std::move(status_or_value).value());
    } else {
      return std::move(status_or_value).status();
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

#endif  // __TSDB2_TSZ_METRIC_H__
