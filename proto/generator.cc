#include "proto/generator.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"
#include "absl/types/span.h"
#include "common/flat_map.h"
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

using ::google::protobuf::FieldDescriptorProto_Label;
using ::google::protobuf::FieldDescriptorProto_Type;
using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::kDescriptorProtoEnumTypeField;
using ::google::protobuf::kDescriptorProtoFieldField;
using ::google::protobuf::kDescriptorProtoNameField;
using ::google::protobuf::kEnumDescriptorProtoNameField;
using ::google::protobuf::kEnumDescriptorProtoValueField;
using ::google::protobuf::kEnumValueDescriptorProtoNameField;
using ::google::protobuf::kEnumValueDescriptorProtoNumberField;
using ::google::protobuf::kFieldDescriptorProtoLabelField;
using ::google::protobuf::kFieldDescriptorProtoNameField;
using ::google::protobuf::kFieldDescriptorProtoTypeField;
using ::google::protobuf::kFieldDescriptorProtoTypeNameField;
using ::google::protobuf::kFileDescriptorProtoEnumTypeField;
using ::google::protobuf::kFileDescriptorProtoMessageTypeField;
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

tsdb2::common::NoDestructor<tsdb2::common::RE> const kIdentifierPattern{
    tsdb2::common::RE::CreateOrDie("^[_A-Za-z][_A-Za-z0-9]*$")};

tsdb2::common::NoDestructor<tsdb2::common::RE> const kPackagePattern{
    tsdb2::common::RE::CreateOrDie("^[_A-Za-z][_A-Za-z0-9]*(?:\\.[_A-Za-z][_A-Za-z0-9]*)*$")};

tsdb2::common::NoDestructor<tsdb2::common::RE> const kFileExtensionPattern{
    tsdb2::common::RE::CreateOrDie("(\\.[^./\\\\]*)$")};

auto constexpr kFieldTypeNames =
    tsdb2::common::fixed_flat_map_of<FieldDescriptorProto_Type, std::string_view>({
        {FieldDescriptorProto_Type::TYPE_DOUBLE, "double"},
        {FieldDescriptorProto_Type::TYPE_FLOAT, "float"},
        {FieldDescriptorProto_Type::TYPE_INT64, "int64_t"},
        {FieldDescriptorProto_Type::TYPE_UINT64, "uint64_t"},
        {FieldDescriptorProto_Type::TYPE_INT32, "int32_t"},
        {FieldDescriptorProto_Type::TYPE_FIXED64, "uint64_t"},
        {FieldDescriptorProto_Type::TYPE_FIXED32, "uint32_t"},
        {FieldDescriptorProto_Type::TYPE_BOOL, "bool"},
        {FieldDescriptorProto_Type::TYPE_STRING, "std::string"},
        {FieldDescriptorProto_Type::TYPE_BYTES, "std::vector<uint8_t>"},
        {FieldDescriptorProto_Type::TYPE_UINT32, "uint32_t"},
        {FieldDescriptorProto_Type::TYPE_SFIXED32, "int32_t"},
        {FieldDescriptorProto_Type::TYPE_SFIXED64, "int64_t"},
        {FieldDescriptorProto_Type::TYPE_SINT32, "int32_t"},
        {FieldDescriptorProto_Type::TYPE_SINT64, "int64_t"},
    });

std::string StringifyCycleItem(std::pair<DependencyManager::Path, std::string> const& item) {
  return absl::StrCat(absl::StrJoin(item.first, "."), ".", item.second);
}

std::string MakeCycleMessage(DependencyManager::Cycle const& cycle) {
  std::vector<std::string> entries;
  entries.reserve(cycle.size() + 1);
  for (auto const& item : cycle) {
    entries.emplace_back(StringifyCycleItem(item));
  }
  if (!cycle.empty()) {
    auto const& first_path = cycle.front().first;
    entries.emplace_back(absl::StrJoin(first_path, "."));
  }
  return absl::StrJoin(entries, " -> ");
}

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

absl::StatusOr<Generator> Generator::Create(
    google::protobuf::FileDescriptorProto const& file_descriptor) {
  auto const package_path = GetPackagePath(file_descriptor);
  DependencyManager dependencies;
  for (auto const& enum_type : file_descriptor.get<kFileDescriptorProtoEnumTypeField>()) {
    DEFINE_CONST_OR_RETURN(name, RequireField<kEnumDescriptorProtoNameField>(enum_type));
    dependencies.AddNode({*name});
  }
  for (auto const& message_type : file_descriptor.get<kFileDescriptorProtoMessageTypeField>()) {
    DEFINE_CONST_OR_RETURN(message_name, RequireField<kEnumDescriptorProtoNameField>(message_type));
    dependencies.AddNode({*message_name});
    for (auto const& enum_type : message_type.get<kDescriptorProtoEnumTypeField>()) {
      DEFINE_CONST_OR_RETURN(enum_name, RequireField<kEnumDescriptorProtoNameField>(enum_type));
      dependencies.AddNode({*message_name, *enum_name});
    }
  }
  for (auto const& message_type : file_descriptor.get<kFileDescriptorProtoMessageTypeField>()) {
    DEFINE_CONST_OR_RETURN(message_name, RequireField<kEnumDescriptorProtoNameField>(message_type));
    for (auto const& field : message_type.get<kDescriptorProtoFieldField>()) {
      auto const& maybe_type_name = field.get<kFieldDescriptorProtoTypeNameField>();
      if (maybe_type_name.has_value()) {
        DEFINE_CONST_OR_RETURN(label, RequireField<kFieldDescriptorProtoLabelField>(field));
        if (*label != FieldDescriptorProto_Label::LABEL_REPEATED) {
          DEFINE_CONST_OR_RETURN(type_path, GetTypePath(package_path, maybe_type_name.value()));
          DEFINE_CONST_OR_RETURN(field_name, RequireField<kFieldDescriptorProtoNameField>(field));
          dependencies.AddDependency({*message_name}, type_path, *field_name);
        }
      }
    }
  }
  auto const cycles = dependencies.FindCycles({});
  if (!cycles.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("message dependency cycle detected: ", MakeCycleMessage(cycles.front())));
  }
  return Generator(file_descriptor, std::move(dependencies));
}

absl::StatusOr<std::string> Generator::GenerateHeaderFileContent() {
  internal::FileWriter fw;
  DEFINE_CONST_OR_RETURN(header_guard_name, GetHeaderGuardName());
  fw.AppendUnindentedLine(absl::StrCat("#ifndef ", header_guard_name));
  fw.AppendUnindentedLine(absl::StrCat("#define ", header_guard_name));
  fw.AppendEmptyLine();
  fw.AppendUnindentedLine("#include <cstdint>");
  fw.AppendUnindentedLine("#include <optional>");
  fw.AppendUnindentedLine("#include <string>");
  fw.AppendUnindentedLine("#include <vector>");
  fw.AppendEmptyLine();
  fw.AppendUnindentedLine("#include \"proto/wire_format.h\"");
  DEFINE_CONST_OR_RETURN(package, GetCppPackage());
  if (!package.empty()) {
    fw.AppendEmptyLine();
    fw.AppendLine(absl::StrCat("namespace ", package, " {"));
  }
  auto const& messages = file_descriptor_.get<kFileDescriptorProtoMessageTypeField>();
  if (!messages.empty()) {
    fw.AppendEmptyLine();
    for (auto const& message_type : messages) {
      RETURN_IF_ERROR(AppendForwardDeclaration(&fw, message_type));
    }
  }
  for (auto const& enum_type : file_descriptor_.get<kFileDescriptorProtoEnumTypeField>()) {
    fw.AppendEmptyLine();
    RETURN_IF_ERROR(AppendEnum(&fw, enum_type));
  }
  RETURN_IF_ERROR(AppendMessages(&fw, messages));
  fw.AppendEmptyLine();
  if (!package.empty()) {
    fw.AppendLine(absl::StrCat("}  // namespace ", package));
    fw.AppendEmptyLine();
  }
  fw.AppendUnindentedLine(absl::StrCat("#endif  // ", header_guard_name));
  return std::move(fw).Finish();
}

absl::StatusOr<std::string> Generator::GenerateSourceFileContent() {
  internal::FileWriter fw;
  DEFINE_CONST_OR_RETURN(header_path, GetHeaderPath());
  fw.AppendUnindentedLine(absl::StrCat("#include \"", header_path, "\""));
  DEFINE_CONST_OR_RETURN(package, GetCppPackage());
  if (!package.empty()) {
    fw.AppendEmptyLine();
    fw.AppendLine(absl::StrCat("namespace ", package, " {"));
  }
  for (auto const& message_type : file_descriptor_.get<kFileDescriptorProtoMessageTypeField>()) {
    // TODO
  }
  if (!package.empty()) {
    fw.AppendEmptyLine();
    fw.AppendLine(absl::StrCat("}  // namespace ", package));
  }
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

DependencyManager::Path Generator::GetPackagePath(
    google::protobuf::FileDescriptorProto const& file_descriptor) {
  auto const& maybe_package_name = file_descriptor.get<kFileDescriptorProtoPackageField>();
  if (!maybe_package_name.has_value()) {
    return DependencyManager::Path();
  }
  auto const& package_name = maybe_package_name.value();
  if (package_name.empty()) {
    return DependencyManager::Path();
  }
  DependencyManager::Path path;
  for (auto const component : absl::StrSplit(package_name, '.')) {
    path.emplace_back(component);
  }
  return path;
}

absl::StatusOr<DependencyManager::Path> Generator::GetTypePath(
    DependencyManager::PathView const package_path, std::string_view const type_name) {
  auto const splitter = absl::StrSplit(type_name, '.');
  std::vector<std::string> const components{splitter.begin(), splitter.end()};
  if (components.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid type name: \"", absl::CEscape(type_name), "\""));
  }
  if (components.front().empty()) {
    // TODO: implement references to external types.
    if (components.size() < package_path.size() + 2) {
      return absl::UnimplementedError(absl::StrCat("cannot resolve \"", absl::CEscape(type_name),
                                                   "\": external types not yet implemented"));
    }
    size_t offset = 1;
    for (; offset <= package_path.size(); ++offset) {
      if (components[offset] != package_path[offset - 1]) {
        return absl::UnimplementedError(absl::StrCat("cannot resolve \"", absl::CEscape(type_name),
                                                     "\": external types not yet implemented"));
      }
    }
    return DependencyManager::Path{components.begin() + offset, components.end()};
  } else {
    // TODO: C++ semantics search algorithm
    return absl::UnimplementedError(
        absl::StrCat("cannot resolve \"", absl::CEscape(type_name),
                     "\": partially qualified types not yet implemented"));
  }
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
  auto const& maybe_package = file_descriptor_.get<kFileDescriptorProtoPackageField>();
  if (!maybe_package.has_value()) {
    return "";
  }
  auto const& package_name = maybe_package.value();
  if (!kPackagePattern->Test(package_name)) {
    return absl::InvalidArgumentError(
        absl::StrCat("package name \"", absl::CEscape(package_name), "\" has an invalid format"));
  }
  return absl::StrReplaceAll(package_name, {{".", "::"}});
}

absl::Status Generator::AppendEnum(internal::FileWriter* const writer,
                                   google::protobuf::EnumDescriptorProto const& enum_descriptor) {
  DEFINE_CONST_OR_RETURN(name, RequireField<kEnumDescriptorProtoNameField>(enum_descriptor));
  if (!kIdentifierPattern->Test(*name)) {
    return absl::InvalidArgumentError(absl::StrCat("invalid enum name: \"", *name, "\""));
  }
  writer->AppendLine(absl::StrCat("enum class ", *name, " {"));
  {
    internal::FileWriter::IndentedScope is{writer};
    for (auto const& value : enum_descriptor.get<kEnumDescriptorProtoValueField>()) {
      DEFINE_CONST_OR_RETURN(name, RequireField<kEnumValueDescriptorProtoNameField>(value));
      if (!kIdentifierPattern->Test(*name)) {
        return absl::InvalidArgumentError(absl::StrCat("invalid enum value name: \"", *name, "\""));
      }
      DEFINE_CONST_OR_RETURN(number, RequireField<kEnumValueDescriptorProtoNumberField>(value));
      writer->AppendLine(absl::StrCat(*name, " = ", *number, ","));
    }
  }
  writer->AppendLine("};");
  return absl::OkStatus();
}

absl::StatusOr<std::string> Generator::GetFieldType(
    google::protobuf::FieldDescriptorProto const& descriptor) {
  auto const& maybe_type_name = descriptor.get<kFieldDescriptorProtoTypeNameField>();
  if (maybe_type_name.has_value()) {
    return absl::StrReplaceAll(maybe_type_name.value(), {{".", "::"}});
  }
  DEFINE_CONST_OR_RETURN(type, RequireField<kFieldDescriptorProtoTypeField>(descriptor));
  auto const it = kFieldTypeNames.find(*type);
  if (it == kFieldTypeNames.end()) {
    return absl::InvalidArgumentError("invalid field type");
  }
  return std::string(it->second);
}

absl::Status Generator::AppendForwardDeclaration(
    internal::FileWriter* const writer, google::protobuf::DescriptorProto const& descriptor) {
  DEFINE_CONST_OR_RETURN(name, RequireField<kDescriptorProtoNameField>(descriptor));
  if (!kIdentifierPattern->Test(*name)) {
    return absl::InvalidArgumentError(absl::StrCat("invalid message name: \"", *name, "\""));
  }
  writer->AppendLine(absl::StrCat("struct ", *name, ";"));
  return absl::OkStatus();
}

absl::Status Generator::AppendMessage(internal::FileWriter* const writer,
                                      google::protobuf::DescriptorProto const& descriptor) {
  DEFINE_CONST_OR_RETURN(name, RequireField<kDescriptorProtoNameField>(descriptor));
  if (!kIdentifierPattern->Test(*name)) {
    return absl::InvalidArgumentError(absl::StrCat("invalid message name: \"", *name, "\""));
  }
  writer->AppendLine(absl::StrCat("struct ", *name, " {"));
  {
    internal::FileWriter::IndentedScope is{writer};
    for (auto const& enum_type : descriptor.get<kDescriptorProtoEnumTypeField>()) {
      RETURN_IF_ERROR(AppendEnum(writer, enum_type));
      writer->AppendEmptyLine();
    }
    for (auto const& field : descriptor.get<kDescriptorProtoFieldField>()) {
      DEFINE_CONST_OR_RETURN(name, RequireField<kFieldDescriptorProtoNameField>(field));
      if (!kIdentifierPattern->Test(*name)) {
        return absl::InvalidArgumentError(absl::StrCat("invalid field name: \"", *name, "\""));
      }
      DEFINE_CONST_OR_RETURN(label, RequireField<kFieldDescriptorProtoLabelField>(field));
      DEFINE_CONST_OR_RETURN(type, GetFieldType(field));
      switch (*label) {
        case FieldDescriptorProto_Label::LABEL_OPTIONAL:
          writer->AppendLine(absl::StrCat("std::optional<", type, "> ", *name, ";"));
          break;
        case FieldDescriptorProto_Label::LABEL_REPEATED:
          writer->AppendLine(absl::StrCat("std::vector<", type, "> ", *name, ";"));
          break;
        case FieldDescriptorProto_Label::LABEL_REQUIRED:
          writer->AppendLine(absl::StrCat(type, " ", *name, ";"));
          break;
        default:
          return absl::InvalidArgumentError("unknown value for field label");
      }
    }
  }
  writer->AppendLine("};");
  return absl::OkStatus();
}

absl::Status Generator::AppendMessages(
    internal::FileWriter* const writer,
    absl::Span<google::protobuf::DescriptorProto const> const descriptors) {
  absl::flat_hash_map<std::string, google::protobuf::DescriptorProto const*> descriptors_by_name;
  descriptors_by_name.reserve(descriptors.size());
  for (auto const& descriptor : descriptors) {
    DEFINE_CONST_OR_RETURN(name, RequireField<kDescriptorProtoNameField>(descriptor));
    descriptors_by_name.try_emplace(*name, &descriptor);
  }
  for (auto const& name : dependencies_.MakeOrder({})) {
    writer->AppendEmptyLine();
    auto const it = descriptors_by_name.find(name);
    if (it != descriptors_by_name.end()) {
      // If not found it means `name` refers to an enum, otherwise it's a regular message. We don't
      // need to process enums here because they're always defined at the start of every lexical
      // scope.
      RETURN_IF_ERROR(AppendMessage(writer, *it->second));
    }
  }
  return absl::OkStatus();
}

}  // namespace proto
}  // namespace tsdb2
