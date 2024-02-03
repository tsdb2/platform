#include "server/crash.h"

#include <errno.h>
#include <execinfo.h>
#include <signal.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string_view>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/log/log.h"
#include "common/flat_map.h"

namespace tsdb2 {
namespace init {

namespace {

auto constexpr signal_names = ::tsdb2::common::fixed_flat_map_of<int, std::string_view>({
    {SIGABRT, "SIGABRT"},
    {SIGBUS, "SIGBUS"},
    {SIGFPE, "SIGFPE"},
    {SIGILL, "SIGILL"},
    {SIGSEGV, "SIGSEGV"},
    {SIGSYS, "SIGSYS"},
    {SIGTERM, "SIGTERM"},
});

size_t constexpr kMaxBacktraceSize = 100;
size_t constexpr kMaxBacktraceBufferSize = 4096;

ABSL_CONST_INIT absl::once_flag once;

void CrashHandler(int const sig) {
  auto const it = signal_names.find(sig);
  if (it != signal_names.end()) {
    LOG(ERROR) << "received " << it->second << " -- backtrace follows (most recent first)";
  } else {
    LOG(ERROR) << "received signal " << sig << " -- backtrace follows (most recent first)";
  }
  void* bt[kMaxBacktraceSize];
  auto const size = backtrace(bt, kMaxBacktraceSize);
  auto const symbols = backtrace_symbols(bt, size);
  char text[kMaxBacktraceBufferSize];
  text[0] = 0;
  size_t offset = 0;
  for (size_t i = 0; i < size; ++i) {
    size_t const length = std::strlen(symbols[i]);
    if (offset + length + 1 < kMaxBacktraceBufferSize) {
      std::memcpy(text + offset, symbols[i], length);
      offset += length;
      text[offset++] = '\n';
      text[offset] = 0;
    } else {
      std::strncat(text, symbols[i], kMaxBacktraceBufferSize - offset - 4);
      std::strcat(text, "...");
      break;
    }
  }
  LOG(ERROR) << text;
  abort();
}

}  // namespace

void InstallCrashHandler() {
  struct sigaction sa;
  std::memset(&sa, 0, sizeof(sa));
  sa.sa_handler = &CrashHandler;
  sa.sa_flags = SA_RESETHAND;
  for (auto const& [sig, name] : signal_names) {
    if (sigaction(sig, &sa, nullptr) < 0) {
      LOG(ERROR) << "failed to install backtrace signal handler for " << name
                 << ", errno=" << errno;
    }
  }
}

CrashHandlerInstaller::CrashHandlerInstaller() { absl::call_once(once, &InstallCrashHandler); }

}  // namespace init
}  // namespace tsdb2
