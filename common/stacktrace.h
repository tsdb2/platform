#ifndef __TSDB2_COMMON_STACKTRACE_H__
#define __TSDB2_COMMON_STACKTRACE_H__

#include <string>

#include "absl/log/log.h"

namespace tsdb2 {
namespace common {

std::string GetStackTrace();

}  // namespace common
}  // namespace tsdb2

#endif  // __TSDB2_COMMON_STACKTRACE_H__
