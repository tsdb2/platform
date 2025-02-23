#include "io/sigusr.h"

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>

#include "absl/base/attributes.h"
#include "absl/base/const_init.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "common/reffed_ptr.h"
#include "common/utilities.h"

namespace tsdb2 {
namespace io {

absl::Status SigUsr1::Notify(pid_t const process_id, pid_t const thread_id) {
  if (::tgkill(process_id, thread_id, SIGUSR1) < 0) {
    return absl::ErrnoToStatus(errno, "tgkill");
  } else {
    return absl::OkStatus();
  }
}

absl::Status SigUsr1::Notify(pid_t const process_id) {
  if (::kill(process_id, SIGUSR1) < 0) {
    return absl::ErrnoToStatus(errno, "kill");
  } else {
    return absl::OkStatus();
  }
}

ABSL_CONST_INIT absl::Mutex SigUsr1::SignalHandler::instance_mutex_{absl::kConstInit};
gsl::owner<SigUsr1::SignalHandler*> SigUsr1::SignalHandler::instance_ = nullptr;

sig_atomic_t volatile SigUsr1::SignalHandler::notified_ = 0;

tsdb2::common::reffed_ptr<SigUsr1::SignalHandler> SigUsr1::SignalHandler::GetOrCreate() {
  absl::MutexLock lock{&instance_mutex_};
  if (instance_ == nullptr) {
    instance_ = new SignalHandler();
  }
  return tsdb2::common::WrapReffed(instance_);
}

SigUsr1::SignalHandler::~SignalHandler() {
  {
    absl::MutexLock lock{&instance_mutex_};
    instance_ = nullptr;
  }
  sigset_t mask;
  ::sigemptyset(&mask);
  ::sigaddset(&mask, SIGUSR1);
  int const result = ::pthread_sigmask(SIG_UNBLOCK, &mask, nullptr);
  if (result > 0) {
    // Yes, on failure `pthread_sigmask` returns the (positive) error number, unlike all other
    // functions. *Sigh*.
    LOG(ERROR) << absl::ErrnoToStatus(result, "pthread_sigmask(SIG_UNBLOCK, SIGUSR1)");
  }
  struct sigaction sa {};
  sa.sa_handler = SIG_DFL;
  if (::sigaction(SIGUSR1, &sa, nullptr) < 0) {
    LOG(ERROR) << absl::ErrnoToStatus(errno, "sigaction(SIGUSR1, ...)");
  }
  notified_ = 0;
}

absl::Status SigUsr1::SignalHandler::Notify() const {
  if (::tgkill(process_id_, thread_id_, SIGUSR1) < 0) {
    return absl::ErrnoToStatus(errno, "tgkill");
  } else {
    return absl::OkStatus();
  }
}

absl::Status SigUsr1::SignalHandler::WaitForNotification() {
  if (notified_ != 0) {
    return absl::OkStatus();
  }
  WaitScope ws{this};
  while (notified_ == 0) {
    if (::sigsuspend(&mask_) < 0 && errno != EINTR) {
      return absl::ErrnoToStatus(errno, "sigsuspend");
    }
  }
  return absl::OkStatus();
}

sigset_t SigUsr1::SignalHandler::BlockSigUsr1() {
  sigset_t mask;
  ::sigemptyset(&mask);
  ::sigaddset(&mask, SIGUSR1);
  sigset_t old_mask;
  ::sigemptyset(&old_mask);
  int const result = ::pthread_sigmask(SIG_BLOCK, &mask, &old_mask);
  if (result > 0) {
    // Ditto.
    LOG(ERROR) << absl::ErrnoToStatus(result, "pthread_sigmask(SIG_BLOCK, SIGUSR1)");
  }
  return old_mask;
}

void SigUsr1::SignalHandler::HandlerFn(int /*unused*/) { notified_ = 1; }

SigUsr1::SignalHandler::SignalHandler()
    : process_id_(::getpid()), thread_id_(::gettid()), mask_(BlockSigUsr1()) {
  struct sigaction sa {};
  sa.sa_handler = &SignalHandler::HandlerFn;
  if (::sigaction(SIGUSR1, &sa, nullptr) < 0) {
    LOG(ERROR) << absl::ErrnoToStatus(errno, "sigaction(SIGUSR1, ...)");
  }
}

bool SigUsr1::SignalHandler::is_not_waiting() const { return !waiting_; }

void SigUsr1::SignalHandler::StartWait() {
  absl::MutexLock lock{&mutex_, absl::Condition(this, &SignalHandler::is_not_waiting)};
  waiting_ = true;
}

void SigUsr1::SignalHandler::EndWait() {
  absl::MutexLock lock{&mutex_};
  waiting_ = false;
}

}  // namespace io
}  // namespace tsdb2
