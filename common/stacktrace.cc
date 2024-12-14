#include "common/stacktrace.h"

#include <string>

#include "absl/debugging/stacktrace.h"
#include "absl/debugging/symbolize.h"
#include "absl/strings/str_cat.h"

namespace tsdb2 {
namespace common {

std::string GetStackTrace() {
  int constexpr kMaxDepth = 64;
  void* stack[kMaxDepth];
  std::string str;
  auto const depth = absl::GetStackTrace(stack, kMaxDepth, /*skip_count=*/1);
  for (auto i = depth; i > 0; --i) {
    char symbol[1024];
    if (absl::Symbolize(stack[depth - i], symbol, sizeof(symbol))) {
      str += absl::StrCat("#", i, " ", symbol, "\n");
    } else {
      str += absl::StrCat("#", i, " 0x", absl::Hex(stack[depth - i], absl::kZeroPad16), "\n");
    }
  }
  return str;
}

}  // namespace common
}  // namespace tsdb2
