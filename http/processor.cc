#include "http/processor.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "common/flat_map.h"
#include "common/utilities.h"
#include "http/channel.h"
#include "http/handlers.h"
#include "http/hpack.h"
#include "http/http.h"
#include "io/buffer.h"
#include "io/cord.h"

namespace tsdb2 {
namespace http {

namespace {

using ::tsdb2::io::Buffer;
using ::tsdb2::io::Cord;

}  // namespace

ChannelProcessor::ChannelProcessor(internal::ChannelInterface* const parent)
    : parent_(parent),
      max_concurrent_streams_(absl::GetFlag(FLAGS_http2_max_concurrent_streams)),
      initial_stream_window_size_(absl::GetFlag(FLAGS_http2_initial_stream_window_size)),
      max_frame_payload_size_(absl::GetFlag(FLAGS_http2_max_frame_payload_size)),
      max_header_list_size_(absl::GetFlag(FLAGS_http2_max_header_list_size)),
      write_queue_(parent_->socket(), max_frame_payload_size_) {}

Error ChannelProcessor::ValidateFrameHeader(FrameHeader const& header) {
  absl::MutexLock lock{&mutex_};
  auto const error = ValidateFrameHeaderLocked(header);
  if (!error.ok() && error.type() == ErrorType::kConnectionError) {
    GoAwayNowLocked(error.code());
  }
  return error;
}

Error ChannelProcessor::ValidateContinuationHeader(uint32_t const stream_id,
                                                   FrameHeader const& header) {
  if (header.frame_type() != FrameType::kContinuation) {
    return ConnectionError(ErrorCode::kProtocolError);
  }
  if (header.stream_id() != stream_id) {
    return ConnectionError(ErrorCode::kProtocolError);
  }
  return NoError();
}

void ChannelProcessor::ProcessFrame(FrameHeader const& header, Buffer payload) {
  switch (header.frame_type()) {
    case FrameType::kData:
      ProcessDataFrame(header, std::move(payload));
      break;
    case FrameType::kHeaders:
      return ProcessHeadersFrame(header, std::move(payload));
    case FrameType::kPriority:
      // PRIORITY is deprecated, nothing to do here.
      break;
    case FrameType::kResetStream:
      ProcessResetStreamFrame(header);
      break;
    case FrameType::kSettings:
      ProcessSettingsFrame(header, payload);
      break;
    case FrameType::kPushPromise:
      ProcessPushPromiseFrame(header);
      break;
    case FrameType::kPing:
      ProcessPingFrame(header, payload);
      break;
    case FrameType::kGoAway:
      ProcessGoAwayFrame(header, std::move(payload));
      break;
    case FrameType::kWindowUpdate:
      // TODO: process WINDOW_UPDATE frames.
      break;
    case FrameType::kContinuation:
      // NOTE: proper CONTINUATION frames are handled inside the processing of HEADERS or
      // PUSH_PROMISE frames, so if we end up here we can assume it's a protocol error.
      return GoAwayNow(ErrorCode::kProtocolError);
    default:
      return GoAwayNow(ErrorCode::kInternalError);
  }
  parent_->Continue();
}

void ChannelProcessor::SendSettings() {
  absl::MutexLock lock{&mutex_};
  write_queue_.AppendFrame(MakeSettingsFrame());
}

void ChannelProcessor::GoAway(ErrorCode const error_code) {
  absl::MutexLock lock{&mutex_};
  if (!going_away_) {
    going_away_ = true;
    write_queue_.GoAway(error_code, last_processed_stream_id_, /*reset_queue=*/false,
                        /*callback=*/nullptr);
  }
}

void ChannelProcessor::GoAwayNow(ErrorCode const error_code) {
  absl::MutexLock lock{&mutex_};
  if (!going_away_) {
    GoAwayNowLocked(error_code);
  }
}

void ChannelProcessor::DataBuffer::AddChunk(Buffer buffer, bool const last) {
  DataCallback callback;
  bool ended;
  {
    absl::MutexLock lock{&mutex_};
    if (callback_) {
      callback_.swap(callback);
      ended = ended_;
    } else {
      data_.Append(std::move(buffer));
      ended_ = last;
      return;
    }
  }
  callback(Cord(std::move(buffer)), ended);
}

void ChannelProcessor::DataBuffer::Read(DataCallback callback) {
  Cord data;
  bool ended;
  {
    absl::MutexLock lock{&mutex_};
    if (data_.empty()) {
      callback_ = std::move(callback);
      return;
    } else {
      data_.swap(data);
      ended = ended_;
    }
  }
  callback(std::move(data), ended);
}

Error ChannelProcessor::Stream::ProcessData(Buffer buffer, bool const end_stream) {
  switch (state_) {
    case StreamState::kIdle:
    case StreamState::kReservedLocal:
    case StreamState::kReservedRemote:
    case StreamState::kHalfClosedRemote:
      return ConnectionError(ErrorCode::kProtocolError);
    case StreamState::kClosed:
      return ConnectionError(ErrorCode::kStreamClosed);
    default:
      data_buffer_.AddChunk(std::move(buffer), end_stream);
      if (end_stream) {
        if (state_ != StreamState::kOpen) {
          state_ = StreamState::kClosed;
        } else {
          state_ = StreamState::kHalfClosedRemote;
        }
      }
      break;
  }
  return NoError();
}

Error ChannelProcessor::Stream::ProcessFields(hpack::HeaderSet fields) {
  switch (state_) {
    case StreamState::kIdle:
      state_ = StreamState::kOpen;
      break;
    case StreamState::kReservedRemote:
      state_ = StreamState::kHalfClosedLocal;
      break;
    case StreamState::kHalfClosedRemote:
      state_ = StreamState::kClosed;
      return StreamError(ErrorCode::kStreamClosed);
    case StreamState::kClosed:
      return ConnectionError(ErrorCode::kStreamClosed);
    default:
      state_ = StreamState::kClosed;
      return ConnectionError(ErrorCode::kProtocolError);
  }

  auto field_map = FlattenFields(std::move(fields));

  auto const method_name_it = field_map.find(kMethodHeaderName);
  if (method_name_it == field_map.end()) {
    return ErrorOut(Status::k400);
  }
  auto const method_it = kMethodsByName.find(method_name_it->second);
  if (method_it == kMethodsByName.end()) {
    return ErrorOut(Status::k405);
  }
  Method const method = method_it->second;

  auto const path_it = field_map.find(kPathHeaderName);
  if (path_it == field_map.end()) {
    return ErrorOut(Status::k400);
  }
  std::string_view const path = path_it->second;

  auto const status_or_handler = parent_->GetHandler(path);
  if (!status_or_handler.ok()) {
    return ErrorOut(Status::k404);
  }
  auto& handler = *status_or_handler.value();

  Request request(method, path);
  request.headers = std::move(field_map);
  handler(this, request);

  return NoError();
}

void ChannelProcessor::Stream::ProcessReset() { state_ = StreamState::kClosed; }

Error ChannelProcessor::Stream::ProcessPushPromise() {
  switch (state_) {
    case StreamState::kIdle:
      state_ = StreamState::kReservedRemote;
      return NoError();
    case StreamState::kHalfClosedRemote:
      return StreamError(ErrorCode::kStreamClosed);
    case StreamState::kClosed:
      return ConnectionError(ErrorCode::kStreamClosed);
    default:
      state_ = StreamState::kClosed;
      return ConnectionError(ErrorCode::kProtocolError);
  }
}

void ChannelProcessor::Stream::ReadData(DataCallback callback) {
  data_buffer_.Read(std::move(callback));
}

absl::Status ChannelProcessor::Stream::SendFields(hpack::HeaderSet const& fields,
                                                  bool const end_stream) {
  switch (state_) {
    case StreamState::kIdle:
      state_ = StreamState::kOpen;
      break;
    case StreamState::kReservedLocal:
      state_ = StreamState::kHalfClosedRemote;
      break;
    case StreamState::kOpen:
      break;
    default:
      return absl::FailedPreconditionError(
          absl::StrCat("cannot send HEADERS from a stream that's already closed ",
                       GetStreamDescriptionForErrors()));
  }
  if (end_stream) {
    RETURN_IF_ERROR(EndStream());
  }
  parent_->SendFields(id_, fields, end_stream);
  return absl::OkStatus();
}

absl::Status ChannelProcessor::Stream::SendData(Buffer const buffer, bool const end_stream) {
  if (state_ != StreamState::kOpen && state_ != StreamState::kHalfClosedRemote) {
    return absl::FailedPreconditionError(absl::StrCat(
        "cannot send DATA from a stream that's already closed ", GetStreamDescriptionForErrors()));
  }
  if (end_stream) {
    RETURN_IF_ERROR(EndStream());
  }
  parent_->SendData(id_, buffer, end_stream);
  return absl::OkStatus();
}

tsdb2::common::flat_map<std::string, std::string> ChannelProcessor::Stream::FlattenFields(
    hpack::HeaderSet fields) {
  tsdb2::common::flat_map<std::string, std::string> result;
  result.reserve(fields.size());
  for (auto& [key, value] : fields) {
    result.insert_or_assign(std::move(key), std::move(value));
  }
  return result;
}

Error ChannelProcessor::Stream::ErrorOut(Status const http_status) {
  parent_->write_queue_.AppendFieldsFrames(
      id_, {{":status", absl::StrCat(tsdb2::util::to_underlying(http_status))}},
      /*end_of_stream=*/true);
  if (state_ == StreamState::kOpen) {
    state_ = StreamState::kHalfClosedLocal;
  } else {
    state_ = StreamState::kClosed;
  }
  return NoError();
}

std::string ChannelProcessor::Stream::GetStreamDescriptionForErrors() {
  return absl::StrCat("(ID: ", id_, ", state: ", kStreamStateNames.at(state_), ")");
}

absl::Status ChannelProcessor::Stream::EndStream() {
  switch (state_) {
    case StreamState::kOpen:
      state_ = StreamState::kHalfClosedLocal;
      break;
    case StreamState::kHalfClosedRemote:
      state_ = StreamState::kClosed;
      break;
    default:
      return absl::FailedPreconditionError(
          absl::StrCat("cannot close an already closed stream ", GetStreamDescriptionForErrors()));
  }
  return absl::OkStatus();
}

Buffer ChannelProcessor::MakeSettingsFrame() const {
  size_t const num_entries = max_concurrent_streams_ ? kNumSettings : kNumSettings - 1;
  auto const header = FrameHeader()
                          .set_length(num_entries * sizeof(SettingsEntry))
                          .set_frame_type(FrameType::kSettings)
                          .set_flags(0)
                          .set_stream_id(0);
  Buffer buffer{sizeof(FrameHeader) + num_entries * sizeof(SettingsEntry)};
  buffer.MemCpy(&header, sizeof(FrameHeader));
  SettingsEntry entries[kNumSettings] = {
      SettingsEntry()
          .set_identifier(SettingsIdentifier::kHeaderTableSize)
          .set_value(field_decoder_.max_dynamic_header_table_size()),
      SettingsEntry()
          .set_identifier(SettingsIdentifier::kEnablePush)
          .set_value(enable_push_ ? 1 : 0),
      SettingsEntry()
          .set_identifier(SettingsIdentifier::kInitialWindowSize)
          .set_value(initial_stream_window_size_),
      SettingsEntry()
          .set_identifier(SettingsIdentifier::kMaxFrameSize)
          .set_value(max_frame_payload_size_),
      SettingsEntry()
          .set_identifier(SettingsIdentifier::kMaxHeaderListSize)
          .set_value(max_header_list_size_),
      SettingsEntry().set_identifier(SettingsIdentifier::kMaxConcurrentStreams).set_value(0),
  };
  if (max_concurrent_streams_) {
    entries[5].set_value(*max_concurrent_streams_);
  }
  buffer.MemCpy(entries, num_entries * sizeof(SettingsEntry));
  return buffer;
}

void ChannelProcessor::GoAwayNowLocked(ErrorCode const error_code) {
  going_away_ = true;
  write_queue_.GoAway(error_code, last_processed_stream_id_, /*reset_queue=*/true,
                      absl::bind_front(&internal::ChannelInterface::CloseConnection, parent_));
}

absl::StatusOr<ChannelProcessor::Stream*> ChannelProcessor::GetOrCreateStreamLocked(
    uint32_t const id) {
  auto it = streams_.find(id);
  if (it != streams_.end()) {
    return it->get();
  }
  if (going_away_) {
    return absl::CancelledError("going away");
  }
  bool unused;
  std::tie(it, unused) =
      streams_.emplace(std::make_unique<Stream>(this, id, initial_stream_window_size_));
  last_processed_stream_id_ = id;
  return it->get();
}

Error ChannelProcessor::ValidateDataHeader(FrameHeader const& header) {
  if (header.stream_id() == 0) {
    return ConnectionError(ErrorCode::kProtocolError);
  }
  if (((header.flags() & kFlagPadded) != 0) && (header.length() < 1)) {
    return ConnectionError(ErrorCode::kFrameSizeError);
  }
  return NoError();
}

Error ChannelProcessor::ValidateHeadersHeader(FrameHeader const& header) {
  if (header.stream_id() == 0) {
    return ConnectionError(ErrorCode::kProtocolError);
  }
  size_t min_size = 0;
  auto const flags = header.flags();
  if ((flags & kFlagPriority) != 0) {
    min_size += 5;
  }
  if ((flags & kFlagPadded) != 0) {
    min_size += 1;
  }
  if (header.length() < min_size) {
    return ConnectionError(ErrorCode::kFrameSizeError);
  }
  return NoError();
}

Error ChannelProcessor::ValidatePriorityHeader(FrameHeader const& header) {
  if (header.stream_id() == 0) {
    return ConnectionError(ErrorCode::kProtocolError);
  }
  if (header.length() != sizeof(PriorityPayload)) {
    return ConnectionError(ErrorCode::kFrameSizeError);
  }
  return NoError();
}

Error ChannelProcessor::ValidateResetStreamHeader(FrameHeader const& header) {
  if (header.stream_id() == 0) {
    return ConnectionError(ErrorCode::kProtocolError);
  }
  if (header.length() != sizeof(ResetStreamPayload)) {
    return ConnectionError(ErrorCode::kFrameSizeError);
  }
  return NoError();
}

Error ChannelProcessor::ValidateSettingsHeader(FrameHeader const& header) {
  if (header.stream_id() != 0) {
    return ConnectionError(ErrorCode::kProtocolError);
  }
  auto const length = header.length();
  if ((header.flags() & kFlagAck) != 0) {
    if (length != 0) {
      return ConnectionError(ErrorCode::kFrameSizeError);
    }
  } else if ((length == 0) || (length % sizeof(SettingsEntry) != 0)) {
    return ConnectionError(ErrorCode::kFrameSizeError);
  }
  return NoError();
}

Error ChannelProcessor::ValidatePushPromiseHeader(FrameHeader const& header) {
  if (header.stream_id() == 0) {
    return ConnectionError(ErrorCode::kProtocolError);
  }
  // TODO
  return NoError();
}

Error ChannelProcessor::ValidatePingHeader(FrameHeader const& header) {
  if (header.stream_id() != 0) {
    return ConnectionError(ErrorCode::kProtocolError);
  }
  if (header.length() != kPingPayloadSize) {
    return ConnectionError(ErrorCode::kFrameSizeError);
  }
  if ((header.flags() & kFlagAck) != 0) {
    return ConnectionError(ErrorCode::kProtocolError);
  }
  return NoError();
}

Error ChannelProcessor::ValidateGoAwayHeader(FrameHeader const& header) {
  if (header.stream_id() != 0) {
    return ConnectionError(ErrorCode::kProtocolError);
  }
  if (header.length() < sizeof(GoAwayPayload)) {
    return ConnectionError(ErrorCode::kFrameSizeError);
  }
  return NoError();
}

Error ChannelProcessor::ValidateWindowUpdateHeader(FrameHeader const& header) {
  if (header.length() != sizeof(WindowUpdatePayload)) {
    return ConnectionError(ErrorCode::kFrameSizeError);
  }
  return NoError();
}

Error ChannelProcessor::ValidateFrameHeaderLocked(FrameHeader const& header) const {
  auto const length = header.length();
  if (length > max_frame_payload_size_) {
    return ConnectionError(ErrorCode::kFrameSizeError);
  }
  switch (header.frame_type()) {
    case FrameType::kData:
      return ValidateDataHeader(header);
    case FrameType::kHeaders:
      return ValidateHeadersHeader(header);
    case FrameType::kPriority:
      return ValidatePriorityHeader(header);
    case FrameType::kResetStream:
      return ValidateResetStreamHeader(header);
    case FrameType::kSettings:
      return ValidateSettingsHeader(header);
    case FrameType::kPushPromise:
      return ValidatePushPromiseHeader(header);
    case FrameType::kPing:
      return ValidatePingHeader(header);
    case FrameType::kGoAway:
      return ValidateGoAwayHeader(header);
    case FrameType::kWindowUpdate:
      return ValidateWindowUpdateHeader(header);
    case FrameType::kContinuation:  // NOLINT(bugprone-branch-clone)
      // NOTE: proper CONTINUATION frames are handled inside the processing of HEADERS and
      // PUSH_PROMISE frames, so if we end up here we can assume it's a protocol error.
      return ConnectionError(ErrorCode::kProtocolError);
    default:
      return ConnectionError(ErrorCode::kProtocolError);
  }
}

void ChannelProcessor::ProcessDataFrame(FrameHeader const& header, Buffer payload) {
  size_t offset = 0;
  size_t pad_length = 0;
  auto const flags = header.flags();
  if ((flags & kFlagPadded) != 0) {
    offset += 1;
    pad_length += payload.at<uint8_t>(0);
  }
  auto const length = header.length();
  if (offset + pad_length > length) {
    return GoAway(ErrorCode::kFrameSizeError);
  }
  Buffer data;
  if (offset > 0 || pad_length > 0) {
    data = Buffer(payload.span(offset, length - offset - pad_length));
  } else {
    data = std::move(payload);
  }
  bool const end_of_stream = (flags & kFlagEndStream) != 0;
  auto const stream_id = header.stream_id();
  absl::ReleasableMutexLock lock{&mutex_};
  auto const status_or_stream = GetOrCreateStreamLocked(stream_id);
  if (!status_or_stream.ok()) {
    return;
  }
  auto* const stream = status_or_stream.value();
  auto const error = stream->ProcessData(std::move(data), end_of_stream);
  if (!error.ok()) {
    if (error.type() == ErrorType::kConnectionError) {
      GoAwayNowLocked(error.code());
    } else {
      lock.Release();
      write_queue_.AppendResetStreamFrame(stream_id, error.code());
    }
  }
}

void ChannelProcessor::ProcessFieldBlock(uint32_t const stream_id, Buffer field_block) {
  absl::ReleasableMutexLock lock{&mutex_};
  auto status_or_fields = field_decoder_.Decode(field_block.span());
  if (!status_or_fields.ok()) {
    return GoAwayNowLocked(ErrorCode::kCompressionError);
  }
  auto const status_or_stream = GetOrCreateStreamLocked(stream_id);
  if (!status_or_stream.ok()) {
    return;
  }
  auto* const stream = status_or_stream.value();
  auto const error = stream->ProcessFields(std::move(status_or_fields).value());
  if (!error.ok()) {
    if (error.type() == ErrorType::kConnectionError) {
      GoAwayNowLocked(error.code());
    } else {
      lock.Release();
      write_queue_.AppendResetStreamFrame(stream_id, error.code());
    }
  }
}

void ChannelProcessor::ProcessHeadersFrame(FrameHeader const& header, Buffer payload) {
  size_t offset = 0;
  size_t pad_length = 0;
  auto const flags = header.flags();
  if ((flags & kFlagPadded) != 0) {
    offset += 1;
    pad_length += payload.at<uint8_t>(0);
  }
  if ((flags & kFlagPriority) != 0) {
    offset += 5;
  }
  auto const length = header.length();
  if (offset + pad_length > length) {
    return GoAwayNow(ErrorCode::kFrameSizeError);
  }
  auto const stream_id = header.stream_id();
  Cord field_block{Buffer(payload.span(offset, length - offset - pad_length))};
  if ((flags & kFlagEndHeaders) != 0) {
    ProcessFieldBlock(stream_id, std::move(field_block).Flatten());
    parent_->Continue();
  } else {
    parent_->ReadContinuationFrame(
        stream_id, absl::bind_front(&ChannelProcessor::ProcessContinuationFrame, this, stream_id,
                                    std::move(field_block)));
  }
}

void ChannelProcessor::ProcessContinuationFrame(uint32_t const stream_id, Cord field_block,
                                                FrameHeader const& header, Buffer payload) {
  field_block.Append(std::move(payload));
  if ((header.flags() & kFlagEndHeaders) != 0) {
    ProcessFieldBlock(stream_id, std::move(field_block).Flatten());
    parent_->Continue();
  } else {
    parent_->ReadContinuationFrame(
        stream_id, absl::bind_front(&ChannelProcessor::ProcessContinuationFrame, this, stream_id,
                                    std::move(field_block)));
  }
}

void ChannelProcessor::ProcessResetStreamFrame(FrameHeader const& header) {
  absl::MutexLock lock{&mutex_};
  auto const status_or_stream = GetOrCreateStreamLocked(header.stream_id());
  if (status_or_stream.ok()) {
    auto* const stream = status_or_stream.value();
    stream->ProcessReset();
  }
}

void ChannelProcessor::ProcessSettingsFrame(FrameHeader const& header, Buffer const& payload) {
  if ((header.flags() & kFlagAck) == 0) {
    write_queue_.AppendSettingsAckFrame();
  }
}

void ChannelProcessor::ProcessPushPromiseFrame(FrameHeader const& header) {
  auto const stream_id = header.stream_id();
  absl::ReleasableMutexLock lock{&mutex_};
  auto const status_or_stream = GetOrCreateStreamLocked(stream_id);
  if (!status_or_stream.ok()) {
    return;
  }
  auto* const stream = status_or_stream.value();
  auto const error = stream->ProcessPushPromise();
  if (!error.ok()) {
    if (error.type() == ErrorType::kConnectionError) {
      GoAwayNowLocked(error.code());
    } else {
      lock.Release();
      write_queue_.AppendResetStreamFrame(stream_id, error.code());
    }
  }
}

void ChannelProcessor::ProcessPingFrame(FrameHeader const& header, Buffer const& payload) {
  if ((header.flags() & kFlagAck) != 0) {
    GoAwayNow(ErrorCode::kProtocolError);
  } else {
    write_queue_.AppendPingAckFrame(payload);
  }
}

void ChannelProcessor::ProcessGoAwayFrame(FrameHeader const& header, Buffer payload) {
  absl::MutexLock lock{&mutex_};
  if (going_away_) {
    parent_->CloseConnection();
  } else {
    GoAwayNowLocked(payload.as<GoAwayPayload>().error_code());
  }
}

void ChannelProcessor::ProcessWindowUpdateFrame(FrameHeader const& header, Buffer buffer) {
  auto const& payload = buffer.as<WindowUpdatePayload>();
  absl::MutexLock lock{&mutex_};
  if (payload.window_size_increment() == 0) {
    return GoAwayNowLocked(ErrorCode::kProtocolError);
  }
  // TODO
}

absl::StatusOr<Handler*> ChannelProcessor::GetHandler(std::string_view const path) const {
  return parent_->GetHandler(path);
}

}  // namespace http
}  // namespace tsdb2
