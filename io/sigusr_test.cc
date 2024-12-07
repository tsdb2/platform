#include "io/sigusr.h"

#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

#include "absl/status/status.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::tsdb2::io::SigUsr1;

TEST(SigUsr1Test, Notify) {
  SigUsr1 sigusr1;
  EXPECT_FALSE(sigusr1.is_notified());
  auto const pid = ::fork();
  ASSERT_GE(pid, 0) << absl::ErrnoToStatus(errno, "fork");
  if (pid) {  // parent
    EXPECT_OK(sigusr1.WaitForNotification());
    EXPECT_TRUE(sigusr1.is_notified());
    ASSERT_GE(::waitpid(pid, nullptr, 0), 0);
  } else {  // child
    EXPECT_OK(sigusr1.Notify());
    EXPECT_FALSE(sigusr1.is_notified());
    ::_exit(0);
  }
}

TEST(SigUsr1Test, NotifyThread) {
  SigUsr1 sigusr1;
  EXPECT_FALSE(sigusr1.is_notified());
  auto const tid = ::gettid();
  auto const pid = ::fork();
  ASSERT_GE(pid, 0) << absl::ErrnoToStatus(errno, "fork");
  if (pid) {  // parent
    EXPECT_OK(sigusr1.WaitForNotification());
    EXPECT_TRUE(sigusr1.is_notified());
    ASSERT_GE(::waitpid(pid, nullptr, 0), 0);
  } else {  // child
    EXPECT_OK(SigUsr1::Notify(::getppid(), tid));
    EXPECT_FALSE(sigusr1.is_notified());
    ::_exit(0);
  }
}

}  // namespace
