#include "io/advisory_file_lock.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "common/testing.h"
#include "common/utilities.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "io/fd.h"
#include "io/sigusr.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::TestTempFile;
using ::tsdb2::io::AdvisoryLockAcquireExclusive;
using ::tsdb2::io::AdvisoryLockRelease;
using ::tsdb2::io::ExclusiveFileLock;
using ::tsdb2::io::FD;
using ::tsdb2::io::SigUsr1;

std::string_view constexpr kTestFileName = "advisory_file_lock_test";

absl::StatusOr<FD> OpenFile(std::string_view const path) {
  FD fd{::open(  // NOLINT(cppcoreguidelines-pro-type-vararg)
      std::string(path).c_str(), O_CLOEXEC, O_RDONLY)};
  if (fd) {
    return std::move(fd);
  } else {
    return absl::ErrnoToStatus(
        errno, absl::StrCat("open(\"", absl::CEscape(path), "\", O_CLOEXEC, O_RDONLY)"));
  }
}

// Opens a separate file description for `file_path` and checks if there's an exclusive lock on the
// file by attempting `flock(fd, LOCK_EX | LOCK_NB)`.
absl::StatusOr<bool> IsLocked(std::string_view file_path) {
  DEFINE_CONST_OR_RETURN(fd, OpenFile(file_path));
  if (::flock(*fd, LOCK_EX | LOCK_NB) < 0) {
    if (errno != EWOULDBLOCK) {
      return absl::ErrnoToStatus(errno, "flock(LOCK_EX | LOCK_NB)");
    } else {
      return true;
    }
  } else {
    return false;
  }
}

TEST(AdvisoryFileLockTest, LowLevelAcquire) {
  auto const status_or_file = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file);
  auto const& file = status_or_file.value();
  EXPECT_OK(AdvisoryLockAcquireExclusive(file.fd()));
  EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(true));
  EXPECT_OK(AdvisoryLockRelease(file.fd()));
  EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(false));
}

TEST(AdvisoryFileLockTest, Acquire) {
  auto const status_or_file = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file);
  auto const& file = status_or_file.value();
  {
    auto const status_or_lock = ExclusiveFileLock::Acquire(file.fd());
    EXPECT_OK(status_or_lock);
    EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(true));
  }
  EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(false));
}

TEST(AdvisoryFileLockTest, NestedAcquire) {
  auto const status_or_file = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file);
  auto const& file = status_or_file.value();
  {
    auto const status_or_lock1 = ExclusiveFileLock::Acquire(file.fd());
    ASSERT_OK(status_or_lock1);
    {
      auto const status_or_lock2 = ExclusiveFileLock::Acquire(file.fd());
      EXPECT_OK(status_or_lock2);
      EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(true));
    }
    EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(true));
  }
  EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(false));
}

TEST(AdvisoryFileLockTest, NestedLocksOnDifferentFileDescriptions) {
  auto const status_or_file1 = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file1);
  auto const& file1 = status_or_file1.value();
  auto const status_or_file2 = OpenFile(file1.path());
  ASSERT_OK(status_or_file2);
  auto const& file2 = status_or_file2.value();
  {
    auto const status_or_lock1 = ExclusiveFileLock::Acquire(file1.fd());
    ASSERT_OK(status_or_lock1);
    {
      auto const status_or_lock1 = ExclusiveFileLock::Acquire(file2);
      ASSERT_OK(status_or_lock1);
      EXPECT_THAT(IsLocked(file1.path()), IsOkAndHolds(true));
    }
    EXPECT_THAT(IsLocked(file1.path()), IsOkAndHolds(true));
  }
  EXPECT_THAT(IsLocked(file1.path()), IsOkAndHolds(false));
}

TEST(AdvisoryFileLockTest, Locked) {
  auto const status_or_file = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file);
  auto const& file = status_or_file.value();
  SigUsr1 sigusr1;
  auto const pid = ::fork();
  ASSERT_GE(pid, 0) << absl::ErrnoToStatus(errno, "fork");
  if (pid != 0) {  // parent
    ASSERT_OK(sigusr1.WaitForNotification());
    EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(true));
    ASSERT_OK(SigUsr1::Notify(pid));
    ASSERT_GE(::waitpid(pid, nullptr, 0), 0) << absl::ErrnoToStatus(errno, "waitpid");
  } else {  // child
    {
      auto const status_or_lock = ExclusiveFileLock::Acquire(file.fd());
      EXPECT_OK(status_or_lock);
      ASSERT_OK(sigusr1.Notify());
      ASSERT_OK(sigusr1.WaitForNotification());
    }
    ::_exit(0);
  }
}

TEST(AdvisoryFileLockTest, NestedLocks) {
  auto const status_or_file = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file);
  auto const& file = status_or_file.value();
  SigUsr1 sigusr1;
  auto const pid = ::fork();
  ASSERT_GE(pid, 0) << absl::ErrnoToStatus(errno, "fork");
  if (pid != 0) {  // parent
    ASSERT_OK(sigusr1.WaitForNotification());
    EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(true));
    ASSERT_OK(SigUsr1::Notify(pid));
    ASSERT_GE(::waitpid(pid, nullptr, 0), 0) << absl::ErrnoToStatus(errno, "waitpid");
  } else {  // child
    {
      auto const status_or_lock1 = ExclusiveFileLock::Acquire(file.fd());
      ASSERT_OK(status_or_lock1);
      auto const status_or_lock2 = ExclusiveFileLock::Acquire(file.fd());
      EXPECT_OK(status_or_lock2);
      ASSERT_OK(sigusr1.Notify());
      ASSERT_OK(sigusr1.WaitForNotification());
    }
    ::_exit(0);
  }
}

TEST(AdvisoryFileLockTest, InnerLockReleased) {
  auto const status_or_file = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file);
  auto const& file = status_or_file.value();
  SigUsr1 sigusr1;
  auto const pid = ::fork();
  ASSERT_GE(pid, 0) << absl::ErrnoToStatus(errno, "fork");
  if (pid != 0) {  // parent
    ASSERT_OK(sigusr1.WaitForNotification());
    EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(true));
    ASSERT_OK(SigUsr1::Notify(pid));
    ASSERT_GE(::waitpid(pid, nullptr, 0), 0) << absl::ErrnoToStatus(errno, "waitpid");
  } else {  // child
    {
      auto const status_or_lock1 = ExclusiveFileLock::Acquire(file.fd());
      ASSERT_OK(status_or_lock1);
      {
        auto const status_or_lock2 = ExclusiveFileLock::Acquire(file.fd());
        EXPECT_OK(status_or_lock2);
      }
      ASSERT_OK(sigusr1.Notify());
      ASSERT_OK(sigusr1.WaitForNotification());
    }
    ::_exit(0);
  }
}

TEST(AdvisoryFileLockTest, CloseFD) {
  auto status_or_file = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file);
  auto& file = status_or_file.value();
  SigUsr1 sigusr1;
  auto const pid = ::fork();
  ASSERT_GE(pid, 0) << absl::ErrnoToStatus(errno, "fork");
  if (pid != 0) {  // parent
    ASSERT_OK(sigusr1.WaitForNotification());
    EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(true));
    ASSERT_OK(SigUsr1::Notify(pid));
    ASSERT_GE(::waitpid(pid, nullptr, 0), 0) << absl::ErrnoToStatus(errno, "waitpid");
  } else {  // child
    {
      auto const status_or_lock = ExclusiveFileLock::Acquire(file.fd());
      EXPECT_OK(status_or_lock);
      file.Close();
      ASSERT_OK(sigusr1.Notify());
      ASSERT_OK(sigusr1.WaitForNotification());
    }
    ::_exit(0);
  }
}

TEST(AdvisoryFileLockTest, MoveConstruct) {
  auto const status_or_file = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file);
  auto const& file = status_or_file.value();
  {
    auto status_or_lock1 = ExclusiveFileLock::Acquire(file.fd());
    ASSERT_OK(status_or_lock1);
    {
      ExclusiveFileLock lock2{std::move(status_or_lock1).value()};
      EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(true));
    }
    EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(false));
  }
  EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(false));
}

TEST(AdvisoryFileLockTest, MoveAssign) {
  auto const status_or_file = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file);
  auto const& file = status_or_file.value();
  {
    auto status_or_lock1 = ExclusiveFileLock::Acquire(file.fd());
    ASSERT_OK(status_or_lock1);
    {
      ExclusiveFileLock lock2;
      lock2 = std::move(status_or_lock1).value();
      EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(true));
    }
    EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(false));
  }
  EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(false));
}

TEST(AdvisoryFileLockTest, SelfMoveAssign) {
  auto const status_or_file = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file);
  auto const& file = status_or_file.value();
  {
    auto status_or_lock = ExclusiveFileLock::Acquire(file.fd());
    ASSERT_OK(status_or_lock);
    auto& lock = status_or_lock.value();
    lock = std::move(lock);  // NOLINT
    EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(true));
  }
  EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(false));
}

TEST(AdvisoryFileLockTest, Swap) {
  auto const status_or_file1 = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file1);
  auto const& file1 = status_or_file1.value();
  auto const status_or_file2 = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file2);
  auto const& file2 = status_or_file2.value();
  {
    auto status_or_lock1 = ExclusiveFileLock::Acquire(file1.fd());
    ASSERT_OK(status_or_lock1);
    auto& lock1 = status_or_lock1.value();
    {
      auto status_or_lock2 = ExclusiveFileLock::Acquire(file2.fd());
      ASSERT_OK(status_or_lock2);
      auto& lock2 = status_or_lock2.value();
      lock1.swap(lock2);
      ASSERT_THAT(IsLocked(file1.path()), IsOkAndHolds(true));
      ASSERT_THAT(IsLocked(file2.path()), IsOkAndHolds(true));
    }
    EXPECT_THAT(IsLocked(file1.path()), IsOkAndHolds(false));
    EXPECT_THAT(IsLocked(file2.path()), IsOkAndHolds(true));
  }
  EXPECT_THAT(IsLocked(file1.path()), IsOkAndHolds(false));
  EXPECT_THAT(IsLocked(file2.path()), IsOkAndHolds(false));
}

TEST(AdvisoryFileLockTest, AdlSwap) {
  auto const status_or_file1 = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file1);
  auto const& file1 = status_or_file1.value();
  auto const status_or_file2 = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file2);
  auto const& file2 = status_or_file2.value();
  {
    auto status_or_lock1 = ExclusiveFileLock::Acquire(file1.fd());
    ASSERT_OK(status_or_lock1);
    auto& lock1 = status_or_lock1.value();
    {
      auto status_or_lock2 = ExclusiveFileLock::Acquire(file2.fd());
      ASSERT_OK(status_or_lock2);
      auto& lock2 = status_or_lock2.value();
      swap(lock1, lock2);
      ASSERT_THAT(IsLocked(file1.path()), IsOkAndHolds(true));
      ASSERT_THAT(IsLocked(file2.path()), IsOkAndHolds(true));
    }
    EXPECT_THAT(IsLocked(file1.path()), IsOkAndHolds(false));
    EXPECT_THAT(IsLocked(file2.path()), IsOkAndHolds(true));
  }
  EXPECT_THAT(IsLocked(file1.path()), IsOkAndHolds(false));
  EXPECT_THAT(IsLocked(file2.path()), IsOkAndHolds(false));
}

TEST(AdvisoryFileLockTest, SelfSwap) {
  auto const status_or_file = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file);
  auto const& file = status_or_file.value();
  {
    auto status_or_lock = ExclusiveFileLock::Acquire(file.fd());
    ASSERT_OK(status_or_lock);
    auto& lock = status_or_lock.value();
    lock.swap(lock);
    EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(true));
  }
  EXPECT_THAT(IsLocked(file.path()), IsOkAndHolds(false));
}

}  // namespace
