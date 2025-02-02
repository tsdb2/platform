#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/utilities.h"
#include "proto/plugin.h"

namespace {

using ::google::protobuf::compiler::CodeGeneratorRequest;
using ::google::protobuf::compiler::CodeGeneratorResponse;

absl::StatusOr<CodeGeneratorResponse> Run(CodeGeneratorRequest const& request) {
  // TODO
  return CodeGeneratorResponse();
}

absl::StatusOr<std::vector<uint8_t>> ReadInput() {
  std::vector<uint8_t> buffer;
  uint8_t temp[4096];
  while (size_t bytesRead = ::fread(temp, 1, sizeof(temp), stdin)) {
    buffer.insert(buffer.end(), temp, temp + bytesRead);
  }
  if (::ferror(stdin) != 0) {
    return absl::ErrnoToStatus(errno, "fread");
  }
  return std::move(buffer);
}

absl::Status Run() {
  DEFINE_CONST_OR_RETURN(input, ReadInput());
  DEFINE_CONST_OR_RETURN(request, CodeGeneratorRequest::Decode(input));
  DEFINE_CONST_OR_RETURN(response, Run(request));
  auto const output = response.Encode().Flatten();
  if (::fwrite(output.span().data(), 1, output.size(), stdout) != output.size()) {
    return absl::ErrnoToStatus(errno, "fwrite");
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
