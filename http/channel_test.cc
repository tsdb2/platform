#include "http/channel.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/default_scheduler.h"
#include "common/mock_clock.h"
#include "common/reffed_ptr.h"
#include "common/scheduler.h"
#include "common/scoped_override.h"
#include "common/simple_condition.h"
#include "common/singleton.h"
#include "common/testing.h"
#include "common/utilities.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "http/handlers.h"
#include "http/http.h"
#include "http/mock_channel_manager.h"
#include "io/buffer_testing.h"
#include "net/base_sockets.h"
#include "net/sockets.h"
#include "net/ssl_sockets.h"
#include "server/testing.h"

namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Field;
using ::testing::Not;
using ::testing::Property;
using ::testing::Return;
using ::testing::UnorderedElementsAre;
using ::tsdb2::common::MockClock;
using ::tsdb2::common::reffed_ptr;
using ::tsdb2::common::Scheduler;
using ::tsdb2::common::ScopedOverride;
using ::tsdb2::common::SimpleCondition;
using ::tsdb2::common::Singleton;
using ::tsdb2::http::BaseChannel;
using ::tsdb2::http::Channel;
using ::tsdb2::http::ChannelManager;
using ::tsdb2::http::ErrorCode;
using ::tsdb2::http::FrameHeader;
using ::tsdb2::http::FrameType;
using ::tsdb2::http::GoAwayFrame;
using ::tsdb2::http::GoAwayPayload;
using ::tsdb2::http::Handler;
using ::tsdb2::http::kClientPreface;
using ::tsdb2::http::kDefaultInitialWindowSize;
using ::tsdb2::http::kDefaultMaxDynamicHeaderTableSize;
using ::tsdb2::http::kDefaultMaxFramePayloadSize;
using ::tsdb2::http::kDefaultMaxHeaderListSize;
using ::tsdb2::http::kFlagAck;
using ::tsdb2::http::kPingPayloadSize;
using ::tsdb2::http::PriorityPayload;
using ::tsdb2::http::SettingsEntry;
using ::tsdb2::http::SettingsIdentifier;
using ::tsdb2::http::WindowUpdatePayload;
using ::tsdb2::net::Buffer;
using ::tsdb2::net::Socket;
using ::tsdb2::net::SSLSocket;
using ::tsdb2::testing::http::MockChannelManager;
using ::tsdb2::testing::io::BufferAs;
using ::tsdb2::testing::io::BufferAsArray;

using SocketTypes = ::testing::Types<tsdb2::net::Socket, tsdb2::net::SSLSocket>;

template <typename Socket>
class ChannelTest : public tsdb2::testing::init::Test {
 protected:
  explicit ChannelTest(std::pair<reffed_ptr<Channel<Socket>>, reffed_ptr<Socket>> sockets)
      : channel_(std::move(sockets.first)), peer_socket_(std::move(sockets.second)) {
    clock_.AdvanceTime(absl::Seconds(100));
    CHECK_OK(scheduler_.WaitUntilAllWorkersAsleep());
  }

  explicit ChannelTest() : ChannelTest(MakeConnection()) {}

  std::pair<reffed_ptr<Channel<Socket>>, reffed_ptr<Socket>> MakeConnection();

  absl::Status StartServer();

  static absl::StatusOr<Buffer> PeerRead(reffed_ptr<Socket> const &peer, size_t length);
  absl::StatusOr<Buffer> PeerRead(size_t const length) { return PeerRead(peer_socket_, length); }

  static absl::Status PeerWrite(reffed_ptr<Socket> const &peer, Buffer buffer);
  absl::Status PeerWrite(Buffer buffer) { return PeerWrite(peer_socket_, std::move(buffer)); }

  MockClock clock_;
  Scheduler scheduler_{Scheduler::Options{
      .num_workers = 10,
      .clock = &clock_,
      .start_now = true,
  }};
  ScopedOverride<Singleton<Scheduler>> scheduler_override_{&tsdb2::common::default_scheduler,
                                                           &scheduler_};

  MockChannelManager manager_;

  reffed_ptr<Channel<Socket>> const channel_;
  reffed_ptr<Socket> const peer_socket_;
};

template <typename Socket>
std::pair<reffed_ptr<Channel<Socket>>, reffed_ptr<Socket>> ChannelTest<Socket>::MakeConnection() {
  auto status_or_sockets = Channel<Socket>::CreatePairWithRawPeerForTesting(&manager_);
  CHECK_OK(status_or_sockets);
  return std::move(status_or_sockets).value();
}

template <typename Socket>
absl::Status ChannelTest<Socket>::StartServer() {
  channel_->StartServer();
  RETURN_IF_ERROR(PeerWrite(Buffer(kClientPreface.data(), kClientPreface.size())));
  RETURN_IF_ERROR(PeerRead(sizeof(FrameHeader) + 5 * sizeof(SettingsEntry)));
  auto const ack_header = FrameHeader()
                              .set_length(0)
                              .set_frame_type(FrameType::kSettings)
                              .set_flags(kFlagAck)
                              .set_stream_id(0);
  return PeerWrite(Buffer(&ack_header, sizeof(FrameHeader)));
}

template <typename Socket>
absl::StatusOr<Buffer> ChannelTest<Socket>::PeerRead(reffed_ptr<Socket> const &peer,
                                                     size_t const length) {
  absl::Mutex mutex;
  std::optional<absl::StatusOr<Buffer>> result;
  RETURN_IF_ERROR(peer->Read(length, [&](absl::StatusOr<Buffer> status_or_buffer) {
    absl::MutexLock lock{&mutex};
    result.emplace(std::move(status_or_buffer));
  }));
  absl::MutexLock lock{&mutex, SimpleCondition([&] { return result.has_value(); })};
  return std::move(result.value());
}

template <typename Socket>
absl::Status ChannelTest<Socket>::PeerWrite(reffed_ptr<Socket> const &peer, Buffer buffer) {
  absl::Mutex mutex;
  std::optional<absl::Status> result;
  RETURN_IF_ERROR(peer->Write(std::move(buffer), [&](absl::Status status) {
    absl::MutexLock lock{&mutex};
    result.emplace(std::move(status));
  }));
  absl::MutexLock lock{&mutex, SimpleCondition([&] { return result.has_value(); })};
  return std::move(result.value());
}

TYPED_TEST_SUITE(ChannelTest, SocketTypes);

TYPED_TEST(ChannelTest, StartServerWithDefaultSettings) {
  this->channel_->StartServer();
  EXPECT_FALSE(this->channel_->is_client());
  EXPECT_TRUE(this->channel_->is_server());
  EXPECT_OK(this->PeerWrite(Buffer(kClientPreface.data(), kClientPreface.size())));
  EXPECT_THAT(this->PeerRead(sizeof(FrameHeader)),
              IsOkAndHolds(BufferAs<FrameHeader>(
                  AllOf(Property(&FrameHeader::length, sizeof(SettingsEntry) * 5),
                        Property(&FrameHeader::frame_type, FrameType::kSettings),
                        Property(&FrameHeader::flags, 0), Property(&FrameHeader::stream_id, 0)))));
  EXPECT_THAT(
      this->PeerRead(sizeof(SettingsEntry) * 5),
      IsOkAndHolds(AllOf(BufferAsArray<SettingsEntry>(UnorderedElementsAre(
          AllOf(Property(&SettingsEntry::identifier, SettingsIdentifier::kHeaderTableSize),
                Property(&SettingsEntry::value, kDefaultMaxDynamicHeaderTableSize)),
          AllOf(Property(&SettingsEntry::identifier, SettingsIdentifier::kEnablePush),
                Property(&SettingsEntry::value, true)),
          AllOf(Property(&SettingsEntry::identifier, SettingsIdentifier::kInitialWindowSize),
                Property(&SettingsEntry::value, kDefaultInitialWindowSize)),
          AllOf(Property(&SettingsEntry::identifier, SettingsIdentifier::kMaxFrameSize),
                Property(&SettingsEntry::value, kDefaultMaxFramePayloadSize)),
          AllOf(Property(&SettingsEntry::identifier, SettingsIdentifier::kMaxHeaderListSize),
                Property(&SettingsEntry::value, kDefaultMaxHeaderListSize)))))));
}

TYPED_TEST(ChannelTest, StartServerWithCustomSettings) {
  absl::SetFlag(&FLAGS_http2_max_dynamic_header_table_size, 8000);
  absl::SetFlag(&FLAGS_http2_max_concurrent_streams, 100);
  absl::SetFlag(&FLAGS_http2_initial_stream_window_size, 30000);
  absl::SetFlag(&FLAGS_http2_max_frame_payload_size, 20000);
  absl::SetFlag(&FLAGS_http2_max_header_list_size, 2000000);
  auto const [channel, peer] = this->MakeConnection();
  channel->StartServer();
  EXPECT_FALSE(channel->is_client());
  EXPECT_TRUE(channel->is_server());
  EXPECT_OK(this->PeerWrite(peer, Buffer(kClientPreface.data(), kClientPreface.size())));
  EXPECT_THAT(this->PeerRead(peer, sizeof(FrameHeader)),
              IsOkAndHolds(BufferAs<FrameHeader>(
                  AllOf(Property(&FrameHeader::length, sizeof(SettingsEntry) * 6),
                        Property(&FrameHeader::frame_type, FrameType::kSettings),
                        Property(&FrameHeader::flags, 0), Property(&FrameHeader::stream_id, 0)))));
  EXPECT_THAT(
      this->PeerRead(peer, sizeof(SettingsEntry) * 6),
      IsOkAndHolds(AllOf(BufferAsArray<SettingsEntry>(UnorderedElementsAre(
          AllOf(Property(&SettingsEntry::identifier, SettingsIdentifier::kHeaderTableSize),
                Property(&SettingsEntry::value, 8000)),
          AllOf(Property(&SettingsEntry::identifier, SettingsIdentifier::kEnablePush),
                Property(&SettingsEntry::value, true)),
          AllOf(Property(&SettingsEntry::identifier, SettingsIdentifier::kMaxConcurrentStreams),
                Property(&SettingsEntry::value, 100)),
          AllOf(Property(&SettingsEntry::identifier, SettingsIdentifier::kInitialWindowSize),
                Property(&SettingsEntry::value, 30000)),
          AllOf(Property(&SettingsEntry::identifier, SettingsIdentifier::kMaxFrameSize),
                Property(&SettingsEntry::value, 20000)),
          AllOf(Property(&SettingsEntry::identifier, SettingsIdentifier::kMaxHeaderListSize),
                Property(&SettingsEntry::value, 2000000)))))));
}

TYPED_TEST(ChannelTest, FrameTooBig) {
  ASSERT_OK(this->StartServer());
  auto const header = FrameHeader()
                          .set_length(kDefaultMaxFramePayloadSize + 1)
                          .set_frame_type(FrameType::kData)
                          .set_flags(0)
                          .set_stream_id(1);
  EXPECT_OK(this->PeerWrite(Buffer(&header, sizeof(FrameHeader))));
  EXPECT_THAT(
      this->PeerRead(sizeof(GoAwayFrame)),
      AnyOf(
          Not(IsOk()),
          IsOkAndHolds(BufferAs<GoAwayFrame>(AllOf(
              Field(&GoAwayFrame::header,
                    AllOf(Property(&FrameHeader::length, sizeof(GoAwayPayload)),
                          Property(&FrameHeader::frame_type, FrameType::kGoAway),
                          Property(&FrameHeader::flags, 0), Property(&FrameHeader::stream_id, 0))),
              Field(&GoAwayFrame::payload,
                    AllOf(Property(&GoAwayPayload::last_stream_id, 0),
                          Property(&GoAwayPayload::error_code, ErrorCode::kFrameSizeError))))))));
  EXPECT_FALSE(this->channel_->is_open());
}

template <typename Socket>
class ServerChannelTest : public ChannelTest<Socket> {
 protected:
  explicit ServerChannelTest() { CHECK_OK(this->StartServer()); }
};

TYPED_TEST_SUITE(ServerChannelTest, SocketTypes);

TYPED_TEST(ServerChannelTest, ValidateEmptySettingsWithoutAck) {
  auto const header = FrameHeader()
                          .set_length(0)
                          .set_frame_type(FrameType::kSettings)
                          .set_flags(0)
                          .set_stream_id(0);
  ASSERT_OK(this->PeerWrite(Buffer(&header, sizeof(FrameHeader))));
  EXPECT_THAT(
      this->PeerRead(sizeof(GoAwayFrame)),
      AnyOf(
          Not(IsOk()),
          IsOkAndHolds(BufferAs<GoAwayFrame>(AllOf(
              Field(&GoAwayFrame::header,
                    AllOf(Property(&FrameHeader::length, sizeof(GoAwayPayload)),
                          Property(&FrameHeader::frame_type, FrameType::kGoAway),
                          Property(&FrameHeader::flags, 0), Property(&FrameHeader::stream_id, 0))),
              Field(&GoAwayFrame::payload,
                    AllOf(Property(&GoAwayPayload::last_stream_id, 0),
                          Property(&GoAwayPayload::error_code, ErrorCode::kFrameSizeError))))))));
  EXPECT_FALSE(this->channel_->is_open());
}

TYPED_TEST(ServerChannelTest, ValidateSettingsAckWithPayload) {
  auto const header = FrameHeader()
                          .set_length(sizeof(SettingsEntry))
                          .set_frame_type(FrameType::kSettings)
                          .set_flags(kFlagAck)
                          .set_stream_id(0);
  ASSERT_OK(this->PeerWrite(Buffer(&header, sizeof(FrameHeader))));
  auto const payload = SettingsEntry().set_identifier(SettingsIdentifier::kEnablePush).set_value(0);
  EXPECT_OK(this->PeerWrite(Buffer(&payload, sizeof(SettingsEntry))));
  EXPECT_THAT(
      this->PeerRead(sizeof(GoAwayFrame)),
      AnyOf(
          Not(IsOk()),
          IsOkAndHolds(BufferAs<GoAwayFrame>(AllOf(
              Field(&GoAwayFrame::header,
                    AllOf(Property(&FrameHeader::length, sizeof(GoAwayPayload)),
                          Property(&FrameHeader::frame_type, FrameType::kGoAway),
                          Property(&FrameHeader::flags, 0), Property(&FrameHeader::stream_id, 0))),
              Field(&GoAwayFrame::payload,
                    AllOf(Property(&GoAwayPayload::last_stream_id, 0),
                          Property(&GoAwayPayload::error_code, ErrorCode::kFrameSizeError))))))));
  EXPECT_FALSE(this->channel_->is_open());
}

TYPED_TEST(ServerChannelTest, ValidateSettingsWithStreamId) {
  auto const header = FrameHeader()
                          .set_length(sizeof(SettingsEntry))
                          .set_frame_type(FrameType::kSettings)
                          .set_flags(0)
                          .set_stream_id(123);
  ASSERT_OK(this->PeerWrite(Buffer(&header, sizeof(FrameHeader))));
  auto const payload = SettingsEntry().set_identifier(SettingsIdentifier::kEnablePush).set_value(0);
  EXPECT_OK(this->PeerWrite(Buffer(&payload, sizeof(SettingsEntry))));
  EXPECT_THAT(
      this->PeerRead(sizeof(GoAwayFrame)),
      AnyOf(
          Not(IsOk()),
          IsOkAndHolds(BufferAs<GoAwayFrame>(AllOf(
              Field(&GoAwayFrame::header,
                    AllOf(Property(&FrameHeader::length, sizeof(GoAwayPayload)),
                          Property(&FrameHeader::frame_type, FrameType::kGoAway),
                          Property(&FrameHeader::flags, 0), Property(&FrameHeader::stream_id, 0))),
              Field(&GoAwayFrame::payload,
                    AllOf(Property(&GoAwayPayload::last_stream_id, 0),
                          Property(&GoAwayPayload::error_code, ErrorCode::kProtocolError))))))));
  EXPECT_FALSE(this->channel_->is_open());
}

TYPED_TEST(ServerChannelTest, AckSettings) {
  auto const header = FrameHeader()
                          .set_length(sizeof(SettingsEntry))
                          .set_frame_type(FrameType::kSettings)
                          .set_flags(0)
                          .set_stream_id(0);
  ASSERT_OK(this->PeerWrite(Buffer(&header, sizeof(FrameHeader))));
  auto const payload = SettingsEntry().set_identifier(SettingsIdentifier::kEnablePush).set_value(0);
  EXPECT_OK(this->PeerWrite(Buffer(&payload, sizeof(SettingsEntry))));
  EXPECT_THAT(this->PeerRead(sizeof(FrameHeader)),
              IsOkAndHolds(BufferAs<FrameHeader>(AllOf(
                  Property(&FrameHeader::length, 0),
                  Property(&FrameHeader::frame_type, FrameType::kSettings),
                  Property(&FrameHeader::flags, kFlagAck), Property(&FrameHeader::stream_id, 0)))));
}

TYPED_TEST(ServerChannelTest, ValidatePingWithStreamId) {
  auto const header = FrameHeader()
                          .set_length(kPingPayloadSize)
                          .set_frame_type(FrameType::kPing)
                          .set_flags(0)
                          .set_stream_id(123);
  ASSERT_OK(this->PeerWrite(Buffer(&header, sizeof(FrameHeader))));
  uint64_t const payload = 0x7110400071104000;
  EXPECT_OK(this->PeerWrite(Buffer(&payload, kPingPayloadSize)));
  EXPECT_THAT(
      this->PeerRead(sizeof(GoAwayFrame)),
      AnyOf(
          Not(IsOk()),
          IsOkAndHolds(BufferAs<GoAwayFrame>(AllOf(
              Field(&GoAwayFrame::header,
                    AllOf(Property(&FrameHeader::length, sizeof(GoAwayPayload)),
                          Property(&FrameHeader::frame_type, FrameType::kGoAway),
                          Property(&FrameHeader::flags, 0), Property(&FrameHeader::stream_id, 0))),
              Field(&GoAwayFrame::payload,
                    AllOf(Property(&GoAwayPayload::last_stream_id, 0),
                          Property(&GoAwayPayload::error_code, ErrorCode::kProtocolError))))))));
  EXPECT_FALSE(this->channel_->is_open());
}

TYPED_TEST(ServerChannelTest, ValidatePingWithWrongSize) {
  auto const header = FrameHeader()
                          .set_length(kPingPayloadSize * 2)
                          .set_frame_type(FrameType::kPing)
                          .set_flags(0)
                          .set_stream_id(0);
  ASSERT_OK(this->PeerWrite(Buffer(&header, sizeof(FrameHeader))));
  uint64_t const payload[2] = {0x7110400071104000, 0x7110400071104000};
  EXPECT_OK(this->PeerWrite(Buffer(payload, kPingPayloadSize * 2)));
  EXPECT_THAT(
      this->PeerRead(sizeof(GoAwayFrame)),
      AnyOf(
          Not(IsOk()),
          IsOkAndHolds(BufferAs<GoAwayFrame>(AllOf(
              Field(&GoAwayFrame::header,
                    AllOf(Property(&FrameHeader::length, sizeof(GoAwayPayload)),
                          Property(&FrameHeader::frame_type, FrameType::kGoAway),
                          Property(&FrameHeader::flags, 0), Property(&FrameHeader::stream_id, 0))),
              Field(&GoAwayFrame::payload,
                    AllOf(Property(&GoAwayPayload::last_stream_id, 0),
                          Property(&GoAwayPayload::error_code, ErrorCode::kFrameSizeError))))))));
  EXPECT_FALSE(this->channel_->is_open());
}

TYPED_TEST(ServerChannelTest, ValidateUnknownPingAck) {
  auto const header = FrameHeader()
                          .set_length(kPingPayloadSize)
                          .set_frame_type(FrameType::kPing)
                          .set_flags(kFlagAck)
                          .set_stream_id(0);
  ASSERT_OK(this->PeerWrite(Buffer(&header, sizeof(FrameHeader))));
  uint64_t const payload = 0x7110400071104000;
  EXPECT_OK(this->PeerWrite(Buffer(&payload, kPingPayloadSize)));
  EXPECT_THAT(
      this->PeerRead(sizeof(GoAwayFrame)),
      AnyOf(
          Not(IsOk()),
          IsOkAndHolds(BufferAs<GoAwayFrame>(AllOf(
              Field(&GoAwayFrame::header,
                    AllOf(Property(&FrameHeader::length, sizeof(GoAwayPayload)),
                          Property(&FrameHeader::frame_type, FrameType::kGoAway),
                          Property(&FrameHeader::flags, 0), Property(&FrameHeader::stream_id, 0))),
              Field(&GoAwayFrame::payload,
                    AllOf(Property(&GoAwayPayload::last_stream_id, 0),
                          Property(&GoAwayPayload::error_code, ErrorCode::kProtocolError))))))));
  EXPECT_FALSE(this->channel_->is_open());
}

TYPED_TEST(ServerChannelTest, AckPing) {
  auto const header = FrameHeader()
                          .set_length(kPingPayloadSize)
                          .set_frame_type(FrameType::kPing)
                          .set_flags(0)
                          .set_stream_id(0);
  ASSERT_OK(this->PeerWrite(Buffer(&header, sizeof(FrameHeader))));
  uint64_t const payload = 0x7110400071104000;
  EXPECT_OK(this->PeerWrite(Buffer(&payload, kPingPayloadSize)));
  EXPECT_THAT(this->PeerRead(sizeof(FrameHeader)),
              IsOkAndHolds(BufferAs<FrameHeader>(AllOf(
                  Property(&FrameHeader::length, kPingPayloadSize),
                  Property(&FrameHeader::frame_type, FrameType::kPing),
                  Property(&FrameHeader::flags, kFlagAck), Property(&FrameHeader::stream_id, 0)))));
  EXPECT_THAT(this->PeerRead(kPingPayloadSize), IsOkAndHolds(BufferAs<uint64_t>(payload)));
}

TYPED_TEST(ServerChannelTest, ValidateChannelLevelWindowUpdateWithWrongSize) {
  auto const header = FrameHeader()
                          .set_length(sizeof(WindowUpdatePayload) * 2)
                          .set_frame_type(FrameType::kWindowUpdate)
                          .set_flags(0)
                          .set_stream_id(0);
  ASSERT_OK(this->PeerWrite(Buffer(&header, sizeof(FrameHeader))));
  Buffer buffer{sizeof(WindowUpdatePayload) * 2};
  auto const payload = WindowUpdatePayload().set_window_size_increment(123);
  buffer.MemCpy(&payload, sizeof(WindowUpdatePayload));
  buffer.MemCpy(&payload, sizeof(WindowUpdatePayload));
  EXPECT_OK(this->PeerWrite(std::move(buffer)));
  EXPECT_THAT(
      this->PeerRead(sizeof(GoAwayFrame)),
      AnyOf(
          Not(IsOk()),
          IsOkAndHolds(BufferAs<GoAwayFrame>(AllOf(
              Field(&GoAwayFrame::header,
                    AllOf(Property(&FrameHeader::length, sizeof(GoAwayPayload)),
                          Property(&FrameHeader::frame_type, FrameType::kGoAway),
                          Property(&FrameHeader::flags, 0), Property(&FrameHeader::stream_id, 0))),
              Field(&GoAwayFrame::payload,
                    AllOf(Property(&GoAwayPayload::last_stream_id, 0),
                          Property(&GoAwayPayload::error_code, ErrorCode::kFrameSizeError))))))));
  EXPECT_FALSE(this->channel_->is_open());
}

TYPED_TEST(ServerChannelTest, GoAway) {
  auto const header = FrameHeader()
                          .set_length(sizeof(GoAwayPayload))
                          .set_frame_type(FrameType::kGoAway)
                          .set_flags(0)
                          .set_stream_id(0);
  ASSERT_OK(this->PeerWrite(Buffer(&header, sizeof(FrameHeader))));
  auto const payload =
      GoAwayPayload().set_last_stream_id(0).set_error_code(ErrorCode::kInternalError);
  EXPECT_OK(this->PeerWrite(Buffer(&payload, sizeof(GoAwayPayload))));
  EXPECT_THAT(
      this->PeerRead(sizeof(GoAwayFrame)),
      AnyOf(
          Not(IsOk()),
          IsOkAndHolds(BufferAs<GoAwayFrame>(AllOf(
              Field(&GoAwayFrame::header,
                    AllOf(Property(&FrameHeader::length, sizeof(GoAwayPayload)),
                          Property(&FrameHeader::frame_type, FrameType::kGoAway),
                          Property(&FrameHeader::flags, 0), Property(&FrameHeader::stream_id, 0))),
              Field(&GoAwayFrame::payload,
                    AllOf(Property(&GoAwayPayload::last_stream_id, 0),
                          Property(&GoAwayPayload::error_code, ErrorCode::kInternalError))))))));
  EXPECT_FALSE(this->channel_->is_open());
}

// TODO

TYPED_TEST(ServerChannelTest, ValidatePriorityWithoutStreamId) {
  auto const header = FrameHeader()
                          .set_length(sizeof(PriorityPayload))
                          .set_frame_type(FrameType::kPriority)
                          .set_flags(0)
                          .set_stream_id(0);
  ASSERT_OK(this->PeerWrite(Buffer(&header, sizeof(FrameHeader))));
  auto const payload =
      PriorityPayload().set_exclusive(false).set_stream_dependency(321).set_weight(42);
  EXPECT_OK(this->PeerWrite(Buffer(&payload, sizeof(PriorityPayload))));
  EXPECT_THAT(
      this->PeerRead(sizeof(GoAwayFrame)),
      AnyOf(
          Not(IsOk()),
          IsOkAndHolds(BufferAs<GoAwayFrame>(AllOf(
              Field(&GoAwayFrame::header,
                    AllOf(Property(&FrameHeader::length, sizeof(GoAwayPayload)),
                          Property(&FrameHeader::frame_type, FrameType::kGoAway),
                          Property(&FrameHeader::flags, 0), Property(&FrameHeader::stream_id, 0))),
              Field(&GoAwayFrame::payload,
                    AllOf(Property(&GoAwayPayload::last_stream_id, 0),
                          Property(&GoAwayPayload::error_code, ErrorCode::kProtocolError))))))));
  EXPECT_FALSE(this->channel_->is_open());
}

// TODO

}  // namespace
