#ifndef __TSDB2_TSZ_INTERNAL_THROW_AWAY_METRIC_PROXY_H__
#define __TSDB2_TSZ_INTERNAL_THROW_AWAY_METRIC_PROXY_H__

#include <memory>

#include "absl/base/thread_annotations.h"
#include "absl/time/time.h"
#include "tsz/internal/metric.h"
#include "tsz/internal/metric_proxy.h"

namespace tsz {
namespace internal {

class ThrowAwayMetricContext final {
 public:
  class Closure {
   public:
    explicit Closure(ThrowAwayMetricContext *const context) : context_(context) {}
    ~Closure() { context_->Close(); }

   private:
    Closure(Closure const &) = delete;
    Closure &operator=(Closure const &) = delete;
    Closure(Closure &&) = delete;
    Closure &operator=(Closure &&) = delete;

    ThrowAwayMetricContext *const context_;
  };

  explicit ThrowAwayMetricContext(std::shared_ptr<Metric> const &metric, absl::Time const time)
      : metric_(metric.get()), time_(time) {
    metric_->Pin();
  }

  absl::Time time() const { return time_; }

 private:
  ThrowAwayMetricContext(ThrowAwayMetricContext const &) = delete;
  ThrowAwayMetricContext &operator=(ThrowAwayMetricContext const &) = delete;
  ThrowAwayMetricContext(ThrowAwayMetricContext &&other) = delete;
  ThrowAwayMetricContext &operator=(ThrowAwayMetricContext &&other) = delete;

  void Close() ABSL_NO_THREAD_SAFETY_ANALYSIS {
    auto const metric = metric_;
    metric_ = nullptr;
    metric->UnpinLocked();
  }

  Metric *metric_;
  absl::Time time_;
};

using ThrowAwayMetricProxy = MetricProxy<ThrowAwayMetricContext>;

}  // namespace internal
}  // namespace tsz

#endif  // __TSDB2_TSZ_INTERNAL_THROW_AWAY_METRIC_PROXY_H__
