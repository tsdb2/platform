#ifndef __TSDB2_COMMON_SEQUENCE_NUMBER_H__
#define __TSDB2_COMMON_SEQUENCE_NUMBER_H__

#include <atomic>
#include <cstdint>

namespace tsdb2 {
namespace common {

// Thread-safe incremental number generator.
class SequenceNumber {
 public:
  // `first` is the first number of the sequence. It defaults to 1 so that 0 can be used as a
  // sentinel value for invalid handles.
  explicit constexpr SequenceNumber(uintptr_t const first = 1) : next_(first) {}

  // Generates the next number. Thread-safe.
  uintptr_t GetNext() { return next_.fetch_add(1, std::memory_order_relaxed); }

 private:
  std::atomic<uintptr_t> next_;
};

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_SEQUENCE_NUMBER_H__
