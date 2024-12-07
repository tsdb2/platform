#include "common/testing.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <string_view>
#include <utility>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "common/env.h"
#include "io/fd.h"

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

char constexpr kTestTmpDirEnvVar[] = "TEST_TMPDIR";
std::string_view constexpr kDefaultTestTmpDir = "/tmp/";

}  // namespace

namespace testing {

std::string GetTestTmpDir() {
  auto maybe_value = tsdb2::common::GetEnv(kTestTmpDirEnvVar);
  if (maybe_value.has_value()) {
    return std::move(maybe_value).value();
  } else {
    return std::string(kDefaultTestTmpDir);
  }
}

absl::StatusOr<TestTempFile> TestTempFile::Create(std::string_view const base_name) {
  std::string path = MakeTempFileTemplate(base_name);
  tsdb2::io::FD fd{::mkstemp(path.data())};
  if (fd.empty()) {
    return absl::ErrnoToStatus(errno, "mkstemp");
  } else {
    return TestTempFile(std::move(path), std::move(fd));
  }
}

TestTempFile::~TestTempFile() {
  fd_.Close();
  if (::unlink(path_.c_str()) < 0) {
    LOG(ERROR) << absl::ErrnoToStatus(errno, "unlink");
  }
}

std::string TestTempFile::MakeTempFileTemplate(std::string_view const base_name) {
  static std::string_view constexpr kSuffix = "_XXXXXX";
  auto const directory = GetTestTmpDir();
  if (absl::EndsWith(directory, "/")) {
    return absl::StrCat(directory, base_name, kSuffix);
  } else {
    return absl::StrCat(directory, "/", base_name, kSuffix);
  }
}

}  // namespace testing
