#include "server/init_tsdb2.h"

#include "absl/flags/parse.h"
#include "absl/log/initialize.h"
#include "absl/synchronization/notification.h"
#include "net/http_node.h"
#include "net/sockets.h"

namespace tsdb2 {
namespace init {

using ::tsdb2::net::HttpNode;
using ::tsdb2::net::SelectServer;

namespace {

absl::Notification init_done;

}  // namespace

void InitServer(int argc, char* argv[]) {
  absl::InitializeLog();
  absl::ParseCommandLine(argc, argv);
  SelectServer::GetInstance()->StartOrDie();
  HttpNode::GetDefault();
  init_done.Notify();
}

void Wait() { init_done.WaitForNotification(); }
bool IsDone() { return init_done.HasBeenNotified(); }

}  // namespace init
}  // namespace tsdb2
