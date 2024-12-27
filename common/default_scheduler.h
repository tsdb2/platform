#ifndef __TSDB2_COMMON_DEFAULT_SCHEDULER_H__
#define __TSDB2_COMMON_DEFAULT_SCHEDULER_H__

#include <string_view>

#include "absl/status/status.h"
#include "common/scheduler.h"
#include "common/singleton.h"

namespace tsdb2 {
namespace common {

// The default scheduler instance.
//
// The number of worker threads for this instance is provided in the `--num_background_workers`
// command line flag.
extern Singleton<Scheduler> default_scheduler;

struct DefaultSchedulerModule {
  static std::string_view constexpr name = "default_scheduler";
  absl::Status Initialize();
};

}  // namespace common
}  // namespace tsdb2

#endif  //  __TSDB2_COMMON_DEFAULT_SCHEDULER_H__
