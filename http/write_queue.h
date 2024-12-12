#ifndef __TSDB2_HTTP_WRITE_QUEUE_H__
#define __TSDB2_HTTP_WRITE_QUEUE_H__

#include <list>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "net/base_sockets.h"

namespace tsdb2 {
namespace http {

class WriteQueue final {
 public:
  using WriteCallback = absl::AnyInvocable<void()>;

  explicit WriteQueue(tsdb2::net::BaseSocket* const socket) : socket_(socket) {}

  ~WriteQueue() { socket_->Close(); }

  void WriteFrame(tsdb2::net::Buffer buffer, WriteCallback callback) ABSL_LOCKS_EXCLUDED(mutex_);

  void WriteFrame(tsdb2::net::Buffer buffer) {
    WriteFrame(std::move(buffer), /*callback=*/nullptr);
  }

  void WriteFrameSkippingQueue(tsdb2::net::Buffer buffer, WriteCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void WriteFrameSkippingQueue(tsdb2::net::Buffer buffer) {
    WriteFrameSkippingQueue(std::move(buffer), /*callback=*/nullptr);
  }

 private:
  WriteQueue(WriteQueue const&) = delete;
  WriteQueue& operator=(WriteQueue const&) = delete;
  WriteQueue(WriteQueue&&) = delete;
  WriteQueue& operator=(WriteQueue&&) = delete;

  void Write(tsdb2::net::Buffer buffer, WriteCallback callback) ABSL_LOCKS_EXCLUDED(mutex_);

  tsdb2::net::BaseSocket* const socket_;

  absl::Mutex mutable mutex_;
  bool writing_ ABSL_GUARDED_BY(mutex_) = false;
  std::list<std::pair<tsdb2::net::Buffer, WriteCallback>> frame_queue_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_WRITE_QUEUE_H__
