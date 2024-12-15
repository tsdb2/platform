#include "common/default_scheduler.h"

#include <cstdint>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/no_destructor.h"
#include "common/scheduler.h"
#include "common/singleton.h"
#include "server/base_module.h"
#include "server/init_tsdb2.h"

namespace {
#ifdef NDEBUG
uint16_t constexpr kDefaultNumBackgroundWorkers = 10;
#else
uint16_t constexpr kDefaultNumBackgroundWorkers = 1;
#endif
}  // namespace

ABSL_FLAG(uint16_t, num_background_workers, kDefaultNumBackgroundWorkers,
          "Number of worker threads in the default scheduler.");

namespace tsdb2 {
namespace common {

Singleton<Scheduler> default_scheduler{[] {
  return new Scheduler(Scheduler::Options{
      .num_workers = absl::GetFlag(FLAGS_num_background_workers),
      .start_now = true,
  });
}};

NoDestructor<DefaultSchedulerModule> DefaultSchedulerModule::instance_;

DefaultSchedulerModule::DefaultSchedulerModule() : tsdb2::init::BaseModule("default_scheduler") {
  tsdb2::init::RegisterModule(this);
}

absl::Status DefaultSchedulerModule::Initialize() {
  default_scheduler.Get();
  return absl::OkStatus();
}

}  // namespace common
}  // namespace tsdb2
