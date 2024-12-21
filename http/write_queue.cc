#include "http/write_queue.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "http/hpack.h"
#include "http/http.h"
#include "io/buffer.h"
#include "io/cord.h"
#include "net/base_sockets.h"

namespace tsdb2 {
namespace http {

namespace {

using ::tsdb2::io::Buffer;
using ::tsdb2::io::Cord;

}  // namespace

void WriteQueue::AppendFrame(Buffer buffer, WriteCallback callback) {
  {
    absl::MutexLock lock{&mutex_};
    if (writing_) {
      frame_queue_.emplace_back(std::move(buffer), std::move(callback));
      return;
    }
    writing_ = true;
  }
  Write(std::move(buffer), std::move(callback));
}

void WriteQueue::AppendFrames(std::vector<Buffer> buffers) {
  auto it = buffers.begin();
  if (it == buffers.end()) {
    return;
  }
  auto first = std::move(*it++);
  {
    absl::MutexLock lock{&mutex_};
    if (writing_) {
      frame_queue_.emplace_back(std::move(first), /*callback=*/nullptr);
    }
    while (it != buffers.end()) {
      frame_queue_.emplace_back(std::move(*it++), /*callback=*/nullptr);
    }
    if (writing_) {
      return;
    }
    writing_ = true;
  }
  Write(std::move(first), /*callback=*/nullptr);
}

void WriteQueue::AppendFrameSkippingQueue(Buffer buffer, WriteCallback callback) {
  {
    absl::MutexLock lock{&mutex_};
    if (writing_) {
      frame_queue_.emplace_front(std::move(buffer), std::move(callback));
      return;
    }
    writing_ = true;
  }
  Write(std::move(buffer), std::move(callback));
}

void WriteQueue::AppendFieldsFrames(uint32_t const stream_id, hpack::HeaderSet const& fields,
                                    bool const end_of_stream) {
  absl::ReleasableMutexLock lock{&mutex_};
  auto buffers = MakeHeadersFrames(stream_id, end_of_stream, fields);
  auto it = buffers.begin();
  if (it == buffers.end()) {
    return;
  }
  auto first = std::move(*it++);
  if (writing_) {
    frame_queue_.emplace_back(std::move(first), /*callback=*/nullptr);
  }
  while (it != buffers.end()) {
    frame_queue_.emplace_back(std::move(*it++), /*callback=*/nullptr);
  }
  if (writing_) {
    return;
  }
  writing_ = true;
  lock.Release();
  Write(std::move(first), /*callback=*/nullptr);
}

void WriteQueue::AppendDataFrames(uint32_t const stream_id, Buffer const& data,
                                  bool const end_of_stream) {
  for (size_t offset = 0; offset < data.size(); offset += frame_size_) {
    auto const header =
        FrameHeader()
            .set_length(data.size())
            .set_frame_type(FrameType::kData)
            .set_flags(end_of_stream && (offset + frame_size_ >= data.size()) ? kFlagEndStream : 0)
            .set_stream_id(stream_id);
    AppendFrame(Cord(Buffer(&header, sizeof(FrameHeader)),
                     Buffer(data.span(offset, std::min(frame_size_, data.size() - offset))))
                    .Flatten());
  }
}

void WriteQueue::GoAway(ErrorCode const error_code, uint32_t const last_processed_stream_id,
                        bool const reset_queue, WriteCallback callback) {
  auto frame = MakeGoAwayFrame(error_code, last_processed_stream_id);
  {
    absl::MutexLock lock{&mutex_};
    if (reset_queue) {
      frame_queue_.clear();
    }
    if (writing_) {
      frame_queue_.emplace_front(std::move(frame), std::move(callback));
      return;
    }
    writing_ = true;
  }
  Write(std::move(frame), std::move(callback));
}

std::vector<Buffer> WriteQueue::MakeHeadersFrames(uint32_t const stream_id,
                                                  bool const end_of_stream,
                                                  hpack::HeaderSet const& fields) {
  auto buffer = field_encoder_.Encode(fields);
  std::vector<Buffer> result;
  result.reserve(buffer.size() / (frame_size_ + 1));
  uint8_t const flags = end_of_stream ? kFlagEndStream : 0;
  if (buffer.size() > frame_size_) {
    auto const header = FrameHeader()
                            .set_length(buffer.size())
                            .set_frame_type(FrameType::kHeaders)
                            .set_flags(flags)
                            .set_stream_id(stream_id);
    result.emplace_back(
        Cord(Buffer(&header, sizeof(header)), Buffer(buffer.span(0, frame_size_))).Flatten());
  } else {
    auto const header = FrameHeader()
                            .set_length(buffer.size())
                            .set_frame_type(FrameType::kHeaders)
                            .set_flags(flags | kFlagEndHeaders)
                            .set_stream_id(stream_id);
    result.emplace_back(Cord(Buffer(&header, sizeof(header)), std::move(buffer)).Flatten());
    return result;
  }
  for (size_t offset = frame_size_; offset < buffer.size(); offset += frame_size_) {
    auto const header = FrameHeader()
                            .set_length(buffer.size())
                            .set_frame_type(FrameType::kContinuation)
                            .set_flags(offset + frame_size_ < buffer.size() ? 0 : kFlagEndHeaders)
                            .set_stream_id(stream_id);
    result.emplace_back(
        Cord(Buffer(&header, sizeof(header)), Buffer(buffer.span(offset, frame_size_))).Flatten());
  }
  return result;
}

Buffer WriteQueue::MakeResetStreamFrame(uint32_t const stream_id, ErrorCode const error_code) {
  auto const header = FrameHeader()
                          .set_length(sizeof(ResetStreamPayload))
                          .set_frame_type(FrameType::kResetStream)
                          .set_flags(0)
                          .set_stream_id(stream_id);
  auto const payload = ResetStreamPayload().set_error_code(error_code);
  Buffer buffer{sizeof(FrameHeader) + sizeof(ResetStreamPayload)};
  buffer.MemCpy(&header, sizeof(FrameHeader));
  buffer.MemCpy(&payload, sizeof(ResetStreamPayload));
  return buffer;
}

Buffer WriteQueue::MakeSettingsAckFrame() {
  auto const header = FrameHeader()
                          .set_length(0)
                          .set_frame_type(FrameType::kSettings)
                          .set_flags(kFlagAck)
                          .set_stream_id(0);
  return Buffer{&header, sizeof(FrameHeader)};
}

Buffer WriteQueue::MakePingAckFrame(Buffer const& payload) {
  auto const header = FrameHeader()
                          .set_length(kPingPayloadSize)
                          .set_frame_type(FrameType::kPing)
                          .set_flags(kFlagAck)
                          .set_stream_id(0);
  Buffer buffer{sizeof(FrameHeader) + kPingPayloadSize};
  buffer.MemCpy(&header, sizeof(header));
  buffer.MemCpy(payload.get(), kPingPayloadSize);
  return buffer;
}

Buffer WriteQueue::MakeGoAwayFrame(ErrorCode const error_code,
                                   uint32_t const last_processed_stream_id) {
  auto const header = FrameHeader()
                          .set_length(sizeof(GoAwayPayload))
                          .set_frame_type(FrameType::kGoAway)
                          .set_flags(0)
                          .set_stream_id(0);
  auto const payload =
      GoAwayPayload().set_last_stream_id(last_processed_stream_id).set_error_code(error_code);
  Buffer buffer{sizeof(FrameHeader) + sizeof(GoAwayPayload)};
  buffer.MemCpy(&header, sizeof(header));
  buffer.MemCpy(&payload, sizeof(payload));
  return buffer;
}

void WriteQueue::Write(Buffer buffer, WriteCallback callback) {
  auto const status = socket_->WriteWithTimeout(
      std::move(buffer),
      [this, callback = std::move(callback)](absl::Status const status)
          ABSL_LOCKS_EXCLUDED(mutex_) mutable {
            if (!status.ok()) {
              socket_->Close();
              return;
            }
            if (callback) {
              callback();
              callback = nullptr;
            }
            Buffer next;
            {
              absl::MutexLock lock{&mutex_};
              if (frame_queue_.empty()) {
                writing_ = false;
                return;
              }
              std::tie(next, callback) = std::move(frame_queue_.front());
              frame_queue_.pop_front();
            }
            Write(std::move(next), std::move(callback));
          },
      absl::GetFlag(FLAGS_http2_io_timeout));
  if (!status.ok()) {
    socket_->Close();
  }
}

}  // namespace http
}  // namespace tsdb2
