#include "net/sockets.h"

#include <utility>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "common/reffed_ptr.h"
#include "common/sequence_number.h"
#include "common/simple_condition.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::testing::Not;
using ::testing::status::IsOk;
using ::testing::status::IsOkAndHolds;
using ::tsdb2::common::reffed_ptr;
using ::tsdb2::common::SimpleCondition;
using ::tsdb2::net::ListenerSocket;
using ::tsdb2::net::SelectServer;
using ::tsdb2::net::Socket;

ABSL_CONST_INIT ::tsdb2::common::SequenceNumber port_number_generator{1024};

uint16_t GetNewPort() { return static_cast<uint16_t>(port_number_generator.GetNext()); }

class SocketTest : public ::testing::Test {
 public:
  explicit SocketTest() { select_server_->StartOrDie(); }

 protected:
  SelectServer* const select_server_ = SelectServer::GetInstance();
};

TEST_F(SocketTest, Listen) {
  EXPECT_THAT(select_server_->CreateSocket<ListenerSocket>(
                  ListenerSocket::kInetSocketTag, "::1", GetNewPort(),
                  [](absl::StatusOr<reffed_ptr<Socket>>) { FAIL(); }),
              IsOkAndHolds(Not(nullptr)));
}

TEST_F(SocketTest, ConnectInetSocket) {
  uint16_t const port = GetNewPort();
  absl::Mutex server_mutex;
  reffed_ptr<Socket> socket;
  auto status_or_listener = select_server_->CreateSocket<ListenerSocket>(
      ListenerSocket::kInetSocketTag, "::1", port,
      [&](absl::StatusOr<reffed_ptr<Socket>> status_or_socket) {
        absl::MutexLock lock{&server_mutex};
        if (socket) {
          EXPECT_THAT(status_or_socket, Not(IsOk()));
          socket = nullptr;
        } else {
          ASSERT_THAT(status_or_socket, IsOkAndHolds(Not(nullptr)));
          socket = std::move(status_or_socket).value();
        }
      });
  ASSERT_THAT(status_or_listener, IsOkAndHolds(Not(nullptr)));
  absl::Mutex client_mutex;
  bool connected = false;
  auto status_or_socket = select_server_->CreateSocket<Socket>(
      Socket::kInetSocketTag, "::1", port, [&](absl::Status status) {
        ASSERT_OK(status);
        absl::MutexLock lock{&client_mutex};
        ASSERT_FALSE(connected);
        connected = true;
      });
  ASSERT_THAT(status_or_socket, IsOkAndHolds(Not(nullptr)));
  absl::MutexLock(&server_mutex, SimpleCondition([&] { return socket.operator bool(); }));
  absl::MutexLock(&client_mutex, absl::Condition(&connected));
}

// TODO

}  // namespace
