#ifndef __TSDB2_IO_BUFFER_TESTING_H__
#define __TSDB2_IO_BUFFER_TESTING_H__

#include <cstdint>
#include <ostream>
#include <string_view>
#include <utility>

#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "io/buffer.h"

namespace tsdb2 {
namespace testing {
namespace io {

// GoogleTest matcher to reinterpret the content of a `Buffer` as the specified type.
//
// Example:
//
//   struct Foo {
//     int value;
//     bool flag;
//   };
//
//   Foo const foo{ .value = 42, .flag = true };
//   tsdb2::io::Buffer buffer{&foo, sizeof(Foo)};
//
//   EXPECT_THAT(buffer, BufferAs<Foo>(AllOf(Field(&Foo::value, 42), Field(&Foo::flag, true))));
//
//
template <typename Value, typename Inner>
class BufferAsMatcher : public ::testing::MatcherInterface<tsdb2::io::Buffer const&> {
 public:
  using is_gtest_matcher = void;

  explicit BufferAsMatcher(Inner inner) : inner_(std::move(inner)) {}

  void DescribeTo(std::ostream* const os) const override {
    *os << "is a buffer containing a value that ";
    ::testing::SafeMatcherCast<Value>(inner_).DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* const os) const override {
    *os << "is a buffer containing a value that ";
    ::testing::SafeMatcherCast<Value>(inner_).DescribeNegationTo(os);
  }

  bool MatchAndExplain(tsdb2::io::Buffer const& buffer,
                       ::testing::MatchResultListener* const listener) const override {
    *listener << "contains a value that ";
    return ::testing::SafeMatcherCast<Value>(inner_).MatchAndExplain(buffer.as<Value>(), listener);
  }

 private:
  Inner inner_;
};

template <typename Value, typename Inner>
BufferAsMatcher<Value, Inner> BufferAs(Inner inner) {
  return BufferAsMatcher<Value, Inner>(std::move(inner));
}

// GoogleTest matcher to reinterpret the content of a `Buffer` as an array of the specified type.
//
// Example:
//
//   struct Foo {
//     int value;
//     bool flag;
//   };
//
//   Foo const foos[2] = {
//       { .value = 42, .flag = true },
//       { .value = 43, .flag = false },
//   };
//   tsdb2::io::Buffer buffer{foos, sizeof(foo)};
//
//   EXPECT_THAT(buffer, BufferAsArray<Foo>(ElementsAre(
//       AllOf(Field(&Foo::value, 42), Field(&Foo::flag, true)),
//       AllOf(Field(&Foo::value, 43), Field(&Foo::flag, false)))));
//
//
template <typename Value, typename Inner>
class BufferAsArrayMatcher : public ::testing::MatcherInterface<tsdb2::io::Buffer const&> {
 public:
  using is_gtest_matcher = void;

  explicit BufferAsArrayMatcher(Inner inner) : inner_(std::move(inner)) {}

  void DescribeTo(std::ostream* const os) const override {
    *os << "is a buffer containing an array of values that ";
    ::testing::SafeMatcherCast<absl::Span<Value const>>(inner_).DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* const os) const override {
    *os << "is a buffer containing an array of values that ";
    ::testing::SafeMatcherCast<absl::Span<Value const>>(inner_).DescribeNegationTo(os);
  }

  bool MatchAndExplain(tsdb2::io::Buffer const& buffer,
                       ::testing::MatchResultListener* const listener) const override {
    if (buffer.size() % sizeof(Value) != 0) {
      *listener << "whose size is not a multiple of sizeof(Value)";
      return false;
    }
    *listener << "contains an array of values that ";
    return ::testing::SafeMatcherCast<absl::Span<Value const>>(inner_).MatchAndExplain(
        buffer.span<Value>(), listener);
  }

 private:
  Inner inner_;
};

template <typename Value, typename Inner>
BufferAsArrayMatcher<Value, Inner> BufferAsArray(Inner inner) {
  return BufferAsArrayMatcher<Value, Inner>(std::move(inner));
}

// A specialization of `BufferAsArray` that reinterprets the content of a `Buffer` as an array of
// `uint8_t`. You can use it to match the bytes contained in the buffer.
template <typename Inner>
BufferAsArrayMatcher<uint8_t, Inner> BufferAsBytes(Inner inner) {
  return BufferAsArrayMatcher<uint8_t, Inner>(std::move(inner));
}

// GoogleTest matcher to reinterpret the content of a `Buffer` as a string.
//
// Example:
//
//   std::string_view const foo = "sator arepo tenet opera rotas";
//   tsdb2::io::Buffer buffer{foo.data, foo.size() + 1};
//   EXPECT_THAT(buffer, BufferAsString("sator arepo tenet opera rotas"));
//
template <typename Inner>
class BufferAsStringMatcher : public ::testing::MatcherInterface<tsdb2::io::Buffer const&> {
 public:
  using is_gtest_matcher = void;

  explicit BufferAsStringMatcher(Inner inner) : inner_(std::move(inner)) {}

  void DescribeTo(std::ostream* const os) const override {
    *os << "is a buffer containing a string that ";
    ::testing::SafeMatcherCast<std::string_view>(inner_).DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* const os) const override {
    *os << "is a buffer containing a string that ";
    ::testing::SafeMatcherCast<std::string_view>(inner_).DescribeNegationTo(os);
  }

  bool MatchAndExplain(tsdb2::io::Buffer const& buffer,
                       ::testing::MatchResultListener* const listener) const override {
    *listener << "is a buffer containing a string that ";
    return ::testing::SafeMatcherCast<std::string_view>(inner_).MatchAndExplain(
        std::string_view(buffer.as_char_array(), buffer.size()), listener);
  }

 private:
  Inner inner_;
};

template <typename Inner>
BufferAsStringMatcher<Inner> BufferAsString(Inner inner) {
  return BufferAsStringMatcher<Inner>(std::move(inner));
}

}  // namespace io
}  // namespace testing
}  // namespace tsdb2

#endif  // __TSDB2_IO_BUFFER_TESTING_H__
