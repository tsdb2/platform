#ifndef __TSDB2_COMMON_UTILITIES_H__
#define __TSDB2_COMMON_UTILITIES_H__

#include <type_traits>
#include <utility>  // IWYU pragma: keep

#include "absl/status/status.h"
#include "absl/status/statusor.h"

// See https://clang.llvm.org/extra/clang-tidy/checks/cppcoreguidelines/owning-memory.html.
namespace gsl {
template <typename T, std::enable_if_t<std::is_pointer_v<T>, bool> = true>
using owner = T;
}  // namespace gsl

namespace tsdb2 {
namespace util {

// Same as `std::to_underlying`, which is only available in C++23.
template <typename Enum>
auto to_underlying(Enum const value) {
  return static_cast<std::underlying_type_t<Enum>>(value);
}

// Like `std::is_integral` but excludes booleans.
template <typename Type>
using IsIntegralStrict =
    std::conjunction<std::is_integral<Type>, std::negation<std::is_same<Type, bool>>>;

// Like `std::is_integral_v` but excludes booleans.
template <typename Type>
inline bool constexpr IsIntegralStrictV = IsIntegralStrict<Type>::value;

namespace internal {

inline absl::Status ReturnIfError_GetStatus(absl::Status const& status) { return status; }

template <typename T>
inline absl::Status ReturnIfError_GetStatus(absl::StatusOr<T> const& status_or) {
  return status_or.status();
}

}  // namespace internal

}  // namespace util
}  // namespace tsdb2

// Evaluates the expression assuming it returns an `absl::Status` or `absl::StatusOr`, and returns
// prematurely if the returned status is an error.
//
// Example:
//
//   absl::Status Foo();
//   absl::Status Baz();
//
//   absl::Status Bar() {
//     RETURN_IF_ERROR(Foo());
//     RETURN_IF_ERROR(Baz());
//     return absl::OkStatus();
//   }
//
// Note that if the expression returns an `absl::StatusOr` with an OK status, the wrapped value is
// lost. If you need to retain it in a separate variable use `ASSIGN_OR_RETURN` instead.
#define RETURN_IF_ERROR(expression)                                    \
  do {                                                                 \
    auto status = (expression);                                        \
    if (!status.ok()) {                                                \
      return ::tsdb2::util::internal::ReturnIfError_GetStatus(status); \
    }                                                                  \
  } while (false)

// Evaluates the right-hand side assuming it returns an `absl::StatusOr`. If the returned status is
// OK the wrapped value is assigned to the left-hand side, otherwise returns prematurely with the
// error status.
//
// Example:
//
//   absl::StatusOr<int> Foo();
//
//   absl::Status Bar() {
//     int n;
//     ASSIGN_OR_RETURN(n, Foo());
//     Baz(n);
//     return absl::OkStatus();
//   }
//
// REQUIRES: the value wrapped in the `absl::StatusOr` must be movable.
#define ASSIGN_OR_RETURN(lhs, rhs)                \
  do {                                            \
    auto status_or_value = (rhs);                 \
    if (status_or_value.ok()) {                   \
      (lhs) = std::move(status_or_value).value(); \
    } else {                                      \
      return std::move(status_or_value).status(); \
    }                                             \
  } while (false)

// Declares a new variable with the given `type` and `name`, evaluates the provided `expression`
// assuming it returns an `absl::StatusOr`, then if the returned status is OK the wrapped value is
// assigned to the variable, otherwise returns prematurely with the error status.
//
// Example:
//
//   absl::StatusOr<int> Foo();
//
//   absl::Status Bar() {
//     ASSIGN_VAR_OR_RETURN(int, n, Foo());
//     Baz(n);
//     return absl::OkStatus();
//   }
//
// REQUIRES: `type` must be movable and default-constructible.
#define ASSIGN_VAR_OR_RETURN(type, name, expression) \
  type name;                                         \
  {                                                  \
    auto status_or_value = (expression);             \
    if (status_or_value.ok()) {                      \
      name = std::move(status_or_value).value();     \
    } else {                                         \
      return std::move(status_or_value).status();    \
    }                                                \
  }

// Evaluates the provided `expression` assuming it returns an `absl::StatusOr` and, if the status is
// OK, assigns the wrapped value to a new variable named with `name`. If the returned status is not
// OK it returns prematurely with the error status.
//
// Example:
//
//   absl::StatusOr<int> Foo();
//
//   absl::Status Bar() {
//     DEFINE_VAR_OR_RETURN(n, Foo());
//     Baz(n);
//     return absl::OkStatus();
//   }
//
// NOTE: this macro will also define an extra intermediate variable called `status_or_##name`, where
// `name` is the first parameter.
#define DEFINE_VAR_OR_RETURN(name, expression)   \
  auto status_or_##name = (expression);          \
  if (!(status_or_##name).ok()) {                \
    return std::move(status_or_##name).status(); \
  }                                              \
  auto& name = status_or_##name.value();

// Like `DEFINE_VAR_OR_RETURN` but the resulting variable is const.
#define DEFINE_CONST_OR_RETURN(name, expression) \
  auto status_or_##name = (expression);          \
  if (!(status_or_##name).ok()) {                \
    return std::move(status_or_##name).status(); \
  }                                              \
  auto const& name = status_or_##name.value();

// Like `DEFINE_VAR_OR_RETURN` and `DEFINE_CONST_OR_RETURN` but it doesn't define any variable named
// with `name`, only the intermediate `status_or_*` variable. This is useful for scoped objects.
#define DEFINE_OR_RETURN(name, expression)       \
  auto const status_or_##name = (expression);    \
  if (!(status_or_##name).ok()) {                \
    return std::move(status_or_##name).status(); \
  }

#endif  // __TSDB2_COMMON_UTILITIES_H__
