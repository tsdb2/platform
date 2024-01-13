#include "server/init_tsdb2.h"

#include "absl/flags/parse.h"
#include "absl/synchronization/notification.h"

namespace tsdb2 {
namespace init {

namespace {

absl::Notification init_done;

}  // namespace

void InitServer(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);
  init_done.Notify();
}

void Wait() { init_done.WaitForNotification(); }
bool IsDone() { return init_done.HasBeenNotified(); }

}  // namespace init
}  // namespace tsdb2
