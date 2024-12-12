#ifndef __TSDB2_TSZ_CELL_READER_H__
#define __TSDB2_TSZ_CELL_READER_H__

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "common/fixed.h"
#include "tsz/base.h"
#include "tsz/internal/exporter.h"
#include "tsz/internal/shard.h"

namespace tsz {
namespace testing {

// This class allows test code to read arbitrary tsz cells of arbitrary metrics. It's the standard
// means for testing tsz metrics.
//
// This class is thread-safe.
//
// Each `CellReader` instance is specific to a given metric. Example usage:
//
//   char constexpr kLoremLabel[] = "lorem";
//   char constexpr kFooField[] = "foo";
//
//   tsz::CellReader<
//       tsz::EntityLabels<tsz::Field<std::string, kLoremLabel>>>,
//       tsz::MetricFields<tsz::Field<int, kFooField>>> foobar_count_reader{"/foo/bar/count"};
//
//   EXPECT_THAT(foobar_count_reader.Read(/*lorem=*/"ipsum", /*foo=*/123), IsOkAndHolds(42));
//
template <typename Value, typename EntityLabels, typename MetricFields>
class CellReader;

template <typename Value, typename... EntityLabelArgs, typename... MetricFieldArgs>
class CellReader<Value, EntityLabels<EntityLabelArgs...>, MetricFields<MetricFieldArgs...>> {
 public:
  using CanonicalValue = CanonicalTypeT<Value>;
  using EntityLabels = EntityLabels<EntityLabelArgs...>;
  using MetricFields = MetricFields<MetricFieldArgs...>;

  struct Options {
    // Deletes all cells, across all entities, for the metric associated to the reader when the
    // reader is destroyed.
    //
    // This aids in testing because the stored cells are global state that could interfere with the
    // next test if it isn't reset at the end.
    bool clear_metric_on_destruction = true;
  };

  template <typename EntityLabelsAlias = EntityLabels, typename MetricFieldsAlias = MetricFields,
            std::enable_if_t<EntityLabelsAlias::kHasTypeNames && MetricFieldsAlias::kHasTypeNames,
                             bool> = true>
  explicit CellReader(std::string_view const metric_name, Options options = {})
      : options_(std::move(options)), metric_name_(metric_name) {}

  template <typename EntityLabelsAlias = EntityLabels, typename MetricFieldsAlias = MetricFields,
            std::enable_if_t<!EntityLabelsAlias::kHasTypeNames && !MetricFieldsAlias::kHasTypeNames,
                             bool> = true>
  explicit CellReader(
      std::string_view const metric_name,
      tsdb2::common::FixedT<std::string_view, EntityLabelArgs> const... entity_label_names,
      tsdb2::common::FixedT<std::string_view, MetricFieldArgs> const... metric_field_names,
      Options options = {})
      : options_(std::move(options)),
        entity_labels_(entity_label_names...),
        metric_fields_(metric_field_names...),
        metric_name_(metric_name) {}

  ~CellReader() {
    if (options_.clear_metric_on_destruction) {
      auto const shard = GetShard();
      if (shard) {
        shard->DeleteMetric(metric_name_);
      }
    }
  }

  // Reads the tsz cell identified by the provided entity label and metric field values, returning
  // the value if the cell was found or an error status otherwise.
  absl::StatusOr<Value> Read(
      ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values,
      ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values) const;

  template <typename ValueAlias = CanonicalValue,
            std::enable_if_t<util::IsIntegralStrictV<ValueAlias>, bool> = true>
  absl::StatusOr<Value> Delta(ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values,
                              ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values);

  template <typename ValueAlias = CanonicalValue,
            std::enable_if_t<util::IsIntegralStrictV<ValueAlias>, bool> = true>
  Value DeltaOrZero(ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values,
                    ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values) {
    return Delta(entity_label_values..., metric_field_values...).value_or(0);
  }

 private:
  using CellKey = util::CatTupleT<typename EntityLabels::Tuple, typename MetricFields::Tuple>;

  CellReader(CellReader const &) = delete;
  CellReader &operator=(CellReader const &) = delete;

  tsz::internal::Shard *GetShard() const;

  absl::StatusOr<CanonicalValue> ReadInternal(
      ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values,
      ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values) const;

  Options const options_;
  EntityLabels const entity_labels_;
  MetricFields const metric_fields_;
  std::string const metric_name_;

  absl::Mutex mutable mutex_;
  absl::flat_hash_map<CellKey, CanonicalValue> snapshot_ ABSL_GUARDED_BY(mutex_);
};

template <typename Value, typename... EntityLabelArgs, typename... MetricFieldArgs>
absl::StatusOr<Value>
CellReader<Value, EntityLabels<EntityLabelArgs...>, MetricFields<MetricFieldArgs...>>::Read(
    ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values,
    ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values) const {
  auto status_or_value = ReadInternal(entity_label_values..., metric_field_values...);
  if (status_or_value.ok()) {
    return std::move(status_or_value).value();
  } else {
    return std::move(status_or_value).status();
  }
}

template <typename Value, typename... EntityLabelArgs, typename... MetricFieldArgs>
template <typename ValueAlias, std::enable_if_t<util::IsIntegralStrictV<ValueAlias>, bool>>
absl::StatusOr<Value>
CellReader<Value, EntityLabels<EntityLabelArgs...>, MetricFields<MetricFieldArgs...>>::Delta(
    ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values,
    ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values) {
  auto status_or_value = ReadInternal(entity_label_values..., metric_field_values...);
  if (!status_or_value.ok()) {
    return std::move(status_or_value).status();
  }
  auto &value = status_or_value.value();
  CellKey key{entity_label_values..., metric_field_values...};
  absl::MutexLock lock{&mutex_};
  auto const [it, inserted] = snapshot_.try_emplace(std::move(key), 0);
  auto const previous = it->second;
  it->second = value;
  return value - previous;
}

template <typename Value, typename... EntityLabelArgs, typename... MetricFieldArgs>
tsz::internal::Shard *CellReader<Value, EntityLabels<EntityLabelArgs...>,
                                 MetricFields<MetricFieldArgs...>>::GetShard() const {
  auto const status_or_shard = tsz::internal::exporter->GetShardForMetric(metric_name_);
  if (status_or_shard.ok()) {
    return status_or_shard.value();
  } else {
    return nullptr;
  }
}

template <typename Value, typename... EntityLabelArgs, typename... MetricFieldArgs>
absl::StatusOr<typename CellReader<Value, EntityLabels<EntityLabelArgs...>,
                                   MetricFields<MetricFieldArgs...>>::CanonicalValue>
CellReader<Value, EntityLabels<EntityLabelArgs...>, MetricFields<MetricFieldArgs...>>::ReadInternal(
    ParameterFieldTypeT<EntityLabelArgs> const... entity_label_values,
    ParameterFieldTypeT<MetricFieldArgs> const... metric_field_values) const {
  auto const shard = GetShard();
  if (!shard) {
    return absl::FailedPreconditionError(
        absl::StrCat("the metric \"", absl::CEscape(metric_name_), "\" is not defined"));
  }
  auto status_or_value =
      shard->GetValue(entity_labels_.MakeFieldMap(entity_label_values...), metric_name_,
                      metric_fields_.MakeFieldMap(metric_field_values...));
  if (status_or_value.ok()) {
    return std::get<CanonicalValue>(std::move(status_or_value).value());
  } else {
    return std::move(status_or_value).status();
  }
}

}  // namespace testing
}  // namespace tsz

#endif  // __TSDB2_TSZ_CELL_READER_H__
