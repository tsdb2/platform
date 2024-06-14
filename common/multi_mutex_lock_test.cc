#include "common/multi_mutex_lock.h"

#include <thread>

#include "absl/base/attributes.h"
#include "absl/synchronization/mutex.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::common::MultiMutexLock;

ABSL_CONST_INIT absl::Mutex mutex1{absl::kConstInit};
ABSL_CONST_INIT absl::Mutex mutex2{absl::kConstInit};
ABSL_CONST_INIT absl::Mutex mutex3{absl::kConstInit};

TEST(MultiMutexLockTest, Lock) { MultiMutexLock lock{&mutex2, &mutex3, &mutex1}; }

TEST(MultiMutexLockTest, ConcurrentLock) {
  std::thread t1{[] { MultiMutexLock lock{&mutex2, &mutex3, &mutex1}; }};
  std::thread t2{[] { MultiMutexLock lock{&mutex1, &mutex3, &mutex2}; }};
  t1.join();
  t2.join();
}

TEST(MultiMutexLockTest, ConditionalLock) {
  MultiMutexLock lock{absl::Condition::kTrue, &mutex2, &mutex3, &mutex1};
}

}  // namespace
