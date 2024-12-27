#include "common/default_scheduler.h"

#include <cstdint>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "common/scheduler.h"
#include "common/singleton.h"
#include "server/module.h"

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

static tsdb2::init::Module<DefaultSchedulerModule> const default_scheduler_module;

absl::Status
DefaultSchedulerModule::Initialize() {  // NOLINT(readability-convert-member-functions-to-static)
  default_scheduler.Get();
  return absl::OkStatus();
}

}  // namespace common
}  // namespace tsdb2
