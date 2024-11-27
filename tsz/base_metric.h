#ifndef __TSDB2_TSZ_BASE_METRIC_H__
#define __TSDB2_TSZ_BASE_METRIC_H__

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "absl/functional/bind_front.h"
#include "absl/log/log.h"
#include "absl/strings/escaping.h"
#include "common/fixed.h"
#include "common/lazy.h"
#include "common/reffed_ptr.h"
#include "tsz/base.h"
#include "tsz/entity.h"
#include "tsz/internal/exporter.h"
#include "tsz/internal/scoped_metric_proxy.h"
#include "tsz/internal/shard.h"

namespace tsz {

template <typename Value, typename... MetricFieldArgs>
class BaseMetric {
 public:
  using MetricFields = MetricFields<MetricFieldArgs...>;

  template <typename... EntityLabelArgs, typename MetricFieldsAlias = MetricFields,
            std::enable_if_t<!MetricFieldsAlias::kHasTypeNames, bool> = true>
  explicit BaseMetric(
      Entity<EntityLabelArgs...> const &entity, std::string_view const name,
      tsdb2::common::FixedT<std::string_view, MetricFieldArgs> const... metric_field_names,
      Options options = {})
      : entity_(&entity),
        name_(name),
        options_(std::move(options)),
        metric_fields_(metric_field_names...),
        proxy_(absl::bind_front(&BaseMetric::DefineMetric, this)) {}

  template <typename... EntityLabelArgs, typename MetricFieldsAlias = MetricFields,
            std::enable_if_t<MetricFieldsAlias::kHasTypeNames, bool> = true>
  explicit BaseMetric(Entity<EntityLabelArgs...> const &entity, std::string_view const name,
                      Options options = {})
      : entity_(&entity),
        name_(name),
        options_(std::move(options)),
        proxy_(absl::bind_front(&BaseMetric::DefineMetric, this)) {}

  template <typename MetricFieldsAlias = MetricFields,
            std::enable_if_t<!MetricFieldsAlias::kHasTypeNames, bool> = true>
  explicit BaseMetric(
      std::string_view const name,
      tsdb2::common::FixedT<std::string_view, MetricFieldArgs> const... metric_field_names,
      Options options = {})
      : BaseMetric(GetDefaultEntity(), name, metric_field_names..., std::move(options)) {}

  template <typename MetricFieldsAlias = MetricFields,
            std::enable_if_t<MetricFieldsAlias::kHasTypeNames, bool> = true>
  explicit BaseMetric(std::string_view const name, Options options = {})
      : BaseMetric(GetDefaultEntity(), name, std::move(options)) {}

  std::string_view name() const { return name_; }
  Options const &options() const { return options_; }

  FieldMap const &entity_labels() const { return entity_->labels(); }
  MetricFields const &metric_fields() const { return metric_fields_; }
  auto const &metric_field_names() const { return metric_fields_.names(); }

 protected:
  internal::ScopedMetricProxy &proxy() { return *proxy_; }
  internal::ScopedMetricProxy const &proxy() const { return *proxy_; }

 private:
  BaseMetric(BaseMetric const &) = delete;
  BaseMetric &operator=(BaseMetric const &) = delete;
  BaseMetric(BaseMetric &&) = delete;
  BaseMetric &operator=(BaseMetric &&) = delete;

  internal::ScopedMetricProxy DefineMetric() const;

  tsdb2::common::reffed_ptr<EntityInterface const> const entity_;
  std::string const name_;
  Options const options_;

  MetricFields const metric_fields_;

  tsdb2::common::Lazy<internal::ScopedMetricProxy> proxy_;
};

template <typename Value, typename... EntityLabelArgs, typename... MetricFieldArgs>
class BaseMetric<Value, EntityLabels<EntityLabelArgs...>, MetricFields<MetricFieldArgs...>> {
 public:
  using EntityLabels = EntityLabels<EntityLabelArgs...>;
  using MetricFields = MetricFields<MetricFieldArgs...>;

  static_assert(EntityLabels::kHasTypeNames == MetricFields::kHasTypeNames,
                "entity labels and metric fields must follow the same pattern: either both or "
                "neither must have type names");

  template <typename EntityLabelsAlias = EntityLabels, typename MetricFieldsAlias = MetricFields,
            std::enable_if_t<EntityLabelsAlias::kHasTypeNames && MetricFieldsAlias::kHasTypeNames,
                             bool> = true>
  explicit BaseMetric(std::string_view const name, Options options = {})
      : name_(name),
        options_(std::move(options)),
        shard_(absl::bind_front(&BaseMetric::DefineMetric, this)) {}

  template <typename EntityLabelsAlias = EntityLabels, typename MetricFieldsAlias = MetricFields,
            std::enable_if_t<!EntityLabelsAlias::kHasTypeNames && !MetricFieldsAlias::kHasTypeNames,
                             bool> = true>
  explicit BaseMetric(
      std::string_view const name,
      tsdb2::common::FixedT<std::string_view, EntityLabelArgs> const... entity_label_names,
      tsdb2::common::FixedT<std::string_view, MetricFieldArgs> const... metric_field_names,
      Options options = {})
      : name_(name),
        options_(std::move(options)),
        entity_labels_(entity_label_names...),
        metric_fields_(metric_field_names...),
        shard_(absl::bind_front(&BaseMetric::DefineMetric, this)) {}

  std::string_view name() const { return name_; }
  Options const &options() const { return options_; }

  EntityLabels const &entity_labels() const { return entity_labels_; }
  MetricFields const &metric_fields() const { return metric_fields_; }

  auto const &entity_label_names() const { return entity_labels_.names(); }
  auto const &metric_field_names() const { return metric_fields_.names(); }

 protected:
  internal::Shard *shard() { return *shard_; }
  internal::Shard const *shard() const { return *shard_; }

 private:
  BaseMetric(BaseMetric const &) = delete;
  BaseMetric &operator=(BaseMetric const &) = delete;
  BaseMetric(BaseMetric &&) = delete;
  BaseMetric &operator=(BaseMetric &&) = delete;

  internal::Shard *DefineMetric() const;

  std::string const name_;
  Options const options_;

  EntityLabels const entity_labels_;
  MetricFields const metric_fields_;

  tsdb2::common::Lazy<internal::Shard *> const shard_;
};

template <typename Value, typename... MetricFieldArgs>
internal::ScopedMetricProxy BaseMetric<Value, MetricFieldArgs...>::DefineMetric() const {
  auto const status_or_shard = internal::exporter->DefineMetricRedundant(name_, options_);
  if (!status_or_shard.ok()) {
    LOG(ERROR) << "Failed to define metric \"" << absl::CEscape(name_)
               << "\" in the tsz exporter: " << status_or_shard.status();
    return internal::ScopedMetricProxy();
  }
  auto const shard = status_or_shard.value();
  auto status_or_proxy = shard->GetPinnedMetric(entity_labels(), name_);
  if (!status_or_proxy.ok()) {
    LOG(ERROR) << "Failed to define metric \"" << absl::CEscape(name_)
               << "\" in the tsz exporter: " << status_or_proxy.status();
    return internal::ScopedMetricProxy();
  }
  return std::move(status_or_proxy).value();
}

template <typename Value, typename... EntityLabelArgs, typename... MetricFieldArgs>
internal::Shard *BaseMetric<Value, EntityLabels<EntityLabelArgs...>,
                            MetricFields<MetricFieldArgs...>>::DefineMetric() const {
  auto const status_or_shard = internal::exporter->DefineMetricRedundant(name_, options_);
  if (status_or_shard.ok()) {
    return status_or_shard.value();
  } else {
    LOG(ERROR) << "Failed to define metric \"" << absl::CEscape(name_)
               << "\" in the tsz exporter: " << status_or_shard.status();
    return nullptr;
  }
}

}  // namespace tsz

#endif  // __TSDB2_TSZ_BASE_METRIC_H__
