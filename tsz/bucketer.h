#ifndef __TSDB2_TSZ_BUCKETER_H__
#define __TSDB2_TSZ_BUCKETER_H__

#include <cstddef>
#include <tuple>
#include <utility>

#include "absl/strings/str_format.h"

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

  auto tie() const { return std::tie(width_, growth_factor_, scale_factor_, num_finite_buckets_); }

  double width() const { return width_; }
  double growth_factor() const { return growth_factor_; }
  double scale_factor() const { return scale_factor_; }
  size_t num_finite_buckets() const { return num_finite_buckets_; }

  // Returns the (inclusive) lower bound of the i-th bucket.
  //
  // NOTE: this function doesn't check that `i` is in the range `[0, num_finite_buckets)`, the
  // caller has to do that.
  double lower_bound(int i) const;

  // Returns the (exclusive) upper bound of the i-th bucket.
  //
  // NOTE: this function doesn't check that `i` is in the range `[0, num_finite_buckets)`, the
  // caller has to do that.
  double upper_bound(int const i) const { return lower_bound(i + 1); }

  // Performs a binary search over the buckets and retrieves the one where `sample` falls. If the
  // returned index is negative the sample falls in the underflow bucket, while if it's greater than
  // or equal to `num_finite_buckets` it falls in the overflow bucket.
  int GetBucketFor(double sample) const;

  template <typename Sink>
  friend void AbslStringify(Sink &sink, Bucketer const &bucketer) {
    absl::Format(&sink, "(%g, %g, %g, %u)", bucketer.width_, bucketer.growth_factor_,
                 bucketer.scale_factor_, bucketer.num_finite_buckets_);
  }

  template <typename H>
  friend H AbslHashValue(H h, Bucketer const &bucketer) {
    return H::combine(std::move(h), bucketer.width_, bucketer.growth_factor_,
                      bucketer.scale_factor_, bucketer.num_finite_buckets_);
  }

  // Bucketers are canonical, so we can simply compare their memory addresses.
  friend bool operator==(Bucketer const &lhs, Bucketer const &rhs) { return &lhs == &rhs; }
  friend bool operator!=(Bucketer const &lhs, Bucketer const &rhs) { return &lhs != &rhs; }

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

}  // namespace tsz

#endif  // __TSDB2_TSZ_BUCKETER_H__
