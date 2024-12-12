#include "common/mutex_lock.h"

#include <thread>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::common::ReaderMutexLock;
using ::tsdb2::common::WriterMutexLock;

TEST(WriterMutexLockTest, ScopedLock) {
  absl::Mutex mutex;
  {
    WriterMutexLock lock{&mutex};
    mutex.AssertHeld();
  }
  mutex.AssertNotHeld();
}

TEST(WriterMutexLockTest, ScopedLockWithTrueCondition) {
  absl::Mutex mutex;
  {
    WriterMutexLock lock{&mutex, absl::Condition::kTrue};
    mutex.AssertHeld();
  }
  mutex.AssertNotHeld();
}

TEST(WriterMutexLockTest, ConditionalScopedLock) {
  absl::Mutex mutex;
  bool flag = false;
  std::thread thread{[&] {
    absl::MutexLock lock{&mutex};
    flag = true;
  }};
  WriterMutexLock(  // NOLINT(bugprone-unused-raii)
      &mutex, absl::Condition(&flag));
  EXPECT_TRUE(flag);
  thread.join();
}

TEST(WriterMutexLockTest, ManualUnlock) {
  absl::Mutex mutex;
  {
    WriterMutexLock lock{&mutex};
    mutex.AssertHeld();
    lock.Unlock();
    mutex.AssertNotHeld();
  }
  mutex.AssertNotHeld();
}

TEST(WriterMutexLockTest, MoveConstruct) {
  absl::Mutex mutex;
  WriterMutexLock lock1{&mutex};
  {
    WriterMutexLock lock2{std::move(lock1)};
    mutex.AssertHeld();
  }
  mutex.AssertNotHeld();
}

TEST(WriterMutexLockTest, MoveAssign) {
  absl::Mutex mutex;
  {
    WriterMutexLock lock1;
    {
      WriterMutexLock lock2{&mutex};
      lock1 = std::move(lock2);
    }
    mutex.AssertHeld();
  }
  mutex.AssertNotHeld();
}

TEST(ReaderMutexLockTest, ScopedLock) {
  absl::Mutex mutex;
  {
    ReaderMutexLock lock{&mutex};
    mutex.AssertReaderHeld();
  }
  mutex.AssertNotHeld();
}

TEST(ReaderMutexLockTest, ScopedLockWithTrueCondition) {
  absl::Mutex mutex;
  {
    ReaderMutexLock lock{&mutex, absl::Condition::kTrue};
    mutex.AssertReaderHeld();
  }
  mutex.AssertNotHeld();
}

TEST(ReaderMutexLockTest, ConditionalScopedLock) {
  absl::Mutex mutex;
  bool flag = false;
  std::thread thread{[&] {
    absl::MutexLock lock{&mutex};
    flag = true;
  }};
  ReaderMutexLock(  // NOLINT(bugprone-unused-raii)
      &mutex, absl::Condition(&flag));
  EXPECT_TRUE(flag);
  thread.join();
}

TEST(ReaderMutexLockTest, ManualUnlock) {
  absl::Mutex mutex;
  {
    ReaderMutexLock lock{&mutex};
    mutex.AssertReaderHeld();
    lock.Unlock();
    mutex.AssertNotHeld();
  }
  mutex.AssertNotHeld();
}

TEST(ReaderMutexLockTest, MoveConstruct) {
  absl::Mutex mutex;
  ReaderMutexLock lock1{&mutex};
  {
    ReaderMutexLock lock2{std::move(lock1)};
    mutex.AssertReaderHeld();
  }
  mutex.AssertNotHeld();
}

TEST(ReaderMutexLockTest, MoveAssign) {
  absl::Mutex mutex;
  {
    ReaderMutexLock lock1;
    {
      ReaderMutexLock lock2{&mutex};
      lock1 = std::move(lock2);
    }
    mutex.AssertReaderHeld();
  }
  mutex.AssertNotHeld();
}

}  // namespace
