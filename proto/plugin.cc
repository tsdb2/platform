#include <cerrno>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/initialize.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/utilities.h"
#include "proto/descriptor.h"
#include "proto/generator.h"
#include "proto/object.h"

namespace {

using ::google::protobuf::compiler::CodeGeneratorRequest;
using ::google::protobuf::compiler::CodeGeneratorResponse;
using ::google::protobuf::compiler::CodeGeneratorResponse_Feature;
using ::google::protobuf::compiler::kCodeGeneratorRequestProtoFileField;
using ::google::protobuf::compiler::kCodeGeneratorResponseFileField;
using ::google::protobuf::compiler::kCodeGeneratorResponseSupportedFeaturesField;
using ::tsdb2::proto::Generator;
using ::tsdb2::proto::generator::ReadFile;
using ::tsdb2::proto::generator::WriteFile;

absl::StatusOr<CodeGeneratorResponse> Run(CodeGeneratorRequest const& request) {
  CodeGeneratorResponse response;
  response.get<kCodeGeneratorResponseSupportedFeaturesField>() =
      tsdb2::util::to_underlying(CodeGeneratorResponse_Feature::FEATURE_NONE);
  for (auto const& proto_file : request.get<kCodeGeneratorRequestProtoFileField>()) {
    Generator generator{proto_file};
    DEFINE_VAR_OR_RETURN(header_file, generator.GenerateHeaderFile());
    response.get<kCodeGeneratorResponseFileField>().emplace_back(std::move(header_file));
    DEFINE_VAR_OR_RETURN(source_file, generator.GenerateSourceFile());
    response.get<kCodeGeneratorResponseFileField>().emplace_back(std::move(source_file));
  }
  return std::move(response);
}

absl::Status Run() {
  DEFINE_CONST_OR_RETURN(input, ReadFile(stdin));
  DEFINE_CONST_OR_RETURN(request, CodeGeneratorRequest::Decode(input));
  DEFINE_CONST_OR_RETURN(response, Run(request));
  auto const output = response.Encode().Flatten();
  return WriteFile(stdout, output.span());
}

}  // namespace

int main() {
  absl::InitializeLog();
  auto const status = Run();
  if (!status.ok()) {
    std::string const message{status.message()};
    ::fprintf(stderr, "Error: %s\n", message.c_str());  // NOLINT
    return 1;
  }
  return 0;
}
