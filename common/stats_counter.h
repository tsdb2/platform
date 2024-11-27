#ifndef __TSDB2_COMMON_STATS_COUNTER_H__
#define __TSDB2_COMMON_STATS_COUNTER_H__

#include <atomic>
#include <cstdint>

namespace tsdb2 {
namespace common {

// Provides a thread-safe, lock-free, unsigned integer counter.
//
// All provided operations use relaxed memory order, so this class guarantees atomicity and thread
// safety but is not suitable for synchronizing operations across different threads. For example,
// the following code provides no guarantees about the value printed by thread #1, which may be true
// or false and may even differ across runs of the same compiled binary:
//
//   StatsCounter counter;
//   bool flag = false;
//
//   std::thread thread1{[&] {
//     while (counter.value() < 42) {}
//     std::cout << flag << std::endl;
//   }};
//
//   std::thread thread2{[&] {
//     counter.IncrementBy(10);
//     counter.IncrementBy(10);
//     flag = true;
//     counter.IncrementBy(10);
//     counter.IncrementBy(10);
//     counter.IncrementBy(10);
//   }};
//
// The undefined behavior stems from the fact that both the compiler and the processor can and will
// freely reorder the flag set instruction across relaxed memory barriers.
class StatsCounter {
 public:
  explicit StatsCounter() = default;

  explicit StatsCounter(uintptr_t const initial_value) : value_(initial_value) {}

  // Reads the current value.
  //
  // The returned value is purely advisory: by the time this function returns, any number of changes
  // might have occurred in parallel.
  uintptr_t value() const { return value_.load(std::memory_order_relaxed); }

  // Atomically increments the value by 1 and returns the value before the increment.
  uintptr_t Increment() { return value_.fetch_add(1, std::memory_order_relaxed); }

  // Alias for `Increment`.
  uintptr_t operator++() { return Increment() + 1; }

  // Alias for `Increment`.
  uintptr_t operator++(int) { return Increment(); }

  // Atomically increments the value by `delta` and returns the value before the increment.
  uintptr_t IncrementBy(uintptr_t const delta) {
    return value_.fetch_add(delta, std::memory_order_relaxed);
  }

  // Alias for `IncrementBy`.
  StatsCounter& operator+=(uintptr_t const delta) {
    IncrementBy(delta);
    return *this;
  }

  // Atomically resets the counter to 0.
  void Reset() { value_.store(0, std::memory_order_relaxed); }

  // Atomically resets the counter to 0 and returns the last value before the reset.
  uintptr_t FetchAndReset() { return value_.exchange(0, std::memory_order_relaxed); }

 private:
  std::atomic<uintptr_t> value_{0};
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_STATS_COUNTER_H__
