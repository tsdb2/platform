#ifndef __TSDB2_TSZ_DISTRIBUTION_H__
#define __TSDB2_TSZ_DISTRIBUTION_H__

#include <cmath>
#include <cstddef>
#include <memory>

#include "absl/status/status.h"
#include "tsz/bucketer.h"

namespace tsz {

// Manages a histogram of sample frequencies. The histogram is conceptually an array of buckets,
// each bucket being an unsigned integer representing the number of samples in that bucket. The
// number and boundaries of the buckets are determined by a `Bucketer`.
//
// Bucketers define a finite number of buckets, but `Distribution` objects keep two extra implicit
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
      : bucketer_(&bucketer), buckets_(new size_t[bucketer_->num_finite_buckets()]) {
    ResetBuckets();
  }

  // Constructs a distribution with the default bucketer.
  explicit Distribution() : Distribution(Bucketer::Default()) {}

  ~Distribution() = default;

  Distribution(Distribution const &other);
  Distribution &operator=(Distribution const &other);

  Distribution(Distribution &&) noexcept = default;
  Distribution &operator=(Distribution &&) noexcept = default;

  void swap(Distribution &other);
  friend void swap(Distribution &lhs, Distribution &rhs) { lhs.swap(rhs); }

  // Returns the bucketer associated to this distribution.
  Bucketer const &bucketer() const { return *bucketer_; }

  // Returns the number of buckets. Equivalent to `bucketer().num_finite_buckets()`.
  size_t num_finite_buckets() const { return bucketer_->num_finite_buckets(); }

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
  [[nodiscard]] bool empty() const { return count_ == 0; }

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

  // Resets all state to an empty distribution.
  void Clear();

 private:
  // Sets all finite buckets to 0 (the underflow and overflow ones are not changed).
  void ResetBuckets();

  // Not owned. Must never be null.
  Bucketer const *bucketer_;

  std::unique_ptr<size_t[]> buckets_;
  size_t underflow_ = 0;
  size_t overflow_ = 0;
  size_t count_ = 0;
  double sum_ = 0;
  double mean_ = 0;
  double ssd_ = 0;
};

}  // namespace tsz

#endif  // __TSDB2_TSZ_DISTRIBUTION_H__
