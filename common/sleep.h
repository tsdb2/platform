#ifndef __TSDB2_COMMON_SLEEP_H__
#define __TSDB2_COMMON_SLEEP_H__

#include "absl/time/time.h"
#include "common/promise.h"

namespace tsdb2 {
namespace common {

// Returns a promise that is fulfilled after the specified delay.
//
// This is better than using `std::this_thread::sleep_for` because the latter blocks the current
// thread making it unable to carry out any work for the whole `duration` time, while this function
// is based on asynchronous programming and doesn't block a thread.
//
// The implementation works by scheduling a one-off task in the `default_scheduler`. The callback
// resolves the promise.
Promise<void> Sleep(absl::Duration duration);

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_SLEEP_H__
