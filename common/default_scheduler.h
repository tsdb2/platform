#ifndef __TSDB2_COMMON_DEFAULT_SCHEDULER_H__
#define __TSDB2_COMMON_DEFAULT_SCHEDULER_H__

#include "common/scheduler.h"
#include "common/singleton.h"

namespace tsdb2 {
namespace common {

// The default scheduler instance.
//
// The number of worker threads for this instance is provided in the `--num_background_workers`
// command line flag.
//
// The scheduler is constructed and started the first time the singleton is accessed.
extern Singleton<Scheduler> default_scheduler;

}  // namespace common
}  // namespace tsdb2

#endif  //  __TSDB2_COMMON_DEFAULT_SCHEDULER_H__
