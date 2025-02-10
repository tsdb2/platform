#include "proto/generator.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

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
using ::tsdb2::proto::internal::DependencyManager;
using ::tsdb2::proto::internal::FileWriter;

}  // namespace

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

absl::StatusOr<CodeGeneratorResponse_File> GenerateHeaderFile(
    FileDescriptorProto const& file_descriptor) {
  DEFINE_VAR_OR_RETURN(content, GenerateHeaderFileContent(file_descriptor));
  CodeGeneratorResponse_File file;
  file.get<kCodeGeneratorResponseFileNameField>() = "proto.pb.h";
  file.get<kCodeGeneratorResponseFileContentField>() = std::move(content);
  return std::move(file);
}

absl::StatusOr<CodeGeneratorResponse_File> GenerateSourceFile(
    FileDescriptorProto const& file_descriptor) {
  DEFINE_VAR_OR_RETURN(content, GenerateSourceFileContent(file_descriptor));
  CodeGeneratorResponse_File file;
  file.get<kCodeGeneratorResponseFileNameField>() = "proto.pb.cc";
  file.get<kCodeGeneratorResponseFileContentField>() = std::move(content);
  return std::move(file);
}

}  // namespace generator
}  // namespace proto
}  // namespace tsdb2
