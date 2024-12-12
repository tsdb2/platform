#include "net/sockets.h"

#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "common/default_scheduler.h"
#include "common/flag_override.h"
#include "common/mock_clock.h"
#include "common/no_destructor.h"
#include "common/reffed_ptr.h"
#include "common/scheduler.h"
#include "common/scoped_override.h"
#include "common/sequence_number.h"
#include "common/simple_condition.h"
#include "common/singleton.h"
#include "common/stats_counter.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "net/base_sockets.h"
#include "net/ssl_sockets.h"
#include "server/testing.h"

ABSL_FLAG(bool, socket_test_use_random_ports, false,
          "Use random TCP/IP ports instead of Unix socket pairs for some of the socket tests.");

namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::GetTestTmpDir;
using ::testing::Not;
using ::testing::Pointee2;
using ::testing::Property;
using ::tsdb2::common::MockClock;
using ::tsdb2::common::reffed_ptr;
using ::tsdb2::common::Scheduler;
using ::tsdb2::common::ScopedOverride;
using ::tsdb2::common::SimpleCondition;
using ::tsdb2::common::testing::FlagOverride;
using ::tsdb2::net::BaseListenerSocket;
using ::tsdb2::net::BaseSocket;
using ::tsdb2::net::Buffer;
using ::tsdb2::net::FD;
using ::tsdb2::net::KeepAliveParams;
using ::tsdb2::net::kInetSocketTag;
using ::tsdb2::net::kLocalHost;
using ::tsdb2::net::kUnixDomainSocketTag;
using ::tsdb2::net::ListenerSocket;
using ::tsdb2::net::Socket;
using ::tsdb2::net::SocketOptions;
using ::tsdb2::net::SSLListenerSocket;
using ::tsdb2::net::SSLSocket;

uint16_t GetNewPort() {
  static tsdb2::common::NoDestructor<tsdb2::common::StatsCounter> next_port{1024};
  return next_port->Increment();
}

std::string MakeTestSocketPath() {
  static tsdb2::common::SequenceNumber id;
  auto const directory = GetTestTmpDir();
  if (absl::EndsWith(directory, "/")) {
    return absl::StrCat(directory, "sockets_test.", id.GetNext(), ".sock");
  } else {
    return absl::StrCat(directory, "/sockets_test.", id.GetNext(), ".sock");
  }
}

class SocketTest : public tsdb2::testing::init::Test {
 protected:
  static void TransferData(reffed_ptr<BaseSocket> const& client_socket,
                           reffed_ptr<BaseSocket> const& server_socket, std::string_view data);
};

void SocketTest::TransferData(reffed_ptr<BaseSocket> const& client_socket,
                              reffed_ptr<BaseSocket> const& server_socket,
                              std::string_view const data) {
  absl::Notification write_notification;
  Buffer buffer{data.size()};
  buffer.MemCpy(data.data(), data.size());
  CHECK_OK(client_socket->Write(std::move(buffer), [&](absl::Status status) {
    CHECK(!write_notification.HasBeenNotified());
    CHECK_OK(status);
    write_notification.Notify();
  }));
  absl::Notification read_notification;
  CHECK_OK(server_socket->Read(data.size(), [&](absl::StatusOr<Buffer> status_or_buffer) {
    CHECK(!read_notification.HasBeenNotified());
    CHECK_OK(status_or_buffer);
    auto const& buffer = status_or_buffer.value();
    CHECK_EQ(buffer.size(), data.size());
    CHECK_EQ(data, std::string_view(buffer.as_char_array(), buffer.size()));
    read_notification.Notify();
  }));
  write_notification.WaitForNotification();
  read_notification.WaitForNotification();
}

TEST_F(SocketTest, InvalidAcceptCallback) {
  EXPECT_THAT(ListenerSocket<Socket>::Create(kInetSocketTag, "", GetNewPort(), SocketOptions(),
                                             /*callback=*/nullptr, /*callback_arg=*/nullptr),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(ListenerSocket<Socket>::Create(kUnixDomainSocketTag, MakeTestSocketPath(),
                                             /*callback=*/nullptr, /*callback_arg=*/nullptr),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(
      SSLListenerSocket<SSLSocket>::Create("", GetNewPort(), SocketOptions(), /*callback=*/nullptr,
                                           /*callback_arg=*/nullptr),
      StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(SocketTest, PortCollision) {
  auto const port = GetNewPort();
  auto const listener = ListenerSocket<Socket>::Create(
      kInetSocketTag, kLocalHost, port, SocketOptions(),
      +[](void*, absl::StatusOr<reffed_ptr<Socket>>) { FAIL(); }, nullptr);
  ASSERT_THAT(listener, IsOkAndHolds(AllOf(
                            Not(nullptr), Pointee2(Property(&BaseListenerSocket::is_open, true)))));
  EXPECT_THAT(ListenerSocket<Socket>::Create(
                  kInetSocketTag, kLocalHost, port, SocketOptions(),
                  +[](void*, absl::StatusOr<reffed_ptr<Socket>>) { FAIL(); }, nullptr),
              Not(IsOk()));
}

TEST_F(SocketTest, SSLPortCollision) {
  auto const port = GetNewPort();
  auto const listener = SSLListenerSocket<SSLSocket>::Create(
      kLocalHost, port, SocketOptions(),
      +[](void*, absl::StatusOr<reffed_ptr<SSLSocket>>) { FAIL(); }, nullptr);
  ASSERT_THAT(listener, IsOkAndHolds(AllOf(
                            Not(nullptr), Pointee2(Property(&BaseListenerSocket::is_open, true)))));
  EXPECT_THAT(SSLListenerSocket<SSLSocket>::Create(
                  kLocalHost, port, SocketOptions(),
                  +[](void*, absl::StatusOr<reffed_ptr<SSLSocket>>) { FAIL(); }, nullptr),
              Not(IsOk()));
}

TEST_F(SocketTest, Listen) {
  EXPECT_THAT(
      ListenerSocket<Socket>::Create(
          kInetSocketTag, kLocalHost, GetNewPort(), SocketOptions(),
          +[](void*, absl::StatusOr<reffed_ptr<Socket>>) { FAIL(); }, nullptr),
      IsOkAndHolds(AllOf(Not(nullptr), Pointee2(Property(&BaseListenerSocket::is_open, true)))));
  EXPECT_THAT(
      ListenerSocket<Socket>::Create(
          kUnixDomainSocketTag, MakeTestSocketPath(),
          +[](void*, absl::StatusOr<reffed_ptr<Socket>>) { FAIL(); }, nullptr),
      IsOkAndHolds(AllOf(Not(nullptr), Pointee2(Property(&BaseListenerSocket::is_open, true)))));
  EXPECT_THAT(
      SSLListenerSocket<SSLSocket>::Create(
          kLocalHost, GetNewPort(), SocketOptions(),
          +[](void*, absl::StatusOr<reffed_ptr<SSLSocket>>) { FAIL(); }, nullptr),
      IsOkAndHolds(AllOf(Not(nullptr), Pointee2(Property(&BaseListenerSocket::is_open, true)))));
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

class TestConnection {
 public:
  virtual ~TestConnection() = default;

  reffed_ptr<BaseSocket> server_socket() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock{&mutex_};
    return server_socket_;
  }

  void reset_server_socket() ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock{&mutex_};
    server_socket_.reset();
  }

  reffed_ptr<BaseSocket>& client_socket() { return client_socket_; }
  reffed_ptr<BaseSocket> const& client_socket() const { return client_socket_; }

 protected:
  explicit TestConnection() = default;

  void AcceptCallbackImpl(absl::StatusOr<reffed_ptr<BaseSocket>> status_or_socket)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock{&mutex_};
    switch (state_++) {
      case ListenerState::kListening:
        CHECK_OK(status_or_socket);
        server_socket_ = std::move(status_or_socket).value();
        CHECK(!server_socket_.empty());
        break;
      case ListenerState::kAccepted:
        CHECK(!status_or_socket.ok());
        LOG(ERROR) << "listener error: " << status_or_socket.status();
        server_socket_ = nullptr;
        break;
      default:
        FAIL();
        break;
    }
  }

  absl::Mutex mutable mutex_;
  ListenerState state_ ABSL_GUARDED_BY(mutex_) = ListenerState::kListening;
  reffed_ptr<BaseSocket> server_socket_ ABSL_GUARDED_BY(mutex_);
  reffed_ptr<BaseSocket> client_socket_;

 private:
  TestConnection(TestConnection const&) = delete;
  TestConnection& operator=(TestConnection const&) = delete;
  TestConnection(TestConnection&&) = delete;
  TestConnection& operator=(TestConnection&&) = delete;
};

class TestInetConnection : public TestConnection {
 public:
  explicit TestInetConnection(bool const use_random_port, SocketOptions const& options) {
    if (use_random_port) {
      MakeConnection(options);
    } else {
      MakeSocketPair();
    }
  }

 private:
  static void AcceptCallback(void* const arg, absl::StatusOr<reffed_ptr<Socket>> status_or_socket) {
    reinterpret_cast<TestInetConnection*>(arg)->AcceptCallbackImpl(std::move(status_or_socket));
  }

  void MakeConnection(SocketOptions const& options) {
    auto const port = GetNewPort();
    auto status_or_listener = ListenerSocket<Socket>::Create(
        kInetSocketTag, kLocalHost, port, options, &TestInetConnection::AcceptCallback, this);
    CHECK_OK(status_or_listener);
    auto const& listener = status_or_listener.value();
    CHECK(!listener.empty());
    absl::Notification connected;
    auto status_or_socket =
        Socket::Create(kInetSocketTag, kLocalHost, port, options,
                       [&](reffed_ptr<Socket> const socket, absl::Status const status) {
                         CHECK_OK(status);
                         CHECK(!connected.HasBeenNotified());
                         connected.Notify();
                       });
    CHECK_OK(status_or_socket);
    client_socket_ = std::move(status_or_socket).value();
    CHECK(!client_socket_.empty());
    absl::MutexLock(  // NOLINT(bugprone-unused-raii)
        &mutex_, SimpleCondition([&]() ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
          return state_ == ListenerState::kAccepted;
        }));
    connected.WaitForNotification();
  }

  void MakeSocketPair() {
    auto status_or_pair = Socket::CreatePair();
    CHECK_OK(status_or_pair);
    auto& pair = status_or_pair.value();
    absl::MutexLock lock{&mutex_};
    server_socket_ = std::move(pair.first);
    client_socket_ = std::move(pair.second);
  }
};

class TestSSLConnection : public TestConnection {
 public:
  explicit TestSSLConnection(bool const use_random_port, SocketOptions const& options) {
    if (use_random_port) {
      MakeConnection(options);
    } else {
      MakeSocketPair();
    }
  }

 private:
  static void AcceptCallback(void* const arg,
                             absl::StatusOr<reffed_ptr<SSLSocket>> status_or_socket) {
    reinterpret_cast<TestSSLConnection*>(arg)->AcceptCallbackImpl(std::move(status_or_socket));
  }

  void MakeConnection(SocketOptions const& options) {
    auto const port = GetNewPort();
    auto status_or_listener = SSLListenerSocket<SSLSocket>::Create(
        kLocalHost, port, options, &TestSSLConnection::AcceptCallback, this);
    CHECK_OK(status_or_listener);
    auto const& listener = status_or_listener.value();
    CHECK(!listener.empty());
    absl::Notification connected;
    auto status_or_socket = SSLSocket::Create(
        kLocalHost, port, options, [&](reffed_ptr<SSLSocket> socket, absl::Status const status) {
          CHECK_OK(status);
          CHECK(!connected.HasBeenNotified());
          connected.Notify();
        });
    CHECK_OK(status_or_socket);
    client_socket_ = std::move(status_or_socket).value();
    CHECK(!client_socket_.empty());
    absl::MutexLock(  // NOLINT(bugprone-unused-raii)
        &mutex_, SimpleCondition([&]() ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
          return state_ == ListenerState::kAccepted;
        }));
    connected.WaitForNotification();
  }

  void MakeSocketPair() {
    auto status_or_pair = SSLSocket::CreatePairForTesting();
    CHECK_OK(status_or_pair);
    auto& pair = status_or_pair.value();
    absl::MutexLock lock{&mutex_};
    server_socket_ = std::move(pair.first);
    client_socket_ = std::move(pair.second);
  }
};

using TestConnectionTypes = ::testing::Types<TestInetConnection, TestSSLConnection>;

template <typename TestConnection>
class SocketSettingsTest : public SocketTest {};

TYPED_TEST_SUITE_P(SocketSettingsTest);

TYPED_TEST_P(SocketSettingsTest, Settings1) {
  uint8_t const ip_tos = IPTOS_LOWDELAY + (2 << 5);
  SocketOptions const options{
      .keep_alive = false,
      .ip_tos = ip_tos,
  };
  TypeParam connection{/*use_random_port=*/true, options};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  EXPECT_THAT(server_socket->ip_tos(), IsOkAndHolds(ip_tos));
  EXPECT_THAT(client_socket->ip_tos(), IsOkAndHolds(ip_tos));
  EXPECT_THAT(server_socket->is_keep_alive(), IsOkAndHolds(false));
  EXPECT_THAT(client_socket->is_keep_alive(), IsOkAndHolds(false));
  EXPECT_THAT(server_socket->keep_alive_params(), Not(IsOk()));
  EXPECT_THAT(client_socket->keep_alive_params(), Not(IsOk()));
  EXPECT_TRUE(server_socket->is_open());
  EXPECT_TRUE(client_socket->is_open());
}

TYPED_TEST_P(SocketSettingsTest, Settings2) {
  uint8_t const ip_tos = IPTOS_THROUGHPUT + (3 << 5);
  SocketOptions const options{
      .keep_alive = true,
      .keep_alive_params{
          .idle = absl::Seconds(90),
          .interval = absl::Seconds(10),
          .count = 42,
      },
      .ip_tos = ip_tos,
  };
  TypeParam connection{/*use_random_port=*/true, options};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  EXPECT_THAT(server_socket->ip_tos(), IsOkAndHolds(ip_tos));
  EXPECT_THAT(client_socket->ip_tos(), IsOkAndHolds(ip_tos));
  EXPECT_THAT(server_socket->is_keep_alive(), IsOkAndHolds(true));
  EXPECT_THAT(client_socket->is_keep_alive(), IsOkAndHolds(true));
  EXPECT_THAT(server_socket->keep_alive_params(),
              IsOkAndHolds(AllOf(Field(&KeepAliveParams::idle, absl::Seconds(90)),
                                 Field(&KeepAliveParams::interval, absl::Seconds(10)),
                                 Field(&KeepAliveParams::count, 42))));
  EXPECT_THAT(client_socket->keep_alive_params(),
              IsOkAndHolds(AllOf(Field(&KeepAliveParams::idle, absl::Seconds(90)),
                                 Field(&KeepAliveParams::interval, absl::Seconds(10)),
                                 Field(&KeepAliveParams::count, 42))));
  EXPECT_TRUE(server_socket->is_open());
  EXPECT_TRUE(client_socket->is_open());
}

REGISTER_TYPED_TEST_SUITE_P(SocketSettingsTest, Settings1, Settings2);

INSTANTIATE_TYPED_TEST_SUITE_P(SocketSettingsTest, SocketSettingsTest, TestConnectionTypes);

template <typename TestConnection>
class TransferTest : public SocketTest {};

TYPED_TEST_SUITE_P(TransferTest);

TYPED_TEST_P(TransferTest, TransferWithKeepAlives) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports),
                       SocketOptions{.keep_alive = true}};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  this->TransferData(client_socket, server_socket, "lorem ipsum");
  this->TransferData(server_socket, client_socket, "dolor sit amet");
  EXPECT_TRUE(server_socket->is_open());
  EXPECT_TRUE(client_socket->is_open());
}

TYPED_TEST_P(TransferTest, TransferWithoutKeepAlives) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports),
                       SocketOptions{.keep_alive = false}};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  this->TransferData(client_socket, server_socket, "lorem ipsum");
  this->TransferData(server_socket, client_socket, "dolor sit amet");
  EXPECT_TRUE(server_socket->is_open());
  EXPECT_TRUE(client_socket->is_open());
}

TYPED_TEST_P(TransferTest, ReadValidation) {
  TypeParam connection{/*use_random_port=*/false, SocketOptions()};
  auto const& server_socket = connection.server_socket();
  EXPECT_THAT(server_socket->Read(0, [](absl::StatusOr<Buffer>) { FAIL(); }),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(server_socket->Read(10, nullptr), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(server_socket->ReadWithTimeout(
                  10, [](absl::StatusOr<Buffer>) { FAIL(); }, absl::Seconds(-42)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_TRUE(server_socket->is_open());
}

TYPED_TEST_P(TransferTest, WriteValidation) {
  TypeParam connection{/*use_random_port=*/false, SocketOptions()};
  auto const& server_socket = connection.server_socket();
  EXPECT_THAT(server_socket->Write(Buffer(10), [](absl::Status) { FAIL(); }),
              StatusIs(absl::StatusCode::kInvalidArgument));
  Buffer buffer{10};
  buffer.Advance(10);
  EXPECT_THAT(server_socket->Write(std::move(buffer), nullptr),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(server_socket->WriteWithTimeout(
                  Buffer(10), [](absl::Status) { FAIL(); }, absl::Seconds(-42)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_TRUE(server_socket->is_open());
}

TYPED_TEST_P(TransferTest, ClientHangUp) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& server_socket = connection.server_socket();
  absl::Notification done;
  ASSERT_OK(server_socket->Read(10, [&](absl::StatusOr<Buffer> const status_or_buffer) {
    EXPECT_THAT(status_or_buffer, Not(IsOk()));
    done.Notify();
  }));
  connection.client_socket().reset();
  done.WaitForNotification();
  EXPECT_FALSE(server_socket->is_open());
  EXPECT_THAT(server_socket->Read(10, [](absl::StatusOr<Buffer>) { FAIL(); }), Not(IsOk()));
}

TYPED_TEST_P(TransferTest, ClientClose) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  absl::Notification done;
  ASSERT_OK(server_socket->Read(10, [&](absl::StatusOr<Buffer> const status_or_buffer) {
    EXPECT_THAT(status_or_buffer, Not(IsOk()));
    done.Notify();
  }));
  client_socket->Close();
  EXPECT_FALSE(client_socket->is_open());
  EXPECT_THAT(client_socket->Write(Buffer("test", 4), [](absl::Status) { FAIL(); }), Not(IsOk()));
  done.WaitForNotification();
  EXPECT_FALSE(server_socket->is_open());
  EXPECT_THAT(server_socket->Read(10, [](absl::StatusOr<Buffer>) { FAIL(); }), Not(IsOk()));
}

TYPED_TEST_P(TransferTest, ServerHangUp) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& client_socket = connection.client_socket();
  absl::Notification done;
  ASSERT_OK(client_socket->Read(10, [&](absl::StatusOr<Buffer> const status_or_buffer) {
    EXPECT_THAT(status_or_buffer, Not(IsOk()));
    done.Notify();
  }));
  connection.reset_server_socket();
  done.WaitForNotification();
  EXPECT_FALSE(client_socket->is_open());
}

TYPED_TEST_P(TransferTest, ServerClose) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  absl::Notification done;
  ASSERT_OK(client_socket->Read(10, [&](absl::StatusOr<Buffer> const status_or_buffer) {
    EXPECT_THAT(status_or_buffer, Not(IsOk()));
    done.Notify();
  }));
  server_socket->Close();
  EXPECT_FALSE(server_socket->is_open());
  EXPECT_THAT(server_socket->Read(1, [](absl::StatusOr<Buffer>) { FAIL(); }), Not(IsOk()));
  done.WaitForNotification();
  EXPECT_FALSE(client_socket->is_open());
}

TYPED_TEST_P(TransferTest, TwoChunks) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  std::string_view constexpr kChunk1 = "01234567890123456789";
  std::string_view constexpr kChunk2 = "987654321098765432109876543210";
  absl::Notification read_notification;
  absl::Notification write_notification;
  ASSERT_OK(server_socket->Read(20, [&](absl::StatusOr<Buffer> const status_or_buffer) {
    ASSERT_THAT(status_or_buffer, IsOkAndHolds(Property(&Buffer::size, 20)));
    auto const& buffer = status_or_buffer.value();
    std::string_view const chunk1{buffer.as_char_array(), buffer.size()};
    EXPECT_EQ(chunk1, kChunk1);
    EXPECT_OK(server_socket->Read(30, [&](absl::StatusOr<Buffer> const status_or_buffer) {
      EXPECT_THAT(status_or_buffer, IsOkAndHolds(Property(&Buffer::size, 30)));
      auto const& buffer = status_or_buffer.value();
      std::string_view const chunk2{buffer.as_char_array(), buffer.size()};
      EXPECT_EQ(chunk2, kChunk2);
      read_notification.Notify();
    }));
  }));
  Buffer buffer1{kChunk1.size()};
  buffer1.MemCpy(kChunk1.data(), kChunk1.size());
  ASSERT_OK(client_socket->Write(std::move(buffer1), [&](absl::Status const status) {
    ASSERT_OK(status);
    Buffer buffer2{kChunk2.size()};
    buffer2.MemCpy(kChunk2.data(), kChunk2.size());
    EXPECT_OK(client_socket->Write(std::move(buffer2), [&](absl::Status const status) {
      EXPECT_OK(status);
      write_notification.Notify();
    }));
  }));
  read_notification.WaitForNotification();
  write_notification.WaitForNotification();
  EXPECT_TRUE(server_socket->is_open());
  EXPECT_TRUE(client_socket->is_open());
}

TYPED_TEST_P(TransferTest, ReadMoreThanImmediatelyAvailable) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  std::string_view constexpr kData = "01234567890123456789987654321098765432109876543210";
  std::string_view constexpr kChunk1 = kData.substr(0, 20);
  std::string_view constexpr kChunk2 = kData.substr(20);
  absl::Notification read_notification1;
  absl::Notification read_notification2;
  absl::Notification write_notification;
  std::thread writer{[&] {
    Buffer buffer1{kChunk1.size()};
    buffer1.MemCpy(kChunk1.data(), kChunk1.size());
    CHECK_OK(client_socket->Write(std::move(buffer1), [&](absl::Status const status) {
      CHECK_OK(status);
      read_notification1.WaitForNotification();
      Buffer buffer2{kChunk2.size()};
      buffer2.MemCpy(kChunk2.data(), kChunk2.size());
      CHECK_OK(client_socket->Write(std::move(buffer2), [&](absl::Status const status) {
        CHECK_OK(status);
        write_notification.Notify();
      }));
    }));
  }};
  std::thread reader{[&] {
    CHECK_OK(server_socket->Read(kData.size(), [&](absl::StatusOr<Buffer> const status_or_buffer) {
      EXPECT_THAT(status_or_buffer, IsOkAndHolds(Property(&Buffer::size, kData.size())));
      auto const& buffer = status_or_buffer.value();
      std::string_view const data{buffer.as_char_array(), buffer.size()};
      EXPECT_EQ(data, kData);
      read_notification2.Notify();
    }));
    read_notification1.Notify();
  }};
  read_notification2.WaitForNotification();
  write_notification.WaitForNotification();
  reader.join();
  writer.join();
  EXPECT_TRUE(server_socket->is_open());
  EXPECT_TRUE(client_socket->is_open());
}

TYPED_TEST_P(TransferTest, Skip) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  std::string_view constexpr kData = "0123456789";
  ASSERT_OK(client_socket->Write(Buffer(kData.data(), kData.size()),
                                 [](absl::Status const status) { ASSERT_OK(status); }));
  absl::Notification done;
  EXPECT_OK(server_socket->Skip(kData.size(), [&](absl::Status const status) {
    EXPECT_OK(status);
    done.Notify();
  }));
  done.WaitForNotification();
  EXPECT_TRUE(server_socket->is_open());
  EXPECT_TRUE(client_socket->is_open());
}

TYPED_TEST_P(TransferTest, SkipManyChunks) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  size_t constexpr kDataSize = 4096 * 5 / 2;
  Buffer buffer{kDataSize};
  buffer.Advance(kDataSize);
  ASSERT_OK(client_socket->Write(std::move(buffer),
                                 [](absl::Status const status) { ASSERT_OK(status); }));
  absl::Notification done;
  EXPECT_OK(server_socket->Skip(kDataSize, [&](absl::Status const status) {
    EXPECT_OK(status);
    done.Notify();
  }));
  done.WaitForNotification();
  EXPECT_TRUE(server_socket->is_open());
  EXPECT_TRUE(client_socket->is_open());
}

TYPED_TEST_P(TransferTest, SkipEvenChunks) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  size_t constexpr kDataSize = size_t{4096} * 3;
  Buffer buffer{kDataSize};
  buffer.Advance(kDataSize);
  ASSERT_OK(client_socket->Write(std::move(buffer),
                                 [](absl::Status const status) { ASSERT_OK(status); }));
  absl::Notification done;
  EXPECT_OK(server_socket->Skip(kDataSize, [&](absl::Status const status) {
    EXPECT_OK(status);
    done.Notify();
  }));
  done.WaitForNotification();
  EXPECT_TRUE(server_socket->is_open());
  EXPECT_TRUE(client_socket->is_open());
}

REGISTER_TYPED_TEST_SUITE_P(TransferTest, TransferWithKeepAlives, TransferWithoutKeepAlives,
                            ReadValidation, WriteValidation, ClientHangUp, ClientClose,
                            ServerHangUp, ServerClose, TwoChunks, ReadMoreThanImmediatelyAvailable,
                            Skip, SkipManyChunks, SkipEvenChunks);

INSTANTIATE_TYPED_TEST_SUITE_P(TransferTest, TransferTest, TestConnectionTypes);

template <typename TestConnection>
class TimeoutTest : public SocketTest {
 protected:
  explicit TimeoutTest() { CHECK_OK(scheduler_.WaitUntilAllWorkersAsleep()); }

  MockClock clock_{absl::UnixEpoch() + absl::Seconds(100)};
  Scheduler scheduler_{Scheduler::Options{
      .num_workers = 1,
      .clock = &clock_,
      .start_now = true,
  }};
  ScopedOverride<tsdb2::common::Singleton<Scheduler>> scheduler_override_{
      &tsdb2::common::default_scheduler, &scheduler_};
};

TYPED_TEST_SUITE_P(TimeoutTest);

TYPED_TEST_P(TimeoutTest, ReadInTime) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  std::string_view constexpr kData = "01234567890123456789";
  absl::Notification read_notification;
  absl::Notification write_notification;
  ASSERT_OK(server_socket->ReadWithTimeout(
      kData.size(),
      [&](absl::StatusOr<Buffer> const status_or_buffer) {
        EXPECT_THAT(status_or_buffer, IsOkAndHolds(Property(&Buffer::size, kData.size())));
        auto const& buffer = status_or_buffer.value();
        std::string_view const data{buffer.as_char_array(), buffer.size()};
        EXPECT_EQ(data, kData);
        read_notification.Notify();
      },
      absl::Seconds(10)));
  this->clock_.AdvanceTime(absl::Seconds(5));
  ASSERT_OK(this->scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(server_socket->is_open());
  EXPECT_TRUE(client_socket->is_open());
  Buffer buffer{kData.data(), kData.size()};
  ASSERT_OK(client_socket->Write(std::move(buffer), [&](absl::Status const status) {
    EXPECT_OK(status);
    write_notification.Notify();
  }));
  read_notification.WaitForNotification();
  write_notification.WaitForNotification();
  EXPECT_TRUE(server_socket->is_open());
  EXPECT_TRUE(client_socket->is_open());
}

TYPED_TEST_P(TimeoutTest, ReadTimeout) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& server_socket = connection.server_socket();
  absl::Notification done;
  CHECK_OK(server_socket->ReadWithTimeout(
      42,
      [&](absl::StatusOr<Buffer> const status_or_buffer) {
        EXPECT_THAT(status_or_buffer, Not(IsOk()));
        done.Notify();
      },
      absl::Seconds(10)));
  this->clock_.AdvanceTime(absl::Seconds(5));
  ASSERT_OK(this->scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_FALSE(done.HasBeenNotified());
  EXPECT_TRUE(server_socket->is_open());
  this->clock_.AdvanceTime(absl::Seconds(5));
  ASSERT_OK(this->scheduler_.WaitUntilAllWorkersAsleep());
  done.WaitForNotification();
  EXPECT_FALSE(server_socket->is_open());
}

TYPED_TEST_P(TimeoutTest, ReadTimeoutOnSecondChunk) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  std::string_view constexpr kData = "01234567890123456789";
  absl::Notification read_notification1;
  absl::Notification read_notification2;
  ASSERT_OK(server_socket->ReadWithTimeout(
      20,
      [&](absl::StatusOr<Buffer> const status_or_buffer) {
        ASSERT_THAT(status_or_buffer, IsOkAndHolds(Property(&Buffer::size, 20)));
        auto const& buffer = status_or_buffer.value();
        std::string_view const chunk{buffer.as_char_array(), buffer.size()};
        EXPECT_EQ(chunk, kData);
        EXPECT_OK(server_socket->ReadWithTimeout(
            30,
            [&](absl::StatusOr<Buffer> const status_or_buffer) {
              EXPECT_THAT(status_or_buffer, Not(IsOk()));
              read_notification2.Notify();
            },
            absl::Seconds(10)));
        read_notification1.Notify();
      },
      absl::Seconds(5)));
  this->clock_.AdvanceTime(absl::Seconds(2));
  ASSERT_OK(this->scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(server_socket->is_open());
  Buffer buffer{kData.data(), kData.size()};
  ASSERT_OK(client_socket->Write(std::move(buffer),
                                 [&](absl::Status const status) { EXPECT_OK(status); }));
  read_notification1.WaitForNotification();
  EXPECT_FALSE(read_notification2.HasBeenNotified());
  EXPECT_TRUE(server_socket->is_open());
  this->clock_.AdvanceTime(absl::Seconds(10));
  ASSERT_OK(this->scheduler_.WaitUntilAllWorkersAsleep());
  read_notification2.WaitForNotification();
  EXPECT_FALSE(server_socket->is_open());
}

TYPED_TEST_P(TimeoutTest, WriteInTime) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  std::string_view constexpr kData = "01234567890123456789";
  absl::Notification read_notification;
  absl::Notification write_notification;
  ASSERT_OK(server_socket->Read(kData.size(), [&](absl::StatusOr<Buffer> const status_or_buffer) {
    EXPECT_THAT(status_or_buffer, IsOkAndHolds(Property(&Buffer::size, kData.size())));
    auto const& buffer = status_or_buffer.value();
    std::string_view const data{buffer.as_char_array(), buffer.size()};
    EXPECT_EQ(data, kData);
    read_notification.Notify();
  }));
  Buffer buffer{kData.data(), kData.size()};
  ASSERT_OK(client_socket->WriteWithTimeout(
      std::move(buffer),
      [&](absl::Status const status) {
        EXPECT_OK(status);
        write_notification.Notify();
      },
      absl::Seconds(10)));
  read_notification.WaitForNotification();
  write_notification.WaitForNotification();
  EXPECT_TRUE(server_socket->is_open());
  EXPECT_TRUE(client_socket->is_open());
}

TYPED_TEST_P(TimeoutTest, SkipInTime) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  std::string_view constexpr kData = "01234567890123456789";
  absl::Notification skip_notification;
  absl::Notification write_notification;
  ASSERT_OK(server_socket->SkipWithTimeout(
      kData.size(),
      [&](absl::Status const status) {
        EXPECT_OK(status);
        skip_notification.Notify();
      },
      absl::Seconds(10)));
  this->clock_.AdvanceTime(absl::Seconds(5));
  ASSERT_OK(this->scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(server_socket->is_open());
  EXPECT_TRUE(client_socket->is_open());
  Buffer buffer{kData.data(), kData.size()};
  ASSERT_OK(client_socket->Write(std::move(buffer), [&](absl::Status const status) {
    EXPECT_OK(status);
    write_notification.Notify();
  }));
  skip_notification.WaitForNotification();
  write_notification.WaitForNotification();
  EXPECT_TRUE(server_socket->is_open());
  EXPECT_TRUE(client_socket->is_open());
}

TYPED_TEST_P(TimeoutTest, SkipTimeout) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& server_socket = connection.server_socket();
  absl::Notification done;
  CHECK_OK(server_socket->SkipWithTimeout(
      42,
      [&](absl::Status const status) {
        EXPECT_THAT(status, Not(IsOk()));
        done.Notify();
      },
      absl::Seconds(10)));
  this->clock_.AdvanceTime(absl::Seconds(5));
  ASSERT_OK(this->scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_FALSE(done.HasBeenNotified());
  EXPECT_TRUE(server_socket->is_open());
  this->clock_.AdvanceTime(absl::Seconds(5));
  ASSERT_OK(this->scheduler_.WaitUntilAllWorkersAsleep());
  done.WaitForNotification();
  EXPECT_FALSE(server_socket->is_open());
}

TYPED_TEST_P(TimeoutTest, SkipTimeoutOnSecondChunk) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  std::string_view constexpr kData = "01234567890123456789";
  absl::Notification read_notification1;
  absl::Notification read_notification2;
  ASSERT_OK(server_socket->SkipWithTimeout(
      20,
      [&](absl::Status const status) {
        ASSERT_OK(status);
        EXPECT_OK(server_socket->SkipWithTimeout(
            30,
            [&](absl::Status const status) {
              EXPECT_THAT(status, Not(IsOk()));
              read_notification2.Notify();
            },
            absl::Seconds(10)));
        read_notification1.Notify();
      },
      absl::Seconds(5)));
  this->clock_.AdvanceTime(absl::Seconds(2));
  ASSERT_OK(this->scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(server_socket->is_open());
  Buffer buffer{kData.data(), kData.size()};
  ASSERT_OK(client_socket->Write(std::move(buffer),
                                 [&](absl::Status const status) { EXPECT_OK(status); }));
  read_notification1.WaitForNotification();
  EXPECT_FALSE(read_notification2.HasBeenNotified());
  EXPECT_TRUE(server_socket->is_open());
  this->clock_.AdvanceTime(absl::Seconds(10));
  ASSERT_OK(this->scheduler_.WaitUntilAllWorkersAsleep());
  read_notification2.WaitForNotification();
  EXPECT_FALSE(server_socket->is_open());
}

TYPED_TEST_P(TimeoutTest, Duplex) {
  TypeParam connection{absl::GetFlag(FLAGS_socket_test_use_random_ports), SocketOptions()};
  auto const& server_socket = connection.server_socket();
  auto const& client_socket = connection.client_socket();
  std::string_view constexpr kData1 = "01234567890123456789";
  std::string_view constexpr kData2 = "98765432109876543210";
  absl::Mutex mutex;
  bool client_write_done = false;
  bool client_read_done = false;
  bool server_write_done = false;
  bool server_read_done = false;
  std::thread client{[&] {
    EXPECT_OK(
        client_socket->Write(Buffer{kData1.data(), kData1.size()}, [&](absl::Status const status) {
          EXPECT_OK(status);
          absl::MutexLock lock{&mutex};
          client_write_done = true;
        }));
    EXPECT_OK(
        client_socket->Read(kData2.size(), [&](absl::StatusOr<Buffer> const status_or_buffer) {
          EXPECT_THAT(status_or_buffer, IsOkAndHolds(Property(&Buffer::size, kData2.size())));
          auto const& buffer = status_or_buffer.value();
          std::string_view const data{buffer.as_char_array(), buffer.size()};
          EXPECT_EQ(data, kData2);
          absl::MutexLock lock{&mutex};
          client_read_done = true;
        }));
  }};
  std::thread server{[&] {
    EXPECT_OK(
        server_socket->Write(Buffer{kData2.data(), kData2.size()}, [&](absl::Status const status) {
          EXPECT_OK(status);
          absl::MutexLock lock{&mutex};
          server_write_done = true;
        }));
    EXPECT_OK(
        server_socket->Read(kData1.size(), [&](absl::StatusOr<Buffer> const status_or_buffer) {
          EXPECT_THAT(status_or_buffer, IsOkAndHolds(Property(&Buffer::size, kData1.size())));
          auto const& buffer = status_or_buffer.value();
          std::string_view const data{buffer.as_char_array(), buffer.size()};
          EXPECT_EQ(data, kData1);
          absl::MutexLock lock{&mutex};
          server_read_done = true;
        }));
  }};
  client.join();
  server.join();
  absl::MutexLock{&mutex, tsdb2::common::SimpleCondition([&] {
                    return client_write_done && client_read_done && server_write_done &&
                           server_read_done;
                  })};
}

REGISTER_TYPED_TEST_SUITE_P(TimeoutTest, ReadInTime, ReadTimeout, ReadTimeoutOnSecondChunk,
                            WriteInTime, SkipInTime, SkipTimeout, SkipTimeoutOnSecondChunk, Duplex);

INSTANTIATE_TYPED_TEST_SUITE_P(TimeoutTest, TimeoutTest, TestConnectionTypes);

class HandshakeTimeoutTest : public SocketTest {};

TEST_F(HandshakeTimeoutTest, Timeout) {
  FlagOverride timeout_override{&FLAGS_ssl_handshake_timeout, absl::Seconds(123)};
  MockClock clock_{absl::UnixEpoch() + absl::Seconds(100)};
  Scheduler scheduler_{Scheduler::Options{
      .num_workers = 1,
      .clock = &clock_,
      .start_now = true,
  }};
  ScopedOverride<tsdb2::common::Singleton<Scheduler>> scheduler_override_{
      &tsdb2::common::default_scheduler, &scheduler_};
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  absl::Notification done;
  int fds[2] = {0, 0};
  CHECK_GE(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds), 0)
      << absl::ErrnoToStatus(errno, "socketpair");
  auto const status_or_socket = SSLSocket::Create(
      FD(fds[0]), [&](reffed_ptr<SSLSocket> const socket, absl::Status const status) {
        EXPECT_THAT(status, StatusIs(absl::StatusCode::kDeadlineExceeded));
        EXPECT_FALSE(socket->is_open());
        done.Notify();
      });
  EXPECT_OK(status_or_socket);
  auto const& socket = status_or_socket.value();
  clock_.AdvanceTime(absl::Seconds(122));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_FALSE(done.HasBeenNotified());
  EXPECT_TRUE(socket->is_open());
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  done.WaitForNotification();
  EXPECT_FALSE(socket->is_open());
}

}  // namespace
