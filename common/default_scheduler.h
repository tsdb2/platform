#ifndef __TSDB2_COMMON_DEFAULT_SCHEDULER_H__
#define __TSDB2_COMMON_DEFAULT_SCHEDULER_H__

#include "absl/status/status.h"
#include "common/scheduler.h"
#include "common/singleton.h"
#include "no_destructor.h"
#include "server/base_module.h"

namespace tsdb2 {
namespace common {

// The default scheduler instance.
//
// The number of worker threads for this instance is provided in the `--num_background_workers`
// command line flag.
extern Singleton<Scheduler> default_scheduler;

class DefaultSchedulerModule : public tsdb2::init::BaseModule {
 public:
  static DefaultSchedulerModule *Get() { return instance_.Get(); }

  absl::Status Initialize() override;

 private:
  friend class NoDestructor<DefaultSchedulerModule>;
  static NoDestructor<DefaultSchedulerModule> instance_;

  explicit DefaultSchedulerModule();
};

}  // namespace common
}  // namespace tsdb2

#endif  //  __TSDB2_COMMON_DEFAULT_SCHEDULER_H__
