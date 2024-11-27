#include "tsz/internal/entity.h"

#include <memory>
#include <string_view>
#include <tuple>
#include <utility>

#include "absl/hash/hash.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "tsz/internal/metric.h"
#include "tsz/internal/scoped_metric_proxy.h"
#include "tsz/internal/throw_away_metric_proxy.h"
#include "tsz/types.h"

namespace tsz {
namespace internal {

void Entity::Unpin() {
  absl::MutexLock lock{&mutex_};
  if (pin_count_.Unref() && metrics_.empty()) {
    manager_->DeleteEntityInternal(labels_);
  }
}

absl::StatusOr<Value> Entity::GetValue(std::string_view const metric_name,
                                       FieldMap const &metric_fields) {
  auto const metric = GetEphemeralMetric(metric_name);
  if (metric) {
    return metric->GetValue(metric_fields);
  } else {
    return absl::NotFoundError("value not found");
  }
}

bool Entity::DeleteValue(EntityContext const &context, std::string_view const metric_name,
                         FieldMap const &metric_fields) {
  auto status_or_metric = GetMetric(context, metric_name);
  if (status_or_metric.ok()) {
    return status_or_metric->DeleteValue(metric_fields);
  } else {
    return false;
  }
}

bool Entity::DeleteMetric(EntityContext const &context, std::string_view const metric_name) {
  auto status_or_metric = GetMetric(context, metric_name);
  if (status_or_metric.ok()) {
    return status_or_metric->Clear();
  } else {
    return false;
  }
}

absl::StatusOr<ScopedMetricProxy> Entity::GetPinnedMetric(EntityContext const &context,
                                                          std::string_view const metric_name) {
  auto const hash = absl::HashOf(metric_name);
  absl::MutexLock lock{&mutex_};
  auto it = metrics_.find(metric_name, hash);
  if (it == metrics_.end()) {
    auto status_or_metric_config = manager_->GetConfigForMetric(metric_name);
    if (!status_or_metric_config.ok()) {
      LOG_EVERY_N_SEC(ERROR, 60) << "cannot define metric " << metric_name << ": "
                                 << status_or_metric_config.status();
      return std::move(status_or_metric_config).status();
    }
    bool unused;
    std::tie(it, unused) = metrics_.emplace(static_cast<MetricManager *>(this), metric_name,
                                            *(status_or_metric_config.value()));
  }
  return it->MakeScopedProxy(context);
}

absl::StatusOr<ScopedMetricProxy> Entity::MetricCell::MakeScopedProxy(
    EntityContext const &context) const {
  return absl::StatusOr<ScopedMetricProxy>(
      std::in_place, std::shared_ptr<MetricManager>(context.entity()), metric_, context.time());
}

absl::StatusOr<ThrowAwayMetricProxy> Entity::MetricCell::MakeThrowAwayProxy(
    EntityContext const &context) const {
  return absl::StatusOr<ThrowAwayMetricProxy>(
      std::in_place, std::shared_ptr<MetricManager>(context.entity()), metric_, context.time());
}

void Entity::DeleteMetricInternal(std::string_view const name) {
  typename MetricSet::node_type node;
  auto const hash = absl::HashOf(name);
  absl::MutexLock lock{&mutex_};
  auto const it = metrics_.find(name, hash);
  if (it != metrics_.end() && !(*it)->is_pinned()) {
    node = metrics_.extract(it);
  }
  if (metrics_.empty() && !is_pinned()) {
    manager_->DeleteEntityInternal(labels_);
  }
}

absl::StatusOr<ThrowAwayMetricProxy> Entity::GetMetric(EntityContext const &context,
                                                       std::string_view const metric_name) {
  auto const hash = absl::HashOf(metric_name);
  {
    absl::MutexLock lock{&mutex_};
    auto it = metrics_.find(metric_name, hash);
    if (it != metrics_.end()) {
      return it->MakeThrowAwayProxy(context);
    }
  }
  return absl::NotFoundError(metric_name);
}

absl::StatusOr<ThrowAwayMetricProxy> Entity::GetOrCreateMetric(EntityContext const &context,
                                                               std::string_view const metric_name) {
  auto const hash = absl::HashOf(metric_name);
  absl::MutexLock lock{&mutex_};
  auto it = metrics_.find(metric_name, hash);
  if (it == metrics_.end()) {
    auto status_or_metric_config = manager_->GetConfigForMetric(metric_name);
    if (!status_or_metric_config.ok()) {
      LOG_EVERY_N_SEC(ERROR, 60) << "cannot define metric " << metric_name << ": "
                                 << status_or_metric_config.status();
      return std::move(status_or_metric_config).status();
    }
    bool unused;
    std::tie(it, unused) = metrics_.emplace(static_cast<MetricManager *>(this), metric_name,
                                            *(status_or_metric_config.value()));
  }
  return it->MakeThrowAwayProxy(context);
}

std::shared_ptr<Metric> Entity::GetEphemeralMetric(std::string_view const metric_name) {
  auto const hash = absl::HashOf(metric_name);
  absl::MutexLock lock{&mutex_};
  auto it = metrics_.find(metric_name, hash);
  if (it != metrics_.end()) {
    return it->ptr();
  } else {
    return nullptr;
  }
}

}  // namespace internal
}  // namespace tsz
