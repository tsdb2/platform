#include "tsz/bucketer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "absl/hash/hash.h"
#include "common/lock_free_hash_set.h"
#include "common/no_destructor.h"

namespace tsz {

Bucketer const& Bucketer::FixedWidth(double const width, size_t const num_finite_buckets) {
  return GetCanonicalBucketer(width, /*growth_factor=*/0.0, /*scale_factor=*/1.0,
                              num_finite_buckets);
}

Bucketer const& Bucketer::PowersOf(double const base) {
  return ScaledPowersOf(base, /*scale_factor=*/1.0, std::numeric_limits<uint32_t>::max());
}

Bucketer const& Bucketer::ScaledPowersOf(double const base, double const scale_factor,
                                         double const max) {
  double const num_finite_buckets =
      std::max(1.0, 1 + std::ceil((log(max) - std::log(scale_factor)) / std::log(base)));
  return GetCanonicalBucketer(/*width=*/0.0, base, scale_factor, std::lround(num_finite_buckets));
}

Bucketer const& Bucketer::Custom(double const width, double const growth_factor,
                                 double const scale_factor, size_t const num_finite_buckets) {
  return GetCanonicalBucketer(width, growth_factor, scale_factor, num_finite_buckets);
}

double Bucketer::lower_bound(int const i) const {
  double result = width_ * i;
  if (growth_factor_ != 0) {
    result += scale_factor_ * std::pow(growth_factor_, i - 1);
  }
  return result;
}

int Bucketer::GetBucketFor(double const sample) const {
  int i = 0;
  int j = num_finite_buckets_ + 1;
  while (j > i) {
    int k = i + ((j - i) >> 1);
    double const l = lower_bound(k);
    if (sample < l) {
      j = k;
    } else if (sample > l) {
      i = k + 1;
    } else {
      return k;
    }
  }
  return i - 1;
}

namespace {

class BucketerImpl final : public Bucketer {
 public:
  explicit BucketerImpl(double const width, double const growth_factor, double const scale_factor,
                        size_t const num_finite_buckets)
      : Bucketer(width, growth_factor, scale_factor, num_finite_buckets) {}
};

struct CompareBucketers {
  bool operator()(Bucketer const& lhs, Bucketer const& rhs) const { return lhs.tie() == rhs.tie(); }
};

}  // namespace

Bucketer const& Bucketer::GetCanonicalBucketer(double const width, double const growth_factor,
                                               double const scale_factor,
                                               size_t const num_finite_buckets) {
  static tsdb2::common::NoDestructor<
      tsdb2::common::lock_free_hash_set<BucketerImpl, absl::Hash<Bucketer>, CompareBucketers>>
      bucketers;
  auto const [it, _] = bucketers->emplace(width, growth_factor, scale_factor,
                                          std::min(num_finite_buckets, kMaxNumFiniteBuckets));
  return *it;
}

}  // namespace tsz
