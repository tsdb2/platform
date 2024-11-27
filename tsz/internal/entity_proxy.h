#ifndef __TSDB2_TSZ_INTERNAL_ENTITY_PROXY_H__
#define __TSDB2_TSZ_INTERNAL_ENTITY_PROXY_H__

#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "tsz/internal/entity.h"
#include "tsz/internal/scoped_metric_proxy.h"
#include "tsz/types.h"

namespace tsz {
namespace internal {

class EntityProxy {
 public:
  explicit EntityProxy() = default;

  explicit EntityProxy(std::shared_ptr<Entity> entity, absl::Time const time)
      : context_(std::move(entity), time) {}

  EntityProxy(EntityProxy &&) noexcept = default;
  EntityProxy &operator=(EntityProxy &&) noexcept = default;

  [[nodiscard]] bool empty() const { return context_.entity() == nullptr; }
  explicit operator bool() const { return context_.entity() != nullptr; }

  absl::StatusOr<Value> GetValue(std::string_view const metric_name,
                                 FieldMap const &metric_fields) {
    auto const &entity = context_.entity();
    if (entity) {
      return entity->GetValue(metric_name, metric_fields);
    } else {
      return absl::FailedPreconditionError("the proxy is empty");
    }
  }

  template <typename MetricFields>
  void SetValue(std::string_view const metric_name, MetricFields &&metric_fields, Value value) {
    auto const &entity = context_.entity();
    if (entity) {
      entity->SetValue(context_, metric_name, std::forward<MetricFields>(metric_fields),
                       std::move(value));
    }
  }

  template <typename MetricFields>
  void AddToInt(std::string_view const metric_name, MetricFields &&metric_fields,
                int64_t const delta) {
    auto const &entity = context_.entity();
    if (entity) {
      entity->AddToInt(context_, metric_name, std::forward<MetricFields>(metric_fields), delta);
    }
  }

  template <typename MetricFields>
  void AddToDistribution(std::string_view const metric_name, MetricFields &&metric_fields,
                         double const sample, size_t const times) {
    auto const &entity = context_.entity();
    if (entity) {
      entity->AddToDistribution(context_, metric_name, std::forward<MetricFields>(metric_fields),
                                sample, times);
    }
  }

  bool DeleteValue(std::string_view const metric_name, FieldMap const &metric_fields) {
    auto const &entity = context_.entity();
    if (entity) {
      return entity->DeleteValue(context_, metric_name, metric_fields);
    } else {
      return false;
    }
  }

  bool DeleteMetric(std::string_view const metric_name) {
    auto const &entity = context_.entity();
    if (entity) {
      return entity->DeleteMetric(context_, metric_name);
    } else {
      return false;
    }
  }

  absl::StatusOr<ScopedMetricProxy> GetPinnedMetric(std::string_view const metric_name) {
    auto const &entity = context_.entity();
    if (entity) {
      return entity->GetPinnedMetric(context_, metric_name);
    } else {
      return absl::FailedPreconditionError("the proxy is empty");
    }
  }

 private:
  EntityProxy(EntityProxy const &) = delete;
  EntityProxy &operator=(EntityProxy const &) = delete;

  EntityContext context_;
};

}  // namespace internal
}  // namespace tsz

#endif  // __TSDB2_TSZ_INTERNAL_ENTITY_PROXY_H__
