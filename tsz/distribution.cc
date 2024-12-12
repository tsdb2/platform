#include "tsz/distribution.h"

#include <algorithm>
#include <cstddef>
#include <memory>

#include "absl/status/status.h"

namespace tsz {

Distribution::Distribution(Distribution const &other)
    : bucketer_(other.bucketer_),
      buckets_(new size_t[bucketer_->num_finite_buckets()]),
      underflow_(other.underflow_),
      overflow_(other.overflow_),
      count_(other.count_),
      sum_(other.sum_),
      mean_(other.mean_),
      ssd_(other.ssd_) {
  for (size_t i = 0; i < num_finite_buckets(); ++i) {
    buckets_[i] = other.buckets_[i];
  }
}

Distribution &Distribution::operator=(Distribution const &other) {
  if (this == &other) {
    return *this;
  }
  bucketer_ = other.bucketer_;
  buckets_ = std::make_unique<size_t[]>(bucketer_->num_finite_buckets());
  for (size_t i = 0; i < bucketer_->num_finite_buckets(); ++i) {
    buckets_[i] = other.buckets_[i];
  }
  underflow_ = other.underflow_;
  overflow_ = other.overflow_;
  count_ = other.count_;
  sum_ = other.sum_;
  mean_ = other.mean_;
  ssd_ = other.ssd_;
  return *this;
}

void Distribution::swap(Distribution &other) {
  if (this == &other) {
    return;
  }
  std::swap(buckets_, other.buckets_);
  std::swap(underflow_, other.underflow_);
  std::swap(overflow_, other.overflow_);
  std::swap(count_, other.count_);
  std::swap(sum_, other.sum_);
  std::swap(mean_, other.mean_);
  std::swap(ssd_, other.ssd_);
}

void Distribution::RecordMany(double const sample, size_t const times) {
  auto const i = bucketer_->GetBucketFor(sample);
  if (i < 0) {
    underflow_ += times;
  } else if (i >= num_finite_buckets()) {
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

absl::Status Distribution::Add(Distribution const &other) {
  if (other.bucketer_ != bucketer_) {
    return absl::InvalidArgumentError("incompatible bucketers");
  }
  for (size_t i = 0; i < num_finite_buckets(); ++i) {
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

void Distribution::Clear() {
  ResetBuckets();
  underflow_ = 0;
  overflow_ = 0;
  count_ = 0;
  sum_ = 0;
  mean_ = 0;
  ssd_ = 0;
}

void Distribution::ResetBuckets() {
  for (size_t i = 0; i < num_finite_buckets(); ++i) {
    buckets_[i] = 0;
  }
}

}  // namespace tsz
