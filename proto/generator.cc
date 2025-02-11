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
namespace generator {

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

}  // namespace

tsdb2::common::NoDestructor<tsdb2::common::RE> const kPackagePattern{
    tsdb2::common::RE::CreateOrDie("^[A-Za-z][A-Za-z0-9]*(?:\\.[A-Za-z][A-Za-z0-9]*)*$")};

tsdb2::common::NoDestructor<tsdb2::common::RE> const kFileExtensionPattern{
    tsdb2::common::RE::CreateOrDie("(\\.[^./\\\\]*)$")};

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

absl::StatusOr<std::string> GetHeaderGuardName(FileDescriptorProto const& file_descriptor) {
  DEFINE_CONST_OR_RETURN(name, RequireField<kFileDescriptorProtoNameField>(file_descriptor));
  // TODO: we are replacing only path separators here, but file paths may contain many more symbols.
  std::string converted = absl::StrReplaceAll(*name, {{"/", "_"}, {"\\", "_"}});
  std::string_view extension;
  if (kFileExtensionPattern->PartialMatchArgs(converted, &extension)) {
    converted.erase(converted.size() - extension.size());
  }
  absl::AsciiStrToUpper(&converted);
  return absl::StrCat("__TSDB2_", converted, "_PB_H__");
}

absl::StatusOr<std::string> GetCppPackage(FileDescriptorProto const& file_descriptor) {
  DEFINE_CONST_OR_RETURN(package, RequireField<kFileDescriptorProtoPackageField>(file_descriptor));
  if (!kPackagePattern->Test(*package)) {
    return absl::InvalidArgumentError(
        absl::StrCat("package name \"", *package, "\" has an invalid format"));
  }
  return absl::StrReplaceAll(*package, {{".", "::"}});
}

absl::StatusOr<std::string> GenerateHeaderFileContent(
    google::protobuf::FileDescriptorProto const& file_descriptor) {
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
  return std::move(fw).Finish();
}

absl::StatusOr<std::string> GenerateSourceFileContent(
    google::protobuf::FileDescriptorProto const& file_descriptor) {
  FileWriter fw;
  fw.AppendUnindentedLine(absl::StrCat("#include \"", "\""));
  fw.AppendEmptyLine();
  DEFINE_CONST_OR_RETURN(package, GetCppPackage(file_descriptor));
  fw.AppendLine(absl::StrCat("namespace ", package, " {"));
  fw.AppendEmptyLine();
  // TODO
  fw.AppendEmptyLine();
  fw.AppendLine(absl::StrCat("}  // namespace ", package));
  return std::move(fw).Finish();
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

absl::StatusOr<CodeGeneratorResponse_File> GenerateHeaderFile(
    FileDescriptorProto const& file_descriptor) {
  DEFINE_CONST_OR_RETURN(name, RequireField<kFileDescriptorProtoNameField>(file_descriptor));
  DEFINE_VAR_OR_RETURN(content, GenerateHeaderFileContent(file_descriptor));
  CodeGeneratorResponse_File file;
  file.get<kCodeGeneratorResponseFileNameField>() = MakeHeaderFileName(*name);
  file.get<kCodeGeneratorResponseFileContentField>() = std::move(content);
  return std::move(file);
}

absl::StatusOr<CodeGeneratorResponse_File> GenerateSourceFile(
    FileDescriptorProto const& file_descriptor) {
  DEFINE_CONST_OR_RETURN(name, RequireField<kFileDescriptorProtoNameField>(file_descriptor));
  DEFINE_VAR_OR_RETURN(content, GenerateSourceFileContent(file_descriptor));
  CodeGeneratorResponse_File file;
  file.get<kCodeGeneratorResponseFileNameField>() = MakeSourceFileName(*name);
  file.get<kCodeGeneratorResponseFileContentField>() = std::move(content);
  return std::move(file);
}

}  // namespace generator
}  // namespace proto
}  // namespace tsdb2
