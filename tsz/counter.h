#ifndef __TSDB2_TSZ_COUNTER_H__
#define __TSDB2_TSZ_COUNTER_H__

#include <cstdint>
#include <string_view>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "tsz/base.h"

namespace tsz {

template <typename EntityLabels, typename MetricFields>
class Counter;

template <typename... EntityLabelArgs, typename... MetricFieldArgs>
class Counter<EntityLabels<EntityLabelArgs...>, MetricFields<MetricFieldArgs...>>
    : public internal::BaseMetric {
 public:
  using EntityLabels = EntityLabels<EntityLabelArgs...>;
  using EntityLabelTuple = typename EntityLabels::Tuple;
  using MetricFields = MetricFields<MetricFieldArgs...>;
  using MetricFieldTuple = typename MetricFields::Tuple;

  explicit Counter(std::string_view const name) : BaseMetric(name) {}

  using BaseMetric::name;

 private:
  absl::Mutex mutable mutex_;
  absl::flat_hash_map<util::CatTupleT<EntityLabelTuple, MetricFieldTuple>, int64_t> values_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace tsz

#endif  // __TSDB2_TSZ_COUNTER_H__
