#include "common/testing.h"

#include "absl/debugging/failure_signal_handler.h"

namespace {

class FailureSignalHandlerInstaller {
 public:
  explicit FailureSignalHandlerInstaller() {
    absl::InstallFailureSignalHandler(absl::FailureSignalHandlerOptions());
  }
};

FailureSignalHandlerInstaller failure_signal_handler_installer;

}  // namespace
