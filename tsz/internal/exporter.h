#ifndef __TSDB2_TSZ_INTERNAL_EXPORTER_H__
#define __TSDB2_TSZ_INTERNAL_EXPORTER_H__

#include <string>
#include <string_view>

#include "absl/status/statusor.h"
#include "common/lock_free_hash_map.h"
#include "common/singleton.h"
#include "tsz/internal/shard.h"
#include "tsz/types.h"

namespace tsz {
namespace internal {

class Exporter {
 public:
  absl::StatusOr<Shard *> DefineMetric(std::string_view metric_name, Options const &options);

  absl::StatusOr<Shard *> DefineMetricRedundant(std::string_view metric_name,
                                                Options const &options);

  absl::StatusOr<Shard *> GetShardForMetric(std::string_view metric_name);

 private:
  friend class tsdb2::common::Singleton<Exporter>;

  explicit Exporter() = default;

  Exporter(Exporter const &) = delete;
  Exporter &operator=(Exporter const &) = delete;
  Exporter(Exporter &&) = delete;
  Exporter &operator=(Exporter &&) = delete;

  static MetricConfig OptionsToConfig(Options const &options);

  tsdb2::common::lock_free_hash_map<std::string, std::string> metrics_to_realms_;
  tsdb2::common::lock_free_hash_map<std::string, Shard> realms_to_shards_;
};

extern tsdb2::common::Singleton<Exporter> exporter;

}  // namespace internal
}  // namespace tsz

#endif  // __TSDB2_TSZ_INTERNAL_EXPORTER_H__
