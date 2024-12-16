#include "http/channel_listener.h"

#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "common/reffed_ptr.h"
#include "common/testing.h"
#include "gtest/gtest.h"
#include "http/channel.h"
#include "http/mock_channel_manager.h"
#include "net/base_sockets.h"
#include "net/sockets.h"
#include "net/testing.h"
#include "server/testing.h"

namespace {

using ::tsdb2::common::reffed_ptr;
using ::tsdb2::http::Channel;
using ::tsdb2::http::ChannelListener;
using ::tsdb2::net::kUnixDomainSocketTag;
using ::tsdb2::net::Socket;
using ::tsdb2::net::testing::MakeTestSocketPath;
using ::tsdb2::testing::http::MockChannelManager;

class ChannelListenerTest : public tsdb2::testing::init::Test {
 protected:
  MockChannelManager manager_;
};

TEST_F(ChannelListenerTest, Accept) {
  auto const socket_path = MakeTestSocketPath();
  absl::Notification done;
  auto const status_or_listener = ChannelListener<Socket>::Create(
      kUnixDomainSocketTag, socket_path,
      +[](void *const arg,
          absl::StatusOr<tsdb2::common::reffed_ptr<Channel<Socket>>> status_or_socket) {
        absl::Notification &done = *reinterpret_cast<absl::Notification *>(arg);
        done.Notify();
      },
      &done, &manager_);
  EXPECT_OK(status_or_listener);
  auto const status_or_peer = Socket::Create(
      kUnixDomainSocketTag, socket_path,
      +[](reffed_ptr<Socket> socket, absl::Status status) { EXPECT_OK(status); });
  EXPECT_OK(status_or_peer);
  done.WaitForNotification();
}

}  // namespace
