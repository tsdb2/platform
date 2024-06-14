#include "common/multi_mutex_lock.h"

#include <thread>

#include "absl/synchronization/mutex.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::common::MultiMutexLock;

TEST(MultiMutexLockTest, Lock) {
  absl::Mutex mutex1;
  absl::Mutex mutex2;
  absl::Mutex mutex3;
  MultiMutexLock lock{&mutex2, &mutex3, &mutex1};
  // TODO
}

TEST(MultiMutexLockTest, ConditionalLock) {
  absl::Mutex mutex1;
  absl::Mutex mutex2;
  absl::Mutex mutex3;
  MultiMutexLock lock{absl::Condition::kTrue, &mutex2, &mutex3, &mutex1};
  // TODO
}

}  // namespace
