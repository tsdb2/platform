#include "common/fingerprint.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace tsdb2 {
namespace common {
namespace internal {

void FingerprintState::Add(uint64_t const k) {
  if (step_) {
    Step(k);
  } else {
    k1_ = k;
    step_ = true;
  }
}

void FingerprintState::Add(uint64_t const* const ks, size_t const count) {
  if (count == 0) {
    return;
  }
  size_t i = 0;
  if (step_) {
    Step(ks[i++]);
  }
  while (i < count - 1) {
    k1_ = ks[i++];
    Step(ks[i++]);
  }
  if (i < count) {
    k1_ = ks[i];
    step_ = true;
  }
}

void FingerprintState::AddBytes(void const* const data, size_t const length) {
  size_t const num_words = length >> 3;
  Add(reinterpret_cast<uint64_t const*>(data), num_words);
  size_t const remainder = length & 7;
  if (remainder > 0) {
    uint8_t const* const bytes = reinterpret_cast<uint8_t const*>(data);
    uint64_t k = 0;
    std::memcpy(&k, bytes + (num_words << 3), remainder);
    Add(k);
  }
}

uint64_t FingerprintState::Finish() && {
  if (step_) {
    k1_ *= c1_;
    k1_ = (k1_ << 31) | (k1_ >> 33);
    k1_ *= c2_;
    h1_ ^= k1_;
    total_length_ += 8;
  }

  h1_ ^= total_length_;
  h2_ ^= total_length_;
  h1_ += h2_;
  h2_ += h1_;

  h1_ ^= h1_ >> 33;
  h1_ *= c3_;
  h1_ ^= h1_ >> 33;
  h1_ *= c4_;
  h1_ ^= h1_ >> 33;

  h2_ ^= h2_ >> 33;
  h2_ *= c3_;
  h2_ ^= h2_ >> 33;
  h2_ *= c4_;
  h2_ ^= h2_ >> 33;

  h1_ += h2_;
  h2_ += h1_;

  return h1_ ^ h2_;
}

void FingerprintState::Step(uint64_t k2) {
  k1_ *= c1_;
  k1_ = (k1_ << 31) | (k1_ >> 33);
  k1_ *= c2_;

  h1_ ^= k1_;
  h1_ = (h1_ << 27) | (h1_ >> 37);
  h1_ += h2_;
  h1_ = h1_ * 5 + 0x52dce729;

  k2 *= c2_;
  k2 = (k2 << 33) | (k2 >> 31);
  k2 *= c1_;

  h2_ ^= k2;
  h2_ = (h2_ << 31) | (h2_ >> 33);
  h2_ += h1_;
  h2_ = h2_ * 5 + 0x38495ab5;

  step_ = false;
  total_length_ += 16;
}

}  // namespace internal
}  // namespace common
}  // namespace tsdb2
