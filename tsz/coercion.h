#ifndef __TSDB2_TSZ_COERCION_H__
#define __TSDB2_TSZ_COERCION_H__

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "common/fixed.h"
#include "common/utilities.h"
#include "tsz/distribution.h"

namespace tsz {

namespace util {

// `fixed` utilities are re-exported here for convenience.

using ::tsdb2::common::Fixed;

template <typename T, typename Unused>
using FixedT = tsdb2::common::FixedT<T, Unused>;

template <typename Unused, typename T>
inline constexpr T FixedV(T&& t) {
  return tsdb2::common::FixedV<Unused, T>(std::forward<T>(t));
}

// Generates an `std::tuple` type corresponding to the concatenation of two tuples. For example,
// `CatTupleT<std::tuple<A, B>, std::tuple<C, D, E>>` is defined as `std::tuple<A, B, C, D, E>`.
//
// Note that `CatTupleT` is the same type returned by `std::tuple_cat`.
template <typename LHS, typename RHS>
struct CatTuple;

template <typename... LHSTypes, typename... RHSTypes>
struct CatTuple<std::tuple<LHSTypes...>, std::tuple<RHSTypes...>> {
  using Tuple = std::tuple<LHSTypes..., RHSTypes...>;
};

template <typename LHS, typename RHS>
using CatTupleT = typename CatTuple<LHS, RHS>::Tuple;

// `IsIntegralStrict` is re-exported here for convenience.

using ::tsdb2::util::IsIntegralStrict;

template <typename Type>
inline bool constexpr IsIntegralStrictV = tsdb2::util::IsIntegralStrictV<Type>;

}  // namespace util

// Converts the given `Type` to the corresponding canonical type. The tsz API is defined in terms of
// user-specified types, but the underlying engine converts and stores all data in the corresponding
// canonical types.
//
// The canonical types are: `int64_t` for all integers, `bool` for booleans, `double` for all
// floating point numbers, `std::string` for all strings, and `tsz::Distribution` for distributions.
//
// NOTE: since all integer types are stored as `int64_t`, unsigned 64-bit integers are not supported
// unless the MSB is always unset. A `uint64_t` to `int64_t` conversion is still provided, but the
// API behavior is undefined if the MSB is set.
template <typename Type, typename Enable = void>
struct CanonicalType;

template <>
struct CanonicalType<bool> {
  using Type = bool;
};

template <typename Integer>
struct CanonicalType<Integer, std::enable_if_t<util::IsIntegralStrictV<Integer>>> {
  using Type = int64_t;
};

template <typename Float>
struct CanonicalType<Float, std::enable_if_t<std::is_floating_point_v<Float>>> {
  using Type = double;
};

template <>
struct CanonicalType<std::string> {
  using Type = std::string;
};

template <>
struct CanonicalType<char*> {
  using Type = std::string;
};

template <>
struct CanonicalType<char const*> {
  using Type = std::string;
};

template <>
struct CanonicalType<Distribution> {
  using Type = Distribution;
};

template <typename Type>
using CanonicalTypeT = typename CanonicalType<Type>::Type;

// Converts the given `Type` to the corresponding type used for parameter passing. String types are
// converted to `std::string_view`, all other types are converted to themselves.
//
// NOTE: moving and move-constructing `Distribution` objects is cheap, so even if they are passed by
// value it's possible to pass them cheaply with `std::move`. If, on the other hand, the caller
// needs to retain ownership of its `Distribution` object, a copy will have to be made at some
// point, so passing by value is okay.
template <typename Type, typename Enable = void>
struct ParameterType;

template <>
struct ParameterType<bool> {
  using Type = bool;
};

template <typename Integer>
struct ParameterType<Integer, std::enable_if_t<util::IsIntegralStrictV<Integer>>> {
  using Type = Integer;
};

template <typename Float>
struct ParameterType<Float, std::enable_if_t<std::is_floating_point_v<Float>>> {
  using Type = Float;
};

template <>
struct ParameterType<std::string> {
  using Type = std::string_view;
};

template <>
struct ParameterType<char*> {
  using Type = std::string_view;
};

template <>
struct ParameterType<char const*> {
  using Type = std::string_view;
};

template <>
struct ParameterType<Distribution> {
  using Type = Distribution;
};

template <typename Type>
using ParameterTypeT = typename ParameterType<Type>::Type;

}  // namespace tsz

#endif  // __TSDB2_TSZ_COERCION_H__
