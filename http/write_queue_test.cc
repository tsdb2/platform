#include "http/write_queue.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "common/mock_clock.h"
#include "common/reffed_ptr.h"
#include "common/scheduler.h"
#include "common/simple_condition.h"
#include "common/utilities.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "io/buffer_testing.h"
#include "net/base_sockets.h"
#include "net/sockets.h"
#include "net/ssl_sockets.h"
#include "server/testing.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::testing::AnyOf;
using ::tsdb2::common::MockClock;
using ::tsdb2::common::reffed_ptr;
using ::tsdb2::common::Scheduler;
using ::tsdb2::common::SimpleCondition;
using ::tsdb2::http::WriteQueue;
using ::tsdb2::net::Buffer;
using ::tsdb2::testing::io::BufferAsString;

template <typename Socket>
std::pair<reffed_ptr<Socket>, reffed_ptr<Socket>> MakeConnection();

template <>
std::pair<reffed_ptr<tsdb2::net::Socket>, reffed_ptr<tsdb2::net::Socket>>
MakeConnection<tsdb2::net::Socket>() {
  auto status_or_sockets = tsdb2::net::Socket::CreatePair();
  CHECK_OK(status_or_sockets);
  return std::move(status_or_sockets).value();
}

template <>
std::pair<reffed_ptr<tsdb2::net::SSLSocket>, reffed_ptr<tsdb2::net::SSLSocket>>
MakeConnection<tsdb2::net::SSLSocket>() {
  auto status_or_sockets = tsdb2::net::SSLSocket::CreatePairForTesting();
  CHECK_OK(status_or_sockets);
  return std::move(status_or_sockets).value();
}

template <typename Socket>
class WriteQueueTest : public tsdb2::testing::init::Test {
 protected:
  explicit WriteQueueTest(std::pair<reffed_ptr<Socket>, reffed_ptr<Socket>> sockets)
      : socket1_(std::move(sockets.first)), socket2_(std::move(sockets.second)) {
    clock_.AdvanceTime(absl::Seconds(123));
  }

  explicit WriteQueueTest() : WriteQueueTest(MakeConnection<Socket>()) {}

  absl::StatusOr<Buffer> Read(size_t length);

  MockClock clock_;
  Scheduler scheduler_{Scheduler::Options{
      .num_workers = 1,
      .clock = &clock_,
      .start_now = true,
  }};
  reffed_ptr<Socket> const socket1_;
  reffed_ptr<Socket> const socket2_;
  WriteQueue write_queue_{socket1_.get()};
};

template <typename Socket>
absl::StatusOr<Buffer> WriteQueueTest<Socket>::Read(size_t const length) {
  absl::Mutex mutex;
  std::optional<absl::StatusOr<Buffer>> result;
  RETURN_IF_ERROR(socket2_->Read(length, [&](absl::StatusOr<Buffer> status_or_buffer) {
    absl::MutexLock lock{&mutex};
    result.emplace(std::move(status_or_buffer));
  }));
  absl::MutexLock lock{&mutex, SimpleCondition([&] { return result.has_value(); })};
  return std::move(result).value();
}

using SocketTypes = ::testing::Types<tsdb2::net::Socket, tsdb2::net::SSLSocket>;
TYPED_TEST_SUITE(WriteQueueTest, SocketTypes);

TYPED_TEST(WriteQueueTest, Write) {
  std::string_view constexpr kData = "01234567890123456789";
  absl::Notification written;
  this->write_queue_.AppendFrame(Buffer(kData.data(), kData.size()), [&] { written.Notify(); });
  EXPECT_THAT(this->Read(kData.size()), IsOkAndHolds(BufferAsString(kData)));
  written.WaitForNotification();
}

TYPED_TEST(WriteQueueTest, WriteWithoutCallback) {
  std::string_view constexpr kData = "01234567890123456789";
  this->write_queue_.AppendFrame(Buffer(kData.data(), kData.size()));
  EXPECT_THAT(this->Read(kData.size()), IsOkAndHolds(BufferAsString(kData)));
}

TYPED_TEST(WriteQueueTest, WriteTwoSeparately) {
  std::string_view constexpr kData1 = "01234567890123456789";
  std::string_view constexpr kData2 = "9876543210";
  this->write_queue_.AppendFrame(Buffer(kData1.data(), kData1.size()));
  this->write_queue_.AppendFrame(Buffer(kData2.data(), kData2.size()));
  EXPECT_THAT(this->Read(kData1.size() + kData2.size()),
              IsOkAndHolds(BufferAsString("012345678901234567899876543210")));
}

TYPED_TEST(WriteQueueTest, WriteManyButThereAreNone) {
  std::string_view constexpr kData = "abcdef";
  std::vector<Buffer> buffers;
  this->write_queue_.AppendFrames(std::move(buffers));
  this->write_queue_.AppendFrame(Buffer(kData.data(), kData.size()));
  EXPECT_THAT(this->Read(kData.size()), IsOkAndHolds(BufferAsString(kData)));
}

TYPED_TEST(WriteQueueTest, WriteManyButItsOnlyOne) {
  std::string_view constexpr kData1 = "9876543210";
  std::string_view constexpr kData2 = "abcdef";
  std::vector<Buffer> buffers;
  buffers.emplace_back(kData1.data(), kData1.size());
  this->write_queue_.AppendFrames(std::move(buffers));
  this->write_queue_.AppendFrame(Buffer(kData2.data(), kData2.size()));
  EXPECT_THAT(this->Read(kData1.size() + kData2.size()),
              IsOkAndHolds(BufferAsString("9876543210abcdef")));
}

TYPED_TEST(WriteQueueTest, WriteTwoTogether) {
  std::string_view constexpr kData1 = "01234567890123456789";
  std::string_view constexpr kData2 = "9876543210";
  std::string_view constexpr kData3 = "abcdef";
  std::vector<Buffer> buffers;
  buffers.emplace_back(kData1.data(), kData1.size());
  buffers.emplace_back(kData2.data(), kData2.size());
  this->write_queue_.AppendFrames(std::move(buffers));
  this->write_queue_.AppendFrame(Buffer(kData3.data(), kData3.size()));
  EXPECT_THAT(this->Read(kData1.size() + kData2.size() + kData3.size()),
              IsOkAndHolds(BufferAsString("012345678901234567899876543210abcdef")));
}

TYPED_TEST(WriteQueueTest, WriteError) {
  this->socket2_->Close();
  std::string_view constexpr kData = "01234567890123456789";
  this->write_queue_.AppendFrame(Buffer(kData.data(), kData.size()));
}

TYPED_TEST(WriteQueueTest, WriteSkippingQueue) {
  std::string_view constexpr kData1 = "01234567890123456789";
  std::string_view constexpr kData2 = "9876543210";
  this->write_queue_.AppendFrame(Buffer(kData1.data(), kData1.size()));
  this->write_queue_.AppendFrameSkippingQueue(Buffer(kData2.data(), kData2.size()));
  EXPECT_THAT(this->Read(kData1.size() + kData2.size()),
              IsOkAndHolds(AnyOf(BufferAsString("012345678901234567899876543210"),
                                 BufferAsString("987654321001234567890123456789"))));
}

}  // namespace
