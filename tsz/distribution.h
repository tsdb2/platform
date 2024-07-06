#ifndef __TSDB2_TSZ_DISTRIBUTION_H__
#define __TSDB2_TSZ_DISTRIBUTION_H__

#include <cmath>
#include <cstddef>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/status.h"

namespace tsdb2 {
namespace tsz {

// Determines the number and boundaries of the buckets of a `Distribution`.
//
// A Bucketer is uniquely identified by four parameters: `width`, `growth_factor`, `scale_factor`,
// and `num_finite_buckets`.
//
// `num_finite_buckets` determines the number of buckets defined by the bucketer. The exclusive
// upper bound of the i-th bucket is calculated as:
//
//   width * i + scale_factor * pow(growth_factor, i - 1)
//
// for any `growth_factor` != 0. If `growth_factor` is zero the upper bound is just `width * i`.
class Bucketer {
 private:
  auto Tie() const { return std::tie(width_, growth_factor_, scale_factor_, num_finite_buckets_); }

 public:
  // Maximum allowed number of buckets defined by a Bucketer. Higher values are clamped.
  static inline size_t constexpr kMaxNumFiniteBuckets = 5000;

  // The following static methods return various types of bucketers.

  static Bucketer const &FixedWidth(double width, size_t num_finite_buckets);
  static Bucketer const &PowersOf(double base);
  static Bucketer const &ScaledPowersOf(double base, double scale_factor, double max);
  static Bucketer const &Custom(double width, double growth_factor, double scale_factor,
                                size_t num_finite_buckets);

  // The default bucketer uses the powers of 4.
  static Bucketer const &Default() { return PowersOf(4); }

  // The empty bucketer defines no buckets at all except for the implicit underflow and overflow
  // ones. `Distribution` objects will still contain information about total number of samples,
  // total sum, and sum of squares.
  static Bucketer const &None() { return Custom(0, 0, 0, 0); }

  double width() const { return width_; }
  double growth_factor() const { return growth_factor_; }
  double scale_factor() const { return scale_factor_; }
  size_t num_finite_buckets() const { return num_finite_buckets_; }

  // Returns the (inclusive) lower bound of the i-th bucket.
  //
  // NOTE: this function doesn't check that `i < num_finite_buckets`, the caller has to do that.
  double lower_bound(size_t const i) const {
    double result = width_ * i;
    if (growth_factor_) {
      result += scale_factor_ * std::pow(growth_factor_, i - 1);
    }
    return result;
  }

  // Returns the (exclusive) upper bound of the i-th bucket.
  //
  // NOTE: this function doesn't check that `i < num_finite_buckets`, the caller has to do that.
  double upper_bound(size_t const i) const { return lower_bound(i + 1); }

  // Performs a binary search over the buckets and retrieves the one where `sample` falls. If the
  // returned index is negative the sample falls in the underflow bucket, while if it's greater than
  // or equal to `num_finite_buckets` it falls in the overflow bucket.
  intptr_t GetBucketFor(double sample) const;

  template <typename H>
  friend H AbslHashValue(H h, Bucketer const &bucketer) {
    return H::combine(std::move(h), bucketer.width_, bucketer.growth_factor_,
                      bucketer.scale_factor_, bucketer.num_finite_buckets_);
  }

  friend bool operator==(Bucketer const &lhs, Bucketer const &rhs) {
    return lhs.Tie() == rhs.Tie();
  }

  friend bool operator!=(Bucketer const &lhs, Bucketer const &rhs) { return !operator==(lhs, rhs); }

 protected:
  explicit Bucketer(double const width, double const growth_factor, double const scale_factor,
                    size_t const num_finite_buckets)
      : width_(width),
        growth_factor_(growth_factor),
        scale_factor_(scale_factor),
        num_finite_buckets_(num_finite_buckets) {}

 private:
  // Moves and copies not allowed because bucketers are canonical.
  Bucketer(Bucketer const &) = delete;
  Bucketer &operator=(Bucketer const &) = delete;
  Bucketer(Bucketer &&) = delete;
  Bucketer &operator=(Bucketer &&) = delete;

  static Bucketer const &GetCanonicalBucketer(double width, double growth_factor,
                                              double scale_factor, size_t num_finite_buckets);

  double width_;
  double growth_factor_;
  double scale_factor_;
  size_t num_finite_buckets_;
};

// Manages a histogram of sample frequencies. The histogram is conceptually an array of buckets,
// each bucket being an unsigned integer representing the number of samples in that bucket. The
// number and boundaries of the buckets are determined by a Bucketer.
//
// Bucketers define a finite number of buckets, but Distribution objects keep two extra implicit
// buckets: samples that fall below the lowest bucket are recorded in an underflow bucket, and
// samples falling above the highest are recorded in an overflow bucket.
//
// Distributions also keep track of a few stats related to the recorded samples, namely: their sum,
// count, mean, and sum of squared deviations from the mean. The latter is used to calculate the
// mean with the least loss of precision thanks to the method of provisional means (see
// http://www.pmean.com/04/ProvisionalMeans.html for more info).
class Distribution {
 public:
  explicit Distribution(Bucketer const &bucketer)
      : bucketer_(&bucketer), buckets_(bucketer.num_finite_buckets(), 0) {
    buckets_.shrink_to_fit();
  }

  // Constructs a distribution with the default bucketer.
  explicit Distribution() : Distribution(Bucketer::Default()) {}

  Distribution(Distribution const &) = default;
  Distribution &operator=(Distribution const &) = default;
  Distribution(Distribution &&) noexcept = default;
  Distribution &operator=(Distribution &&) noexcept = default;

  // Returns the bucketer associated to this distribution.
  Bucketer const &bucketer() const { return *bucketer_; }

  // Returns the number of buckets. Equivalent to `bucketer().num_finite_buckets()`.
  size_t num_finite_buckets() const { return buckets_.size(); }

  // Returns the number of samples in the i-th finite bucket. Undefined behavior if i is greater
  // than or equal to `num_finite_buckets`.
  size_t bucket(size_t const i) const { return buckets_[i]; }

  // Returns the number of samples in the underflow bucket.
  size_t underflow() const { return underflow_; }

  // Returns the number of samples in the overflow bucket.
  size_t overflow() const { return overflow_; }

  // Returns the sum of all samples.
  double sum() const { return sum_; }

  // Returns the sum of the squared deviations from the mean. `Distribution` keeps track of this
  // information in order to calculate the mean, variance, and standard deviation.
  double sum_of_squared_deviations() const { return ssd_; }

  // Returns the number of samples, including the ones in the underflow and overflow buckets.
  size_t count() const { return count_; }

  // True iff there are no samples (i.e. `count() == 0`).
  bool empty() const { return count_ == 0; }

  // Various stats about the recorded samples.
  double mean() const { return mean_; }
  double variance() const { return ssd_ / count_; }
  double stddev() const { return std::sqrt(variance()); }

  // Records a sample in the corresponding bucket.
  void Record(double const sample) { RecordMany(sample, 1); }

  // Records a sample `times` times.
  void RecordMany(double sample, size_t times);

  // Adds `other` to this distribution. The two distributions must have the same bucketer, otherwise
  // the operation will fail with an error status.
  absl::Status Add(Distribution const &other);

 private:
  intptr_t FindBucket(double const sample) const { return bucketer_->GetBucketFor(sample); }

  // Not owned. Must never be null.
  Bucketer const *bucketer_;

  std::vector<size_t> buckets_;
  size_t underflow_ = 0;
  size_t overflow_ = 0;
  size_t count_ = 0;
  double sum_ = 0;
  double mean_ = 0;
  double ssd_ = 0;
};

}  // namespace tsz
}  // namespace tsdb2

#endif  // __TSDB2_TSZ_DISTRIBUTION_H__
