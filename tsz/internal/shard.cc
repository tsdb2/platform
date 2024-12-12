#include "tsz/internal/shard.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/re.h"
#include "tsz/internal/entity.h"
#include "tsz/internal/entity_proxy.h"
#include "tsz/internal/metric_config.h"
#include "tsz/types.h"

namespace tsz {
namespace internal {

namespace {

NoDestructor<tsdb2::common::RE> const kMetricNamePattern{
    tsdb2::common::RE::CreateOrDie("(?:/[A-Za-z0-9._-]+)+")};

}  // namespace

absl::Status Shard::DefineMetric(std::string_view const metric_name, MetricConfig metric_config) {
  if (!kMetricNamePattern->Test(metric_name)) {
    return absl::InvalidArgumentError(metric_name);
  }
  auto const [unused_it, inserted] =
      metric_configs_.try_emplace(metric_name, std::move(metric_config));
  if (inserted) {
    return absl::OkStatus();
  } else {
    return absl::AlreadyExistsError(metric_name);
  }
}

absl::Status Shard::DefineMetricRedundant(std::string_view const metric_name,
                                          MetricConfig metric_config) {
  if (!kMetricNamePattern->Test(metric_name)) {
    return absl::InvalidArgumentError(metric_name);
  }
  auto const [it, inserted] = metric_configs_.try_emplace(metric_name, std::move(metric_config));
  if (!inserted) {
    // TODO: merge configs.
  }
  return absl::OkStatus();
}

absl::StatusOr<Value> Shard::GetValue(FieldMap const &entity_labels,
                                      std::string_view const metric_name,
                                      FieldMap const &metric_fields) const {
  auto const entity = GetEphemeralEntity(entity_labels);
  if (entity) {
    return entity->GetValue(metric_name, metric_fields);
  } else {
    return absl::NotFoundError("value not found");
  }
}

bool Shard::DeleteValue(FieldMap const &entity_labels, std::string_view const metric_name,
                        FieldMap const &metric_fields) {
  auto entity = GetEntity(entity_labels);
  if (entity) {
    return entity.DeleteValue(metric_name, metric_fields);
  } else {
    return false;
  }
}

bool Shard::DeleteMetric(FieldMap const &entity_labels, std::string_view const metric_name) {
  auto entity = GetEntity(entity_labels);
  if (entity) {
    return entity.DeleteMetric(metric_name);
  } else {
    return false;
  }
}

void Shard::DeleteMetric(std::string_view const metric_name) {
  bool done;
  do {
    done = true;
    for (auto &proxy : GetAllProxies()) {
      if (proxy.DeleteMetric(metric_name)) {
        done = false;
      }
    }
  } while (!done);
}

absl::StatusOr<MetricConfig const *> Shard::GetConfigForMetric(
    std::string_view const metric_name) const {
  auto const it = metric_configs_.find(metric_name);
  if (it != metric_configs_.end()) {
    return &(it->second.value());
  } else {
    return absl::NotFoundError(metric_name);
  }
}

bool Shard::DeleteEntityInternal(FieldMap const &labels) {
  typename EntitySet::node_type node;
  FieldMapView const label_view{labels};
  absl::WriterMutexLock lock{&mutex_};
  auto const it = entities_.find(label_view);
  if (it != entities_.end() && !(*it)->is_pinned()) {
    node = entities_.extract(it);
  }
  return !node.empty();
}

EntityProxy Shard::GetEntity(FieldMap const &entity_labels) const {
  absl::Time const now = clock_.TimeNow();
  FieldMapView const entity_label_view{entity_labels};
  {
    absl::WriterMutexLock lock{&mutex_};
    auto it = entities_.find(entity_label_view);
    if (it != entities_.end()) {
      return it->MakeProxy(now);
    }
  }
  return EntityProxy();
}

std::shared_ptr<Entity> Shard::GetEphemeralEntity(FieldMap const &entity_labels) const {
  FieldMapView const entity_label_view{entity_labels};
  absl::WriterMutexLock lock{&mutex_};
  auto it = entities_.find(entity_label_view);
  if (it != entities_.end()) {
    return it->ptr();
  } else {
    return nullptr;
  }
}

std::vector<EntityProxy> Shard::GetAllProxies() const {
  std::vector<EntityProxy> proxies;
  auto const now = clock_.TimeNow();
  absl::ReaderMutexLock lock{&mutex_};
  proxies.reserve(entities_.size());
  for (auto const &entity : entities_) {
    proxies.emplace_back(entity.MakeProxy(now));
  }
  return proxies;
}

}  // namespace internal
}  // namespace tsz
