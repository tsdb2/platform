#include "tsz/internal/exporter.h"

#include <string_view>
#include <utility>

#include "common/singleton.h"
#include "common/utilities.h"
#include "tsz/internal/metric_config.h"
#include "tsz/types.h"

namespace tsz {
namespace internal {

tsdb2::common::Singleton<Exporter> exporter{std::in_place};

absl::StatusOr<Shard *> Exporter::DefineMetric(std::string_view const metric_name,
                                               Options const &options) {
  auto const realm_name = options.realm->name();
  auto const [shard_it, unused_shard_inserted] = realms_to_shards_.try_emplace(realm_name);
  auto &shard = shard_it->second;
  RETURN_IF_ERROR(shard.DefineMetric(metric_name, OptionsToConfig(options)));
  auto const [unused_metric_it, metric_inserted] =
      metrics_to_realms_.try_emplace(metric_name, realm_name);
  if (!metric_inserted) {
    return absl::AlreadyExistsError(metric_name);
  }
  return &shard;
}

absl::StatusOr<Shard *> Exporter::DefineMetricRedundant(std::string_view const metric_name,
                                                        Options const &options) {
  auto const realm_name = options.realm->name();
  auto const [shard_it, unused_shard_inserted] = realms_to_shards_.try_emplace(realm_name);
  auto &shard = shard_it->second;
  RETURN_IF_ERROR(shard.DefineMetricRedundant(metric_name, OptionsToConfig(options)));
  metrics_to_realms_.try_emplace(metric_name, realm_name);
  return &shard;
}

absl::StatusOr<Shard *> Exporter::GetShardForMetric(std::string_view const metric_name) {
  auto const metric_it = metrics_to_realms_.find(metric_name);
  if (metric_it == metrics_to_realms_.end()) {
    return absl::NotFoundError(metric_name);
  }
  auto const shard_it = realms_to_shards_.find(metric_it->second);
  if (shard_it == realms_to_shards_.end()) {
    return absl::NotFoundError(metric_name);
  }
  return &shard_it->second;
}

MetricConfig Exporter::OptionsToConfig(Options const &options) {
  return MetricConfig{
      // TODO: copy all fields over
      .skip_stable_cells = options.skip_stable_cells,
      .delta_mode = options.delta_mode,
      .user_timestamps = options.user_timestamps,
      .bucketer = options.bucketer,
      .max_entity_staleness = options.max_entity_staleness,
      .max_value_staleness = options.max_value_staleness,
  };
}

}  // namespace internal
}  // namespace tsz
