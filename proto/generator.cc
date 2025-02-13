#include "proto/generator.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/types/span.h"
#include "common/no_destructor.h"
#include "common/re.h"
#include "common/utilities.h"
#include "proto/dependencies.h"
#include "proto/descriptor.h"
#include "proto/file_writer.h"
#include "proto/object.h"

namespace tsdb2 {
namespace proto {

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
using ::tsdb2::proto::RequireField;
using ::tsdb2::proto::internal::DependencyManager;
using ::tsdb2::proto::internal::FileWriter;

tsdb2::common::NoDestructor<tsdb2::common::RE> const kPackagePattern{
    tsdb2::common::RE::CreateOrDie("^[A-Za-z][A-Za-z0-9]*(?:\\.[A-Za-z][A-Za-z0-9]*)*$")};

tsdb2::common::NoDestructor<tsdb2::common::RE> const kFileExtensionPattern{
    tsdb2::common::RE::CreateOrDie("(\\.[^./\\\\]*)$")};

}  // namespace

namespace generator {

absl::StatusOr<std::vector<uint8_t>> ReadFile(FILE* const fp) {
  std::vector<uint8_t> buffer;
  uint8_t temp[4096];
  while (size_t bytesRead = ::fread(temp, 1, sizeof(temp), fp)) {
    buffer.insert(buffer.end(), temp, temp + bytesRead);
  }
  if (::ferror(stdin) != 0) {
    return absl::ErrnoToStatus(errno, "fread");
  }
  return std::move(buffer);
}

absl::Status WriteFile(FILE* const fp, absl::Span<uint8_t const> const data) {
  if (::fwrite(data.data(), 1, data.size(), fp) != data.size()) {
    return absl::ErrnoToStatus(errno, "fwrite");
  }
  return absl::OkStatus();
}

std::string MakeHeaderFileName(std::string_view proto_file_name) {
  std::string_view extension;
  if (kFileExtensionPattern->PartialMatchArgs(proto_file_name, &extension)) {
    proto_file_name.remove_suffix(extension.size());
  }
  return absl::StrCat(proto_file_name, ".pb.h");
}

std::string MakeSourceFileName(std::string_view proto_file_name) {
  std::string_view extension;
  if (kFileExtensionPattern->PartialMatchArgs(proto_file_name, &extension)) {
    proto_file_name.remove_suffix(extension.size());
  }
  return absl::StrCat(proto_file_name, ".pb.cc");
}

}  // namespace generator

Generator::Generator(google::protobuf::FileDescriptorProto const& file_descriptor)
    : file_descriptor_(file_descriptor) {
  // TODO
}

absl::StatusOr<std::string> Generator::GenerateHeaderFileContent() {
  DependencyManager dm;
  FileWriter fw;
  DEFINE_CONST_OR_RETURN(header_guard_name, GetHeaderGuardName());
  fw.AppendUnindentedLine(absl::StrCat("#ifndef ", header_guard_name));
  fw.AppendUnindentedLine(absl::StrCat("#define ", header_guard_name));
  fw.AppendEmptyLine();
  fw.AppendUnindentedLine("#include <optional>");
  fw.AppendUnindentedLine("#include <string>");
  fw.AppendUnindentedLine("#include <vector>");
  fw.AppendEmptyLine();
  fw.AppendUnindentedLine("#include \"proto/wire_format.h\"");
  fw.AppendEmptyLine();
  DEFINE_CONST_OR_RETURN(package, GetCppPackage());
  fw.AppendLine(absl::StrCat("namespace ", package, " {"));
  fw.AppendEmptyLine();
  // TODO
  fw.AppendEmptyLine();
  fw.AppendLine(absl::StrCat("}  // namespace ", package));
  fw.AppendEmptyLine();
  fw.AppendUnindentedLine(absl::StrCat("#endif  // ", header_guard_name));
  return std::move(fw).Finish();
}

absl::StatusOr<std::string> Generator::GenerateSourceFileContent() {
  FileWriter fw;
  DEFINE_CONST_OR_RETURN(header_path, GetHeaderPath());
  fw.AppendUnindentedLine(absl::StrCat("#include \"", header_path, "\""));
  fw.AppendEmptyLine();
  DEFINE_CONST_OR_RETURN(package, GetCppPackage());
  fw.AppendLine(absl::StrCat("namespace ", package, " {"));
  fw.AppendEmptyLine();
  // TODO
  fw.AppendEmptyLine();
  fw.AppendLine(absl::StrCat("}  // namespace ", package));
  return std::move(fw).Finish();
}

absl::StatusOr<CodeGeneratorResponse_File> Generator::GenerateHeaderFile() {
  DEFINE_CONST_OR_RETURN(name, RequireField<kFileDescriptorProtoNameField>(file_descriptor_));
  DEFINE_VAR_OR_RETURN(content, GenerateHeaderFileContent());
  CodeGeneratorResponse_File file;
  file.get<kCodeGeneratorResponseFileNameField>() = generator::MakeHeaderFileName(*name);
  file.get<kCodeGeneratorResponseFileContentField>() = std::move(content);
  return std::move(file);
}

absl::StatusOr<CodeGeneratorResponse_File> Generator::GenerateSourceFile() {
  DEFINE_CONST_OR_RETURN(name, RequireField<kFileDescriptorProtoNameField>(file_descriptor_));
  DEFINE_VAR_OR_RETURN(content, GenerateSourceFileContent());
  CodeGeneratorResponse_File file;
  file.get<kCodeGeneratorResponseFileNameField>() = generator::MakeSourceFileName(*name);
  file.get<kCodeGeneratorResponseFileContentField>() = std::move(content);
  return std::move(file);
}

absl::StatusOr<std::string> Generator::GetHeaderPath() {
  DEFINE_CONST_OR_RETURN(name, RequireField<kFileDescriptorProtoNameField>(file_descriptor_));
  std::string converted = *name;
  std::string_view extension;
  if (kFileExtensionPattern->PartialMatchArgs(converted, &extension)) {
    converted.erase(converted.size() - extension.size());
  }
  return absl::StrCat(converted, ".pb.h");
}

absl::StatusOr<std::string> Generator::GetHeaderGuardName() {
  DEFINE_CONST_OR_RETURN(name, RequireField<kFileDescriptorProtoNameField>(file_descriptor_));
  // TODO: we are replacing only path separators here, but file paths may contain many more symbols.
  std::string converted = absl::StrReplaceAll(*name, {{"/", "_"}, {"\\", "_"}});
  std::string_view extension;
  if (kFileExtensionPattern->PartialMatchArgs(converted, &extension)) {
    converted.erase(converted.size() - extension.size());
  }
  absl::AsciiStrToUpper(&converted);
  return absl::StrCat("__TSDB2_", converted, "_PB_H__");
}

absl::StatusOr<std::string> Generator::GetCppPackage() {
  DEFINE_CONST_OR_RETURN(package, RequireField<kFileDescriptorProtoPackageField>(file_descriptor_));
  if (!kPackagePattern->Test(*package)) {
    return absl::InvalidArgumentError(
        absl::StrCat("package name \"", *package, "\" has an invalid format"));
  }
  return absl::StrReplaceAll(*package, {{".", "::"}});
}

}  // namespace proto
}  // namespace tsdb2
