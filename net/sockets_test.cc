#include "net/sockets.h"

#include <string_view>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "common/buffer.h"
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
using ::tsdb2::common::Buffer;
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
  static void TransferData(reffed_ptr<Socket> const& client_socket,
                           reffed_ptr<Socket> const& server_socket, std::string_view data);

  SelectServer* const select_server_ = SelectServer::GetInstance();
};

TEST_F(SocketTest, Listen) {
  EXPECT_THAT(select_server_->CreateSocket<ListenerSocket>(
                  ListenerSocket::kInetSocketTag, "::1", GetNewPort(),
                  [](absl::StatusOr<reffed_ptr<Socket>>) { FAIL(); }),
              IsOkAndHolds(Not(nullptr)));
}

enum class ListenerState {
  kListening = 0,
  kAccepted = 1,
  kShuttingDown = 2,
};

ListenerState& operator++(ListenerState& value) {
  return value = static_cast<ListenerState>(static_cast<uint8_t>(value) + 1);
}

ListenerState operator++(ListenerState& value, int) {
  ListenerState original = value;
  ++value;
  return original;
}

void SocketTest::TransferData(reffed_ptr<Socket> const& client_socket,
                              reffed_ptr<Socket> const& server_socket,
                              std::string_view const data) {
  absl::Notification write_notification;
  Buffer buffer{data.size()};
  buffer.MemCpy(data.data(), data.size());
  ASSERT_OK(client_socket->Write(std::move(buffer), [&](absl::Status status) {
    ASSERT_FALSE(write_notification.HasBeenNotified());
    ASSERT_OK(status);
    write_notification.Notify();
  }));
  absl::Notification read_notification;
  ASSERT_OK(server_socket->Read(data.size(), [&](absl::StatusOr<Buffer> status_or_buffer) {
    ASSERT_FALSE(read_notification.HasBeenNotified());
    ASSERT_OK(status_or_buffer);
    auto const& buffer = status_or_buffer.value();
    ASSERT_EQ(buffer.size(), data.size());
    ASSERT_EQ(data, std::string_view(buffer.as_char_array(), buffer.size()));
    read_notification.Notify();
  }));
  write_notification.WaitForNotification();
  read_notification.WaitForNotification();
}

TEST_F(SocketTest, InetSocket) {
  uint16_t const port = GetNewPort();
  absl::Mutex server_mutex;
  ListenerState server_state = ListenerState::kListening;
  reffed_ptr<Socket> server_socket;
  auto status_or_listener = select_server_->CreateSocket<ListenerSocket>(
      ListenerSocket::kInetSocketTag, "::1", port,
      [&](absl::StatusOr<reffed_ptr<Socket>> status_or_socket) {
        absl::MutexLock lock{&server_mutex};
        switch (server_state++) {
          case ListenerState::kListening:
            ASSERT_THAT(status_or_socket, IsOkAndHolds(Not(nullptr)));
            server_socket = std::move(status_or_socket).value();
            break;
          case ListenerState::kAccepted:
            ASSERT_THAT(status_or_socket, Not(IsOk()));
            server_socket = nullptr;
            break;
          default:
            FAIL();
            break;
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
  auto const client_socket = std::move(status_or_socket).value();
  absl::MutexLock(&server_mutex, SimpleCondition([&] { return server_socket.operator bool(); }));
  absl::MutexLock(&client_mutex, absl::Condition(&connected));
  std::string_view constexpr kClientToServerData = "client to server";
  TransferData(client_socket, server_socket, "lorem ipsum");
  TransferData(server_socket, client_socket, "dolor sit amet");
}

// TODO

}  // namespace
