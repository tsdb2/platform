#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "common/no_destructor.h"
#include "common/re.h"
#include "common/utilities.h"
#include "proto/dependencies.h"
#include "proto/file_writer.h"
#include "proto/object.h"
#include "proto/plugin.h"

namespace {

using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::kFileDescriptorProtoNameField;
using ::google::protobuf::kFileDescriptorProtoPackageField;
using ::google::protobuf::compiler::CodeGeneratorRequest;
using ::google::protobuf::compiler::CodeGeneratorResponse;
using ::google::protobuf::compiler::CodeGeneratorResponse_Feature;
using ::google::protobuf::compiler::CodeGeneratorResponse_File;
using ::google::protobuf::compiler::kCodeGeneratorRequestProtoFileField;
using ::google::protobuf::compiler::kCodeGeneratorResponseFileContentField;
using ::google::protobuf::compiler::kCodeGeneratorResponseFileField;
using ::google::protobuf::compiler::kCodeGeneratorResponseFileNameField;
using ::google::protobuf::compiler::kCodeGeneratorResponseSupportedFeaturesField;
using ::tsdb2::proto::kInitialize;
using ::tsdb2::proto::internal::DependencyManager;
using ::tsdb2::proto::internal::FileWriter;

template <char const name[], typename Proto>
class RequireFieldImpl;

template <char const name[], typename Value, size_t tag, typename... OtherFields>
class RequireFieldImpl<
    name,
    tsdb2::proto::Object<tsdb2::proto::Field<std::optional<Value>, name, tag>, OtherFields...>> {
 public:
  using Proto =
      tsdb2::proto::Object<tsdb2::proto::Field<std::optional<Value>, name, tag>, OtherFields...>;

  explicit RequireFieldImpl(Proto const& proto) : proto_(proto) {}

  absl::StatusOr<Value const*> operator()() {
    auto const& maybe_field = proto_.template get<name>();
    if (maybe_field) {
      return &(*maybe_field);
    } else {
      return absl::InvalidArgumentError(absl::StrCat("missing required field \"", name, "\""));
    }
  }

 private:
  Proto const& proto_;
};

template <char const name[], typename Proto>
auto RequireField(Proto const& proto) {
  return RequireFieldImpl<name, typename Proto::template Base<name>>{proto}();
}

tsdb2::common::NoDestructor<tsdb2::common::RE> const kPackagePattern{
    tsdb2::common::RE::CreateOrDie("^[A-Za-z][A-Za-z0-9]*(?:\\.[A-Za-z][A-Za-z0-9]*)*$")};

absl::StatusOr<std::string> GetHeaderGuardName(FileDescriptorProto const& file_descriptor) {
  DEFINE_CONST_OR_RETURN(name, RequireField<kFileDescriptorProtoNameField>(file_descriptor));
  // TODO: we are replacing only path separators here, but file paths may contain many more symbols.
  std::string converted = absl::StrReplaceAll(*name, {{"/", "_"}, {"\\", "_"}});
  auto const last_dot = converted.find_last_of('.');
  if (last_dot != std::string::npos) {
    converted.erase(last_dot);
  }
  absl::AsciiStrToUpper(&converted);
  return absl::StrCat("__TSDB2_", converted, "_H__");
}

absl::StatusOr<std::string> GetCppPackage(FileDescriptorProto const& file_descriptor) {
  DEFINE_CONST_OR_RETURN(package, RequireField<kFileDescriptorProtoPackageField>(file_descriptor));
  if (!kPackagePattern->Test(*package)) {
    return absl::InvalidArgumentError(
        absl::StrCat("package name \"", *package, "\" has an invalid format"));
  }
  return absl::StrReplaceAll(*package, {{".", "::"}});
}

absl::StatusOr<CodeGeneratorResponse_File> GenerateHeaderFile(
    FileDescriptorProto const& file_descriptor) {
  DependencyManager dm;
  FileWriter fw;
  DEFINE_CONST_OR_RETURN(header_guard_name, GetHeaderGuardName(file_descriptor));
  fw.AppendUnindentedLine(absl::StrCat("#ifndef ", header_guard_name));
  fw.AppendUnindentedLine(absl::StrCat("#define ", header_guard_name));
  fw.AppendEmptyLine();
  DEFINE_CONST_OR_RETURN(package, GetCppPackage(file_descriptor));
  fw.AppendLine(absl::StrCat("namespace ", package, " {"));
  fw.AppendEmptyLine();
  // TODO
  fw.AppendEmptyLine();
  fw.AppendLine(absl::StrCat("}  // namespace ", package));
  fw.AppendEmptyLine();
  fw.AppendUnindentedLine(absl::StrCat("#endif  // ", header_guard_name));
  CodeGeneratorResponse_File file;
  file.get<kCodeGeneratorResponseFileNameField>() = "proto.h";
  file.get<kCodeGeneratorResponseFileContentField>() = std::move(fw).Finish();
  return std::move(file);
}

absl::StatusOr<CodeGeneratorResponse_File> GenerateSourceFile(
    FileDescriptorProto const& file_descriptor) {
  FileWriter fw;
  fw.AppendUnindentedLine(absl::StrCat("#include \"", "\""));
  fw.AppendEmptyLine();
  DEFINE_CONST_OR_RETURN(package, GetCppPackage(file_descriptor));
  fw.AppendLine(absl::StrCat("namespace ", package, " {"));
  fw.AppendEmptyLine();
  // TODO
  fw.AppendEmptyLine();
  fw.AppendLine(absl::StrCat("}  // namespace ", package));
  CodeGeneratorResponse_File file;
  file.get<kCodeGeneratorResponseFileNameField>() = "proto.cc";
  file.get<kCodeGeneratorResponseFileContentField>() = std::move(fw).Finish();
  return std::move(file);
}

absl::StatusOr<CodeGeneratorResponse> Run(CodeGeneratorRequest const& request) {
  CodeGeneratorResponse response;
  response.get<kCodeGeneratorResponseSupportedFeaturesField>() =
      tsdb2::util::to_underlying(CodeGeneratorResponse_Feature::FEATURE_NONE);
  for (auto const& proto_file : request.get<kCodeGeneratorRequestProtoFileField>()) {
    DEFINE_VAR_OR_RETURN(header_file, GenerateHeaderFile(proto_file));
    response.get<kCodeGeneratorResponseFileField>().emplace_back(std::move(header_file));
    DEFINE_VAR_OR_RETURN(source_file, GenerateSourceFile(proto_file));
    response.get<kCodeGeneratorResponseFileField>().emplace_back(std::move(source_file));
  }
  return std::move(response);
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
