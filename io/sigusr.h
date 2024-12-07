#ifndef __TSDB2_IO_SIGUSR_H__
#define __TSDB2_IO_SIGUSR_H__

#include <signal.h>
#include <sys/types.h>

#include <algorithm>

#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "common/ref_count.h"
#include "common/reffed_ptr.h"

namespace tsdb2 {
namespace io {

// Cross-process notifications based on `SIGUSR` made easy!
//
// ExampleUsage:
//
//   SigUsr1 sigusr1;
//   auto const pid = ::fork();
//   CHECK_GE(pid, 0);
//   if (pid) {  // parent
//     sigusr1.WaitForNotification();
//   } else {  // child
//     sigurs1.Notify();
//   }
//
class SigUsr1 {
 public:
  // Sends `SIGUSR1` to the given process/thread.
  static absl::Status Notify(pid_t process_id, pid_t thread_id);

  // Sends `SIGUSR1` to (an arbitrary thread of) the given process.
  static absl::Status Notify(pid_t process_id);

  // Constructing a `SigUsr1` installs the signal handler if it's not already installed. The signal
  // handler is reference-counted by `SigUsr1` instances, so it will be uninstalled when the last
  // `SigUsr1` is destroyed. Everything is thread-safe.
  explicit SigUsr1() : handler_(SignalHandler::GetOrCreate()) {}

  SigUsr1(SigUsr1&&) noexcept = default;
  SigUsr1& operator=(SigUsr1&&) noexcept = default;

  void swap(SigUsr1& other) {
    using std::swap;  // ensure ADL
    swap(handler_, other.handler_);
  }

  friend void swap(SigUsr1& lhs, SigUsr1& rhs) { lhs.swap(rhs); }

  // Indicates whether `SIGUSR1` has been received.
  //
  // This method is thread-safe.
  bool is_notified() const { return handler_->is_notified(); }

  // Sends `SIGUSR1` to the thread that's handling `SIGUSR1` in the parent process.
  //
  // If you use this method, the `SigUsr1` object must be created before forking.
  //
  // This method is thread-safe.
  absl::Status Notify() { return handler_->Notify(); }

  // Waits until `SIGUSR1` is received.
  //
  // This method is thread-safe.
  absl::Status WaitForNotification() { return handler_->WaitForNotification(); }

 private:
  class SignalHandler final : public tsdb2::common::SimpleRefCounted {
   public:
    static tsdb2::common::reffed_ptr<SignalHandler> GetOrCreate()
        ABSL_LOCKS_EXCLUDED(instance_mutex_);

    ~SignalHandler() ABSL_LOCKS_EXCLUDED(instance_mutex_);

    bool is_notified() const { return notified_; }

    absl::Status Notify();

    absl::Status WaitForNotification() ABSL_LOCKS_EXCLUDED(mutex_);

   private:
    class WaitScope final {
     public:
      explicit WaitScope(SignalHandler* const parent) : parent_(parent) { parent_->StartWait(); }
      ~WaitScope() { parent_->EndWait(); }

     private:
      SignalHandler* const parent_;
    };

    static sigset_t BlockSigUsr1();

    static void HandlerFn(int);

    static absl::Mutex instance_mutex_;
    static SignalHandler* instance_ ABSL_GUARDED_BY(instance_mutex_);

    static sig_atomic_t volatile notified_;

    explicit SignalHandler();

    SignalHandler(SignalHandler&&) = delete;
    SignalHandler& operator=(SignalHandler&&) = delete;
    SignalHandler(SignalHandler const&) = delete;
    SignalHandler& operator=(SignalHandler const&) = delete;

    bool is_not_waiting() const ABSL_SHARED_LOCKS_REQUIRED(mutex_);
    void StartWait() ABSL_LOCKS_EXCLUDED(mutex_);
    void EndWait() ABSL_LOCKS_EXCLUDED(mutex_);

    pid_t const process_id_;
    pid_t const thread_id_;

    sigset_t const mask_;

    absl::Mutex mutable mutex_;
    bool waiting_ ABSL_GUARDED_BY(mutex_) = false;
  };

  SigUsr1(SigUsr1 const&) = delete;
  SigUsr1& operator=(SigUsr1 const&) = delete;

  tsdb2::common::reffed_ptr<SignalHandler> handler_;
};

}  // namespace io
}  // namespace tsdb2

#endif  // __TSDB2_IO_SIGUSR_H__
