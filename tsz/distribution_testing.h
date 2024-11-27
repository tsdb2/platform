#ifndef __TSDB2_TSZ_DISTRIBUTION_TESTING_H__
#define __TSDB2_TSZ_DISTRIBUTION_TESTING_H__

#include <cstddef>
#include <ostream>
#include <utility>

#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tsz/bucketer.h"
#include "tsz/distribution.h"

namespace tsz {
namespace testing {

// googletest matcher matching empty `Distribution`s.
//
// Example:
//
//   tsz::Distribution d;
//   EXPECT_THAT(d, EmptyDistribution());
//
class EmptyDistribution : public ::testing::MatcherInterface<Distribution const&> {
 public:
  using is_gtest_matcher = void;

  explicit EmptyDistribution() = default;

  void DescribeTo(std::ostream* const os) const override { *os << "is an empty tsz distribution"; }

  void DescribeNegationTo(std::ostream* const os) const override {
    *os << "is a non-empty tsz distribution";
  }

  bool MatchAndExplain(Distribution const& value,
                       ::testing::MatchResultListener* const listener) const override {
    if (value.empty()) {
      *listener << "is empty";
      return true;
    } else {
      *listener << "is not empty";
      return false;
    }
  }
};

// googletest matcher matching `Distribution`s with a given `Bucketer`.
//
// Example:
//
//   auto const& bucketer = Bucketer::PowersOf(2);
//   tsz::Distribution d{bucketer};
//   EXPECT_THAT(d, DistributionBucketerIs(bucketer));
//
class DistributionBucketerIs : public ::testing::MatcherInterface<Distribution const&> {
 public:
  using is_gtest_matcher = void;

  explicit DistributionBucketerIs(Bucketer const& bucketer) : bucketer_(bucketer) {}

  void DescribeTo(std::ostream* const os) const override {
    *os << "is a tsz distribution whose bucketer is " << absl::StrCat(bucketer_);
  }

  void DescribeNegationTo(std::ostream* const os) const override {
    *os << "is a tsz distribution whose bucketer is not " << absl::StrCat(bucketer_);
  }

  bool MatchAndExplain(Distribution const& value,
                       ::testing::MatchResultListener* const listener) const override {
    auto const& bucketer = value.bucketer();
    *listener << "whose bucketer is " << absl::StrCat(bucketer);
    return bucketer == bucketer_;
  }

 private:
  Bucketer const& bucketer_;
};

// googletest matcher matching `Distribution`s with a given sum.
//
// Example:
//
//   tsz::Distribution d;
//   d.RecordMany(12, 2);
//   d.Record(34);
//   EXPECT_THAT(d, DistributionSumIs(58));
//
template <typename Inner>
class DistributionSumIs : public ::testing::MatcherInterface<Distribution const&> {
 public:
  using is_gtest_matcher = void;

  explicit DistributionSumIs(Inner inner) : inner_(std::move(inner)) {}

  void DescribeTo(std::ostream* const os) const override {
    *os << "is a tsz distribution whose sum ";
    ::testing::SafeMatcherCast<double>(inner_).DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* const os) const override {
    *os << "is a tsz distribution whose sum ";
    ::testing::SafeMatcherCast<double>(inner_).DescribeNegationTo(os);
  }

  bool MatchAndExplain(Distribution const& value,
                       ::testing::MatchResultListener* const listener) const override {
    *listener << "whose sum ";
    return ::testing::SafeMatcherCast<double>(inner_).MatchAndExplain(value.sum(), listener);
  }

 private:
  Inner inner_;
};

// googletest matcher matching `Distribution`s with a given number of samples.
//
// Example:
//
//   tsz::Distribution d;
//   d.RecordMany(12, 2);
//   d.Record(34);
//   EXPECT_THAT(d, DistributionCountIs(3));
//
template <typename Inner>
class DistributionCountIs : public ::testing::MatcherInterface<Distribution const&> {
 public:
  using is_gtest_matcher = void;

  explicit DistributionCountIs(Inner inner) : inner_(std::move(inner)) {}

  void DescribeTo(std::ostream* const os) const override {
    *os << "is a tsz distribution whose count ";
    ::testing::SafeMatcherCast<size_t>(inner_).DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* const os) const override {
    *os << "is a tsz distribution whose count ";
    ::testing::SafeMatcherCast<size_t>(inner_).DescribeNegationTo(os);
  }

  bool MatchAndExplain(Distribution const& value,
                       ::testing::MatchResultListener* const listener) const override {
    *listener << "whose count ";
    return ::testing::SafeMatcherCast<size_t>(inner_).MatchAndExplain(value.count(), listener);
  }

 private:
  Inner inner_;
};

// googletest matcher matching `Distribution`s with a given sum and count.
//
// Example:
//
//   tsz::Distribution d;
//   d.RecordMany(12, 2);
//   d.Record(34);
//   EXPECT_THAT(d, DistributionSumAndCountAre(58, 3));
//
template <typename SumMatcher, typename CountMatcher>
class DistributionSumAndCountAre : public ::testing::MatcherInterface<Distribution const&> {
 public:
  using is_gtest_matcher = void;

  explicit DistributionSumAndCountAre(SumMatcher sum_matcher, CountMatcher count_matcher)
      : sum_matcher_(std::move(sum_matcher)), count_matcher_(std::move(count_matcher)) {}

  void DescribeTo(std::ostream* const os) const override {
    *os << "is a tsz distribution whose sum ";
    ::testing::SafeMatcherCast<double>(sum_matcher_).DescribeTo(os);
    *os << " and whose count ";
    ::testing::SafeMatcherCast<size_t>(count_matcher_).DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* const os) const override {
    *os << "is a tsz distribution whose sum ";
    ::testing::SafeMatcherCast<double>(sum_matcher_).DescribeNegationTo(os);
    *os << " and whose count ";
    ::testing::SafeMatcherCast<size_t>(count_matcher_).DescribeNegationTo(os);
  }

  bool MatchAndExplain(Distribution const& value,
                       ::testing::MatchResultListener* const listener) const override {
    *listener << "whose sum ";
    if (!::testing::SafeMatcherCast<double>(sum_matcher_).MatchAndExplain(value.sum(), listener)) {
      return false;
    }
    *listener << " and whose count ";
    if (!::testing::SafeMatcherCast<size_t>(count_matcher_)
             .MatchAndExplain(value.count(), listener)) {
      return false;
    }
    return true;
  }

 private:
  SumMatcher sum_matcher_;
  CountMatcher count_matcher_;
};

// googletest matcher matching `Distribution`s with a given mean.
//
// Example:
//
//   tsz::Distribution d;
//   d.RecordMany(12, 2);
//   d.Record(34);
//   EXPECT_THAT(d, DistributionMeanIs(DoubleNear(19.33, 0.01)));
//
template <typename Inner>
class DistributionMeanIs : public ::testing::MatcherInterface<Distribution const&> {
 public:
  using is_gtest_matcher = void;

  explicit DistributionMeanIs(Inner inner) : inner_(std::move(inner)) {}

  void DescribeTo(std::ostream* const os) const override {
    *os << "is a tsz distribution whose mean ";
    ::testing::SafeMatcherCast<double>(inner_).DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* const os) const override {
    *os << "is a tsz distribution whose mean ";
    ::testing::SafeMatcherCast<double>(inner_).DescribeNegationTo(os);
  }

  bool MatchAndExplain(Distribution const& value,
                       ::testing::MatchResultListener* const listener) const override {
    *listener << "whose mean ";
    return ::testing::SafeMatcherCast<double>(inner_).MatchAndExplain(value.mean(), listener);
  }

 private:
  Inner inner_;
};

}  // namespace testing
}  // namespace tsz

#endif  // __TSDB2_TSZ_DISTRIBUTION_TESTING_H__
