#ifndef __TSDB2_TSZ_INTERNAL_METRIC_PROXY_H__
#define __TSDB2_TSZ_INTERNAL_METRIC_PROXY_H__

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "tsz/internal/metric.h"
#include "tsz/types.h"

namespace tsz {
namespace internal {

class Entity;

template <typename MetricContext>
class MetricProxy {
 public:
  explicit MetricProxy() = default;

  explicit MetricProxy(std::shared_ptr<MetricManager> manager, std::shared_ptr<Metric> metric,
                       absl::Time const time)
      : manager_(std::move(manager)), metric_(std::move(metric)), context_(metric_, time) {}

  MetricProxy(MetricProxy&&) noexcept = default;
  MetricProxy& operator=(MetricProxy&&) noexcept = default;

  void swap(MetricProxy& other) {
    using std::swap;  // ensure ADL
    swap(manager_, other.manager_);
    swap(metric_, other.metric_);
    swap(context_, other.context_);
  }

  friend void swap(MetricProxy& lhs, MetricProxy& rhs) { lhs.swap(rhs); }

  [[nodiscard]] bool empty() const { return metric_ == nullptr; }
  explicit operator bool() const { return metric_ != nullptr; }

  absl::StatusOr<Value> GetValue(FieldMap const& metric_fields) const {
    if (metric_) {
      return metric_->GetValue(&context_, metric_fields);
    } else {
      return absl::FailedPreconditionError("the proxy is empty");
    }
  }

  template <typename MetricFields>
  void SetValue(MetricFields&& metric_fields, Value value) {
    if (metric_) {
      metric_->SetValue(&context_, std::forward<MetricFields>(metric_fields), std::move(value));
    }
  }

  template <typename MetricFields>
  void AddToInt(MetricFields&& metric_fields, int64_t const delta) {
    if (metric_) {
      metric_->AddToInt(&context_, std::forward<MetricFields>(metric_fields), delta);
    }
  }

  template <typename MetricFields>
  void AddToDistribution(MetricFields&& metric_fields, double const sample, size_t const times) {
    if (metric_) {
      metric_->AddToDistribution(&context_, std::forward<MetricFields>(metric_fields), sample,
                                 times);
    }
  }

  bool DeleteValue(FieldMap const& metric_fields) {
    if (metric_) {
      return metric_->DeleteValue(&context_, metric_fields);
    } else {
      return false;
    }
  }

  bool Clear() {
    if (metric_) {
      return metric_->Clear(&context_);
    } else {
      return false;
    }
  }

 private:
  MetricProxy(MetricProxy const&) = delete;
  MetricProxy& operator=(MetricProxy const&) = delete;

  std::shared_ptr<MetricManager> manager_;
  std::shared_ptr<Metric> metric_;
  MetricContext mutable context_;
};

}  // namespace internal
}  // namespace tsz

#endif  // __TSDB2_TSZ_INTERNAL_METRIC_PROXY_H__
