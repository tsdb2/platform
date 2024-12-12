#ifndef __TSDB2_HTTP_PROCESSOR_H__
#define __TSDB2_HTTP_PROCESSOR_H__

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/btree_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "http/hpack.h"
#include "http/http.h"
#include "http/write_queue.h"
#include "net/base_sockets.h"

namespace tsdb2 {
namespace http {

namespace internal {
class ChannelInterface;
}  // namespace internal

class ChannelProcessor {
 public:
  explicit ChannelProcessor(internal::ChannelInterface* parent);

  ConnectionError ValidateContinuationHeader(uint32_t stream_id, FrameHeader const& header)
      ABSL_LOCKS_EXCLUDED(mutex_);

  ConnectionError ValidateFrameHeader(FrameHeader const& header) ABSL_LOCKS_EXCLUDED(mutex_);

  void ProcessFrame(FrameHeader const& header, tsdb2::net::Buffer payload);

  void ProcessContinuationFrame(uint32_t stream_id, FrameHeader const& header,
                                tsdb2::net::Buffer payload) ABSL_LOCKS_EXCLUDED(mutex_);

  void SendSettings() ABSL_LOCKS_EXCLUDED(mutex_);
  void GoAway(ConnectionError error) ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  // Holds per-stream state.
  struct Stream {
    explicit Stream(size_t const window_size) : window_size(window_size) {}
    ~Stream() = default;

    Stream(Stream const&) = delete;
    Stream& operator=(Stream const&) = delete;
    Stream(Stream&&) noexcept = default;
    Stream& operator=(Stream&&) noexcept = default;

    // Stream state.
    StreamState state = StreamState::kIdle;

    // Window size (for flow control).
    size_t window_size;

    // Indicates whether the stream is receiving fields, aka headers or trailers. If true it means
    // we're expecting another CONTINUATION frame for this stream.
    bool receiving_fields = false;

    // Holds the field block (i.e. the concatenation of all field fragments) during field reception.
    std::vector<uint8_t> field_block;

    // If true the stream will transition into the closed state after receiving the last
    // CONTINUATION frame for the current field set.
    bool last_field_block = false;
  };

  void OnData(uint32_t stream_id, absl::Span<uint8_t const> data);
  void OnFields(uint32_t stream_id, hpack::HeaderSet fields);

  static tsdb2::net::Buffer MakeResetStreamFrame(uint32_t id, ConnectionError error);
  tsdb2::net::Buffer MakeSettingsFrame() const ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  static tsdb2::net::Buffer MakeSettingsAckFrame();
  static tsdb2::net::Buffer MakePingFrame(bool ack, tsdb2::net::Buffer const& payload);
  tsdb2::net::Buffer MakeGoAwayFrame(ConnectionError error) const
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  void ResetStreamLocked(uint32_t stream_id, ConnectionError error)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void AckSettings();

  void GoAwayLocked(ConnectionError error) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Stream& GetOrCreateStreamLocked(uint32_t id) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  static ConnectionError ValidateDataHeader(FrameHeader const& header);
  static ConnectionError ValidateHeadersHeader(FrameHeader const& header);
  static ConnectionError ValidatePriorityHeader(FrameHeader const& header);
  static ConnectionError ValidateResetStreamHeader(FrameHeader const& header);
  static ConnectionError ValidateSettingsHeader(FrameHeader const& header);
  static ConnectionError ValidatePushPromiseHeader(FrameHeader const& header);
  static ConnectionError ValidatePingHeader(FrameHeader const& header);
  static ConnectionError ValidateGoAwayHeader(FrameHeader const& header);
  static ConnectionError ValidateWindowUpdateHeader(FrameHeader const& header);

  ConnectionError ValidateContinuationHeaderLocked(uint32_t stream_id,
                                                   FrameHeader const& header) const
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  ConnectionError ValidateFrameHeaderLocked(FrameHeader const& header) const
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  void ProcessDataFrame(FrameHeader const& header, tsdb2::net::Buffer payload)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void ProcessHeadersFrame(FrameHeader const& header, tsdb2::net::Buffer payload)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void ProcessResetStreamFrame(FrameHeader const& header) ABSL_LOCKS_EXCLUDED(mutex_);
  void ProcessSettingsFrame(FrameHeader const& header, tsdb2::net::Buffer payload)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void ProcessPushPromiseFrame(FrameHeader const& header) ABSL_LOCKS_EXCLUDED(mutex_);
  void ProcessPingFrame(FrameHeader const& header, tsdb2::net::Buffer payload);
  void ProcessGoAwayFrame(FrameHeader const& header, tsdb2::net::Buffer payload)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void ProcessWindowUpdateFrame(FrameHeader const& header, tsdb2::net::Buffer buffer)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // TODO

  internal::ChannelInterface* const parent_;

  absl::Mutex mutable mutex_;

  bool enable_push_ ABSL_GUARDED_BY(mutex_) = true;
  std::optional<size_t> max_concurrent_streams_ ABSL_GUARDED_BY(mutex_) = std::nullopt;
  size_t initial_stream_window_size_ ABSL_GUARDED_BY(mutex_) = kDefaultInitialWindowSize;
  size_t max_frame_payload_size_ ABSL_GUARDED_BY(mutex_) = kDefaultMaxFramePayloadSize;
  size_t max_header_list_size_ ABSL_GUARDED_BY(mutex_) = kDefaultMaxHeaderListSize;

  hpack::Decoder field_decoder_ ABSL_GUARDED_BY(mutex_);

  absl::btree_map<uint32_t, Stream> streams_ ABSL_GUARDED_BY(mutex_);
  uint32_t last_processed_stream_id_ ABSL_GUARDED_BY(mutex_) = 0;
  bool going_away_ ABSL_GUARDED_BY(mutex_) = false;

  WriteQueue write_queue_;
};

}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_PROCESSOR_H__
