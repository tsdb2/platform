#ifndef __TSDB2_TSZ_INTERNAL_SCOPED_METRIC_PROXY_H__
#define __TSDB2_TSZ_INTERNAL_SCOPED_METRIC_PROXY_H__

#include <algorithm>
#include <memory>

#include "absl/time/time.h"
#include "tsz/internal/metric.h"
#include "tsz/internal/metric_proxy.h"

namespace tsz {
namespace internal {

class ScopedMetricContext final {
 public:
  struct Closure final {
    explicit Closure(ScopedMetricContext* const context) {}
  };

  explicit ScopedMetricContext() : metric_(nullptr) {}

  explicit ScopedMetricContext(std::shared_ptr<Metric> const& metric, absl::Time const time)
      : metric_(metric.get()), time_(time) {
    if (metric_ != nullptr) {
      metric_->Pin();
    }
  }

  ~ScopedMetricContext() { MaybeUnpin(); }

  ScopedMetricContext(ScopedMetricContext&& other) noexcept
      : metric_(other.metric_), time_(other.time_) {
    other.metric_ = nullptr;
  }

  ScopedMetricContext& operator=(ScopedMetricContext&& other) noexcept {
    if (this != &other) {
      MaybeUnpin();
      metric_ = other.metric_;
      other.metric_ = nullptr;
      time_ = other.time_;
    }
    return *this;
  }

  void swap(ScopedMetricContext& other) {
    std::swap(metric_, other.metric_);
    std::swap(time_, other.time_);
  }

  friend void swap(ScopedMetricContext& lhs, ScopedMetricContext& rhs) { lhs.swap(rhs); }

  [[nodiscard]] bool empty() const { return metric_ == nullptr; }
  explicit operator bool() const { return metric_ != nullptr; }
  absl::Time time() const { return time_; }

 private:
  ScopedMetricContext(ScopedMetricContext const&) = delete;
  ScopedMetricContext& operator=(ScopedMetricContext const&) = delete;

  void MaybeUnpin() {
    if (metric_ != nullptr) {
      metric_->Unpin();
    }
  }

  Metric* metric_;
  absl::Time time_;
};

using ScopedMetricProxy = MetricProxy<ScopedMetricContext>;

}  // namespace internal
}  // namespace tsz

#endif  // __TSDB2_TSZ_INTERNAL_SCOPED_METRIC_PROXY_H__
