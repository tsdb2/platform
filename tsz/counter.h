#ifndef __TSDB2_TSZ_COUNTER_H__
#define __TSDB2_TSZ_COUNTER_H__

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "common/fixed.h"
#include "tsz/base.h"

namespace tsz {

template <typename EntityLabels, typename MetricFields>
class Counter;

template <typename... EntityLabelArgs, typename... MetricFieldArgs>
class Counter<EntityLabels<EntityLabelArgs...>, MetricFields<MetricFieldArgs...>> {
 public:
  using EntityLabels = EntityLabels<EntityLabelArgs...>;
  using EntityLabelTuple = typename EntityLabels::Tuple;
  using MetricFields = MetricFields<MetricFieldArgs...>;
  using MetricFieldTuple = typename MetricFields::Tuple;

  static_assert(EntityLabels::kHasTypeNames == MetricFields::kHasTypeNames,
                "entity labels and metric fields must follow the same pattern: either both or "
                "neither must have type names");

  template <typename EntityLabelsAlias = EntityLabels, typename MetricFieldsAlias = MetricFields,
            std::enable_if_t<EntityLabelsAlias::kHasTypeNames && MetricFieldsAlias::kHasTypeNames,
                             bool> = true>
  explicit Counter(std::string_view const name) : name_(name) {}

  template <typename EntityLabelsAlias = EntityLabels, typename MetricFieldsAlias = MetricFields,
            std::enable_if_t<!EntityLabelsAlias::kHasTypeNames && !MetricFieldsAlias::kHasTypeNames,
                             bool> = true>
  explicit Counter(
      std::string_view const name,
      tsdb2::common::FixedT<std::string_view, EntityLabelArgs> const... entity_label_names,
      tsdb2::common::FixedT<std::string_view, MetricFieldArgs> const... metric_field_names)
      : name_(name), entity_labels_(entity_label_names...), metric_fields_(metric_field_names...) {}

  std::string_view name() const { return name_; }

  EntityLabels const &entity_labels() const { return entity_labels_; }
  MetricFields const &metric_fields() const { return metric_fields_; }

  auto const &entity_label_names() const { return entity_labels_.names(); }
  auto const &metric_field_names() const { return metric_fields_.names(); }

 private:
  Counter(Counter const &) = delete;
  Counter &operator=(Counter const &) = delete;
  Counter(Counter &&) = delete;
  Counter &operator=(Counter &&) = delete;

  std::string const name_;

  EntityLabels const entity_labels_;
  MetricFields const metric_fields_;

  absl::Mutex mutable mutex_;
  absl::flat_hash_map<util::CatTupleT<EntityLabelTuple, MetricFieldTuple>, int64_t> values_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace tsz

#endif  // __TSDB2_TSZ_COUNTER_H__
