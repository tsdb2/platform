#include "net/sockets.h"

#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>

#include <random>
#include <string_view>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "common/buffer.h"
#include "common/reffed_ptr.h"
#include "common/simple_condition.h"
#include "common/testing.h"
#include "common/utilities.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using tsdb2::common::Buffer;
using tsdb2::common::reffed_ptr;
using tsdb2::common::SimpleCondition;
using tsdb2::net::KeepAliveParams;
using tsdb2::net::kInetSocketTag;
using tsdb2::net::kLocalHost;
using tsdb2::net::kUnixDomainSocketTag;
using tsdb2::net::ListenerSocket;
using tsdb2::net::SelectServer;
using tsdb2::net::Socket;
using tsdb2::net::SocketOptions;

using ::testing::AllOf;
using ::testing::Field;
using ::testing::Not;
using ::testing::Property;
using ::testing::status::IsOk;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

uint16_t GetNewPort() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(1024, 65535);
  return dis(gen);
}

class SocketTest : public ::testing::Test {
 public:
  explicit SocketTest() { select_server_->StartOrDie(); }

 protected:
  static Socket::ReadCallback ReadCallbackAdapter(
      absl::AnyInvocable<void(absl::StatusOr<Buffer>)> callback,
      absl::Status result = absl::OkStatus()) {
    return [status = std::move(result),
            callback = std::move(callback)](absl::StatusOr<Buffer> status_or_buffer) mutable {
      absl::Status result = status_or_buffer.ok() ? status : status_or_buffer.status();
      callback(std::move(status_or_buffer));
      return result;
    };
  }

  static Socket::WriteCallback WriteCallbackAdapter(absl::AnyInvocable<void(absl::Status)> callback,
                                                    absl::Status result = absl::OkStatus()) {
    return
        [result = std::move(result), callback = std::move(callback)](absl::Status status) mutable {
          callback(status);
          return status.ok() ? status : result;
        };
  }

  static void TransferData(reffed_ptr<Socket> const& client_socket,
                           reffed_ptr<Socket> const& server_socket, std::string_view data);

  static uint16_t const port_;
  SelectServer* const select_server_ = SelectServer::GetInstance();
};

void SocketTest::TransferData(reffed_ptr<Socket> const& client_socket,
                              reffed_ptr<Socket> const& server_socket,
                              std::string_view const data) {
  absl::Notification write_notification;
  Buffer buffer{data.size()};
  buffer.MemCpy(data.data(), data.size());
  ASSERT_OK(client_socket->Write(std::move(buffer), WriteCallbackAdapter([&](absl::Status status) {
                                   ASSERT_FALSE(write_notification.HasBeenNotified());
                                   ASSERT_OK(status);
                                   write_notification.Notify();
                                 })));
  absl::Notification read_notification;
  ASSERT_OK(server_socket->Read(
      data.size(), ReadCallbackAdapter([&](absl::StatusOr<Buffer> status_or_buffer) {
        ASSERT_FALSE(read_notification.HasBeenNotified());
        ASSERT_OK(status_or_buffer);
        auto const& buffer = status_or_buffer.value();
        ASSERT_EQ(buffer.size(), data.size());
        ASSERT_EQ(data, std::string_view(buffer.as_char_array(), buffer.size()));
        read_notification.Notify();
      })));
  write_notification.WaitForNotification();
  read_notification.WaitForNotification();
}

uint16_t const SocketTest::port_ = GetNewPort();

TEST_F(SocketTest, InvalidAcceptCallback) {
  EXPECT_THAT(select_server_->CreateSocket<ListenerSocket<Socket>>(kInetSocketTag, "", GetNewPort(),
                                                                   SocketOptions(), nullptr),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(select_server_->CreateSocket<ListenerSocket<Socket>>(kUnixDomainSocketTag,
                                                                   "/tmp/socket", nullptr),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(SocketTest, PortCollision) {
  auto const port = GetNewPort();
  auto const listener = select_server_->CreateSocket<ListenerSocket<Socket>>(
      kInetSocketTag, kLocalHost, port, SocketOptions(),
      [](absl::StatusOr<reffed_ptr<Socket>>) { FAIL(); });
  ASSERT_THAT(listener, IsOkAndHolds(Not(nullptr)));
  EXPECT_THAT(select_server_->CreateSocket<ListenerSocket<Socket>>(
                  kInetSocketTag, kLocalHost, port, SocketOptions(),
                  [](absl::StatusOr<reffed_ptr<Socket>>) { FAIL(); }),
              Not(IsOk()));
}

TEST_F(SocketTest, Listen) {
  EXPECT_THAT(select_server_->CreateSocket<ListenerSocket<Socket>>(
                  kInetSocketTag, kLocalHost, GetNewPort(), SocketOptions(),
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

class TestInetConnection {
 public:
  explicit TestInetConnection(SelectServer* const select_server, SocketOptions const& options) {
    MakeConnection(select_server, options);
  }

  reffed_ptr<Socket>& server_socket() { return server_socket_; }
  reffed_ptr<Socket> const& server_socket() const { return server_socket_; }

  reffed_ptr<Socket>& client_socket() { return client_socket_; }
  reffed_ptr<Socket> const& client_socket() const { return client_socket_; }

 private:
  TestInetConnection(TestInetConnection const&) = delete;
  TestInetConnection& operator=(TestInetConnection const&) = delete;
  TestInetConnection(TestInetConnection&&) = delete;
  TestInetConnection& operator=(TestInetConnection&&) = delete;

  void AcceptCallback(absl::StatusOr<reffed_ptr<Socket>> status_or_socket)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock{&mutex_};
    switch (state_++) {
      case ListenerState::kListening:
        ASSERT_THAT(status_or_socket, IsOkAndHolds(Not(nullptr)));
        server_socket_ = std::move(status_or_socket).value();
        break;
      case ListenerState::kAccepted:
        ASSERT_THAT(status_or_socket, Not(IsOk()));
        server_socket_ = nullptr;
        break;
      default:
        FAIL();
        break;
    }
  }

  void MakeConnection(SelectServer* const select_server, SocketOptions const& options) {
    auto const port = GetNewPort();
    auto status_or_listener = select_server->CreateSocket<ListenerSocket<Socket>>(
        kInetSocketTag, kLocalHost, port, options,
        absl::bind_front(&TestInetConnection::AcceptCallback, this));
    ASSERT_THAT(status_or_listener, IsOkAndHolds(Not(nullptr)));
    listener_ = std::move(status_or_listener).value();
    absl::Notification connected;
    auto status_or_socket = select_server->CreateSocket<Socket>(
        kInetSocketTag, kLocalHost, port, options, [&](absl::Status const status) {
          ASSERT_OK(status);
          ASSERT_FALSE(connected.HasBeenNotified());
          connected.Notify();
        });
    ASSERT_THAT(status_or_socket, IsOkAndHolds(Not(nullptr)));
    client_socket_ = std::move(status_or_socket).value();
    absl::MutexLock(&mutex_, SimpleCondition([&]() ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
      return state_ == ListenerState::kAccepted;
    }));
    connected.WaitForNotification();
  }

  reffed_ptr<ListenerSocket<Socket>> listener_;

  absl::Mutex mutable mutex_;
  ListenerState state_ ABSL_GUARDED_BY(mutex_) = ListenerState::kListening;
  reffed_ptr<Socket> server_socket_ ABSL_GUARDED_BY(mutex_);
  reffed_ptr<Socket> client_socket_;
};

class SocketSettingsTest : public SocketTest,
                           public ::testing::WithParamInterface<SocketOptions> {};

TEST_P(SocketSettingsTest, Settings) {
  SocketOptions const& options = GetParam();
  TestInetConnection connection{select_server_, options};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  EXPECT_THAT(server_socket->ip_tos(), IsOkAndHolds(*(options.ip_tos)));
  EXPECT_THAT(client_socket->ip_tos(), IsOkAndHolds(*(options.ip_tos)));
  EXPECT_THAT(server_socket->is_keep_alive(), IsOkAndHolds(options.keep_alive));
  EXPECT_THAT(client_socket->is_keep_alive(), IsOkAndHolds(options.keep_alive));
  if (options.keep_alive) {
    EXPECT_THAT(
        server_socket->keep_alive_params(),
        IsOkAndHolds(AllOf(Field(&KeepAliveParams::idle, options.keep_alive_params.idle),
                           Field(&KeepAliveParams::interval, options.keep_alive_params.interval),
                           Field(&KeepAliveParams::count, options.keep_alive_params.count))));
    EXPECT_THAT(
        client_socket->keep_alive_params(),
        IsOkAndHolds(AllOf(Field(&KeepAliveParams::idle, options.keep_alive_params.idle),
                           Field(&KeepAliveParams::interval, options.keep_alive_params.interval),
                           Field(&KeepAliveParams::count, options.keep_alive_params.count))));
  } else {
    EXPECT_THAT(server_socket->keep_alive_params(), Not(IsOk()));
    EXPECT_THAT(client_socket->keep_alive_params(), Not(IsOk()));
  }
}

INSTANTIATE_TEST_SUITE_P(SocketSettingsTest, SocketSettingsTest,
                         ::testing::Values(
                             SocketOptions{
                                 .keep_alive = false,
                                 .ip_tos = IPTOS_LOWDELAY + (2 << 5),
                             },
                             SocketOptions{
                                 .keep_alive = true,
                                 .keep_alive_params{
                                     .idle = absl::Seconds(90),
                                     .interval = absl::Seconds(10),
                                     .count = 42,
                                 },
                                 .ip_tos = IPTOS_THROUGHPUT + (3 << 5),
                             }));

class TransferTest : public SocketTest, public ::testing::WithParamInterface<SocketOptions> {};

TEST_P(TransferTest, InetSocket) {
  TestInetConnection connection{select_server_, GetParam()};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  TransferData(client_socket, server_socket, "lorem ipsum");
  TransferData(server_socket, client_socket, "dolor sit amet");
}

INSTANTIATE_TEST_SUITE_P(TransferTest, TransferTest,
                         ::testing::Values(SocketOptions{.keep_alive = false},
                                           SocketOptions{.keep_alive = true}));

TEST_F(SocketTest, ReadValidation) {
  TestInetConnection connection{select_server_, SocketOptions()};
  EXPECT_THAT(connection.server_socket()->Read(
                  0, ReadCallbackAdapter([](absl::StatusOr<Buffer>) { FAIL(); })),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(connection.server_socket()->Read(10, nullptr),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(SocketTest, WriteValidation) {
  TestInetConnection connection{select_server_, SocketOptions()};
  EXPECT_THAT(connection.server_socket()->Write(Buffer(10),
                                                WriteCallbackAdapter([](absl::Status) { FAIL(); })),
              StatusIs(absl::StatusCode::kInvalidArgument));
  Buffer buffer{10};
  buffer.Advance(10);
  EXPECT_THAT(connection.server_socket()->Write(std::move(buffer), nullptr),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(SocketTest, ClientHangUp) {
  TestInetConnection connection{select_server_, SocketOptions()};
  auto const& server_socket = connection.server_socket();
  absl::Notification done;
  ASSERT_OK(server_socket->Read(
      10, ReadCallbackAdapter([&](absl::StatusOr<Buffer> const status_or_buffer) {
        EXPECT_THAT(status_or_buffer, Not(IsOk()));
        done.Notify();
      })));
  connection.client_socket().reset();
  done.WaitForNotification();
  EXPECT_THAT(server_socket->Read(10, ReadCallbackAdapter([](absl::StatusOr<Buffer>) { FAIL(); })),
              Not(IsOk()));
}

TEST_F(SocketTest, ServerHangUp) {
  TestInetConnection connection{select_server_, SocketOptions()};
  absl::Notification done;
  ASSERT_OK(connection.client_socket()->Read(
      10, ReadCallbackAdapter([&](absl::StatusOr<Buffer> const status_or_buffer) {
        EXPECT_THAT(status_or_buffer, Not(IsOk()));
        done.Notify();
      })));
  connection.server_socket().reset();
  done.WaitForNotification();
}

TEST_F(SocketTest, CancelRead) {
  TestInetConnection connection{select_server_, SocketOptions()};
  auto const& server_socket = connection.server_socket();
  absl::Notification done;
  ASSERT_OK(server_socket->Read(
      10, ReadCallbackAdapter([&](absl::StatusOr<Buffer> const status_or_buffer) {
        EXPECT_THAT(status_or_buffer, Not(IsOk()));
        done.Notify();
      })));
  EXPECT_TRUE(server_socket->CancelRead());
  done.WaitForNotification();
  EXPECT_THAT(server_socket->Read(10, ReadCallbackAdapter([](absl::StatusOr<Buffer>) { FAIL(); })),
              Not(IsOk()));
}

TEST_F(SocketTest, TwoChunks) {
  TestInetConnection connection{select_server_, SocketOptions()};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  std::string_view constexpr kChunk1 = "01234567890123456789";
  std::string_view constexpr kChunk2 = "987654321098765432109876543210";
  absl::Notification read_notification;
  absl::Notification write_notification;
  ASSERT_OK(server_socket->Read(
      20, ReadCallbackAdapter([&](absl::StatusOr<Buffer> const status_or_buffer) {
        ASSERT_THAT(status_or_buffer, IsOkAndHolds(Property(&Buffer::size, 20)));
        auto const& buffer = status_or_buffer.value();
        std::string_view const chunk1{buffer.as_char_array(), buffer.size()};
        EXPECT_EQ(chunk1, kChunk1);
        EXPECT_OK(server_socket->Read(
            30, ReadCallbackAdapter([&](absl::StatusOr<Buffer> const status_or_buffer) {
              EXPECT_THAT(status_or_buffer, IsOkAndHolds(Property(&Buffer::size, 30)));
              auto const& buffer = status_or_buffer.value();
              std::string_view const chunk2{buffer.as_char_array(), buffer.size()};
              EXPECT_EQ(chunk2, kChunk2);
              read_notification.Notify();
            })));
      })));
  Buffer buffer1{kChunk1.size()};
  buffer1.MemCpy(kChunk1.data(), kChunk1.size());
  ASSERT_OK(client_socket->Write(
      std::move(buffer1), WriteCallbackAdapter([&](absl::Status const status) {
        ASSERT_OK(status);
        Buffer buffer2{kChunk2.size()};
        buffer2.MemCpy(kChunk2.data(), kChunk2.size());
        EXPECT_OK(client_socket->Write(std::move(buffer2),
                                       WriteCallbackAdapter([&](absl::Status const status) {
                                         EXPECT_OK(status);
                                         write_notification.Notify();
                                       })));
      })));
  read_notification.WaitForNotification();
  write_notification.WaitForNotification();
}

}  // namespace
