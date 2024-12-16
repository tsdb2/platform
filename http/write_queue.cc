#include "http/write_queue.h"

#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "http/http.h"
#include "net/base_sockets.h"

namespace tsdb2 {
namespace http {

namespace {

using ::tsdb2::net::Buffer;

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
