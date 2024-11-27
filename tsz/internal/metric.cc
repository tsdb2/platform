#include "tsz/internal/metric.h"

#include <cstdint>
#include <string>

#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "tsz/types.h"

namespace tsz {
namespace internal {

bool Metric::Unpin() {
  absl::WriterMutexLock lock{&mutex_};
  return UnpinLocked();
}

absl::StatusOr<Value> Metric::GetValue(FieldMap const &metric_fields) const {
  FieldMapView const metric_field_view{metric_fields};
  absl::ReaderMutexLock lock{&mutex_};
  auto const it = cells_.find(metric_field_view);
  if (it != cells_.end()) {
    return it->value();
  } else {
    return absl::NotFoundError("value not found");
  }
}

bool Metric::ResetIfCumulative(absl::Time const start_time) {
  if (!config_.cumulative) {
    return false;
  }
  absl::WriterMutexLock lock{&mutex_};
  for (auto const &cell : cells_) {
    const_cast<Cell &>(cell).Reset(start_time);
  }
  return true;
}

bool Metric::UnpinLocked() {
  auto const result = pin_count_.Unref();
  if (result && cells_.empty()) {
    manager_->DeleteMetricInternal(name_);
  }
  return result;
}

}  // namespace internal
}  // namespace tsz
