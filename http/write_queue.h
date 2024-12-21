#ifndef __TSDB2_HTTP_WRITE_QUEUE_H__
#define __TSDB2_HTTP_WRITE_QUEUE_H__

#include <cstddef>
#include <cstdint>
#include <list>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "http/hpack.h"
#include "http/http.h"
#include "net/base_sockets.h"

namespace tsdb2 {
namespace http {

class WriteQueue final {
 public:
  using WriteCallback = absl::AnyInvocable<void()>;

  explicit WriteQueue(tsdb2::net::BaseSocket* const socket, size_t const frame_size)
      : frame_size_(frame_size), socket_(socket) {}

  ~WriteQueue() { socket_->Close(); }

  void AppendFrame(tsdb2::net::Buffer buffer, WriteCallback callback) ABSL_LOCKS_EXCLUDED(mutex_);

  void AppendFrame(tsdb2::net::Buffer buffer) {
    AppendFrame(std::move(buffer), /*callback=*/nullptr);
  }

  void AppendFrames(std::vector<tsdb2::net::Buffer> buffers) ABSL_LOCKS_EXCLUDED(mutex_);

  void AppendFrameSkippingQueue(tsdb2::net::Buffer buffer, WriteCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void AppendFrameSkippingQueue(tsdb2::net::Buffer buffer) {
    AppendFrameSkippingQueue(std::move(buffer), /*callback=*/nullptr);
  }

  // Serializes the provided `HeaderSet` into a HEADERS frame and zero or more CONTINUATION frames,
  // and appends the generated frames to the queue.
  void AppendFieldsFrames(uint32_t stream_id, hpack::HeaderSet const& fields, bool end_of_stream)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Serializes one or more DATA frames and appends them to the queue.
  //
  // TODO: we should probably take the `data` as a `Cord` rather than a `Buffer` so that the caller
  // doesn't have to flatten it and we can improve the framing process.
  void AppendDataFrames(uint32_t stream_id, tsdb2::net::Buffer const& data, bool end_of_stream);

  void AppendResetStreamFrame(uint32_t const stream_id, ErrorCode const error_code) {
    AppendFrame(MakeResetStreamFrame(stream_id, error_code));
  }

  void AppendSettingsAckFrame() { AppendFrame(MakeSettingsAckFrame()); }

  void AppendPingAckFrame(tsdb2::net::Buffer const& payload) {
    AppendFrame(MakePingAckFrame(payload));
  }

  // Serializes and enqueues a GOAWAY frame.
  //
  // If the `reset_queue` flag is true this method will also clear the queue. The `reset_queue` flag
  // can be used when the connection can no longer progress in any way, e.g. a frame size error.
  void GoAway(ErrorCode error_code, uint32_t last_processed_stream_id, bool reset_queue,
              WriteCallback callback) ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  WriteQueue(WriteQueue const&) = delete;
  WriteQueue& operator=(WriteQueue const&) = delete;
  WriteQueue(WriteQueue&&) = delete;
  WriteQueue& operator=(WriteQueue&&) = delete;

  std::vector<tsdb2::net::Buffer> MakeHeadersFrames(uint32_t stream_id, bool end_of_stream,
                                                    hpack::HeaderSet const& fields)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  static tsdb2::net::Buffer MakeResetStreamFrame(uint32_t stream_id, ErrorCode error_code);

  static tsdb2::net::Buffer MakeSettingsAckFrame();

  static tsdb2::net::Buffer MakePingAckFrame(tsdb2::net::Buffer const& payload);

  static tsdb2::net::Buffer MakeGoAwayFrame(ErrorCode error_code,
                                            uint32_t last_processed_stream_id);

  void Write(tsdb2::net::Buffer buffer, WriteCallback callback) ABSL_LOCKS_EXCLUDED(mutex_);

  size_t const frame_size_;

  tsdb2::net::BaseSocket* const socket_;

  absl::Mutex mutable mutex_;
  bool writing_ ABSL_GUARDED_BY(mutex_) = false;
  std::list<std::pair<tsdb2::net::Buffer, WriteCallback>> frame_queue_ ABSL_GUARDED_BY(mutex_);

  // NOTE: the HPACK encoder MUST be guarded by the same mutex used to synchronize outbound packets
  // because the status of the encoder (i.e. the dynamic table) is mirrored by the peer endpoint and
  // must be in sync with the HEADERS+CONTINUATION frames that are actually sent. We cannot for
  // example encode header set H1, then H2, and then fire H2 before H1 because of our local
  // concurrency. The order of the queue must be the same order of the encoding because the encoder
  // is stateful. For this reason the write queue is responsible for managing the HPACK encoder,
  // serializing outbound HEADERS+CONTINUATION frames, etc.
  hpack::Encoder field_encoder_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_WRITE_QUEUE_H__
