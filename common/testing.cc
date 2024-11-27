#include "common/testing.h"

#include "absl/debugging/failure_signal_handler.h"
#include "absl/log/initialize.h"

namespace {

class FailureSignalHandlerInstaller {
 public:
  explicit FailureSignalHandlerInstaller() {
    // NOTE: this is a subset of `tsdb2::init::InitServer`. It would be nice to have a way to call
    // that instead, but we don't have argc+argv.
    absl::InitializeLog();
    absl::InstallFailureSignalHandler(absl::FailureSignalHandlerOptions());
  }
};

FailureSignalHandlerInstaller failure_signal_handler_installer;

}  // namespace
