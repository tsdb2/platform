#include "common/distribution.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "absl/base/attributes.h"
#include "absl/container/node_hash_set.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "common/no_destructor.h"

namespace tsdb2 {
namespace common {

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
      std::max(1.0, std::min(1 + ceil((log(max) - log(scale_factor)) / log(base)),
                             static_cast<double>(kMaxNumFiniteBuckets)));
  return GetCanonicalBucketer(/*width=*/0.0, base, scale_factor, lround(num_finite_buckets));
}

Bucketer const& Bucketer::Custom(double const width, double const growth_factor,
                                 double const scale_factor, size_t const num_finite_buckets) {
  return GetCanonicalBucketer(width, growth_factor, scale_factor, num_finite_buckets);
}

intptr_t Bucketer::GetBucketFor(double const sample) const {
  size_t i = 0, j = num_finite_buckets_ + 1;
  while (j > i) {
    size_t k = i + ((j - i) >> 1);
    double const l = lower_bound(k);
    if (sample < l) {
      j = k;
    } else if (sample > l) {
      i = k + 1;
    } else {
      return k;
    }
  }
  return static_cast<intptr_t>(i) - 1;
}

namespace {

class BucketerImpl final : public Bucketer {
 public:
  explicit BucketerImpl(double const width, double const growth_factor, double const scale_factor,
                        size_t const num_finite_buckets)
      : Bucketer(width, growth_factor, scale_factor, num_finite_buckets) {}
};

}  // namespace

Bucketer const& Bucketer::GetCanonicalBucketer(double const width, double const growth_factor,
                                               double const scale_factor,
                                               size_t const num_finite_buckets) {
  static NoDestructor<absl::node_hash_set<BucketerImpl>> bucketers;
  ABSL_CONST_INIT static absl::Mutex mutex{absl::kConstInit};
  absl::MutexLock lock{&mutex};
  auto const [it, _] = bucketers->emplace(width, growth_factor, scale_factor,
                                          std::min(num_finite_buckets, kMaxNumFiniteBuckets));
  return *it;
}

void Distribution::RecordMany(double const sample, size_t const times) {
  auto const i = FindBucket(sample);
  if (i < 0) {
    underflow_ += times;
  } else if (i >= bucketer_->num_finite_buckets()) {
    overflow_ += times;
  } else {
    buckets_[i] += times;
  }
  count_ += times;
  sum_ += sample * times;
  double const dev = sample - mean_;
  double const new_mean = mean_ + times * dev / count_;
  ssd_ += times * dev * (sample - new_mean);
  mean_ = new_mean;
}

namespace {

inline double Square(double const x) { return x * x; }

}  // namespace

absl::Status Distribution::Add(Distribution const& other) {
  if (other.bucketer_ != bucketer_) {
    return absl::InvalidArgumentError("incompatible bucketers");
  }
  for (size_t i = 0; i < buckets_.size(); ++i) {
    buckets_[i] += other.buckets_[i];
  }
  underflow_ += other.underflow_;
  overflow_ += other.overflow_;
  double const old_count = count_;
  count_ += other.count_;
  sum_ += other.sum_;
  double const old_mean = mean_;
  if (count_ > 0) {
    mean_ = sum_ / count_;
  } else {
    mean_ = 0;
  }
  ssd_ += other.ssd_ + old_count * Square(mean_ - old_mean) +
          other.count_ * Square(mean_ - other.mean_);
  return absl::OkStatus();
}

}  // namespace common
}  // namespace tsdb2
