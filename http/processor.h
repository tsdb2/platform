#ifndef __TSDB2_HTTP_PROCESSOR_H__
#define __TSDB2_HTTP_PROCESSOR_H__

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "absl/base/thread_annotations.h"
#include "absl/container/btree_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "common/flat_map.h"
#include "http/handlers.h"
#include "http/hpack.h"
#include "http/http.h"
#include "http/write_queue.h"
#include "io/buffer.h"
#include "io/cord.h"

namespace tsdb2 {
namespace http {

namespace internal {
class ChannelInterface;
}  // namespace internal

class ChannelProcessor final {
 public:
  explicit ChannelProcessor(internal::ChannelInterface* parent);
  ~ChannelProcessor() = default;

  Error ValidateFrameHeader(FrameHeader const& header) ABSL_LOCKS_EXCLUDED(mutex_);

  static Error ValidateContinuationHeader(uint32_t stream_id, FrameHeader const& header);

  void ProcessFrame(FrameHeader const& header, tsdb2::io::Buffer payload);

  void SendSettings() ABSL_LOCKS_EXCLUDED(mutex_);

  // Shuts down the connection gracefully, waiting for the peer to process all outstanding frames.
  void GoAway(ErrorCode error_code) ABSL_LOCKS_EXCLUDED(mutex_);

  // Shuts down the connection abruptly, usually as a result of an HTTP/2 connection error
  // (as per https://httpwg.org/specs/rfc9113.html#rfc.section.5.4.1).
  void GoAwayNow(ErrorCode error_code) ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  // Buffers any DATA packets that have been received but not yet processed by the corresponding
  // stream handler.
  //
  // This class is thread-safe so that it can be used concurrently both by a thread that's receiving
  // a new DATA packet and by a thread that's continuing processing for a stream. The former would
  // typically call the `AddChunk` method while the latter would call `Read`.
  class DataBuffer {
   public:
    using DataCallback = StreamInterface::DataCallback;

    explicit DataBuffer() = default;
    ~DataBuffer() = default;

    void AddChunk(tsdb2::io::Buffer buffer, bool last) ABSL_LOCKS_EXCLUDED(mutex_);

    void Read(DataCallback callback) ABSL_LOCKS_EXCLUDED(mutex_);

   private:
    DataBuffer(DataBuffer const&) = delete;
    DataBuffer& operator=(DataBuffer const&) = delete;
    DataBuffer(DataBuffer&&) = delete;
    DataBuffer& operator=(DataBuffer&&) = delete;

    absl::Mutex mutable mutex_;
    tsdb2::io::Cord data_ ABSL_GUARDED_BY(mutex_);
    bool ended_ ABSL_GUARDED_BY(mutex_) = false;
    DataCallback callback_ ABSL_GUARDED_BY(mutex_);
  };

  // Holds per-stream state.
  //
  // Save for the `DataBuffer` field which is thread-safe in its own, this class is NOT thread-safe.
  // Per-stream data is guarded by the parent processor's mutex.
  class Stream final : public StreamInterface {
   public:
    // Allows indexing streams by ID in ordered data structures.
    struct Compare {
      using is_transparent = void;
      static uint32_t ToId(std::unique_ptr<Stream> const& stream) { return stream->id(); }
      static uint32_t ToId(uint32_t const id) { return id; }
      template <typename LHS, typename RHS>
      bool operator()(LHS&& lhs, RHS&& rhs) const {
        return ToId(std::forward<LHS>(lhs)) < ToId(std::forward<RHS>(rhs));
      }
    };

    explicit Stream(ChannelProcessor* const parent, uint32_t const id, size_t const window_size)
        : parent_(parent), id_(id), window_size_(window_size) {}

    ~Stream() override = default;

    uint32_t id() const { return id_; }

    Error ProcessData(tsdb2::io::Buffer buffer, bool end_stream);
    Error ProcessFields(hpack::HeaderSet fields);
    void ProcessReset();
    Error ProcessPushPromise();

    void ReadData(DataCallback callback) override;
    absl::Status SendFields(hpack::HeaderSet const& fields, bool end_stream) override;
    absl::Status SendData(tsdb2::io::Buffer buffer, bool end_stream) override;

   private:
    Stream(Stream const&) = delete;
    Stream& operator=(Stream const&) = delete;
    Stream(Stream&&) = delete;
    Stream& operator=(Stream&&) = delete;

    static tsdb2::common::flat_map<std::string, std::string> FlattenFields(hpack::HeaderSet fields);

    Error ErrorOut(Status http_status);

    std::string GetStreamDescriptionForErrors();

    absl::Status EndStream();

    ChannelProcessor* const parent_;
    uint32_t const id_;

    // Stream state.
    StreamState state_ = StreamState::kIdle;

    // Flow control window size.
    size_t window_size_;

    // Chunks of data received by DATA packets that are not yet processed by the handler are
    // buffered here.
    DataBuffer data_buffer_;
  };

  ChannelProcessor(ChannelProcessor const&) = delete;
  ChannelProcessor& operator=(ChannelProcessor const&) = delete;
  ChannelProcessor(ChannelProcessor&&) = delete;
  ChannelProcessor& operator=(ChannelProcessor&&) = delete;

  void SendFields(uint32_t const stream_id, hpack::HeaderSet const& fields, bool const end_stream) {
    write_queue_.AppendFieldsFrames(stream_id, fields, end_stream);
  }

  void SendData(uint32_t const stream_id, tsdb2::io::Buffer const& data, bool const end_of_stream) {
    write_queue_.AppendDataFrames(stream_id, data, end_of_stream);
  }

  tsdb2::io::Buffer MakeSettingsFrame() const ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  void GoAwayNowLocked(ErrorCode error_code) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  absl::StatusOr<Stream*> GetOrCreateStreamLocked(uint32_t id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  static Error ValidateDataHeader(FrameHeader const& header);
  static Error ValidateHeadersHeader(FrameHeader const& header);
  static Error ValidatePriorityHeader(FrameHeader const& header);
  static Error ValidateResetStreamHeader(FrameHeader const& header);
  static Error ValidateSettingsHeader(FrameHeader const& header);
  static Error ValidatePushPromiseHeader(FrameHeader const& header);
  static Error ValidatePingHeader(FrameHeader const& header);
  static Error ValidateGoAwayHeader(FrameHeader const& header);
  static Error ValidateWindowUpdateHeader(FrameHeader const& header);

  Error ValidateFrameHeaderLocked(FrameHeader const& header) const
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);

  void ProcessDataFrame(FrameHeader const& header, tsdb2::io::Buffer payload)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void ProcessFieldBlock(uint32_t stream_id, tsdb2::io::Buffer field_block)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void ProcessHeadersFrame(FrameHeader const& header, tsdb2::io::Buffer payload);

  void ProcessContinuationFrame(uint32_t stream_id, tsdb2::io::Cord field_block,
                                FrameHeader const& header, tsdb2::io::Buffer payload);

  void ProcessResetStreamFrame(FrameHeader const& header) ABSL_LOCKS_EXCLUDED(mutex_);

  void ProcessSettingsFrame(FrameHeader const& header, tsdb2::io::Buffer const& payload);

  void ProcessPushPromiseFrame(FrameHeader const& header) ABSL_LOCKS_EXCLUDED(mutex_);

  void ProcessPingFrame(FrameHeader const& header, tsdb2::io::Buffer const& payload);

  void ProcessGoAwayFrame(FrameHeader const& header, tsdb2::io::Buffer payload)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void ProcessWindowUpdateFrame(FrameHeader const& header, tsdb2::io::Buffer buffer)
      ABSL_LOCKS_EXCLUDED(mutex_);

  absl::StatusOr<Handler*> GetHandler(std::string_view path) const;

  internal::ChannelInterface* const parent_;

  // Most of our local settings are stored here. The max HPACK dynamic header table size is inside
  // the decoder below.
  bool const enable_push_ = true;
  std::optional<size_t> const max_concurrent_streams_;
  size_t const initial_stream_window_size_;
  size_t const max_frame_payload_size_;
  size_t const max_header_list_size_;

  absl::Mutex mutable mutex_;

  hpack::Decoder field_decoder_ ABSL_GUARDED_BY(mutex_);

  absl::btree_set<std::unique_ptr<Stream>, Stream::Compare> streams_ ABSL_GUARDED_BY(mutex_);
  uint32_t last_processed_stream_id_ ABSL_GUARDED_BY(mutex_) = 0;
  bool going_away_ ABSL_GUARDED_BY(mutex_) = false;

  WriteQueue write_queue_;
};

}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_PROCESSOR_H__
