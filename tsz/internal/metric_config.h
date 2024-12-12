#ifndef __TSDB2_TSZ_INTERNAL_METRIC_CONFIG_H__
#define __TSDB2_TSZ_INTERNAL_METRIC_CONFIG_H__

#include <optional>

#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "tsz/types.h"

namespace tsz {
namespace internal {

struct MetricConfig {
  bool cumulative = false;

  // TODO: `skip_stable_cells` and `delta_mode` only work if all backends have the same (explicit)
  // sampling period. Otherwise we must log an error and ignore them. They wouldn't work in
  // target-writing mode even if there was only one backend, because an entity may map to multiple
  // targets and a user may change the sampling period for a target at any time in a retention
  // policy; at that point we would no longer know the last push time for each target the entity
  // maps to (keeping track of it is infeasible).

  bool skip_stable_cells = false;
  bool delta_mode = false;

  bool user_timestamps = false;

  Bucketer const* bucketer = nullptr;

  std::optional<absl::Duration> max_entity_staleness = std::nullopt;
  std::optional<absl::Duration> max_value_staleness = std::nullopt;

  Options::BackendSettings default_backend_settings{};
  absl::flat_hash_map<Options::BackendKey, Options::BackendSettings> backend_settings{};
};

}  // namespace internal
}  // namespace tsz

#endif  // __TSDB2_TSZ_INTERNAL_METRIC_CONFIG_H__
