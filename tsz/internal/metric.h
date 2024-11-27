#ifndef __TSDB2_TSZ_INTERNAL_METRIC_H__
#define __TSDB2_TSZ_INTERNAL_METRIC_H__

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/ref_count.h"
#include "tsz/internal/cell.h"
#include "tsz/internal/metric_config.h"
#include "tsz/types.h"

namespace tsz {
namespace internal {

class MetricManager {
 public:
  virtual ~MetricManager() = default;
  virtual void DeleteMetricInternal(std::string_view name) = 0;
};

class Metric {
 public:
  explicit Metric(MetricManager *const manager, std::string_view const name,
                  MetricConfig const &metric_config)
      : manager_(manager), name_(name), hash_(absl::HashOf(name_)), config_(metric_config) {}

  std::string_view name() const { return name_; }
  size_t hash() const { return hash_; }
  MetricConfig const &config() const { return config_; }

  bool is_pinned() const { return pin_count_.is_referenced(); }
  void Pin() { pin_count_.Ref(); }
  bool Unpin() ABSL_LOCKS_EXCLUDED(mutex_);

  absl::StatusOr<Value> GetValue(FieldMap const &metric_fields) const ABSL_LOCKS_EXCLUDED(mutex_);

  template <typename MetricContext>
  absl::StatusOr<Value> GetValue(MetricContext *context, FieldMap const &metric_fields) const
      ABSL_LOCKS_EXCLUDED(mutex_);

  template <typename MetricContext, typename MetricFields>
  void SetValue(MetricContext *context, MetricFields &&metric_fields, Value value)
      ABSL_LOCKS_EXCLUDED(mutex_);

  template <typename MetricContext, typename MetricFields>
  void AddToInt(MetricContext *context, MetricFields &&metric_fields, int64_t delta)
      ABSL_LOCKS_EXCLUDED(mutex_);

  template <typename MetricContext, typename MetricFields>
  void AddToDistribution(MetricContext *context, MetricFields &&metric_fields, double sample,
                         size_t times) ABSL_LOCKS_EXCLUDED(mutex_);

  template <typename MetricContext>
  bool DeleteValue(MetricContext *context, FieldMap const &metric_fields)
      ABSL_LOCKS_EXCLUDED(mutex_);

  template <typename MetricContext>
  bool Clear(MetricContext *context) ABSL_LOCKS_EXCLUDED(mutex_);

  bool ResetIfCumulative(absl::Time start_time) ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  using CellSet = absl::flat_hash_set<Cell, Cell::Hash, Cell::Eq>;

  friend class ThrowAwayMetricContext;

  Metric(Metric const &) = delete;
  Metric &operator=(Metric const &) = delete;
  Metric(Metric &&) = delete;
  Metric &operator=(Metric &&) = delete;

  bool UnpinLocked() ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  MetricManager *const manager_;  // not owned

  std::string const name_;
  size_t const hash_;
  MetricConfig const &config_;

  absl::Mutex mutable mutex_;

  tsdb2::common::RefCount pin_count_;

  CellSet cells_ ABSL_GUARDED_BY(mutex_);
  absl::Time last_update_time_ ABSL_GUARDED_BY(mutex_) = absl::InfinitePast();
};

template <typename MetricContext>
absl::StatusOr<Value> Metric::GetValue(MetricContext *const context,
                                       FieldMap const &metric_fields) const {
  FieldMapView const metric_field_view{metric_fields};
  {
    absl::ReaderMutexLock lock{&mutex_};
    typename MetricContext::Closure context_closure{context};
    auto const it = cells_.find(metric_field_view);
    if (it != cells_.end()) {
      return it->value();
    }
  }
  return absl::NotFoundError("value not found");
}

template <typename MetricContext, typename MetricFields>
void Metric::SetValue(MetricContext *const context, MetricFields &&metric_fields, Value value) {
  FieldMapView const metric_field_view{metric_fields};
  absl::WriterMutexLock lock{&mutex_};
  typename MetricContext::Closure context_closure{context};
  auto const it = cells_.find(metric_field_view);
  if (it != cells_.end()) {
    const_cast<Cell &>(*it).SetValue(std::move(value), context->time());
  } else {
    cells_.emplace(std::forward<MetricFields>(metric_fields), metric_field_view.hash(),
                   std::move(value), context->time());
  }
  last_update_time_ = context->time();
}

template <typename MetricContext, typename MetricFields>
void Metric::AddToInt(MetricContext *const context, MetricFields &&metric_fields,
                      int64_t const delta) {
  FieldMapView const metric_field_view{metric_fields};
  absl::WriterMutexLock lock{&mutex_};
  typename MetricContext::Closure context_closure{context};
  auto const it = cells_.find(metric_field_view);
  if (it != cells_.end()) {
    const_cast<Cell &>(*it).AddToInt(delta, context->time());
  } else {
    cells_.emplace(std::forward<MetricFields>(metric_fields), metric_field_view.hash(), delta,
                   context->time());
  }
  last_update_time_ = context->time();
}

template <typename MetricContext, typename MetricFields>
void Metric::AddToDistribution(MetricContext *const context, MetricFields &&metric_fields,
                               double const sample, size_t const times) {
  FieldMapView const metric_field_view{metric_fields};
  absl::WriterMutexLock lock{&mutex_};
  typename MetricContext::Closure context_closure{context};
  auto it = cells_.find(metric_field_view);
  if (it == cells_.end()) {
    bool unused;
    std::tie(it, unused) =
        cells_.emplace(Cell::kDistributionCell, std::forward<MetricFields>(metric_fields),
                       metric_field_view.hash(), config_.bucketer, context->time());
  }
  const_cast<Cell &>(*it).AddToDistribution(sample, times, context->time());
  last_update_time_ = context->time();
}

template <typename MetricContext>
bool Metric::DeleteValue(MetricContext *const context, FieldMap const &metric_fields) {
  typename CellSet::node_type node;
  {
    FieldMapView const metric_field_view{metric_fields};
    absl::WriterMutexLock lock{&mutex_};
    typename MetricContext::Closure context_closure{context};
    node = cells_.extract(metric_field_view);
  }
  return !node.empty();
}

template <typename MetricContext>
bool Metric::Clear(MetricContext *const context) {
  CellSet cells;
  {
    absl::WriterMutexLock lock{&mutex_};
    typename MetricContext::Closure context_closure{context};
    cells_.swap(cells);
  }
  return !cells.empty();
}

}  // namespace internal
}  // namespace tsz

#endif  // __TSDB2_TSZ_INTERNAL_METRIC_H__
