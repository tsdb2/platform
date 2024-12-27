#include "server/init_tsdb2.h"

#include <initializer_list>

#include "absl/base/attributes.h"
#include "absl/base/const_init.h"
#include "absl/base/thread_annotations.h"
#include "absl/debugging/failure_signal_handler.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/synchronization/mutex.h"
#include "server/base_module.h"
#include "server/module_manager.h"

#ifndef NDEBUG
#include "absl/debugging/symbolize.h"
#endif  // NDEBUG

namespace tsdb2 {
namespace init {

namespace {

ABSL_CONST_INIT absl::Mutex init_mutex{absl::kConstInit};
bool init_done ABSL_GUARDED_BY(init_mutex) = false;

}  // namespace

void RegisterModule(BaseModule* const module,
                    std::initializer_list<ModuleDependency> const dependencies) {
  ModuleManager::GetInstance()->RegisterModule(module, dependencies);
}

void InitServer(int argc, char* argv[]) {
  absl::MutexLock lock{&init_mutex};
  if (init_done) {
    return;
  }
#ifndef NDEBUG
  absl::InitializeSymbolizer(argv[0]);
#endif  // NDEBUG
  absl::InitializeLog();
  absl::InstallFailureSignalHandler(absl::FailureSignalHandlerOptions());
  absl::ParseCommandLine(argc, argv);
  CHECK_OK(ModuleManager::GetInstance()->InitializeModules()) << "module initialization failed";
  init_done = true;
}

void InitForTesting() {
  absl::MutexLock lock{&init_mutex};
  if (init_done) {
    return;
  }
  // NOTE:
  //
  //  * we don't need to initialize Abseil logs because Googletest does it for us;
  //  * we don't need to install the failure signal handler because the `//common:testing` target
  //    linked in all tests already does it;
  //  * we'd actually need to parse the command line flags but we don't have argc+argv.
  //
  // So the only thing we can do here is initialize our modules and notify the `init_done` flag.
  CHECK_OK(ModuleManager::GetInstance()->InitializeModulesForTesting())
      << "module initialization failed";
  init_done = true;
}

void Wait() ABSL_LOCKS_EXCLUDED(init_mutex) {
  absl::MutexLock(&init_mutex, absl::Condition(&init_done));
}

bool IsDone() ABSL_LOCKS_EXCLUDED(init_mutex) {
  absl::MutexLock lock{&init_mutex};
  return init_done;
}

}  // namespace init
}  // namespace tsdb2
