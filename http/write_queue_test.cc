#include "http/write_queue.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
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
#include "http/hpack.h"
#include "http/http.h"
#include "io/buffer_testing.h"
#include "net/base_sockets.h"
#include "net/sockets.h"
#include "net/ssl_sockets.h"
#include "server/testing.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::ElementsAreArray;
using ::testing::Property;
using ::tsdb2::common::MockClock;
using ::tsdb2::common::reffed_ptr;
using ::tsdb2::common::Scheduler;
using ::tsdb2::common::SimpleCondition;
using ::tsdb2::http::ErrorCode;
using ::tsdb2::http::FrameHeader;
using ::tsdb2::http::FrameType;
using ::tsdb2::http::GoAwayPayload;
using ::tsdb2::http::kFlagAck;
using ::tsdb2::http::kFlagEndHeaders;
using ::tsdb2::http::kFlagEndStream;
using ::tsdb2::http::kPingPayloadSize;
using ::tsdb2::http::ResetStreamPayload;
using ::tsdb2::http::WriteQueue;
using ::tsdb2::http::hpack::HeaderSet;
using ::tsdb2::net::Buffer;
using ::tsdb2::testing::io::BufferAs;
using ::tsdb2::testing::io::BufferAsBytes;
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
  WriteQueue write_queue_{socket1_.get(), absl::GetFlag(FLAGS_http2_max_frame_payload_size)};
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

TYPED_TEST(WriteQueueTest, AppendHeaders) {
  HeaderSet const headers{
      {":status", "302"},
      {"cache-control", "private"},
      {"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
      {"location", "https://www.example.com"},
  };
  this->write_queue_.AppendFieldsFrames(123, headers, /*end_of_stream=*/false);
  EXPECT_THAT(this->Read(sizeof(FrameHeader)),
              IsOkAndHolds(BufferAs<FrameHeader>(
                  AllOf(Property(&FrameHeader::length, 54),
                        Property(&FrameHeader::frame_type, FrameType::kHeaders),
                        Property(&FrameHeader::flags, kFlagEndHeaders),
                        Property(&FrameHeader::stream_id, 123)))));
  EXPECT_THAT(
      this->Read(54),
      IsOkAndHolds(BufferAsBytes(ElementsAreArray({
          0x48, 0x82, 0x64, 0x02, 0x58, 0x85, 0xAE, 0xC3, 0x77, 0x1A, 0x4B, 0x61, 0x96, 0xD0,
          0x7A, 0xBE, 0x94, 0x10, 0x54, 0xD4, 0x44, 0xA8, 0x20, 0x05, 0x95, 0x04, 0x0B, 0x81,
          0x66, 0xE0, 0x82, 0xA6, 0x2D, 0x1B, 0xFF, 0x6E, 0x91, 0x9D, 0x29, 0xAD, 0x17, 0x18,
          0x63, 0xC7, 0x8F, 0x0B, 0x97, 0xC8, 0xE9, 0xAE, 0x82, 0xAE, 0x43, 0xD3,
      }))));
}

TYPED_TEST(WriteQueueTest, AppendOtherHeaders) {
  HeaderSet const headers1{
      {":status", "302"},
      {"cache-control", "private"},
      {"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
      {"location", "https://www.example.com"},
  };
  HeaderSet const headers2{
      {":status", "307"},
      {"cache-control", "private"},
      {"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
      {"location", "https://www.example.com"},
  };
  this->write_queue_.AppendFieldsFrames(345, headers1, /*end_of_stream=*/true);
  this->write_queue_.AppendFieldsFrames(567, headers2, /*end_of_stream=*/false);
  ASSERT_THAT(this->Read(sizeof(FrameHeader)),
              IsOkAndHolds(BufferAs<FrameHeader>(
                  AllOf(Property(&FrameHeader::length, 54),
                        Property(&FrameHeader::frame_type, FrameType::kHeaders),
                        Property(&FrameHeader::flags, kFlagEndHeaders | kFlagEndStream),
                        Property(&FrameHeader::stream_id, 345)))));
  ASSERT_THAT(
      this->Read(54),
      IsOkAndHolds(BufferAsBytes(ElementsAreArray({
          0x48, 0x82, 0x64, 0x02, 0x58, 0x85, 0xAE, 0xC3, 0x77, 0x1A, 0x4B, 0x61, 0x96, 0xD0,
          0x7A, 0xBE, 0x94, 0x10, 0x54, 0xD4, 0x44, 0xA8, 0x20, 0x05, 0x95, 0x04, 0x0B, 0x81,
          0x66, 0xE0, 0x82, 0xA6, 0x2D, 0x1B, 0xFF, 0x6E, 0x91, 0x9D, 0x29, 0xAD, 0x17, 0x18,
          0x63, 0xC7, 0x8F, 0x0B, 0x97, 0xC8, 0xE9, 0xAE, 0x82, 0xAE, 0x43, 0xD3,
      }))));
  ASSERT_THAT(this->Read(sizeof(FrameHeader)),
              IsOkAndHolds(BufferAs<FrameHeader>(
                  AllOf(Property(&FrameHeader::length, 8),
                        Property(&FrameHeader::frame_type, FrameType::kHeaders),
                        Property(&FrameHeader::flags, kFlagEndHeaders),
                        Property(&FrameHeader::stream_id, 567)))));
  ASSERT_THAT(this->Read(8), IsOkAndHolds(BufferAsBytes(ElementsAreArray({
                                 0x48,
                                 0x03,
                                 0x33,
                                 0x30,
                                 0x37,
                                 0xC1,
                                 0xC0,
                                 0xBF,
                             }))));
}

TYPED_TEST(WriteQueueTest, AppendDataFrame) {
  std::string_view constexpr kData = "0123456789";
  this->write_queue_.AppendDataFrames(123, Buffer(kData.data(), kData.size()),
                                      /*end_of_stream=*/false);
  EXPECT_THAT(this->Read(sizeof(FrameHeader)),
              IsOkAndHolds(BufferAs<FrameHeader>(AllOf(
                  Property(&FrameHeader::length, kData.size()),
                  Property(&FrameHeader::frame_type, FrameType::kData),
                  Property(&FrameHeader::flags, 0), Property(&FrameHeader::stream_id, 123)))));
  EXPECT_THAT(this->Read(kData.size()), IsOkAndHolds(BufferAsString(kData)));
}

TYPED_TEST(WriteQueueTest, AppendDataFrameEndingStream) {
  std::string_view constexpr kData = "9876543210";
  this->write_queue_.AppendDataFrames(321, Buffer(kData.data(), kData.size()),
                                      /*end_of_stream=*/true);
  EXPECT_THAT(
      this->Read(sizeof(FrameHeader)),
      IsOkAndHolds(BufferAs<FrameHeader>(AllOf(Property(&FrameHeader::length, kData.size()),
                                               Property(&FrameHeader::frame_type, FrameType::kData),
                                               Property(&FrameHeader::flags, kFlagEndStream),
                                               Property(&FrameHeader::stream_id, 321)))));
  EXPECT_THAT(this->Read(kData.size()), IsOkAndHolds(BufferAsString(kData)));
}

TYPED_TEST(WriteQueueTest, AppendResetStream) {
  this->write_queue_.AppendResetStreamFrame(123, ErrorCode::kStreamClosed);
  EXPECT_THAT(this->Read(sizeof(FrameHeader)),
              IsOkAndHolds(BufferAs<FrameHeader>(AllOf(
                  Property(&FrameHeader::length, sizeof(ResetStreamPayload)),
                  Property(&FrameHeader::frame_type, FrameType::kResetStream),
                  Property(&FrameHeader::flags, 0), Property(&FrameHeader::stream_id, 123)))));
  EXPECT_THAT(this->Read(sizeof(ResetStreamPayload)),
              IsOkAndHolds(BufferAs<ResetStreamPayload>(
                  Property(&ResetStreamPayload::error_code, ErrorCode::kStreamClosed))));
}

TYPED_TEST(WriteQueueTest, AppendSettingsAck) {
  this->write_queue_.AppendSettingsAckFrame();
  EXPECT_THAT(this->Read(sizeof(FrameHeader)),
              IsOkAndHolds(BufferAs<FrameHeader>(AllOf(
                  Property(&FrameHeader::length, 0),
                  Property(&FrameHeader::frame_type, FrameType::kSettings),
                  Property(&FrameHeader::flags, kFlagAck), Property(&FrameHeader::stream_id, 0)))));
}

TYPED_TEST(WriteQueueTest, AppendPingAck) {
  uint64_t const payload = 71104;
  this->write_queue_.AppendPingAckFrame(Buffer(&payload, kPingPayloadSize));
  EXPECT_THAT(this->Read(sizeof(FrameHeader)),
              IsOkAndHolds(BufferAs<FrameHeader>(AllOf(
                  Property(&FrameHeader::length, kPingPayloadSize),
                  Property(&FrameHeader::frame_type, FrameType::kPing),
                  Property(&FrameHeader::flags, kFlagAck), Property(&FrameHeader::stream_id, 0)))));
  EXPECT_THAT(this->Read(kPingPayloadSize), IsOkAndHolds(BufferAs<uint64_t>(payload)));
}

TYPED_TEST(WriteQueueTest, GoAway) {
  absl::Notification done;
  this->write_queue_.GoAway(/*error_code=*/ErrorCode::kStreamClosed,
                            /*last_processed_stream_id=*/123,
                            /*reset_queue=*/false,
                            /*callback=*/[&done] { done.Notify(); });
  EXPECT_THAT(this->Read(sizeof(FrameHeader)),
              IsOkAndHolds(BufferAs<FrameHeader>(
                  AllOf(Property(&FrameHeader::length, sizeof(GoAwayPayload)),
                        Property(&FrameHeader::frame_type, FrameType::kGoAway),
                        Property(&FrameHeader::flags, 0), Property(&FrameHeader::stream_id, 0)))));
  EXPECT_THAT(this->Read(sizeof(GoAwayPayload)),
              IsOkAndHolds(BufferAs<GoAwayPayload>(
                  AllOf(Property(&GoAwayPayload::last_stream_id, 123),
                        Property(&GoAwayPayload::error_code, ErrorCode::kStreamClosed)))));
  done.WaitForNotification();
}

}  // namespace
