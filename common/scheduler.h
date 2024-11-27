#ifndef __TSDB2_COMMON_SCHEDULER_H__
#define __TSDB2_COMMON_SCHEDULER_H__

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/container/node_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "common/clock.h"
#include "common/sequence_number.h"

namespace tsdb2 {
namespace common {

// Manages the scheduling of generic runnable tasks. Supports both blocking and non-blocking task
// cancellation, as well as recurring (aka periodic) tasks that are automatically rescheduled after
// every run.
//
// Under the hood this class uses a fixed (configurable) number of worker threads that wait on the
// task queue and run each task as soon as it's due.
//
// This class is fully thread-safe.
class Scheduler {
 public:
  struct Options {
    // The number of worker threads. Must be > 0. At most 65535 workers are supported (but you
    // definitely shouldn't use that many; each worker is a system thread).
    uint16_t num_workers = 2;

    // Clock used to schedule actions. `nullptr` means the scheduler uses the `RealClock`.
    Clock const *clock = nullptr;

    // If `true` the constructor calls `Start()` right away, otherwise you need to call it manually.
    // You need to set this to `false` e.g. when instantiating a Scheduler in global scope, so that
    // it doesn't spin up its worker threads right away.
    bool start_now = false;
  };

  // Describe the state of the scheduler.
  enum class State {
    // Constructed but not yet started.
    IDLE = 0,

    // Started. The worker threads are processing the tasks.
    STARTED = 1,

    // Stopping. Waiting for current tasks to finish, no more tasks will be executed.
    STOPPING = 2,

    // Stopped. All workers joined, no more tasks will be executed.
    STOPPED = 3,
  };

  // Type of the callback functions that can be scheduled in the scheduler.
  using Callback = absl::AnyInvocable<void()>;

  // Handles are unique task IDs.
  using Handle = uintptr_t;

  static Handle constexpr kInvalidHandle = 0;

  // Scoped object to manage a scheduled task. Blocking cancellation of the task is performed
  // automatically by the `~ScopedHandle` destructor, and it's a no-op if the task has already run
  // or has already been cancelled.
  //
  // ScopedHandles are movable and they may be in an empty state, in which case they are no-ops.
  //
  // Non-empty ScopedHandle objects cannot be constructed directly by the user. They are returned by
  // `ScheduleScoped*` methods of the parent `Scheduler`.
  class ScopedHandle final {
   public:
    // Constructs an empty ScopedHandle.
    explicit ScopedHandle() : scheduler_(nullptr), handle_(kInvalidHandle) {}

    // If the ScopedHandle is not empty, the destructor automatically performs blocking cancellation
    // of the managed task.
    ~ScopedHandle() {
      if (scheduler_) {
        scheduler_->BlockingCancel(handle_);
      }
    }

    ScopedHandle(ScopedHandle &&other) noexcept
        : scheduler_(other.scheduler_), handle_(other.handle_) {
      other.scheduler_ = nullptr;
      other.handle_ = kInvalidHandle;
    }

    ScopedHandle &operator=(ScopedHandle &&other) noexcept {
      if (scheduler_) {
        scheduler_->BlockingCancel(handle_);
      }
      scheduler_ = other.scheduler_;
      handle_ = other.handle_;
      other.scheduler_ = nullptr;
      other.handle_ = kInvalidHandle;
      return *this;
    }

    void swap(ScopedHandle &other) noexcept {
      std::swap(scheduler_, other.scheduler_);
      std::swap(handle_, other.handle_);
    }

    friend void swap(ScopedHandle &lhs, ScopedHandle &rhs) noexcept { lhs.swap(rhs); }

    bool empty() const { return scheduler_ == nullptr; }
    explicit operator bool() const { return scheduler_ != nullptr; }

    Scheduler *parent() const { return scheduler_; }
    Handle value() const { return handle_; }
    Handle operator*() const { return handle_; }

    // Releases ownership of the wrapped task handle and returns it. The ScopedHandle object becomes
    // empty. If the ScopedObject is already empty it remains empty and `Scheduler::kInvalidHandle`
    // is returned.
    Handle Release() {
      auto const handle = handle_;
      scheduler_ = nullptr;
      handle_ = kInvalidHandle;
      return handle;
    }

    // Triggers non-blocking cancellation of the managed task and empties this ScopedHandle.
    bool Cancel() { return scheduler_ != nullptr && scheduler_->Cancel(Release()); }

    // Triggers blocking cancellation of the managed task and empties this ScopedHandle. Usually you
    // don't need to call this method because the `~ScopedHandle` destructor does it for you.
    //
    // WARNING: calling this method inside the callback of a task scheduled in the `Scheduler` will
    // cause a deadlock.
    bool BlockingCancel() { return scheduler_ != nullptr && scheduler_->BlockingCancel(Release()); }

   private:
    friend class Scheduler;

    explicit ScopedHandle(Scheduler *const scheduler, Handle const handle)
        : scheduler_(scheduler), handle_(handle) {}

    ScopedHandle(ScopedHandle const &) = delete;
    ScopedHandle &operator=(ScopedHandle const &) = delete;

    Scheduler *scheduler_;
    Handle handle_;
  };

  explicit Scheduler(Options options)
      : options_(std::move(options)),
        clock_(options_.clock ? options_.clock : RealClock::GetInstance()) {
    DCHECK_GT(options_.num_workers, 0) << "Scheduler must have at least 1 worker thread";
    if (options_.start_now) {
      Start();
    }
  }

  // The destructor calls `Stop`, implicitly stopping and joining all workers.
  ~Scheduler() { Stop(); }

  Clock const *clock() const { return clock_; }

  // Returns the current state of the scheduler. See the `State` enum for more details.
  State state() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock{&mutex_};
    return state_;
  }

  // Returns the handle of the current task if the current thread is a worker thread of some
  // `Scheduler` object, or `kInvalidHandle` otherwise.
  static Handle current_task_handle();

  // Starts the workers. Has no effect if the scheduler is in any other state than `IDLE`. The
  // scheduler is guaranteed to be in state `STARTED` when `Start` returns. If this method is called
  // concurrently, the workers are initialized and started only once.
  void Start() ABSL_LOCKS_EXCLUDED(mutex_);

  // Stops and joins all workers.
  //
  // The scheduler will be in state `STOPPING` through the execution of this method. When `Stop`
  // returns, the scheduler is guaranteed to be in state `STOPPED`.
  //
  // If `Start` had never been called (i.e. the scheduler is in state `IDLE` when `Stop` is called)
  // this method will still make the scheduler transition to the `STOPPED` state, preventing it from
  // ever being able to run any tasks.
  //
  // If `Stop is called concurrently multiple times, all calls block until the workers are joined.
  void Stop() ABSL_LOCKS_EXCLUDED(mutex_);

  // Schedules a task to be executed ASAP. `callback` is the function to execute. The returned
  // `Key` can be used to cancel the task using `Cancel`.
  Handle ScheduleNow(Callback callback) {
    return ScheduleInternal(std::move(callback), clock_->TimeNow(), std::nullopt);
  }

  // Schedules a task to be executed at the specified time.
  Handle ScheduleAt(Callback callback, absl::Time const due_time) {
    return ScheduleInternal(std::move(callback), due_time, std::nullopt);
  }

  // Schedules a task to be executed at now+`delay`.
  Handle ScheduleIn(Callback callback, absl::Duration const delay) {
    return ScheduleInternal(std::move(callback), clock_->TimeNow() + delay, std::nullopt);
  }

  // Schedules a recurring task to be executed once every `period`, starting ASAP.
  Handle ScheduleRecurring(Callback callback, absl::Duration const period) {
    return ScheduleInternal(std::move(callback), clock_->TimeNow(), period);
  }

  // Schedules a recurring task to be executed once every `period`, starting at `due_time`.
  Handle ScheduleRecurringAt(Callback callback, absl::Time const due_time,
                             absl::Duration const period) {
    return ScheduleInternal(std::move(callback), due_time, period);
  }

  // Schedules a recurring task to be executed once every `period`, starting at now+`delay`.
  Handle ScheduleRecurringIn(Callback callback, absl::Duration const delay,
                             absl::Duration const period) {
    return ScheduleInternal(std::move(callback), clock_->TimeNow() + delay, period);
  }

  // Like `ScheduleNow` but returns a `ScopedHandle`.
  ScopedHandle ScheduleScopedNow(Callback callback) {
    return ScopedHandle(this, ScheduleNow(std::move(callback)));
  }

  // Like `ScheduleAt` but returns a `ScopedHandle`.
  ScopedHandle ScheduleScopedAt(Callback callback, absl::Time const due_time) {
    return ScopedHandle(this, ScheduleAt(std::move(callback), due_time));
  }

  // Like `ScheduleIn` but returns a `ScopedHandle`.
  ScopedHandle ScheduleScopedIn(Callback callback, absl::Duration const delay) {
    return ScopedHandle(this, ScheduleIn(std::move(callback), delay));
  }

  // Like `ScheduleRecurring` but returns a `ScopedHandle`.
  ScopedHandle ScheduleScopedRecurring(Callback callback, absl::Duration const period) {
    return ScopedHandle(this, ScheduleRecurring(std::move(callback), period));
  }

  // Like `ScheduleRecurringAt` but returns a `ScopedHandle`.
  ScopedHandle ScheduleScopedRecurringAt(Callback callback, absl::Time const due_time,
                                         absl::Duration const period) {
    return ScopedHandle(this, ScheduleRecurringAt(std::move(callback), due_time, period));
  }

  // Like `ScheduleRecurringIn` but returns a `ScopedHandle`.
  ScopedHandle ScheduleScopedRecurringIn(Callback callback, absl::Duration const delay,
                                         absl::Duration const period) {
    return ScopedHandle(this, ScheduleRecurringIn(std::move(callback), delay, period));
  }

  // Cancels the task with the specified handle. Does nothing if the handle is invalid for any
  // reason, e.g. if a past task with this handle has already finished running.
  //
  // If the task has already started running but hasn't yet finished when this method is invoked,
  // the task will finish running normally. In any case this method returns immediately.
  //
  // The returned boolean indicates whether actual cancellation happened: it is true iff the task
  // was in the queue and hadn't yet started.
  bool Cancel(Handle const handle) { return CancelInternal(handle, /*blocking=*/false); }

  // Cancels the task with the specified handle. Does nothing if the handle is invalid for any
  // reason, e.g. if a past task with this handle has already finished running.
  //
  // If the task has already started running but hasn't yet finished when this method is invoked,
  // the task will finish running normally and this method will block until then.
  //
  // The returned boolean indicates whether actual cancellation happened: it is true iff the task
  // was in the queue and hadn't started yet.
  bool BlockingCancel(Handle const handle) { return CancelInternal(handle, /*blocking=*/true); }

  // TEST ONLY: wait until all due tasks have been processed and all workers are asleep. This
  // method only makes sense in testing scenarios using a `MockClock`, otherwise if more tasks are
  // scheduled in the queue and close enough in the future there's no guarantee that the workers
  // won't wake up again by the time this method returns. The time of a `MockClock` on the other
  // hand will only advance as decided in the test, so the workers are guaranteed to remain asleep
  // until then.
  absl::Status WaitUntilAllWorkersAsleep() const ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  class TaskRef;

  // Represents a scheduled task. This class is NOT thread-safe. Thread safety must be guaranteed by
  // `Scheduler::mutex_`.
  class Task final {
   public:
    // Custom hash functor to store Task objects in a node_hash_set.
    struct Hash {
      using is_transparent = void;
      size_t operator()(Task const &task) const { return absl::HashOf(task.handle()); }
      size_t operator()(Handle const &handle) const { return absl::HashOf(handle); }
    };

    // Custom equals functor to store Task objects in a node_hash_set.
    struct Equals {
      using is_transparent = void;

      static Handle ToHandle(Task const &task) { return task.handle(); }
      static Handle ToHandle(Handle const &handle) { return handle; }

      template <typename LHS, typename RHS>
      bool operator()(LHS const &lhs, RHS const &rhs) const {
        return ToHandle(lhs) == ToHandle(rhs);
      }
    };

    explicit Task(Callback callback, absl::Time const due_time,
                  std::optional<absl::Duration> const period)
        : callback_(std::move(callback)), due_time_(due_time), period_(period) {}

    Handle handle() const { return handle_; }

    TaskRef const *ref() const { return ref_; }
    void set_ref(TaskRef const *const ref) { ref_ = ref; }

    void CheckRef(TaskRef const *const ref) const {
      DCHECK_EQ(ref_, ref) << "invalid scheduler task backlink!";
    }

    absl::Time due_time() const { return due_time_; }
    void set_due_time(absl::Time const value) { due_time_ = value; }

    bool cancelled() const { return cancelled_; }
    void set_cancelled(bool const value) { cancelled_ = value; }

    bool is_periodic() const { return period_.has_value(); }
    absl::Duration period() const { return period_.value(); }

    void Run() { callback_(); }

   private:
    Task(Task const &) = delete;
    Task &operator=(Task const &) = delete;
    Task(Task &&) = delete;
    Task &operator=(Task &&) = delete;

    // Generates task handles.
    static SequenceNumber handle_generator_;

    // Unique ID for the task.
    Handle const handle_ = handle_generator_.GetNext();

    Callback callback_;
    absl::Time due_time_;
    std::optional<absl::Duration> const period_;

    // Backlink to the `TaskRef` of this Task. The `TaskRef` move constructor and assignment
    // operator take care of keeping this up to date.
    TaskRef const *ref_ = nullptr;

    bool cancelled_ = false;
  };

  // This class acts as a smart pointer to a `Task` object and maintains a backlink to itself inside
  // the referenced Task. The priority queue of the scheduler is a min-heap backed by an array of
  // `TaskRef` objects. The heap swap operations move the `TaskRef`s which in turn update the
  // backlinks of their respective tasks, and thanks to the backlinks a `Task` can determine its
  // current index in the priority queue array. The index is in turn used for cancellation.
  //
  // A TaskRef can be empty, in which case no task backlinks to it. The backlink of a task may also
  // be null, in which case it means the task is not in the priority queue (e.g. it's being
  // processed by a worker).
  class TaskRef final {
   public:
    explicit TaskRef(Task *const task = nullptr) : task_(task) {
      if (task_) {
        task_->CheckRef(nullptr);
        task_->set_ref(this);
      }
    }

    ~TaskRef() {
      if (task_) {
        task_->CheckRef(this);
        task_->set_ref(nullptr);
      }
    }

    TaskRef(TaskRef &&other) noexcept : task_(other.task_) {
      if (task_) {
        task_->CheckRef(&other);
        other.task_ = nullptr;
        task_->set_ref(this);
      }
    }

    TaskRef &operator=(TaskRef &&other) noexcept {
      if (task_) {
        task_->CheckRef(this);
        task_->set_ref(nullptr);
      }
      task_ = other.task_;
      if (task_) {
        task_->CheckRef(&other);
        other.task_ = nullptr;
        task_->set_ref(this);
      }
      return *this;
    }

    void swap(TaskRef &other) noexcept {
      std::swap(task_, other.task_);
      if (task_) {
        task_->set_ref(this);
      }
      if (other.task_) {
        other.task_->set_ref(&other);
      }
    }

    friend void swap(TaskRef &lhs, TaskRef &rhs) noexcept { lhs.swap(rhs); }

    Task *Get() const { return task_; }
    explicit operator bool() const { return task_ != nullptr; }
    Task *operator->() const { return task_; }
    Task &operator*() const { return *task_; }

   private:
    TaskRef(TaskRef const &) = delete;
    TaskRef &operator=(TaskRef const &) = delete;

    Task *task_;
  };

  // This functor is used to index the tasks by due time in the `queue_` min-heap. We use
  // `operator>` to do the comparison because the STL algorithms implement a max-heap if `operator<`
  // is used, whereas we need a min-heap.
  struct CompareTasks {
    bool operator()(TaskRef const &lhs, TaskRef const &rhs) const {
      return lhs->due_time() > rhs->due_time();
    }
  };

  // Worker thread implementation.
  //
  // NOTE: the worker's sleeping flag is guarded by the Scheduler's mutex. Unfortunately we can't
  // note that via thread annotalysis because the compiler can't track that `parent_` points to the
  // parent `Scheduler`.
  class Worker final {
   public:
    // Used by `Scheduler::FetchTask` to manage the sleeping flag of the calling worker.
    class SleepScope final {
     public:
      explicit SleepScope(Worker *const worker) : worker_(worker) { worker_->set_sleeping(true); }
      ~SleepScope() { worker_->set_sleeping(false); }

     private:
      SleepScope(SleepScope const &) = delete;
      SleepScope &operator=(SleepScope const &) = delete;
      SleepScope(SleepScope &&) = delete;
      SleepScope &operator=(SleepScope &&) = delete;

      Worker *const worker_;
    };

    explicit Worker(Scheduler *const parent)
        : parent_(parent), thread_(absl::bind_front(&Worker::Run, this)) {}

    // Indicates whether the worker is waiting for a task.
    bool is_sleeping() const { return sleeping_; }

    // Sets the worker's sleeping flag.
    void set_sleeping(bool const value) { sleeping_ = value; }

    void Join() { thread_.join(); }

   private:
    Worker(Worker const &) = delete;
    Worker &operator=(Worker const &) = delete;
    Worker(Worker &&) = delete;
    Worker &operator=(Worker &&) = delete;

    void Run();

    Scheduler *const parent_;

    bool sleeping_ = false;
    std::thread thread_;
  };

  Scheduler(Scheduler const &) = delete;
  Scheduler &operator=(Scheduler const &) = delete;
  Scheduler(Scheduler &&) = delete;
  Scheduler &operator=(Scheduler &&) = delete;

  bool stopped() const ABSL_SHARED_LOCKS_REQUIRED(mutex_) { return state_ == State::STOPPED; }

  bool queue_not_empty() const ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
    return state_ > State::STARTED || !queue_.empty();
  }

  Handle ScheduleInternal(Callback callback, absl::Time due_time,
                          std::optional<absl::Duration> period) ABSL_LOCKS_EXCLUDED(mutex_);

  bool CancelInternal(Handle handle, bool blocking) ABSL_LOCKS_EXCLUDED(mutex_);

  absl::StatusOr<Task *> FetchTask(Worker *worker, Task *previous) ABSL_LOCKS_EXCLUDED(mutex_);

  Options const options_;
  Clock const *const clock_;

  absl::Condition const stopped_condition_{this, &Scheduler::stopped};
  absl::Condition const queue_not_empty_condition_{this, &Scheduler::queue_not_empty};

  absl::Mutex mutable mutex_;

  // Contains all tasks, indexed by handle.
  absl::node_hash_set<Task, Task::Hash, Task::Equals> tasks_ ABSL_GUARDED_BY(mutex_);

  // Priority queue of tasks indexed by due time. This is a min-heap and is managed with STL
  // algorithms (std::push_heap and std::pop_heap).
  std::vector<TaskRef> queue_ ABSL_GUARDED_BY(mutex_);

  State state_ ABSL_GUARDED_BY(mutex_) = State::IDLE;
  std::vector<std::unique_ptr<Worker>> workers_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_SCHEDULER_H__
