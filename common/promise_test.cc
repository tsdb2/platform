#include "common/promise.h"

#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "common/mock_clock.h"
#include "common/scheduler.h"
#include "common/testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::_;
using ::tsdb2::common::MockClock;
using ::tsdb2::common::Promise;
using ::tsdb2::common::Scheduler;

class PromiseTest : public ::testing::Test {
 protected:
  explicit PromiseTest() { CHECK_OK(scheduler_.WaitUntilAllWorkersAsleep()); }

  MockClock clock_;
  Scheduler scheduler_{Scheduler::Options{
      .clock = &clock_,
      .start_now = true,
  }};
};

TEST_F(PromiseTest, ResolveImmediatelySkipErrorThenVoid) {
  Promise<int>{[](auto resolve) { resolve(42); }}.Then(
      [&](int const value) { EXPECT_EQ(value, 42); });
}

TEST_F(PromiseTest, ResolveImmediatelySkipErrorThenInt) {
  int answer = 0;
  Promise<int>{[](auto resolve) { resolve(42); }}.Then([&](int const value) {
    EXPECT_EQ(value, 42);
    return answer = value;
  });
  EXPECT_EQ(answer, 42);
}

TEST_F(PromiseTest, ResolveImmediatelySkipErrorChainVoid) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(42); }}
      .Then([&](int const value) {
        EXPECT_EQ(value, 42);
        done1 = true;
      })
      .Then([&](absl::Status const status) {
        EXPECT_OK(status);
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveImmediatelySkipErrorChainString) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(42); }}
      .Then([&](int const value) -> std::string {
        EXPECT_EQ(value, 42);
        done1 = true;
        return "lorem";
      })
      .Then([&](absl::StatusOr<std::string> const status_or_string) {
        EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveImmediatelySkipErrorChainSuccessStatusOrInt) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(42); }}
      .Then([&](int const value) -> absl::StatusOr<int> {
        EXPECT_EQ(value, 42);
        done1 = true;
        return 43;
      })
      .Then([&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, IsOkAndHolds(43));
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveImmediatelySkipErrorChainErrorStatusOrInt) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(42); }}
      .Then([&](int const value) -> absl::StatusOr<int> {
        EXPECT_EQ(value, 42);
        done1 = true;
        return absl::NotFoundError("testing");
      })
      .Then([&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kNotFound));
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveImmediatelySkipErrorChainPromiseStringResolveImmediately) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(42); }}
      .Then([&](int const value) {
        EXPECT_EQ(value, 42);
        done1 = true;
        return Promise<std::string>([](auto resolve) { resolve("lorem"); });
      })
      .Then([&](absl::StatusOr<std::string> const status_or_string) {
        EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveImmediatelySkipErrorChainPromiseStringResolveLater) {
  bool done1 = false;
  bool done2 = false;
  auto const p =
      Promise<int>{[](auto resolve) { resolve(42); }}
          .Then([&](int const value) {
            EXPECT_EQ(value, 42);
            done1 = true;
            return Promise<std::string>([this](auto resolve) {
              scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve("lorem"); },
                                    absl::Seconds(1));
            });
          })
          .Then([&](absl::StatusOr<std::string> const status_or_string) {
            EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
            done2 = true;
          });
  EXPECT_TRUE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveImmediatelySkipErrorChainPromiseStringRejectImmediately) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(42); }}
      .Then([&](int const value) {
        EXPECT_EQ(value, 42);
        done1 = true;
        return Promise<std::string>([](auto resolve) { resolve(absl::NotFoundError("testing")); });
      })
      .Then([&](absl::StatusOr<std::string> const status_or_string) {
        EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kNotFound));
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveImmediatelySkipErrorChainPromiseStringRejectLater) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[](auto resolve) { resolve(42); }}
                     .Then([&](int const value) {
                       EXPECT_EQ(value, 42);
                       done1 = true;
                       return Promise<std::string>([this](auto resolve) {
                         scheduler_.ScheduleIn(
                             [resolve = std::move(resolve)]() mutable {
                               resolve(absl::NotFoundError("testing"));
                             },
                             absl::Seconds(1));
                       });
                     })
                     .Then([&](absl::StatusOr<std::string> const status_or_string) {
                       EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kNotFound));
                       done2 = true;
                     });
  EXPECT_TRUE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveImmediatelyThenVoid) {
  bool done = false;
  Promise<int>{[](auto resolve) { resolve(42); }}.Then(
      [&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, IsOkAndHolds(42));
        done = true;
      });
  EXPECT_TRUE(done);
}

TEST_F(PromiseTest, ResolveImmediatelyThenInt) {
  int answer = 0;
  Promise<int>{[](auto resolve) { resolve(42); }}.Then(
      [&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, IsOkAndHolds(42));
        return answer = status_or_value.value();
      });
  EXPECT_EQ(answer, 42);
}

TEST_F(PromiseTest, ResolveImmediatelyChainVoid) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(42); }}
      .Then([&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, IsOkAndHolds(42));
        done1 = true;
      })
      .Then([&](absl::Status const status) {
        EXPECT_OK(status);
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveImmediatelyChainString) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(42); }}
      .Then([&](absl::StatusOr<int> const status_or_value) -> std::string {
        EXPECT_THAT(status_or_value, IsOkAndHolds(42));
        done1 = true;
        return "lorem";
      })
      .Then([&](absl::StatusOr<std::string> const status_or_string) {
        EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveImmediatelyChainSuccessStatusOrInt) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(42); }}
      .Then([&](absl::StatusOr<int> const status_or_value) -> absl::StatusOr<int> {
        EXPECT_THAT(status_or_value, IsOkAndHolds(42));
        done1 = true;
        return 43;
      })
      .Then([&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, IsOkAndHolds(43));
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveImmediatelyChainErrorStatusOrInt) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(42); }}
      .Then([&](absl::StatusOr<int> const status_or_value) -> absl::StatusOr<int> {
        EXPECT_THAT(status_or_value, IsOkAndHolds(42));
        done1 = true;
        return absl::NotFoundError("testing");
      })
      .Then([&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kNotFound));
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveImmediatelyChainPromiseStringResolveImmediately) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(42); }}
      .Then([&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, IsOkAndHolds(42));
        done1 = true;
        return Promise<std::string>([](auto resolve) { resolve("lorem"); });
      })
      .Then([&](absl::StatusOr<std::string> const status_or_string) {
        EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveImmediatelyChainPromiseStringResolveLater) {
  bool done1 = false;
  bool done2 = false;
  auto const p =
      Promise<int>{[](auto resolve) { resolve(42); }}
          .Then([&](absl::StatusOr<int> const status_or_value) {
            EXPECT_THAT(status_or_value, IsOkAndHolds(42));
            done1 = true;
            return Promise<std::string>([this](auto resolve) {
              scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve("lorem"); },
                                    absl::Seconds(1));
            });
          })
          .Then([&](absl::StatusOr<std::string> const status_or_string) {
            EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
            done2 = true;
          });
  EXPECT_TRUE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveImmediatelyChainPromiseStringRejectImmediately) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(42); }}
      .Then([&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, IsOkAndHolds(42));
        done1 = true;
        return Promise<std::string>([](auto resolve) { resolve(absl::NotFoundError("testing")); });
      })
      .Then([&](absl::StatusOr<std::string> const status_or_string) {
        EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kNotFound));
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveImmediatelyChainPromiseStringRejectLater) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[](auto resolve) { resolve(42); }}
                     .Then([&](absl::StatusOr<int> const status_or_value) {
                       EXPECT_THAT(status_or_value, IsOkAndHolds(42));
                       done1 = true;
                       return Promise<std::string>([this](auto resolve) {
                         scheduler_.ScheduleIn(
                             [resolve = std::move(resolve)]() mutable {
                               resolve(absl::NotFoundError("testing"));
                             },
                             absl::Seconds(1));
                       });
                     })
                     .Then([&](absl::StatusOr<std::string> const status_or_string) {
                       EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kNotFound));
                       done2 = true;
                     });
  EXPECT_TRUE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveLaterSkipErrorThenVoid) {
  bool done = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}.Then([&](int const value) {
    EXPECT_EQ(value, 42);
    done = true;
  });
  EXPECT_FALSE(done);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done);
}

TEST_F(PromiseTest, ResolveLaterSkipErrorThenInt) {
  int answer = 0;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}.Then([&](int const value) {
    EXPECT_EQ(value, 42);
    return answer = value;
  });
  EXPECT_EQ(answer, 0);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_EQ(answer, 42);
}

TEST_F(PromiseTest, ResolveLaterSkipErrorChainVoid) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}
                     .Then([&](int const value) {
                       EXPECT_EQ(value, 42);
                       done1 = true;
                     })
                     .Then([&](absl::Status const status) {
                       EXPECT_OK(status);
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveLaterSkipErrorChainString) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}
                     .Then([&](int const value) -> std::string {
                       EXPECT_EQ(value, 42);
                       done1 = true;
                       return "lorem";
                     })
                     .Then([&](absl::StatusOr<std::string> const status_or_string) {
                       EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveLaterSkipErrorChainSuccessStatusOrInt) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}
                     .Then([&](int const value) -> absl::StatusOr<int> {
                       EXPECT_EQ(value, 42);
                       done1 = true;
                       return 43;
                     })
                     .Then([&](absl::StatusOr<int> const status_or_value) {
                       EXPECT_THAT(status_or_value, IsOkAndHolds(43));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveLaterSkipErrorChainErrorStatusOrInt) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}
                     .Then([&](int const value) -> absl::StatusOr<int> {
                       EXPECT_EQ(value, 42);
                       done1 = true;
                       return absl::NotFoundError("testing");
                     })
                     .Then([&](absl::StatusOr<int> const status_or_value) {
                       EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kNotFound));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveLaterSkipErrorChainPromiseStringResolveImmediately) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}
                     .Then([&](int const value) {
                       EXPECT_EQ(value, 42);
                       done1 = true;
                       return Promise<std::string>([](auto resolve) { resolve("lorem"); });
                     })
                     .Then([&](absl::StatusOr<std::string> const status_or_string) {
                       EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveLaterSkipErrorChainPromiseStringResolveLater) {
  bool done1 = false;
  bool done2 = false;
  auto const p =
      Promise<int>{[this](auto resolve) {
        scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                              absl::Seconds(1));
      }}
          .Then([&](int const value) {
            EXPECT_EQ(value, 42);
            done1 = true;
            return Promise<std::string>([this](auto resolve) {
              scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve("lorem"); },
                                    absl::Seconds(1));
            });
          })
          .Then([&](absl::StatusOr<std::string> const status_or_string) {
            EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
            done2 = true;
          });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveLaterSkipErrorChainPromiseStringRejectImmediately) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}
                     .Then([&](int const value) {
                       EXPECT_EQ(value, 42);
                       done1 = true;
                       return Promise<std::string>(
                           [](auto resolve) { resolve(absl::NotFoundError("testing")); });
                     })
                     .Then([&](absl::StatusOr<std::string> const status_or_string) {
                       EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kNotFound));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveLaterSkipErrorChainPromiseStringRejectLater) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}
                     .Then([&](int const value) {
                       EXPECT_EQ(value, 42);
                       done1 = true;
                       return Promise<std::string>([this](auto resolve) {
                         scheduler_.ScheduleIn(
                             [resolve = std::move(resolve)]() mutable {
                               resolve(absl::NotFoundError("testing"));
                             },
                             absl::Seconds(1));
                       });
                     })
                     .Then([&](absl::StatusOr<std::string> const status_or_string) {
                       EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kNotFound));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveLaterThenVoid) {
  bool done = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}.Then([&](absl::StatusOr<int> const status_or_value) {
    EXPECT_THAT(status_or_value, IsOkAndHolds(42));
    done = true;
  });
  EXPECT_FALSE(done);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done);
}

TEST_F(PromiseTest, ResolveLaterThenInt) {
  int answer = 0;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}.Then([&](absl::StatusOr<int> const status_or_value) {
    EXPECT_THAT(status_or_value, IsOkAndHolds(42));
    return answer = status_or_value.value();
  });
  EXPECT_EQ(answer, 0);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_EQ(answer, 42);
}

TEST_F(PromiseTest, ResolveLaterChainVoid) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}
                     .Then([&](absl::StatusOr<int> const status_or_value) {
                       EXPECT_THAT(status_or_value, IsOkAndHolds(42));
                       done1 = true;
                     })
                     .Then([&](absl::Status const status) {
                       EXPECT_OK(status);
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveLaterChainString) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}
                     .Then([&](absl::StatusOr<int> const status_or_value) -> std::string {
                       EXPECT_THAT(status_or_value, IsOkAndHolds(42));
                       done1 = true;
                       return "lorem";
                     })
                     .Then([&](absl::StatusOr<std::string> const status_or_string) {
                       EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveLaterChainSuccessStatusOrInt) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}
                     .Then([&](absl::StatusOr<int> const status_or_value) -> absl::StatusOr<int> {
                       EXPECT_THAT(status_or_value, IsOkAndHolds(42));
                       done1 = true;
                       return 43;
                     })
                     .Then([&](absl::StatusOr<int> const status_or_value) {
                       EXPECT_THAT(status_or_value, IsOkAndHolds(43));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveLaterChainErrorStatusOrInt) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}
                     .Then([&](absl::StatusOr<int> const status_or_value) -> absl::StatusOr<int> {
                       EXPECT_THAT(status_or_value, IsOkAndHolds(42));
                       done1 = true;
                       return absl::NotFoundError("testing");
                     })
                     .Then([&](absl::StatusOr<int> const status_or_value) {
                       EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kNotFound));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveLaterChainPromiseStringResolveImmediately) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}
                     .Then([&](absl::StatusOr<int> const status_or_value) {
                       EXPECT_THAT(status_or_value, IsOkAndHolds(42));
                       done1 = true;
                       return Promise<std::string>([](auto resolve) { resolve("lorem"); });
                     })
                     .Then([&](absl::StatusOr<std::string> const status_or_string) {
                       EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveLaterChainPromiseStringResolveLater) {
  bool done1 = false;
  bool done2 = false;
  auto const p =
      Promise<int>{[this](auto resolve) {
        scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                              absl::Seconds(1));
      }}
          .Then([&](absl::StatusOr<int> const status_or_value) {
            EXPECT_THAT(status_or_value, IsOkAndHolds(42));
            done1 = true;
            return Promise<std::string>([this](auto resolve) {
              scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve("lorem"); },
                                    absl::Seconds(1));
            });
          })
          .Then([&](absl::StatusOr<std::string> const status_or_string) {
            EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
            done2 = true;
          });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveLaterChainPromiseStringRejectImmediately) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}
                     .Then([&](absl::StatusOr<int> const status_or_value) {
                       EXPECT_THAT(status_or_value, IsOkAndHolds(42));
                       done1 = true;
                       return Promise<std::string>(
                           [](auto resolve) { resolve(absl::NotFoundError("testing")); });
                     })
                     .Then([&](absl::StatusOr<std::string> const status_or_string) {
                       EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kNotFound));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveLaterChainPromiseStringRejectLater) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve(42); },
                                         absl::Seconds(1));
                 }}
                     .Then([&](absl::StatusOr<int> const status_or_value) {
                       EXPECT_THAT(status_or_value, IsOkAndHolds(42));
                       done1 = true;
                       return Promise<std::string>([this](auto resolve) {
                         scheduler_.ScheduleIn(
                             [resolve = std::move(resolve)]() mutable {
                               resolve(absl::NotFoundError("testing"));
                             },
                             absl::Seconds(1));
                       });
                     })
                     .Then([&](absl::StatusOr<std::string> const status_or_string) {
                       EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kNotFound));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectImmediatelySkipErrorThenVoid) {
  Promise<int>{[](auto resolve) { resolve(absl::CancelledError("cancelled")); }}.Then(
      [](int const value) { [] { FAIL(); }(); });
}

TEST_F(PromiseTest, RejectImmediatelySkipErrorThenInt) {
  Promise<int>{[](auto resolve) { resolve(absl::CancelledError("cancelled")); }}.Then(
      [](int const value) {
        [] { FAIL(); }();
        return 42;
      });
}

TEST_F(PromiseTest, RejectImmediatelySkipErrorChainVoid) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(absl::CancelledError("cancelled")); }}
      .Then([&](int const value) {
        [] { FAIL(); }();
        done1 = true;
      })
      .Then([&](absl::Status const status) {
        EXPECT_THAT(status, StatusIs(absl::StatusCode::kCancelled));
        done2 = true;
      });
  EXPECT_FALSE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectImmediatelySkipErrorChainString) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(absl::CancelledError("cancelled")); }}
      .Then([&](int const value) -> std::string {
        [] { FAIL(); }();
        done1 = true;
        return "lorem";
      })
      .Then([&](absl::StatusOr<std::string> const status_or_string) {
        EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kCancelled));
        done2 = true;
      });
  EXPECT_FALSE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectImmediatelySkipErrorChainStatusOrInt) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(absl::CancelledError("cancelled")); }}
      .Then([&](int const value) -> absl::StatusOr<int> {
        [] { FAIL(); }();
        done1 = true;
        return 43;
      })
      .Then([&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
        done2 = true;
      });
  EXPECT_FALSE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectImmediatelySkipErrorChainPromiseString) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(absl::CancelledError("cancelled")); }}
      .Then([&](int const value) {
        done1 = true;
        return Promise<std::string>([](auto resolve) { resolve("lorem"); });
      })
      .Then([&](absl::StatusOr<std::string> const status_or_string) {
        EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kCancelled));
        done2 = true;
      });
  EXPECT_FALSE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectImmediatelyThenVoid) {
  bool done = false;
  Promise<int>{[](auto resolve) { resolve(absl::CancelledError("cancelled")); }}.Then(
      [&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
        done = true;
      });
  EXPECT_TRUE(done);
}

TEST_F(PromiseTest, RejectImmediatelyThenInt) {
  int answer = 0;
  Promise<int>{[](auto resolve) { resolve(absl::CancelledError("cancelled")); }}.Then(
      [&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
        return answer = 42;
      });
  EXPECT_EQ(answer, 42);
}

TEST_F(PromiseTest, RejectImmediatelyChainVoid) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(absl::CancelledError("cancelled")); }}
      .Then([&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
        done1 = true;
      })
      .Then([&](absl::Status const status) {
        EXPECT_OK(status);
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectImmediatelyChainString) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(absl::CancelledError("cancelled")); }}
      .Then([&](absl::StatusOr<int> const status_or_value) -> std::string {
        EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
        done1 = true;
        return "lorem";
      })
      .Then([&](absl::StatusOr<std::string> const status_or_string) {
        EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectImmediatelyChainSuccessStatusOrInt) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(absl::CancelledError("cancelled")); }}
      .Then([&](absl::StatusOr<int> const status_or_value) -> absl::StatusOr<int> {
        EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
        done1 = true;
        return 43;
      })
      .Then([&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, IsOkAndHolds(43));
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectImmediatelyChainErrorStatusOrInt) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(absl::CancelledError("cancelled")); }}
      .Then([&](absl::StatusOr<int> const status_or_value) -> absl::StatusOr<int> {
        EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
        done1 = true;
        return absl::NotFoundError("testing");
      })
      .Then([&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kNotFound));
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectImmediatelyChainPromiseStringResolveImmediately) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(absl::CancelledError("cancelled")); }}
      .Then([&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
        done1 = true;
        return Promise<std::string>([](auto resolve) { resolve("lorem"); });
      })
      .Then([&](absl::StatusOr<std::string> const status_or_string) {
        EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectImmediatelyChainPromiseStringResolveLater) {
  bool done1 = false;
  bool done2 = false;
  auto const p =
      Promise<int>{[](auto resolve) { resolve(absl::CancelledError("cancelled")); }}
          .Then([&](absl::StatusOr<int> const status_or_value) {
            EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
            done1 = true;
            return Promise<std::string>([this](auto resolve) {
              scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve("lorem"); },
                                    absl::Seconds(1));
            });
          })
          .Then([&](absl::StatusOr<std::string> const status_or_string) {
            EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
            done2 = true;
          });
  EXPECT_TRUE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectImmediatelyChainPromiseStringRejectImmediately) {
  bool done1 = false;
  bool done2 = false;
  Promise<int>{[](auto resolve) { resolve(absl::CancelledError("cancelled")); }}
      .Then([&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
        done1 = true;
        return Promise<std::string>([](auto resolve) { resolve(absl::NotFoundError("testing")); });
      })
      .Then([&](absl::StatusOr<std::string> const status_or_string) {
        EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kNotFound));
        done2 = true;
      });
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectImmediatelyChainPromiseStringRejectLater) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[](auto resolve) { resolve(absl::CancelledError("cancelled")); }}
                     .Then([&](absl::StatusOr<int> const status_or_value) {
                       EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
                       done1 = true;
                       return Promise<std::string>([this](auto resolve) {
                         scheduler_.ScheduleIn(
                             [resolve = std::move(resolve)]() mutable {
                               resolve(absl::NotFoundError("testing"));
                             },
                             absl::Seconds(1));
                       });
                     })
                     .Then([&](absl::StatusOr<std::string> const status_or_string) {
                       EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kNotFound));
                       done2 = true;
                     });
  EXPECT_TRUE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectLaterThenVoid) {
  bool done = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn(
                       [resolve = std::move(resolve)]() mutable {
                         resolve(absl::CancelledError("cancelled"));
                       },
                       absl::Seconds(1));
                 }}.Then([&](absl::StatusOr<int> const status_or_value) {
    EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
    done = true;
  });
  EXPECT_FALSE(done);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done);
}

TEST_F(PromiseTest, RejectLaterThenInt) {
  int answer = 0;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn(
                       [resolve = std::move(resolve)]() mutable {
                         resolve(absl::CancelledError("cancelled"));
                       },
                       absl::Seconds(1));
                 }}.Then([&](absl::StatusOr<int> const status_or_value) {
    EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
    return answer = 42;
  });
  EXPECT_EQ(answer, 0);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_EQ(answer, 42);
}

TEST_F(PromiseTest, RejectLaterChainVoid) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn(
                       [resolve = std::move(resolve)]() mutable {
                         resolve(absl::CancelledError("cancelled"));
                       },
                       absl::Seconds(1));
                 }}
                     .Then([&](absl::StatusOr<int> const status_or_value) {
                       EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
                       done1 = true;
                     })
                     .Then([&](absl::Status const status) {
                       EXPECT_OK(status);
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectLaterChainString) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn(
                       [resolve = std::move(resolve)]() mutable {
                         resolve(absl::CancelledError("cancelled"));
                       },
                       absl::Seconds(1));
                 }}
                     .Then([&](absl::StatusOr<int> const status_or_value) -> std::string {
                       EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
                       done1 = true;
                       return "lorem";
                     })
                     .Then([&](absl::StatusOr<std::string> const status_or_string) {
                       EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectLaterChainSuccessStatusOrInt) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn(
                       [resolve = std::move(resolve)]() mutable {
                         resolve(absl::CancelledError("cancelled"));
                       },
                       absl::Seconds(1));
                 }}
                     .Then([&](absl::StatusOr<int> const status_or_value) -> absl::StatusOr<int> {
                       EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
                       done1 = true;
                       return 43;
                     })
                     .Then([&](absl::StatusOr<int> const status_or_value) {
                       EXPECT_THAT(status_or_value, IsOkAndHolds(43));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectLaterChainErrorStatusOrInt) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn(
                       [resolve = std::move(resolve)]() mutable {
                         resolve(absl::CancelledError("cancelled"));
                       },
                       absl::Seconds(1));
                 }}
                     .Then([&](absl::StatusOr<int> const status_or_value) -> absl::StatusOr<int> {
                       EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
                       done1 = true;
                       return absl::NotFoundError("testing");
                     })
                     .Then([&](absl::StatusOr<int> const status_or_value) {
                       EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kNotFound));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectLaterChainPromiseStringResolveImmediately) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn(
                       [resolve = std::move(resolve)]() mutable {
                         resolve(absl::CancelledError("cancelled"));
                       },
                       absl::Seconds(1));
                 }}
                     .Then([&](absl::StatusOr<int> const status_or_value) {
                       EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
                       done1 = true;
                       return Promise<std::string>([](auto resolve) { resolve("lorem"); });
                     })
                     .Then([&](absl::StatusOr<std::string> const status_or_string) {
                       EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectLaterChainPromiseStringResolveLater) {
  bool done1 = false;
  bool done2 = false;
  auto const p =
      Promise<int>{[this](auto resolve) {
        scheduler_.ScheduleIn(
            [resolve = std::move(resolve)]() mutable {
              resolve(absl::CancelledError("cancelled"));
            },
            absl::Seconds(1));
      }}
          .Then([&](absl::StatusOr<int> const status_or_value) {
            EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
            done1 = true;
            return Promise<std::string>([this](auto resolve) {
              scheduler_.ScheduleIn([resolve = std::move(resolve)]() mutable { resolve("lorem"); },
                                    absl::Seconds(1));
            });
          })
          .Then([&](absl::StatusOr<std::string> const status_or_string) {
            EXPECT_THAT(status_or_string, IsOkAndHolds("lorem"));
            done2 = true;
          });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectLaterChainPromiseStringRejectImmediately) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn(
                       [resolve = std::move(resolve)]() mutable {
                         resolve(absl::CancelledError("cancelled"));
                       },
                       absl::Seconds(1));
                 }}
                     .Then([&](absl::StatusOr<int> const status_or_value) {
                       EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
                       done1 = true;
                       return Promise<std::string>(
                           [](auto resolve) { resolve(absl::NotFoundError("testing")); });
                     })
                     .Then([&](absl::StatusOr<std::string> const status_or_string) {
                       EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kNotFound));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, RejectLaterChainPromiseStringRejectLater) {
  bool done1 = false;
  bool done2 = false;
  auto const p = Promise<int>{[this](auto resolve) {
                   scheduler_.ScheduleIn(
                       [resolve = std::move(resolve)]() mutable {
                         resolve(absl::CancelledError("cancelled"));
                       },
                       absl::Seconds(1));
                 }}
                     .Then([&](absl::StatusOr<int> const status_or_value) {
                       EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kCancelled));
                       done1 = true;
                       return Promise<std::string>([this](auto resolve) {
                         scheduler_.ScheduleIn(
                             [resolve = std::move(resolve)]() mutable {
                               resolve(absl::NotFoundError("testing"));
                             },
                             absl::Seconds(1));
                       });
                     })
                     .Then([&](absl::StatusOr<std::string> const status_or_string) {
                       EXPECT_THAT(status_or_string, StatusIs(absl::StatusCode::kNotFound));
                       done2 = true;
                     });
  EXPECT_FALSE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_FALSE(done2);
  clock_.AdvanceTime(absl::Seconds(1));
  ASSERT_OK(scheduler_.WaitUntilAllWorkersAsleep());
  EXPECT_TRUE(done1);
  EXPECT_TRUE(done2);
}

TEST_F(PromiseTest, ResolveInt) {
  bool done = false;
  Promise<int>::Resolve(42).Then([&](absl::StatusOr<int> const status_or_value) {
    EXPECT_THAT(status_or_value, IsOkAndHolds(42));
    done = true;
  });
  EXPECT_TRUE(done);
}

TEST_F(PromiseTest, ResolveVoid) {
  bool done = false;
  Promise<void>::Resolve().Then([&](absl::Status const status) {
    EXPECT_OK(status);
    done = true;
  });
  EXPECT_TRUE(done);
}

TEST_F(PromiseTest, RejectInt) {
  bool done = false;
  Promise<int>::Reject(absl::FailedPreconditionError("test"))
      .Then([&](absl::StatusOr<int> const status_or_value) {
        EXPECT_THAT(status_or_value, StatusIs(absl::StatusCode::kFailedPrecondition));
        done = true;
      });
  EXPECT_TRUE(done);
}

TEST_F(PromiseTest, RejectVoid) {
  bool done = false;
  Promise<void>::Reject(absl::FailedPreconditionError("test")).Then([&](absl::Status const status) {
    EXPECT_THAT(status, StatusIs(absl::StatusCode::kFailedPrecondition));
    done = true;
  });
  EXPECT_TRUE(done);
}

TEST_F(PromiseTest, MoveConstruct) {
  auto p1 = Promise<int>::Resolve(42);
  Promise<int> p2{std::move(p1)};
  std::move(p2).Then([](absl::StatusOr<int> const value) { EXPECT_THAT(value, IsOkAndHolds(42)); });
}

TEST_F(PromiseTest, MoveAssign) {
  auto p1 = Promise<int>::Resolve(42);
  Promise<int> p2;
  p2 = std::move(p1);
  std::move(p2).Then([](absl::StatusOr<int> const value) { EXPECT_THAT(value, IsOkAndHolds(42)); });
}

TEST_F(PromiseTest, Swap) {
  auto p1 = Promise<int>::Resolve(42);
  auto p2 = Promise<int>::Resolve(43);
  p1.swap(p2);
  std::move(p1).Then([](absl::StatusOr<int> const value) { EXPECT_THAT(value, IsOkAndHolds(43)); });
  std::move(p2).Then([](absl::StatusOr<int> const value) { EXPECT_THAT(value, IsOkAndHolds(42)); });
}

TEST_F(PromiseTest, AdlSwap) {
  auto p1 = Promise<int>::Resolve(42);
  auto p2 = Promise<int>::Resolve(43);
  swap(p1, p2);
  std::move(p1).Then([](absl::StatusOr<int> const value) { EXPECT_THAT(value, IsOkAndHolds(43)); });
  std::move(p2).Then([](absl::StatusOr<int> const value) { EXPECT_THAT(value, IsOkAndHolds(42)); });
}

}  // namespace
