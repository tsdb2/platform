#ifndef __TSDB2_COMMON_UTILITIES_H__
#define __TSDB2_COMMON_UTILITIES_H__

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace tsdb2 {
namespace util {
namespace internal {

inline absl::Status ReturnIfError_GetStatus(absl::Status const& status) { return status; }

template <typename T>
inline absl::Status ReturnIfError_GetStatus(absl::StatusOr<T> const& status_or) {
  return status_or.status();
}

}  // namespace internal
}  // namespace util
}  // namespace tsdb2

#define RETURN_IF_ERROR(expression)                                    \
  do {                                                                 \
    auto status = (expression);                                        \
    if (!status.ok()) {                                                \
      return ::tsdb2::util::internal::ReturnIfError_GetStatus(status); \
    }                                                                  \
  } while (false);

#endif  // __TSDB2_COMMON_UTILITIES_H__
