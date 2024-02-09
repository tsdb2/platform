// This header provides a generic fingerprinting framework similar to Abseil's hashing framework.
// The main difference between hashing and fingerprinting is that the latter uses a predefined
// constant seed, so fingerprints never change. By contrast, Abseil's hashing framework uses an
// internal seed value that is calculated randomly at every process restart and cannot be changed by
// the user.
//
// WARNING: because of the above, fingerprinting is NOT suitable for use in hash table data
// structures. Doing so would expose them to DoS attacks because, knowing the seed, an attacker can
// precalculate large amounts of collisions and flood the hash table with colliding data, degrading
// its performance and turning it into a list. Note that finding collisions in a hash table with a
// given hash algorithm is easier than finding collisions in the hash algorithm itself because the
// hash table has limited size and therefore uses some modulo of a hash value; that significantly
// restricts the space across which an attacker needs to find collisions.
//
// Despite the above weakness, fingerprinting is still useful to generate deterministic
// pseudo-random numbers based on some data. TSDB2 uses it to avoid RPC spikes by scattering RPC
// fire times across a time window, for example.
//
// This fingerprinting framework uses a 32-bit Murmur 3 hash, which is non-cryptographic but very
// fast. See https://en.wikipedia.org/wiki/MurmurHash for more information. To improve speed even
// further, we use the algorithm in a way that avoids all unaligned reads. Thanks to that we can
// read the input data 32 bits at a time rather than 1 byte at a time.
//
// Similarly to Abseil's hashing framework, custom types can be made hashable by adding a friend
// function called `Tsdb2FingerprintValue`, like in the following example:
//
//   class Point {
//    public:
//     explicit Point(double const x, double const y) : x_(x), y_(y) {}
//
//     template <typename State>
//     friend State Tsdb2FingerprintValue(State state, Point const& point) {
//       return State::Combine(std::move(state), point.x_, point.y_);
//     }
//
//    private:
//     double x_;
//     double y_;
//   };
//
#ifndef __TSDB2_COMMON_FINGERPRINT_H__
#define __TSDB2_COMMON_FINGERPRINT_H__

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "absl/types/span.h"
#include "common/flat_map.h"
#include "common/flat_set.h"

template <typename State, typename Integer>
constexpr State Tsdb2FingerprintValue(State state, Integer value,
                                      std::enable_if_t<std::is_integral_v<Integer>, bool> = true);

template <typename State, typename Float>
State Tsdb2FingerprintValue(State state, Float value,
                            std::enable_if_t<std::is_floating_point_v<Float>, bool> = true);

template <typename State, typename Pointee>
constexpr State Tsdb2FingerprintValue(State state, Pointee* value);

template <typename State>
constexpr State Tsdb2FingerprintValue(State state, std::nullptr_t);

template <typename State>
State Tsdb2FingerprintValue(State state, std::string_view value);

template <typename State, typename... Values>
constexpr State Tsdb2FingerprintValue(State state, std::tuple<Values...> const& value);

template <typename State, typename Optional>
constexpr State Tsdb2FingerprintValue(State state, std::optional<Optional> const& value);

template <typename State>
constexpr State Tsdb2FingerprintValue(State state, std::nullopt_t const&);

template <typename State, typename... Values>
constexpr State Tsdb2FingerprintValue(State state, std::variant<Values...> const& value);

template <typename State, typename Inner>
constexpr State Tsdb2FingerprintValue(State state, absl::Span<Inner const> const value,
                                      std::enable_if_t<!std::is_integral_v<Inner>, bool> = true);

template <typename State, typename Integer>
State Tsdb2FingerprintValue(State state, absl::Span<Integer const> const value,
                            std::enable_if_t<std::is_integral_v<Integer>, bool> = true);

template <typename State, typename Inner>
constexpr State Tsdb2FingerprintValue(State state, std::vector<Inner> const& value);

template <typename State>
State Tsdb2FingerprintValue(State state, std::string_view const value);

template <typename State>
constexpr State Tsdb2FingerprintValue(State state, char const value[]);

template <typename State, typename Inner, typename Compare>
State Tsdb2FingerprintValue(State state, std::set<Inner, Compare> const& value);

template <typename State, typename Key, typename Value, typename Compare>
State Tsdb2FingerprintValue(State state, std::map<Key, Value, Compare> const& value);

template <typename State, typename Inner, typename Compare, typename Representation>
State Tsdb2FingerprintValue(State state,
                            tsdb2::common::flat_set<Inner, Compare, Representation> const& value);

template <typename State, typename Key, typename Value, typename Compare, typename Representation>
State Tsdb2FingerprintValue(
    State state, tsdb2::common::flat_map<Key, Value, Compare, Representation> const& value);

template <typename State, typename Integer>
constexpr State Tsdb2FingerprintValue(State state, Integer const value,
                                      std::enable_if_t<std::is_integral_v<Integer>, bool>) {
  if constexpr (sizeof(Integer) > sizeof(uint32_t)) {
    static_assert(sizeof(Integer) % sizeof(uint32_t) == 0, "unsupported type");
    return state.Add(reinterpret_cast<uint32_t const*>(&value), sizeof(Integer) / sizeof(uint32_t));
  } else {
    return state.Add(value);
  }
}

template <typename State, typename Float>
State Tsdb2FingerprintValue(State state, Float const value,
                            std::enable_if_t<std::is_floating_point_v<Float>, bool>) {
  static_assert(sizeof(Float) >= sizeof(uint32_t) && sizeof(Float) % sizeof(uint32_t) == 0,
                "unsupported type");
  return state.Add(reinterpret_cast<uint32_t const*>(&value), sizeof(Float) / sizeof(uint32_t));
}

template <typename State, typename Pointee>
constexpr State Tsdb2FingerprintValue(State state, Pointee* const value) {
  state.Add(value != nullptr);
  if (value) {
    return Tsdb2FingerprintValue(std::move(state), *value);
  } else {
    return state;
  }
}

template <typename State>
constexpr State Tsdb2FingerprintValue(State state, std::nullptr_t) {
  return state.Add(false);
}

template <typename State, typename... Values>
constexpr State Tsdb2FingerprintValue(State state, std::tuple<Values...> const& value) {
  return std::apply(
      [state = std::move(state)](auto const&... elements) mutable {
        return ((state = Tsdb2FingerprintValue(std::move(state), elements)), ...);
      },
      value);
}

template <typename State, typename Optional>
constexpr State Tsdb2FingerprintValue(State state, std::optional<Optional> const& value) {
  state.Add(value.has_value());
  if (value) {
    return Tsdb2FingerprintValue(std::move(state), *value);
  } else {
    return state;
  }
}

template <typename State>
constexpr State Tsdb2FingerprintValue(State state, std::nullopt_t const&) {
  return state.Add(false);
}

template <typename State, typename... Values>
constexpr State Tsdb2FingerprintValue(State state, std::variant<Values...> const& value) {
  state.Add(value.index());
  return std::visit(
      [state = std::move(state)](auto const& value) mutable {
        return Tsdb2FingerprintValue(std::move(state), value);
      },
      value);
}

template <typename State, typename Inner>
constexpr State Tsdb2FingerprintValue(State state, absl::Span<Inner const> const value,
                                      std::enable_if_t<!std::is_integral_v<Inner>, bool>) {
  state.Add(value.size());
  for (auto const& element : value) {
    state = Tsdb2FingerprintValue(std::move(state), element);
  }
  return state;
}

template <typename State, typename Integer>
State Tsdb2FingerprintValue(State state, absl::Span<Integer const> const value,
                            std::enable_if_t<std::is_integral_v<Integer>, bool>) {
  state = Tsdb2FingerprintValue(std::move(state), value.size());
  if constexpr (sizeof(Integer) >= sizeof(uint32_t)) {
    static_assert(sizeof(Integer) % sizeof(uint32_t) == 0, "unsupported type");
    return Tsdb2FingerprintValue(std::move(state), reinterpret_cast<uint32_t const*>(value.data()),
                                 value.size() * sizeof(Integer) / sizeof(uint32_t));
  } else {
    auto const data = reinterpret_cast<uint8_t const*>(value.data());
    auto const main_chunk_size = value.size() >> 2;
    state.Add(reinterpret_cast<uint32_t const*>(data), main_chunk_size);
    auto const remainder = value.size() % 4;
    if (remainder > 0) {
      uint32_t last = 0;
      std::memcpy(&last, data + (main_chunk_size << 2), remainder);
      return state.Add(last);
    }
    return state;
  }
}

template <typename State, typename Inner>
constexpr State Tsdb2FingerprintValue(State state, std::vector<Inner> const& value) {
  return Tsdb2FingerprintValue(std::move(state), absl::Span<Inner const>(value));
}

template <typename State>
State Tsdb2FingerprintValue(State state, std::string_view const value) {
  return Tsdb2FingerprintValue(std::move(state), absl::Span<char const>(value));
}

template <typename State>
constexpr State Tsdb2FingerprintValue(State state, char const value[]) {
  if (value) {
    return Tsdb2FingerprintValue(std::move(state), std::string_view(value));
  } else {
    return Tsdb2FingerprintValue(std::move(state), "");
  }
}

template <typename State, typename Inner, typename Compare>
State Tsdb2FingerprintValue(State state, std::set<Inner, Compare> const& value) {
  state.Add(value.size());
  for (auto const& element : value) {
    state = Tsdb2FingerprintValue(std::move(state), element);
  }
  return state;
}

template <typename State, typename Key, typename Value, typename Compare>
State Tsdb2FingerprintValue(State state, std::map<Key, Value, Compare> const& value) {
  state.Add(value.size());
  for (auto const& [key, value] : value) {
    state = Tsdb2FingerprintValue(std::move(state), key);
    state = Tsdb2FingerprintValue(std::move(state), value);
  }
  return state;
}

template <typename State, typename Inner, typename Compare, typename Representation>
State Tsdb2FingerprintValue(State state,
                            tsdb2::common::flat_set<Inner, Compare, Representation> const& value) {
  state.Add(value.size());
  for (auto const& element : value) {
    state = Tsdb2FingerprintValue(std::move(state), element);
  }
  return state;
}

template <typename State, typename Key, typename Value, typename Compare, typename Representation>
State Tsdb2FingerprintValue(
    State state, tsdb2::common::flat_map<Key, Value, Compare, Representation> const& value) {
  state.Add(value.size());
  for (auto const& [key, value] : value) {
    state = Tsdb2FingerprintValue(std::move(state), key);
    state = Tsdb2FingerprintValue(std::move(state), value);
  }
  return state;
}

namespace tsdb2 {
namespace common {

template <typename Value>
struct Fingerprint;

namespace internal {

// A 32-bit Murmur3 hasher that assumes the content to hash is always made up of 32-bit words.
// Thanks to that assumption the implementation is very fast because it doesn't have to fetch
// unaligned words, take care of endianness, etc.
//
// This implementation uses the fixed seed value defined above and is only suitable for
// fingerprinting, not for general-purpose hashing.
//
// Objects of this class expect zero or more `Add` calls and one final `Finish` call. The `Hash`
// method is a shorthand for an `Add` call followed by a `Finish`.
//
// WARNING: Calling `Add` after `Finish` or calling `Finish` multiple times leads to undefined
// behavior. It is okay to call `Finish` without calling `Add` at all.
class FingerprintState {
 public:
  explicit FingerprintState() = default;

  FingerprintState(FingerprintState const&) = default;
  FingerprintState& operator=(FingerprintState const&) = default;
  FingerprintState(FingerprintState&&) noexcept = default;
  FingerprintState& operator=(FingerprintState&&) noexcept = default;

  // Hashes the provided arguments into `state`. This function allows hashing structured types
  // recursively without having to call `Add` and decomposing the structured contents into 32-bit
  // words manually.
  template <typename... Values>
  static constexpr FingerprintState Combine(FingerprintState state, Values const&... values) {
    return Tsdb2FingerprintValue(std::move(state), std::tie(values...));
  }

  // Adds the provided 32-bit word to the hash calculation.
  constexpr FingerprintState& Add(uint32_t const k) {
    AddInternal(k);
    ++length_;
    return *this;
  }

  // Adds the provided 32-bit words to the hash calculation.
  constexpr FingerprintState& Add(uint32_t const* const ks, size_t const count) {
    for (size_t i = 0; i < count; ++i) {
      AddInternal(ks[i]);
    }
    length_ += count;
    return *this;
  }

  // Finishes the hash calculation and returns the calculated value.
  constexpr uint32_t Finish() {
    hash_ ^= length_;
    hash_ ^= hash_ >> 16;
    hash_ *= 0x85ebca6b;
    hash_ ^= hash_ >> 13;
    hash_ *= 0xc2b2ae35;
    hash_ ^= hash_ >> 16;
    return hash_;
  }

  // Shorthand for `Add(k).Finish()`.
  constexpr uint32_t Hash(uint32_t const k) { return Add(k).Finish(); }

  // Shorthand for `Add(ks, count).Finish()`.
  constexpr uint32_t Hash(uint32_t const* const ks, size_t const count) {
    return Add(ks, count).Finish();
  }

 private:
  constexpr void AddInternal(uint32_t k) {
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    hash_ ^= k;
    hash_ = (hash_ << 13) | (hash_ >> 19);
    hash_ = hash_ * 5 + 0xe6546b64;
  }

  static uint32_t constexpr kSeed = 71104;

  uint32_t hash_ = kSeed;
  uint32_t length_ = 0;
};

}  // namespace internal

template <typename Value>
struct Fingerprint {
  constexpr uint32_t operator()(Value const& value) const {
    return Tsdb2FingerprintValue(internal::FingerprintState(), value).Finish();
  }
};

namespace {

uint32_t constexpr kFalseFingerprint = Fingerprint<uint32_t>{}(false);
uint32_t constexpr kTrueFingerprint = Fingerprint<uint32_t>{}(true);

}  // namespace

template <>
struct Fingerprint<bool> {
  constexpr uint32_t operator()(bool const value) const {
    return value ? kTrueFingerprint : kFalseFingerprint;
  }
};

template <typename Value>
constexpr uint32_t FingerprintOf(Value&& value) {
  return Fingerprint<std::decay_t<Value>>{}(std::forward<Value>(value));
}

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_FINGERPRINT_H__
