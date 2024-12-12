#include "common/multi_mutex_lock.h"

#include <thread>

#include "absl/base/attributes.h"
#include "absl/base/const_init.h"
#include "absl/synchronization/mutex.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::common::MultiMutexLock;

ABSL_CONST_INIT absl::Mutex mutex1{absl::kConstInit};
ABSL_CONST_INIT absl::Mutex mutex2{absl::kConstInit};
ABSL_CONST_INIT absl::Mutex mutex3{absl::kConstInit};

void AssertAllHeld() {
  mutex1.AssertHeld();
  mutex2.AssertHeld();
  mutex3.AssertHeld();
}

TEST(MultiMutexLockTest, NoMutexes) { MultiMutexLock lock; }

TEST(MultiMutexLockTest, OneMutex) {
  {
    MultiMutexLock lock{&mutex1};
    mutex1.AssertHeld();
  }
  mutex1.AssertNotHeld();
}

TEST(MultiMutexLockTest, TwoMutexes) {
  {
    MultiMutexLock lock{&mutex1, &mutex2};
    mutex1.AssertHeld();
    mutex2.AssertHeld();
  }
  mutex1.AssertNotHeld();
  mutex2.AssertNotHeld();
}

TEST(MultiMutexLockTest, ConcurrentLocks) {
  std::thread t1{[] {
    MultiMutexLock lock{&mutex2, &mutex3, &mutex1};
    AssertAllHeld();
  }};
  std::thread t2{[] {
    MultiMutexLock lock{&mutex1, &mutex3, &mutex2};
    AssertAllHeld();
  }};
  t1.join();
  t2.join();
}

TEST(MultiMutexLockTest, AllLocks) {
  std::thread t1{[] {
    MultiMutexLock lock{&mutex1, &mutex2, &mutex3};
    AssertAllHeld();
  }};
  std::thread t2{[] {
    MultiMutexLock lock{&mutex1, &mutex3, &mutex2};
    AssertAllHeld();
  }};
  std::thread t3{[] {
    MultiMutexLock lock{&mutex2, &mutex1, &mutex3};
    AssertAllHeld();
  }};
  std::thread t4{[] {
    MultiMutexLock lock{&mutex2, &mutex3, &mutex1};
    AssertAllHeld();
  }};
  std::thread t5{[] {
    MultiMutexLock lock{&mutex3, &mutex1, &mutex2};
    AssertAllHeld();
  }};
  std::thread t6{[] {
    MultiMutexLock lock{&mutex3, &mutex2, &mutex1};
    AssertAllHeld();
  }};
  t1.join();
  t2.join();
  t3.join();
  t4.join();
  t5.join();
  t6.join();
}

TEST(MultiMutexLockTest, ConditionalLock) {
  MultiMutexLock lock{absl::Condition::kTrue, &mutex2, &mutex3, &mutex1};
  AssertAllHeld();
}

}  // namespace
