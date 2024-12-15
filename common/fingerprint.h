// This header provides a generic fingerprinting framework similar to Abseil's hashing framework.
// The main difference between hashing and fingerprinting is that the latter uses a predefined
// constant seed, so fingerprints never change. By contrast, Abseil's hashing framework uses an
// internal seed value that is calculated randomly at every process restart and cannot be changed by
// the user.
//
// WARNING: because of the above, fingerprinting is NOT suitable for use in hash tables. Doing so
// would expose them to DoS attacks because, knowing the seed, an attacker can precalculate large
// amounts of collisions and flood the hash table with colliding data, degrading its performance and
// turning it into a list. Note that finding collisions in a hash table with a given hash algorithm
// is easier than finding collisions in the hash algorithm itself because the hash table has limited
// size and therefore uses some modulo of a hash value; that significantly restricts the space
// across which an attacker needs to find collisions.
//
// Despite the above weakness, fingerprinting is still useful to generate deterministic
// pseudo-random numbers based on some data. TSDB2 uses it to avoid RPC spikes by scattering RPC
// fire times across a time window, for example.
//
// This fingerprinting framework uses a 128-bit Murmur 3 hash, which is non-cryptographic but very
// fast. See https://en.wikipedia.org/wiki/MurmurHash for more information. The two 64-bit words
// calculated by Murmur3 are eventually XOR'd to produce a single 64-bit hash value.
//
// Similarly to Abseil's hashing framework, custom types can be made fingerprintable by adding a
// friend function called `Tsdb2FingerprintValue`, like in the following example:
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

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/container/node_hash_map.h"
#include "absl/container/node_hash_set.h"
#include "absl/numeric/int128.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "common/reffed_ptr.h"
#include "common/to_array.h"
#include "common/utilities.h"

// Forward declarations for all predefined `Tsdb2FingerprintValue` overloads follow. We
// forward-declare all of them so that they can refer to each other independently of their order (we
// don't know if the user is fingerprinting e.g. a set of tuples or a tuple of sets).

template <typename State, typename Integer,
          std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool> = true>
State Tsdb2FingerprintValue(State state, Integer value);

template <typename State>
State Tsdb2FingerprintValue(State state, absl::int128 value);

template <typename State>
State Tsdb2FingerprintValue(State state, absl::uint128 value);

template <typename State>
State Tsdb2FingerprintValue(State state, bool value);

template <typename State, typename Float,
          std::enable_if_t<std::is_floating_point_v<Float>, bool> = true>
State Tsdb2FingerprintValue(State state, Float const& value);

template <typename State, typename Pointee>
State Tsdb2FingerprintValue(State state, Pointee* value);

template <typename State>
State Tsdb2FingerprintValue(State state, std::nullptr_t);

template <typename State, typename Pointee>
State Tsdb2FingerprintValue(State state, std::unique_ptr<Pointee> const& value);

template <typename State, typename Pointee>
State Tsdb2FingerprintValue(State state, std::shared_ptr<Pointee> const& value);

template <typename State, typename Pointee>
State Tsdb2FingerprintValue(State state, tsdb2::common::reffed_ptr<Pointee> const& value);

template <typename State>
State Tsdb2FingerprintValue(State state, std::string_view value);

template <typename State>
State Tsdb2FingerprintValue(State state, char const* value);

template <typename State>
State Tsdb2FingerprintValue(State state, absl::Time value);

template <typename State>
State Tsdb2FingerprintValue(State state, absl::Duration value);

template <typename State, typename First, typename Second>
State Tsdb2FingerprintValue(State state, std::pair<First, Second> const& value);

template <typename State, typename... Values>
State Tsdb2FingerprintValue(State state, std::tuple<Values...> const& value);

template <typename State, typename Optional>
State Tsdb2FingerprintValue(State state, std::optional<Optional> const& value);

template <typename State>
State Tsdb2FingerprintValue(State state, std::nullopt_t const& value);

template <typename State, typename... Values>
State Tsdb2FingerprintValue(State state, std::variant<Values...> const& value);

template <typename State, typename Inner>
State Tsdb2FingerprintValue(State state, absl::Span<Inner const> value);

template <typename State, typename Inner, typename Allocator>
State Tsdb2FingerprintValue(State state, std::vector<Inner, Allocator> const& value);

template <typename State, typename Inner, size_t N>
State Tsdb2FingerprintValue(State state, std::array<Inner, N> const& value);

template <typename State, typename Inner, size_t N, typename Allocator>
State Tsdb2FingerprintValue(State state, absl::InlinedVector<Inner, N, Allocator> const& value);

template <typename State, typename Inner, typename Allocator>
State Tsdb2FingerprintValue(State state, std::deque<Inner, Allocator> const& value);

template <typename State, typename Inner, typename Compare, typename Allocator>
State Tsdb2FingerprintValue(State state, std::set<Inner, Compare, Allocator> const& value);

template <typename State, typename Key, typename Value, typename Compare, typename Allocator>
State Tsdb2FingerprintValue(State state, std::map<Key, Value, Compare, Allocator> const& value);

template <typename State, typename Inner, typename Compare, typename Allocator>
State Tsdb2FingerprintValue(State state, absl::btree_set<Inner, Compare, Allocator> const& value);

template <typename State, typename Key, typename Value, typename Compare, typename Allocator>
State Tsdb2FingerprintValue(State state,
                            absl::btree_map<Key, Value, Compare, Allocator> const& value);

template <typename State, typename Inner, typename Hash, typename Equal, typename Allocator>
State Tsdb2FingerprintValue(State state,
                            std::unordered_set<Inner, Hash, Equal, Allocator> const& value);

template <typename State, typename Key, typename Value, typename Hash, typename Equal,
          typename Allocator>
State Tsdb2FingerprintValue(State state,
                            std::unordered_map<Key, Value, Hash, Equal, Allocator> const& value);

template <typename State, typename Inner, typename Hash, typename Equal, typename Allocator>
State Tsdb2FingerprintValue(State state,
                            absl::flat_hash_set<Inner, Hash, Equal, Allocator> const& value);

template <typename State, typename Key, typename Value, typename Hash, typename Equal,
          typename Allocator>
State Tsdb2FingerprintValue(State state,
                            absl::flat_hash_map<Key, Value, Hash, Equal, Allocator> const& value);

template <typename State, typename Inner, typename Hash, typename Equal, typename Allocator>
State Tsdb2FingerprintValue(State state,
                            absl::node_hash_set<Inner, Hash, Equal, Allocator> const& value);

template <typename State, typename Key, typename Value, typename Hash, typename Equal,
          typename Allocator>
State Tsdb2FingerprintValue(State state,
                            absl::node_hash_map<Key, Value, Hash, Equal, Allocator> const& value);

namespace tsdb2 {
namespace common {
namespace internal {

// Helper functions for fingerprinting ordered and unordered ranges (defined at the bottom).
//
// The unordered helper works deterministically by hashing the elements individually and then
// fingerprinting the ordered set of hashes.

template <typename State, typename Range>
State FingerprintOrderedRange(State state, Range const& range);

template <typename State, typename Range>
State FingerprintUnorderedRange(State state, Range const& range);

}  // namespace internal
}  // namespace common
}  // namespace tsdb2

// Definitions of all predefined `Tsdb2FingerprintValue` overloads follow.

template <typename State, typename Integer,
          std::enable_if_t<tsdb2::util::IsIntegralStrictV<Integer>, bool>>
State Tsdb2FingerprintValue(State state, Integer const value) {
  state.Add(static_cast<uint64_t>(value));
  return state;
}

template <typename State>
State Tsdb2FingerprintValue(State state, absl::int128 const value) {
  state.Add(absl::Int128High64(value));
  state.Add(absl::Int128Low64(value));
  return state;
}

template <typename State>
State Tsdb2FingerprintValue(State state, absl::uint128 const value) {
  state.Add(absl::Uint128High64(value));
  state.Add(absl::Uint128Low64(value));
  return state;
}

template <typename State>
State Tsdb2FingerprintValue(State state, bool const value) {
  state.Add(value ? 1 : 0);
  return state;
}

template <typename State, typename Float, std::enable_if_t<std::is_floating_point_v<Float>, bool>>
State Tsdb2FingerprintValue(State state, Float const& value) {
  state.AddBytes(reinterpret_cast<uint8_t const*>(&value), sizeof(Float));
  return state;
}

template <typename State, typename Pointee>
State Tsdb2FingerprintValue(State state, Pointee* const value) {
  if (value) {
    state.Add(1);
    return Tsdb2FingerprintValue(std::move(state), *value);
  } else {
    state.Add(0);
    return state;
  }
}

template <typename State>
State Tsdb2FingerprintValue(State state, std::nullptr_t) {
  state.Add(0);
  return state;
}

template <typename State, typename Pointee>
State Tsdb2FingerprintValue(State state, std::unique_ptr<Pointee> const& value) {
  return Tsdb2FingerprintValue(std::move(state), value.get());
}

template <typename State, typename Pointee>
State Tsdb2FingerprintValue(State state, std::shared_ptr<Pointee> const& value) {
  return Tsdb2FingerprintValue(std::move(state), value.get());
}

template <typename State, typename Pointee>
State Tsdb2FingerprintValue(State state, tsdb2::common::reffed_ptr<Pointee> const& value) {
  return Tsdb2FingerprintValue(std::move(state), value.get());
}

template <typename State>
State Tsdb2FingerprintValue(State state, std::string_view const value) {
  state.Add(value.size());
  state.AddBytes(value.data(), value.size());
  return state;
}

template <typename State>
State Tsdb2FingerprintValue(State state, char const* const value) {
  return Tsdb2FingerprintValue(std::move(state), std::string_view(value));
}

template <typename State>
State Tsdb2FingerprintValue(State state, absl::Time const value) {
  return Tsdb2FingerprintValue(std::move(state), value - absl::UnixEpoch());
}

template <typename State>
State Tsdb2FingerprintValue(State state, absl::Duration const value) {
  // We shouldn't do this, but it's the only way to get an exact integer representation of a
  // duration...
  state.Add(absl::time_internal::GetRepHi(value));
  state.Add(absl::time_internal::GetRepLo(value));
  return state;
}

template <typename State, typename First, typename Second>
State Tsdb2FingerprintValue(State state, std::pair<First, Second> const& value) {
  state = Tsdb2FingerprintValue(std::move(state), value.first);
  state = Tsdb2FingerprintValue(std::move(state), value.second);
  return state;
}

template <typename State, typename... Values>
State Tsdb2FingerprintValue(State state, std::tuple<Values...> const& value) {
  return std::apply(
      [state = std::move(state)](auto const&... elements) mutable {
        return ((state = Tsdb2FingerprintValue(std::move(state), elements)), ...);
      },
      value);
}

template <typename State, typename Optional>
State Tsdb2FingerprintValue(State state, std::optional<Optional> const& value) {
  if (value) {
    state.Add(1);
    return Tsdb2FingerprintValue(std::move(state), value.value());
  } else {
    state.Add(0);
    return state;
  }
}

template <typename State>
State Tsdb2FingerprintValue(State state, std::nullopt_t const& /*value*/) {
  state.Add(0);
  return state;
}

template <typename State, typename... Values>
State Tsdb2FingerprintValue(State state, std::variant<Values...> const& value) {
  state.Add(value.index());
  return std::visit(
      [state = std::move(state)](auto const& value) mutable {
        return Tsdb2FingerprintValue(std::move(state), value);
      },
      value);
}

template <typename State, typename Inner>
State Tsdb2FingerprintValue(State state, absl::Span<Inner const> const value) {
  return tsdb2::common::internal::FingerprintOrderedRange(std::move(state), value);
}

template <typename State, typename Inner, typename Allocator>
State Tsdb2FingerprintValue(State state, std::vector<Inner, Allocator> const& value) {
  return Tsdb2FingerprintValue(std::move(state), absl::Span<Inner const>(value));
}

template <typename State, typename Inner, size_t N>
State Tsdb2FingerprintValue(State state, std::array<Inner, N> const& value) {
  return Tsdb2FingerprintValue(std::move(state), absl::Span<Inner const>(value));
}

template <typename State, typename Inner, size_t N, typename Allocator>
State Tsdb2FingerprintValue(State state, absl::InlinedVector<Inner, N, Allocator> const& value) {
  return Tsdb2FingerprintValue(std::move(state), absl::Span<Inner const>(value));
}

template <typename State, typename Inner, typename Allocator>
State Tsdb2FingerprintValue(State state, std::deque<Inner, Allocator> const& value) {
  return tsdb2::common::internal::FingerprintOrderedRange(std::move(state), value);
}

template <typename State, typename Inner, typename Compare, typename Allocator>
State Tsdb2FingerprintValue(State state, std::set<Inner, Compare, Allocator> const& value) {
  return tsdb2::common::internal::FingerprintOrderedRange(std::move(state), value);
}

template <typename State, typename Key, typename Value, typename Compare, typename Allocator>
State Tsdb2FingerprintValue(State state, std::map<Key, Value, Compare, Allocator> const& value) {
  return tsdb2::common::internal::FingerprintOrderedRange(std::move(state), value);
}

template <typename State, typename Inner, typename Compare, typename Allocator>
State Tsdb2FingerprintValue(State state, absl::btree_set<Inner, Compare, Allocator> const& value) {
  return tsdb2::common::internal::FingerprintOrderedRange(std::move(state), value);
}

template <typename State, typename Key, typename Value, typename Compare, typename Allocator>
State Tsdb2FingerprintValue(State state,
                            absl::btree_map<Key, Value, Compare, Allocator> const& value) {
  return tsdb2::common::internal::FingerprintOrderedRange(std::move(state), value);
}

template <typename State, typename Inner, typename Hash, typename Equal, typename Allocator>
State Tsdb2FingerprintValue(State state,
                            std::unordered_set<Inner, Hash, Equal, Allocator> const& value) {
  return tsdb2::common::internal::FingerprintUnorderedRange(std::move(state), value);
}

template <typename State, typename Key, typename Value, typename Hash, typename Equal,
          typename Allocator>
State Tsdb2FingerprintValue(State state,
                            std::unordered_map<Key, Value, Hash, Equal, Allocator> const& value) {
  return tsdb2::common::internal::FingerprintUnorderedRange(std::move(state), value);
}

template <typename State, typename Inner, typename Hash, typename Equal, typename Allocator>
State Tsdb2FingerprintValue(State state,
                            absl::flat_hash_set<Inner, Hash, Equal, Allocator> const& value) {
  return tsdb2::common::internal::FingerprintUnorderedRange(std::move(state), value);
}

template <typename State, typename Key, typename Value, typename Hash, typename Equal,
          typename Allocator>
State Tsdb2FingerprintValue(State state,
                            absl::flat_hash_map<Key, Value, Hash, Equal, Allocator> const& value) {
  return tsdb2::common::internal::FingerprintUnorderedRange(std::move(state), value);
}

template <typename State, typename Inner, typename Hash, typename Equal, typename Allocator>
State Tsdb2FingerprintValue(State state,
                            absl::node_hash_set<Inner, Hash, Equal, Allocator> const& value) {
  return tsdb2::common::internal::FingerprintUnorderedRange(std::move(state), value);
}

template <typename State, typename Key, typename Value, typename Hash, typename Equal,
          typename Allocator>
State Tsdb2FingerprintValue(State state,
                            absl::node_hash_map<Key, Value, Hash, Equal, Allocator> const& value) {
  return tsdb2::common::internal::FingerprintUnorderedRange(std::move(state), value);
}

namespace tsdb2 {
namespace common {

namespace internal {

// A 64-bit Murmur3 hasher implemented using the 128-bit version and XOR'ing the high and low 64-bit
// words of the result.
//
// This implementation uses a predefined fixed seed value and is only suitable for fingerprinting,
// not for general-purpose hashing.
//
// Objects of this class expect zero or more `Add` calls and one final `Finish` call. The `Hash`
// method is a shorthand for an `Add` call followed by a `Finish`.
//
// WARNING: Calling `Add` after `Finish` or calling `Finish` multiple times leads to undefined
// behavior. It is okay to call `Finish` without calling `Add` at all.
class FingerprintState {
 public:
  explicit FingerprintState() = default;
  ~FingerprintState() = default;

  FingerprintState(FingerprintState const&) = default;
  FingerprintState& operator=(FingerprintState const&) = default;
  FingerprintState(FingerprintState&&) noexcept = default;
  FingerprintState& operator=(FingerprintState&&) noexcept = default;

  // Hashes the provided arguments into `state`. This function allows hashing structured types
  // recursively without having to call `Add` and decomposing the structured contents into 64-bit
  // words manually.
  template <typename... Values>
  static FingerprintState Combine(FingerprintState state, Values const&... values) {
    return Tsdb2FingerprintValue(state, std::tie(values...));
  }

  // Adds a 64-bit word to the calculation.
  void Add(uint64_t k);

  // Adds the given 64-bit words to the calculation.
  void Add(uint64_t const* ks, size_t count);

  // Adds the given bytes to the calculation.
  void AddBytes(void const* data, size_t length);

  // Finishes the hash calculation and returns the calculated value.
  uint64_t Finish() &&;

 private:
  static inline uint64_t constexpr kSeed = 0x7110400071104000ull;

  static inline uint64_t constexpr c1_ = 0x87c37b91114253d5ull;
  static inline uint64_t constexpr c2_ = 0x4cf5ad432745937full;

  static inline uint64_t constexpr c3_ = 0xff51afd7ed558ccdull;
  static inline uint64_t constexpr c4_ = 0xc4ceb9fe1a85ec53ull;

  void Step(uint64_t k2);

  uint64_t h1_ = kSeed;
  uint64_t h2_ = kSeed;
  uint64_t k1_ = 0;
  bool step_ = false;
  size_t total_length_ = 0;
};

}  // namespace internal

template <typename Value>
struct Fingerprint;

template <typename Value>
struct Fingerprint {
  uint64_t operator()(Value const& value) const {
    return Tsdb2FingerprintValue(internal::FingerprintState(), value).Finish();
  }
};

template <typename Element, size_t N>
struct Fingerprint<Element[N]> {
  uint64_t operator()(Element (&value)[N]) const {
    return Tsdb2FingerprintValue(internal::FingerprintState(), to_array(value)).Finish();
  }
  uint64_t operator()(Element (&&value)[N]) const {
    return Tsdb2FingerprintValue(internal::FingerprintState(), to_array(std::move(value))).Finish();
  }
};

template <size_t N>
struct Fingerprint<char const[N]> {
  static size_t GetLength(char const* const value) { return value[N - 1] != 0 ? N : N - 1; }

  uint64_t operator()(char const (&value)[N]) const {
    return Tsdb2FingerprintValue(internal::FingerprintState(),
                                 std::string_view(value, GetLength(value)))
        .Finish();
  }

  uint64_t operator()(char const (&&value)[N]) const {
    return Tsdb2FingerprintValue(internal::FingerprintState(),
                                 std::string_view(value, GetLength(value)))
        .Finish();
  }
};

template <size_t N>
struct Fingerprint<char[N]> {
  static size_t GetLength(char const* const value) { return value[N - 1] != 0 ? N : N - 1; }

  uint64_t operator()(char (&value)[N]) const {
    return Tsdb2FingerprintValue(internal::FingerprintState(),
                                 std::string_view(value, GetLength(value)))
        .Finish();
  }

  uint64_t operator()(char (&&value)[N]) const {
    return Tsdb2FingerprintValue(internal::FingerprintState(),
                                 std::string_view(value, GetLength(value)))
        .Finish();
  }
};

template <typename Value>
uint64_t FingerprintOf(Value&& value) {
  return Fingerprint<std::decay_t<Value>>{}(std::forward<Value>(value));
}

template <typename Element, size_t N>
uint64_t FingerprintOf(Element (&value)[N]) {
  return Fingerprint<Element[N]>{}(value);
}

template <typename Element, size_t N>
uint64_t FingerprintOf(Element (&&value)[N]) {
  return Fingerprint<Element[N]>{}(std::move(value));
}

namespace internal {

template <typename State, typename Range>
State FingerprintOrderedRange(State state, Range const& range) {
  state = Tsdb2FingerprintValue(std::move(state), range.size());
  for (auto const& item : range) {
    state = Tsdb2FingerprintValue(std::move(state), item);
  }
  return state;
}

template <typename State, typename Range>
State FingerprintUnorderedRange(State state, Range const& range) {
  std::vector<uint64_t> fingerprints;
  fingerprints.reserve(range.size());
  for (auto const& item : range) {
    fingerprints.emplace_back(FingerprintOf(item));
  }
  std::sort(fingerprints.begin(), fingerprints.end());
  return FingerprintOrderedRange(std::move(state), fingerprints);
}

}  // namespace internal

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_FINGERPRINT_H__
