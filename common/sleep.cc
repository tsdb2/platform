#include "common/sleep.h"

#include <utility>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "common/default_scheduler.h"
#include "common/promise.h"

namespace tsdb2 {
namespace common {

Promise<void> Sleep(absl::Duration const duration) {
  return Promise<void>([duration](auto resolve) {
    default_scheduler->ScheduleIn(
        [resolve = std::move(resolve)]() mutable { resolve(absl::OkStatus()); }, duration);
  });
}

}  // namespace common
}  // namespace tsdb2
