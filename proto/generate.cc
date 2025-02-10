#include <cerrno>
#include <cstdio>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "proto/generator.h"
#include "proto/plugin.h"

ABSL_FLAG(
    std::vector<std::string>, file_descriptor_sets, {},
    "One or more comma-separated file paths containing serialized FileDescriptorSet protobufs.");

namespace {

using ::google::protobuf::FileDescriptorSet;
using ::tsdb2::proto::generator::GenerateHeaderFileContent;
using ::tsdb2::proto::generator::GenerateSourceFileContent;

absl::Status Run() {
  for (auto const& file : absl::GetFlag(FLAGS_file_descriptor_sets)) {
    // TODO
  }
  return absl::OkStatus();
}

}  // namespace

int main() {
  auto const status = Run();
  if (!status.ok()) {
    std::string const message{status.message()};
    ::fprintf(stderr, "Error: %s\n", message.c_str());  // NOLINT
    return 1;
  }
  return 0;
}
