#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/utilities.h"
#include "proto/dependencies.h"
#include "proto/file_writer.h"
#include "proto/object.h"
#include "proto/plugin.h"

namespace {

using ::google::protobuf::compiler::CodeGeneratorRequest;
using ::google::protobuf::compiler::CodeGeneratorResponse;
using ::google::protobuf::compiler::CodeGeneratorResponse_Feature;
using ::google::protobuf::compiler::CodeGeneratorResponse_File;
using ::google::protobuf::compiler::kCodeGeneratorResponseFileContentField;
using ::google::protobuf::compiler::kCodeGeneratorResponseFileField;
using ::google::protobuf::compiler::kCodeGeneratorResponseFileNameField;
using ::google::protobuf::compiler::kCodeGeneratorResponseSupportedFeaturesField;
using ::tsdb2::proto::kInitialize;
using ::tsdb2::proto::internal::DependencyManager;
using ::tsdb2::proto::internal::FileWriter;

std::string GetHeaderGuardName() {
  // TODO
  return absl::StrCat("__TSDB2_", "_H__");
}

absl::StatusOr<CodeGeneratorResponse_File> GenerateHeaderFile() {
  DependencyManager dm;
  FileWriter fw;
  auto const header_guard_name = GetHeaderGuardName();
  fw.AppendUnindentedLine(absl::StrCat("#ifndef ", header_guard_name));
  fw.AppendUnindentedLine(absl::StrCat("#define ", header_guard_name));
  fw.AppendEmptyLine();
  // TODO
  fw.AppendEmptyLine();
  fw.AppendUnindentedLine(absl::StrCat("#endif  // ", header_guard_name));
  CodeGeneratorResponse_File file;
  file.get<kCodeGeneratorResponseFileNameField>() = "proto.h";
  file.get<kCodeGeneratorResponseFileContentField>() = std::move(fw).Finish();
  return std::move(file);
}

absl::StatusOr<CodeGeneratorResponse_File> GenerateSourceFile() {
  FileWriter fw;
  fw.AppendUnindentedLine(absl::StrCat("#include \"", "\""));
  fw.AppendEmptyLine();
  // TODO
  CodeGeneratorResponse_File file;
  file.get<kCodeGeneratorResponseFileNameField>() = "proto.cc";
  file.get<kCodeGeneratorResponseFileContentField>() = std::move(fw).Finish();
  return std::move(file);
}

absl::StatusOr<CodeGeneratorResponse> Run(CodeGeneratorRequest const& request) {
  // TODO
  CodeGeneratorResponse response;
  response.get<kCodeGeneratorResponseSupportedFeaturesField>() =
      tsdb2::util::to_underlying(CodeGeneratorResponse_Feature::FEATURE_NONE);
  DEFINE_VAR_OR_RETURN(header_file, GenerateHeaderFile());
  response.get<kCodeGeneratorResponseFileField>().emplace_back(std::move(header_file));
  DEFINE_VAR_OR_RETURN(source_file, GenerateSourceFile());
  response.get<kCodeGeneratorResponseFileField>().emplace_back(std::move(source_file));
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
