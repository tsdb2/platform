#include "http/processor.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "absl/log/log.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "http/channel.h"
#include "http/hpack.h"
#include "http/http.h"
#include "net/base_sockets.h"

namespace tsdb2 {
namespace http {

namespace {

using ::tsdb2::net::Buffer;

}  // namespace

ChannelProcessor::ChannelProcessor(internal::ChannelInterface* const parent)
    : parent_(parent), write_queue_(parent_->socket()) {}

ConnectionError ChannelProcessor::ValidateContinuationHeader(uint32_t const stream_id,
                                                             FrameHeader const& header) {
  absl::MutexLock lock{&mutex_};
  auto const error = ValidateContinuationHeaderLocked(stream_id, header);
  if (error != ConnectionError::kNoError) {
    GoAwayLocked(error);
  }
  return error;
}

ConnectionError ChannelProcessor::ValidateFrameHeader(FrameHeader const& header) {
  absl::MutexLock lock{&mutex_};
  auto const error = ValidateFrameHeaderLocked(header);
  if (error != ConnectionError::kNoError) {
    GoAwayLocked(error);
  }
  return error;
}

void ChannelProcessor::ProcessFrame(FrameHeader const& header, Buffer payload) {
  switch (header.frame_type()) {
    case FrameType::kData:
      ProcessDataFrame(header, std::move(payload));
      break;
    case FrameType::kHeaders:
      // NOTE: `ProcessHeadersFrame` is different from other process methods because it also
      // continues by invoking `ReadNextFrame` or `ReadContinuationFrame` depending on whether a
      // CONTINUATION frame is expected. Because of that we return immediately here, and skip the
      // final `ReadNextFrame()` call.
      return ProcessHeadersFrame(header, std::move(payload));
    case FrameType::kPriority:
      // PRIORITY is deprecated, nothing to do here.
      break;
    case FrameType::kResetStream:
      ProcessResetStreamFrame(header);
      break;
    case FrameType::kSettings:
      ProcessSettingsFrame(header, std::move(payload));
      break;
    case FrameType::kPushPromise:
      ProcessPushPromiseFrame(header);
      break;
    case FrameType::kPing:
      ProcessPingFrame(header, std::move(payload));
      break;
    case FrameType::kGoAway:
      ProcessGoAwayFrame(header, std::move(payload));
      break;
    case FrameType::kWindowUpdate:
      ProcessWindowUpdateFrame(header, std::move(payload));
      break;
    case FrameType::kContinuation:
      // NOTE: proper CONTINUATION frames are handled inside the processing of HEADERS frames, so
      // if we end up here we can assume it's a protocol error.
      GoAway(ConnectionError::kProtocolError);
      break;
    default:
      GoAway(ConnectionError::kInternalError);
      break;
  }
  parent_->ReadNextFrame();
}

void ChannelProcessor::ProcessContinuationFrame(uint32_t const stream_id, FrameHeader const& header,
                                                Buffer const payload) {
  auto const span = payload.span();
  absl::ReleasableMutexLock lock{&mutex_};
  auto& stream = GetOrCreateStreamLocked(stream_id);
  if ((stream.state != StreamState::kIdle && stream.state != StreamState::kReservedRemote) ||
      !stream.receiving_fields) {
    switch (stream.state) {
      case StreamState::kHalfClosedRemote:
      case StreamState::kClosed:
        ResetStreamLocked(stream_id, ConnectionError::kStreamClosed);
        break;
      default:
        ResetStreamLocked(stream_id, ConnectionError::kProtocolError);
        break;
    }
    stream.state = StreamState::kClosed;
    lock.Release();
    return parent_->ReadNextFrame();
  }
  stream.field_block.reserve(stream.field_block.size() + span.size());
  stream.field_block.insert(stream.field_block.end(), span.begin(), span.end());
  if ((header.flags() & kFlagEndHeaders) != 0) {
    switch (stream.state) {
      case StreamState::kIdle:
        stream.state = StreamState::kOpen;
        break;
      case StreamState::kReservedRemote:
        stream.state = StreamState::kHalfClosedLocal;
        break;
      default:
        // Nothing to do for other states here.
        break;
    }
    stream.receiving_fields = false;
    auto status_or_fields = field_decoder_.Decode(stream.field_block);
    stream.field_block.clear();
    if (status_or_fields.ok()) {
      OnFields(stream_id, std::move(status_or_fields).value());
    } else {
      stream.state = StreamState::kClosed;
      ResetStreamLocked(stream_id, ConnectionError::kCompressionError);
    }
    lock.Release();
    parent_->ReadNextFrame();
  } else {
    lock.Release();
    parent_->ReadContinuationFrame(stream_id);
  }
}

void ChannelProcessor::SendSettings() {
  absl::MutexLock lock{&mutex_};
  write_queue_.AppendFrame(MakeSettingsFrame());
}

void ChannelProcessor::GoAway(ConnectionError const error) {
  absl::MutexLock lock{&mutex_};
  GoAwayLocked(error);
}

void ChannelProcessor::OnData(uint32_t const stream_id, absl::Span<uint8_t const> const data) {
  // TODO
}

void ChannelProcessor::OnFields(uint32_t const stream_id, hpack::HeaderSet const fields) {
  // TODO: remove this temporary code.
  LOG(ERROR) << "--------";
  for (auto const& [key, value] : fields) {
    LOG(ERROR) << "### " << key << ": " << value;
  }
  LOG(ERROR) << "--------";
}

Buffer ChannelProcessor::MakeResetStreamFrame(uint32_t const id, ConnectionError const error) {
  auto const header = FrameHeader()
                          .set_length(sizeof(ResetStreamPayload))
                          .set_frame_type(FrameType::kResetStream)
                          .set_flags(0)
                          .set_stream_id(id);
  auto const payload = ResetStreamPayload().set_error_code(error);
  Buffer buffer{sizeof(FrameHeader) + sizeof(ResetStreamPayload)};
  buffer.MemCpy(&header, sizeof(FrameHeader));
  buffer.MemCpy(&payload, sizeof(ResetStreamPayload));
  return buffer;
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

Buffer ChannelProcessor::MakeSettingsAckFrame() {
  auto const header = FrameHeader()
                          .set_length(0)
                          .set_frame_type(FrameType::kSettings)
                          .set_flags(kFlagAck)
                          .set_stream_id(0);
  return Buffer{&header, sizeof(FrameHeader)};
}

Buffer ChannelProcessor::MakePingFrame(bool const ack, Buffer const& payload) {
  auto const header = FrameHeader()
                          .set_length(kPingPayloadSize)
                          .set_frame_type(FrameType::kPing)
                          .set_flags(ack ? kFlagAck : 0)
                          .set_stream_id(0);
  Buffer buffer{sizeof(FrameHeader) + kPingPayloadSize};
  buffer.MemCpy(&header, sizeof(header));
  buffer.MemCpy(payload.get(), kPingPayloadSize);
  return buffer;
}

Buffer ChannelProcessor::MakeGoAwayFrame(ConnectionError const error) const {
  auto const header = FrameHeader()
                          .set_length(sizeof(GoAwayPayload))
                          .set_frame_type(FrameType::kGoAway)
                          .set_flags(0)
                          .set_stream_id(0);
  auto const payload =
      GoAwayPayload().set_last_stream_id(last_processed_stream_id_).set_error_code(error);
  Buffer buffer{sizeof(FrameHeader) + sizeof(GoAwayPayload)};
  buffer.MemCpy(&header, sizeof(header));
  buffer.MemCpy(&payload, sizeof(payload));
  return buffer;
}

void ChannelProcessor::ResetStreamLocked(uint32_t const stream_id, ConnectionError const error) {
  write_queue_.AppendFrame(MakeResetStreamFrame(stream_id, error));
}

void ChannelProcessor::AckSettings() { write_queue_.AppendFrame(MakeSettingsAckFrame()); }

void ChannelProcessor::GoAwayLocked(ConnectionError const error) {
  going_away_ = true;
  write_queue_.AppendFrameSkippingQueue(MakeGoAwayFrame(error));
}

ChannelProcessor::Stream& ChannelProcessor::GetOrCreateStreamLocked(uint32_t const id) {
  auto const [it, inserted] = streams_.try_emplace(id, initial_stream_window_size_);
  if (inserted) {
    last_processed_stream_id_ = id;
  }
  return it->second;
}

ConnectionError ChannelProcessor::ValidateDataHeader(FrameHeader const& header) {
  if (header.stream_id() == 0) {
    return ConnectionError::kProtocolError;
  }
  if (((header.flags() & kFlagPadded) != 0) && (header.length() < 1)) {
    return ConnectionError::kFrameSizeError;
  }
  return ConnectionError::kNoError;
}

ConnectionError ChannelProcessor::ValidateHeadersHeader(FrameHeader const& header) {
  if (header.stream_id() == 0) {
    return ConnectionError::kProtocolError;
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
    return ConnectionError::kFrameSizeError;
  }
  return ConnectionError::kNoError;
}

ConnectionError ChannelProcessor::ValidatePriorityHeader(FrameHeader const& header) {
  if (header.stream_id() == 0) {
    return ConnectionError::kProtocolError;
  }
  if (header.length() != sizeof(PriorityPayload)) {
    return ConnectionError::kFrameSizeError;
  }
  return ConnectionError::kNoError;
}

ConnectionError ChannelProcessor::ValidateResetStreamHeader(FrameHeader const& header) {
  if (header.stream_id() == 0) {
    return ConnectionError::kProtocolError;
  }
  if (header.length() != sizeof(ResetStreamPayload)) {
    return ConnectionError::kFrameSizeError;
  }
  return ConnectionError::kNoError;
}

ConnectionError ChannelProcessor::ValidateSettingsHeader(FrameHeader const& header) {
  if (header.stream_id() != 0) {
    return ConnectionError::kProtocolError;
  }
  auto const length = header.length();
  if ((header.flags() & kFlagAck) != 0) {
    if (length != 0) {
      return ConnectionError::kFrameSizeError;
    }
  } else if ((length == 0) || (length % sizeof(SettingsEntry) != 0)) {
    return ConnectionError::kFrameSizeError;
  }
  return ConnectionError::kNoError;
}

ConnectionError ChannelProcessor::ValidatePushPromiseHeader(FrameHeader const& header) {
  // TODO
  return ConnectionError::kNoError;
}

ConnectionError ChannelProcessor::ValidatePingHeader(FrameHeader const& header) {
  if (header.stream_id() != 0) {
    return ConnectionError::kProtocolError;
  }
  if (header.length() != kPingPayloadSize) {
    return ConnectionError::kFrameSizeError;
  }
  if ((header.flags() & kFlagAck) != 0) {
    return ConnectionError::kProtocolError;
  }
  return ConnectionError::kNoError;
}

ConnectionError ChannelProcessor::ValidateGoAwayHeader(FrameHeader const& header) {
  if (header.stream_id() != 0) {
    return ConnectionError::kProtocolError;
  }
  if (header.length() < sizeof(GoAwayPayload)) {
    return ConnectionError::kFrameSizeError;
  }
  return ConnectionError::kNoError;
}

ConnectionError ChannelProcessor::ValidateWindowUpdateHeader(FrameHeader const& header) {
  if (header.length() != sizeof(WindowUpdatePayload)) {
    return ConnectionError::kFrameSizeError;
  }
  return ConnectionError::kNoError;
}

ConnectionError ChannelProcessor::ValidateContinuationHeaderLocked(
    uint32_t const stream_id, FrameHeader const& header) const {
  if (header.stream_id() != stream_id) {
    return ConnectionError::kProtocolError;
  }
  auto const it = streams_.find(stream_id);
  if (it == streams_.end() || !it->second.receiving_fields) {
    return ConnectionError::kInternalError;
  }
  return ConnectionError::kNoError;
}

ConnectionError ChannelProcessor::ValidateFrameHeaderLocked(FrameHeader const& header) const {
  auto const length = header.length();
  if (length > max_frame_payload_size_) {
    return ConnectionError::kFrameSizeError;
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
    case FrameType::kContinuation:
      // NOTE: proper CONTINUATION frames are handled inside the processing of HEADERS frames, so
      // if we end up here we can assume it's a protocol error.
      return ConnectionError::kProtocolError;
    default:
      return ConnectionError::kProtocolError;
  }
}

void ChannelProcessor::ProcessDataFrame(FrameHeader const& header, Buffer const payload) {
  size_t offset = 0;
  size_t pad_length = 0;
  auto const flags = header.flags();
  if ((flags & kFlagPadded) != 0) {
    offset += 1;
    pad_length += payload.at<uint8_t>(0);
  }
  auto const length = header.length();
  if (offset + pad_length > length) {
    return GoAway(ConnectionError::kFrameSizeError);
  }
  auto const span = payload.span(offset, length - offset - pad_length);
  auto const stream_id = header.stream_id();
  absl::MutexLock lock{&mutex_};
  auto& stream = GetOrCreateStreamLocked(stream_id);
  if (stream.state != StreamState::kOpen && stream.state != StreamState::kHalfClosedLocal) {
    stream.state = StreamState::kClosed;
    return ResetStreamLocked(stream_id, ConnectionError::kStreamClosed);
  }
  if ((flags & kFlagEndStream) != 0) {
    switch (stream.state) {
      case StreamState::kOpen:
        stream.state = StreamState::kHalfClosedRemote;
        break;
      case StreamState::kHalfClosedLocal:
        stream.state = StreamState::kClosed;
        break;
      default:
        stream.state = StreamState::kClosed;
        return ResetStreamLocked(stream_id, ConnectionError::kStreamClosed);
    }
  }
  OnData(stream_id, span);
}

void ChannelProcessor::ProcessHeadersFrame(FrameHeader const& header, Buffer const payload) {
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
    GoAway(ConnectionError::kFrameSizeError);
    return parent_->ReadNextFrame();
  }
  auto const span = payload.span(offset, length - offset - pad_length);
  auto const stream_id = header.stream_id();
  absl::ReleasableMutexLock lock{&mutex_};
  auto& stream = GetOrCreateStreamLocked(stream_id);
  if ((stream.state != StreamState::kIdle && stream.state != StreamState::kReservedRemote) ||
      stream.receiving_fields) {
    stream.state = StreamState::kClosed;
    switch (stream.state) {
      case StreamState::kHalfClosedRemote:
      case StreamState::kClosed:
        ResetStreamLocked(stream_id, ConnectionError::kStreamClosed);
        break;
      default:
        ResetStreamLocked(stream_id, ConnectionError::kProtocolError);
        break;
    }
    lock.Release();
    return parent_->ReadNextFrame();
  }
  stream.receiving_fields = true;
  if ((flags & kFlagEndHeaders) != 0) {
    switch (stream.state) {
      case StreamState::kIdle:
        stream.state = StreamState::kOpen;
        break;
      case StreamState::kReservedRemote:
        stream.state = StreamState::kHalfClosedLocal;
        break;
      default:
        // Nothing to do for other states here.
        break;
    }
    stream.receiving_fields = false;
    auto status_or_fields = field_decoder_.Decode(span);
    if (status_or_fields.ok()) {
      OnFields(stream_id, std::move(status_or_fields).value());
    } else {
      stream.state = StreamState::kClosed;
      ResetStreamLocked(stream_id, ConnectionError::kCompressionError);
    }
    lock.Release();
    parent_->ReadNextFrame();
  } else {
    stream.field_block.clear();
    stream.field_block.reserve(span.size());
    stream.field_block.insert(stream.field_block.end(), span.begin(), span.end());
    lock.Release();
    parent_->ReadContinuationFrame(stream_id);
  }
}

void ChannelProcessor::ProcessResetStreamFrame(FrameHeader const& header) {
  absl::MutexLock lock{&mutex_};
  auto& stream = GetOrCreateStreamLocked(header.stream_id());
  stream.state = StreamState::kClosed;
}

void ChannelProcessor::ProcessSettingsFrame(FrameHeader const& header, Buffer const payload) {
  if ((header.flags() & kFlagAck) == 0) {
    AckSettings();
  }
}

void ChannelProcessor::ProcessPushPromiseFrame(FrameHeader const& header) {
  auto const stream_id = header.stream_id();
  absl::MutexLock lock{&mutex_};
  auto& stream = GetOrCreateStreamLocked(stream_id);
  if (stream.state != StreamState::kIdle) {
    return ResetStreamLocked(stream_id, ConnectionError::kProtocolError);
  }
  stream.state = StreamState::kReservedRemote;
}

void ChannelProcessor::ProcessPingFrame(FrameHeader const& header, Buffer const payload) {
  if ((header.flags() & kFlagAck) != 0) {
    GoAway(ConnectionError::kProtocolError);
  } else {
    write_queue_.AppendFrameSkippingQueue(MakePingFrame(/*ack=*/true, std::move(payload)));
  }
}

void ChannelProcessor::ProcessGoAwayFrame(FrameHeader const& header, Buffer const payload) {
  absl::MutexLock lock{&mutex_};
  if (going_away_) {
    parent_->CloseConnection();
  } else {
    GoAwayLocked(payload.as<GoAwayPayload>().error_code());
  }
}

void ChannelProcessor::ProcessWindowUpdateFrame(FrameHeader const& header, Buffer const buffer) {
  auto const& payload = buffer.as<WindowUpdatePayload>();
  absl::MutexLock lock{&mutex_};
  if (payload.window_size_increment() == 0) {
    return GoAwayLocked(ConnectionError::kProtocolError);
  }
  // TODO
}

}  // namespace http
}  // namespace tsdb2
