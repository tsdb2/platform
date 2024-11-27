#ifndef __TSDB2_COMMON_FLAT_CONTAINER_TESTING_H__
#define __TSDB2_COMMON_FLAT_CONTAINER_TESTING_H__

#include <cstdlib>
#include <deque>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace tsdb2 {
namespace testing {

struct TestKey {
  TestKey(int const field_value) : field(field_value) {}

  friend bool operator==(TestKey const& lhs, TestKey const& rhs) { return lhs.field == rhs.field; }
  friend bool operator!=(TestKey const& lhs, TestKey const& rhs) { return !operator==(lhs, rhs); }

  friend bool operator<(TestKey const& lhs, TestKey const& rhs) { return lhs.field < rhs.field; }
  friend bool operator<=(TestKey const& lhs, TestKey const& rhs) { return lhs.field <= rhs.field; }
  friend bool operator>(TestKey const& lhs, TestKey const& rhs) { return lhs.field > rhs.field; }
  friend bool operator>=(TestKey const& lhs, TestKey const& rhs) { return lhs.field >= rhs.field; }

  template <typename H>
  friend H AbslHashValue(H h, TestKey const& key) {
    return H::combine(std::move(h), key.field);
  }

  template <typename H>
  friend H Tsdb2FingerprintValue(H h, TestKey const& key) {
    return H::Combine(std::move(h), key.field);
  }

  int field;
};

struct OtherTestKey {
  explicit OtherTestKey(int const field_value) : field(field_value) {}
  int field;
};

using TestValue = std::pair<TestKey, std::string>;

struct TestCompare {
  bool operator()(TestKey const& lhs, TestKey const& rhs) const { return lhs.field < rhs.field; }
};

struct ReverseTestCompare {
  bool operator()(TestKey const& lhs, TestKey const& rhs) const { return lhs.field > rhs.field; }
};

struct TransparentTestCompare {
  using is_transparent = void;
  template <typename LHS, typename RHS>
  bool operator()(LHS const& lhs, RHS const& rhs) const {
    return lhs.field < rhs.field;
  }
};

template <typename T>
class TestAllocator {
 public:
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using propagate_on_container_move_assignment = std::true_type;
  using is_always_equal = std::true_type;

  explicit TestAllocator() = default;
  explicit TestAllocator(int const tag) : tag_(tag) {}

  TestAllocator(TestAllocator const&) = default;
  TestAllocator& operator=(TestAllocator const&) = default;
  TestAllocator(TestAllocator&&) noexcept = default;
  TestAllocator& operator=(TestAllocator&&) noexcept = default;

  int tag() const { return tag_; }

  [[nodiscard]] value_type* allocate(size_t const n) {
    return static_cast<value_type*>(::malloc(n * sizeof(value_type)));
  }

  void deallocate(value_type* const ptr, size_t const) { ::free(ptr); }

  template <typename T2>
  bool operator==(TestAllocator<T2> const&) noexcept {
    return true;
  }

  template <typename T2>
  bool operator!=(TestAllocator<T2> const&) noexcept {
    return false;
  }

 private:
  int tag_ = 0;
};

using TestRepresentation = std::deque<TestKey>;
using TestValueRepresentation = std::deque<TestValue>;

template <typename Inner>
class TestKeyMatcher : public ::testing::MatcherInterface<TestKey const&> {
 public:
  using is_gtest_matcher = void;

  explicit TestKeyMatcher(Inner inner) : inner_(std::move(inner)) {}
  ~TestKeyMatcher() override = default;

  bool MatchAndExplain(TestKey const& value,
                       ::testing::MatchResultListener* const listener) const override {
    return ::testing::MatcherCast<int>(inner_).MatchAndExplain(value.field, listener);
  }

  void DescribeTo(std::ostream* const os) const override {
    ::testing::MatcherCast<int>(inner_).DescribeTo(os);
  }

 private:
  Inner inner_;
};

template <typename Inner>
TestKeyMatcher<std::decay_t<Inner>> TestKeyEq(Inner&& inner) {
  return TestKeyMatcher<std::decay_t<Inner>>(std::forward<Inner>(inner));
}

}  // namespace testing
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_FLAT_CONTAINER_TESTING_H__
