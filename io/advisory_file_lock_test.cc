#include "io/advisory_file_lock.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string_view>

#include "absl/status/status.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "io/sigusr.h"

namespace {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::TestTempFile;
using ::tsdb2::io::ExclusiveFileLock;
using ::tsdb2::io::SigUsr1;

using LockInfo = struct flock;

std::string_view constexpr kTestFileName = "advisory_file_lock_test";

TEST(AdvisoryFileLockTest, Acquire) {
  auto const status_or_file = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file);
  auto const& file = status_or_file.value();
  EXPECT_OK(ExclusiveFileLock::Acquire(file.fd()));
}

TEST(AdvisoryFileLockTest, NestedAcquire) {
  auto const status_or_file = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file);
  auto const& file = status_or_file.value();
  auto const status_or_lock1 = ExclusiveFileLock::Acquire(file.fd());
  ASSERT_OK(status_or_lock1);
  EXPECT_OK(ExclusiveFileLock::Acquire(file.fd()));
}

TEST(AdvisoryFileLockTest, FullRange) {
  auto const status_or_file = TestTempFile::Create(kTestFileName);
  ASSERT_OK(status_or_file);
  auto const& file = status_or_file.value();
  SigUsr1 sigusr1;
  auto const pid = ::fork();
  ASSERT_GE(pid, 0) << absl::ErrnoToStatus(errno, "fork");
  if (pid != 0) {  // parent
    ASSERT_OK(sigusr1.WaitForNotification());
    LockInfo fl{};
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;
    int const result =
        ::fcntl(file.fd().get(), F_GETLK, &fl);  // NOLINT(cppcoreguidelines-pro-type-vararg)
    ASSERT_LE(result, 0) << absl::ErrnoToStatus(errno, "fnctl");
    EXPECT_THAT(fl, AllOf(Field(&LockInfo::l_type, F_WRLCK), Field(&LockInfo::l_whence, SEEK_SET),
                          Field(&LockInfo::l_start, 0), Field(&LockInfo::l_len, 0),
                          Field(&LockInfo::l_pid, pid)));
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
    LockInfo fl{};
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;
    int const result =
        ::fcntl(file.fd().get(), F_GETLK, &fl);  // NOLINT(cppcoreguidelines-pro-type-vararg)
    ASSERT_LE(result, 0) << absl::ErrnoToStatus(errno, "fnctl");
    EXPECT_THAT(fl, AllOf(Field(&LockInfo::l_type, F_WRLCK), Field(&LockInfo::l_whence, SEEK_SET),
                          Field(&LockInfo::l_start, 0), Field(&LockInfo::l_len, 0),
                          Field(&LockInfo::l_pid, pid)));
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
    LockInfo fl{};
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;
    auto const result =
        ::fcntl(file.fd().get(), F_GETLK, &fl);  // NOLINT(cppcoreguidelines-pro-type-vararg)
    ASSERT_LE(result, 0) << absl::ErrnoToStatus(errno, "fnctl");
    EXPECT_THAT(fl, AllOf(Field(&LockInfo::l_type, F_WRLCK), Field(&LockInfo::l_whence, SEEK_SET),
                          Field(&LockInfo::l_start, 0), Field(&LockInfo::l_len, 0),
                          Field(&LockInfo::l_pid, pid)));
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

}  // namespace
