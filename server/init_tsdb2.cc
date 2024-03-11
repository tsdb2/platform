#include "server/init_tsdb2.h"

#include "absl/debugging/failure_signal_handler.h"
#include "absl/flags/parse.h"
#include "absl/log/initialize.h"
#include "absl/synchronization/notification.h"
#include "http/http_node.h"
#include "net/sockets.h"

namespace tsdb2 {
namespace init {

using tsdb2::http::HttpNode;
using tsdb2::net::SelectServer;

namespace {

absl::Notification init_done;

}  // namespace

void InitServer(int argc, char* argv[]) {
  absl::InitializeLog();
  absl::InstallFailureSignalHandler(absl::FailureSignalHandlerOptions());
  absl::ParseCommandLine(argc, argv);
  SelectServer::GetInstance();
  HttpNode::GetDefault();
  init_done.Notify();
}

void Wait() { init_done.WaitForNotification(); }
bool IsDone() { return init_done.HasBeenNotified(); }

}  // namespace init
}  // namespace tsdb2
