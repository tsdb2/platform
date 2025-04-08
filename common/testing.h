#ifndef __TSDB2_COMMON_TESTING_H__
#define __TSDB2_COMMON_TESTING_H__

#include <algorithm>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "absl/status/status.h"           // IWYU pragma: export
#include "absl/status/status_matchers.h"  // IWYU pragma: export
#include "absl/status/statusor.h"         // IWYU pragma: export
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "io/fd.h"

namespace testing {

// Returns the path of the test temp directory, which is provided in the `TEST_TMPDIR` environment
// variable. Falls back to `/tmp/` if the variable is not set.
std::string GetTestTmpDir();

// Manages a temporary file created with `mkstemp` inside the test temp directory returned by
// `GetTestTmpDir`. Closes and deletes the file automatically upon destruction.
class TestTempFile {
 public:
  static absl::StatusOr<TestTempFile> Create(std::string_view base_name);

  ~TestTempFile();

  TestTempFile(TestTempFile&&) noexcept = default;
  TestTempFile& operator=(TestTempFile&&) noexcept = default;

  void swap(TestTempFile& other) noexcept {
    using std::swap;  // ensure ADL
    swap(path_, other.path_);
    swap(fd_, other.fd_);
  }

  friend void swap(TestTempFile& lhs, TestTempFile& rhs) noexcept { lhs.swap(rhs); }

  std::string_view path() const { return path_; }

  tsdb2::io::FD& fd() { return fd_; }
  tsdb2::io::FD const& fd() const { return fd_; }

  // Closes the file and empties the file descriptor. No-op if already closed.
  void Close() { fd_.Close(); }

 private:
  static std::string MakeTempFileTemplate(std::string_view base_name);

  explicit TestTempFile(std::string path, tsdb2::io::FD fd)
      : path_(std::move(path)), fd_(std::move(fd)) {}

  TestTempFile(TestTempFile const&) = delete;
  TestTempFile& operator=(TestTempFile const&) = delete;

  std::string path_;
  tsdb2::io::FD fd_;
};

// GoogleTest's `Pointee` matcher doesn't work with smart pointers, so we deploy our own.
template <typename Pointer>
class Pointee2Impl : public MatcherInterface<Pointer const&> {
 public:
  using is_gtest_matcher = void;

  using Pointee = decltype(*std::declval<std::decay_t<Pointer>>());

  template <typename Inner>
  explicit Pointee2Impl(Inner&& inner)
      : inner_(SafeMatcherCast<Pointee>(std::forward<Inner>(inner))) {}

  ~Pointee2Impl() = default;

  void DescribeTo(std::ostream* const os) const override {
    *os << "points to a value that ";
    inner_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* const os) const override {
    *os << "points to a value that ";
    inner_.DescribeNegationTo(os);
  }

  bool MatchAndExplain(Pointer const& value, MatchResultListener* const listener) const override {
    *listener << "points to a value that ";
    return inner_.MatchAndExplain(*value, listener);
  }

 private:
  Pointee2Impl(Pointee2Impl const&) = delete;
  Pointee2Impl& operator=(Pointee2Impl const&) = delete;
  Pointee2Impl(Pointee2Impl&&) = delete;
  Pointee2Impl& operator=(Pointee2Impl&&) = delete;

  Matcher<Pointee> inner_;
};

template <typename Inner>
class Pointee2 {
 public:
  explicit Pointee2(Inner inner) : inner_(std::move(inner)) {}

  template <typename Pointer>
  operator Matcher<Pointer>() const {  // NOLINT(google-explicit-constructor)
    return Matcher<Pointer>(new Pointee2Impl<std::decay_t<Pointer>>(inner_));
  }

 private:
  std::decay_t<Inner> inner_;
};

}  // namespace testing

// Macros for testing the results of functions that return absl::Status or absl::StatusOr<T> (for
// any type T).
#define EXPECT_OK(expression) EXPECT_THAT((expression), ::absl_testing::IsOk())
#define ASSERT_OK(expression) ASSERT_THAT((expression), ::absl_testing::IsOk())

#endif  // __TSDB2_COMMON_TESTING_H__
