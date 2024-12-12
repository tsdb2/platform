#ifndef __TSDB2_COMMON_PERIODIC_THREAD_H__
#define __TSDB2_COMMON_PERIODIC_THREAD_H__

#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "common/clock.h"
#include "common/scheduler.h"

namespace tsdb2 {
namespace common {

// A background thread that runs periodically at a specified rate.
//
// To implement a periodic thread, inherit this class and override the `Run` method, which is
// automatically invoked periodically. The run period is specified at construction in the `Options`.
//
// Since `PeriodicThread` uses a dedicated system thread, doing blocking work won't impact other
// threads in the process.
//
// NOTE: if the execution of a single run is slower than a period, `PeriodicThread` will schedule
// the next run at the next period boundary rather than trying to execute the missed runs.
//
// `PeriodicThread` uses a `Scheduler` instance with a single worker under the hood. The scheduler
// starts out as `IDLE` and `PeriodicThread` exposes its state machine via the `state`, `Start`, and
// `Stop` methods (see the corresponding methods in the `Scheduler` class for more information). To
// start executing the periodic code you ned to call `Start` manually.
class PeriodicThread {
 public:
  // Options used at construction.
  struct Options {
    // The run period. Must be > 0.
    absl::Duration period;

    // The clock used to schedule runs, which can be overridden in tests.
    Clock const* clock = nullptr;
  };

  using State = Scheduler::State;

  explicit PeriodicThread(Options const& options)
      : scheduler_({
            .num_workers = 1,
            .clock = options.clock,
            .start_now = false,
        }) {
    CHECK_GT(options.period, absl::ZeroDuration())
        << "the period of a PeriodicThread must be strictly greater than zero!";
    scheduler_.ScheduleRecurringIn(absl::bind_front(&PeriodicThread::Run, this),
                                   absl::ZeroDuration(), options.period);
  }

  virtual ~PeriodicThread() = default;

  // Returns the state of the background thread / underlying `Scheduler`.
  State state() const { return scheduler_.state(); }

  // Starts the background thread. It works by calling `Start` on the underlying `Scheduler`.
  void Start() { scheduler_.Start(); }

  // Stops and joins the background thread. It works by calling `Stop` on the underlying
  // `Scheduler`. You don't need to call this explicitly, the destructor will automatically call it
  // for you if you haven't.
  void Stop() { scheduler_.Stop(); }

  // Waits until the background thread is asleep. It works by calling `WaitUntilAllWorkersAsleep` on
  // the underlying `Scheduler`. Much like `Scheduler::WaitUntilAllWorkersAsleep`, this method only
  // makes sense in tests with a `MockClock`, otherwise there's no guarantee that the thread hasn't
  // woken up again by the time this method returns.
  absl::Status WaitUntilAsleep() const { return scheduler_.WaitUntilAllWorkersAsleep(); }

 protected:
  // Called in the context of the background thread once every run period. Implementors run their
  // periodic code here.
  virtual void Run() = 0;

 private:
  PeriodicThread(PeriodicThread const&) = delete;
  PeriodicThread& operator=(PeriodicThread const&) = delete;
  PeriodicThread(PeriodicThread&&) = delete;
  PeriodicThread& operator=(PeriodicThread&&) = delete;

  Scheduler scheduler_;
};

// A `PeriodicThread` implementation that takes the runnable code as an `AnyInvocable` closure
// rather than requiring the user to override the virtual `Run` method.
class PeriodicClosure : public PeriodicThread {
 public:
  using Options = PeriodicThread::Options;
  using State = PeriodicThread::State;

  explicit PeriodicClosure(Options const& options, absl::AnyInvocable<void()> closure)
      : PeriodicThread(options), closure_(std::move(closure)) {}

 protected:
  void Run() override { closure_(); }

 private:
  absl::AnyInvocable<void()> closure_;
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_PERIODIC_THREAD_H__
