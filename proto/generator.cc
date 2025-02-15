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
#include "absl/container/flat_hash_set.h"
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
using ::google::protobuf::kFieldDescriptorProtoNumberField;
using ::google::protobuf::kFieldDescriptorProtoOptionsField;
using ::google::protobuf::kFieldDescriptorProtoTypeField;
using ::google::protobuf::kFieldDescriptorProtoTypeNameField;
using ::google::protobuf::kFieldOptionsPackedField;
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
using ::tsdb2::proto::internal::FileWriter;

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

auto constexpr kFieldEncoderNames =
    tsdb2::common::fixed_flat_map_of<FieldDescriptorProto_Type, std::string_view>({
        {FieldDescriptorProto_Type::TYPE_DOUBLE, "EncodeDoubleField"},
        {FieldDescriptorProto_Type::TYPE_FLOAT, "EncodeFloatField"},
        {FieldDescriptorProto_Type::TYPE_INT64, "EncodeInt64Field"},
        {FieldDescriptorProto_Type::TYPE_UINT64, "EncodeUInt64Field"},
        {FieldDescriptorProto_Type::TYPE_INT32, "EncodeInt32Field"},
        {FieldDescriptorProto_Type::TYPE_FIXED64, "EncodeFixedUInt64Field"},
        {FieldDescriptorProto_Type::TYPE_FIXED32, "EncodeFixedUInt32Field"},
        {FieldDescriptorProto_Type::TYPE_BOOL, "EncodeBoolField"},
        {FieldDescriptorProto_Type::TYPE_STRING, "EncodeStringField"},
        {FieldDescriptorProto_Type::TYPE_BYTES, "EncodeBytesField"},
        {FieldDescriptorProto_Type::TYPE_UINT32, "EncodeUInt32Field"},
        {FieldDescriptorProto_Type::TYPE_SFIXED32, "EncodeFixedInt32Field"},
        {FieldDescriptorProto_Type::TYPE_SFIXED64, "EncodeFixedInt64Field"},
        {FieldDescriptorProto_Type::TYPE_SINT32, "EncodeSInt32Field"},
        {FieldDescriptorProto_Type::TYPE_SINT64, "EncodeSInt64Field"},
    });

auto constexpr kPackedFieldEncoderNames =
    tsdb2::common::fixed_flat_map_of<FieldDescriptorProto_Type, std::string_view>({
        {FieldDescriptorProto_Type::TYPE_DOUBLE, "EncodePackedDoubles"},
        {FieldDescriptorProto_Type::TYPE_FLOAT, "EncodePackedFloats"},
        {FieldDescriptorProto_Type::TYPE_INT64, "EncodePackedInt64s"},
        {FieldDescriptorProto_Type::TYPE_UINT64, "EncodePackedUInt64s"},
        {FieldDescriptorProto_Type::TYPE_INT32, "EncodePackedInt32s"},
        {FieldDescriptorProto_Type::TYPE_FIXED64, "EncodePackedFixedUInt64s"},
        {FieldDescriptorProto_Type::TYPE_FIXED32, "EncodePackedFixedUInt32s"},
        {FieldDescriptorProto_Type::TYPE_BOOL, "EncodePackedBools"},
        {FieldDescriptorProto_Type::TYPE_UINT32, "EncodePackedUInt32s"},
        {FieldDescriptorProto_Type::TYPE_SFIXED32, "EncodePackedFixedInt32s"},
        {FieldDescriptorProto_Type::TYPE_SFIXED64, "EncodePackedFixedInt64s"},
        {FieldDescriptorProto_Type::TYPE_SINT32, "EncodePackedSInt32s"},
        {FieldDescriptorProto_Type::TYPE_SINT64, "EncodePackedSInt64s"},
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
  absl::flat_hash_set<internal::DependencyManager::Path> enums;
  DependencyManager dependencies;
  for (auto const& enum_type : file_descriptor.get<kFileDescriptorProtoEnumTypeField>()) {
    DEFINE_CONST_OR_RETURN(name, RequireField<kEnumDescriptorProtoNameField>(enum_type));
    DependencyManager::Path const path{*name};
    enums.emplace(path);
    dependencies.AddNode(path);
  }
  for (auto const& message_type : file_descriptor.get<kFileDescriptorProtoMessageTypeField>()) {
    DEFINE_CONST_OR_RETURN(message_name, RequireField<kEnumDescriptorProtoNameField>(message_type));
    dependencies.AddNode({*message_name});
    for (auto const& enum_type : message_type.get<kDescriptorProtoEnumTypeField>()) {
      DEFINE_CONST_OR_RETURN(enum_name, RequireField<kEnumDescriptorProtoNameField>(enum_type));
      DependencyManager::Path const path{*message_name, *enum_name};
      enums.emplace(path);
      dependencies.AddNode(path);
    }
  }
  for (auto const& message_type : file_descriptor.get<kFileDescriptorProtoMessageTypeField>()) {
    DEFINE_CONST_OR_RETURN(message_name, RequireField<kEnumDescriptorProtoNameField>(message_type));
    for (auto const& field : message_type.get<kDescriptorProtoFieldField>()) {
      auto const& maybe_type_name = field.get<kFieldDescriptorProtoTypeNameField>();
      if (maybe_type_name.has_value()) {
        DEFINE_CONST_OR_RETURN(label, RequireField<kFieldDescriptorProtoLabelField>(field));
        if (*label != FieldDescriptorProto_Label::LABEL_REPEATED) {
          DEFINE_CONST_OR_RETURN(type_path, GetTypePath(file_descriptor, maybe_type_name.value()));
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
  return Generator(file_descriptor, std::move(enums), std::move(dependencies));
}

absl::StatusOr<std::string> Generator::GenerateHeaderFileContent() {
  FileWriter fw;
  DEFINE_CONST_OR_RETURN(header_guard_name, GetHeaderGuardName());
  fw.AppendUnindentedLine(absl::StrCat("#ifndef ", header_guard_name));
  fw.AppendUnindentedLine(absl::StrCat("#define ", header_guard_name));
  fw.AppendEmptyLine();
  fw.AppendUnindentedLine("#include <cstdint>");
  fw.AppendUnindentedLine("#include <optional>");
  fw.AppendUnindentedLine("#include <string>");
  fw.AppendUnindentedLine("#include <vector>");
  fw.AppendEmptyLine();
  fw.AppendUnindentedLine("#include \"absl/status/statusor.h\"");
  fw.AppendUnindentedLine("#include \"absl/types/span.h\"");
  fw.AppendUnindentedLine("#include \"io/cord.h\"");
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
  RETURN_IF_ERROR(AppendMessageHeaders(&fw, messages));
  if (!package.empty()) {
    fw.AppendEmptyLine();
    fw.AppendLine(absl::StrCat("}  // namespace ", package));
  }
  fw.AppendEmptyLine();
  fw.AppendUnindentedLine(absl::StrCat("#endif  // ", header_guard_name));
  return std::move(fw).Finish();
}

absl::StatusOr<std::string> Generator::GenerateSourceFileContent() {
  FileWriter fw;
  DEFINE_CONST_OR_RETURN(header_path, GetHeaderPath());
  fw.AppendUnindentedLine(absl::StrCat("#include \"", header_path, "\""));
  fw.AppendEmptyLine();
  fw.AppendUnindentedLine("#include <cstdint>");
  fw.AppendUnindentedLine("#include <utility>");
  fw.AppendEmptyLine();
  fw.AppendUnindentedLine("#include \"absl/status/statusor.h\"");
  fw.AppendUnindentedLine("#include \"absl/types/span.h\"");
  fw.AppendUnindentedLine("#include \"common/utilities.h\"");
  fw.AppendUnindentedLine("#include \"io/cord.h\"");
  fw.AppendUnindentedLine("#include \"proto/wire_format.h\"");
  DEFINE_CONST_OR_RETURN(package, GetCppPackage());
  if (!package.empty()) {
    fw.AppendEmptyLine();
    fw.AppendLine(absl::StrCat("namespace ", package, " {"));
  }
  for (auto const& message_type : file_descriptor_.get<kFileDescriptorProtoMessageTypeField>()) {
    fw.AppendEmptyLine();
    RETURN_IF_ERROR(EmitMessageImplementation(&fw, message_type));
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
    google::protobuf::FileDescriptorProto const& descriptor,
    std::string_view const proto_type_name) {
  auto const package_path = GetPackagePath(descriptor);
  auto const splitter = absl::StrSplit(proto_type_name, '.');
  std::vector<std::string> const components{splitter.begin(), splitter.end()};
  if (components.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid type name: \"", absl::CEscape(proto_type_name), "\""));
  }
  if (components.front().empty()) {
    // TODO: implement references to external types.
    if (components.size() < package_path.size() + 2) {
      return absl::UnimplementedError(absl::StrCat("cannot resolve \"",
                                                   absl::CEscape(proto_type_name),
                                                   "\": external types not yet implemented"));
    }
    size_t offset = 1;
    for (; offset <= package_path.size(); ++offset) {
      if (components[offset] != package_path[offset - 1]) {
        return absl::UnimplementedError(absl::StrCat("cannot resolve \"",
                                                     absl::CEscape(proto_type_name),
                                                     "\": external types not yet implemented"));
      }
    }
    return DependencyManager::Path{components.begin() + offset, components.end()};
  } else {
    // TODO: C++ semantics search algorithm
    return absl::UnimplementedError(
        absl::StrCat("cannot resolve \"", absl::CEscape(proto_type_name),
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

absl::Status Generator::AppendEnum(FileWriter* const writer,
                                   google::protobuf::EnumDescriptorProto const& enum_descriptor) {
  DEFINE_CONST_OR_RETURN(name, RequireField<kEnumDescriptorProtoNameField>(enum_descriptor));
  if (!kIdentifierPattern->Test(*name)) {
    return absl::InvalidArgumentError(absl::StrCat("invalid enum name: \"", *name, "\""));
  }
  writer->AppendLine(absl::StrCat("enum class ", *name, " {"));
  {
    FileWriter::IndentedScope is{writer};
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
    FileWriter* const writer, google::protobuf::DescriptorProto const& descriptor) {
  DEFINE_CONST_OR_RETURN(name, RequireField<kDescriptorProtoNameField>(descriptor));
  if (!kIdentifierPattern->Test(*name)) {
    return absl::InvalidArgumentError(absl::StrCat("invalid message name: \"", *name, "\""));
  }
  writer->AppendLine(absl::StrCat("struct ", *name, ";"));
  return absl::OkStatus();
}

absl::Status Generator::AppendMessageHeader(FileWriter* const writer,
                                            google::protobuf::DescriptorProto const& descriptor) {
  DEFINE_CONST_OR_RETURN(name, RequireField<kDescriptorProtoNameField>(descriptor));
  if (!kIdentifierPattern->Test(*name)) {
    return absl::InvalidArgumentError(absl::StrCat("invalid message name: \"", *name, "\""));
  }
  writer->AppendLine(absl::StrCat("struct ", *name, " {"));
  {
    FileWriter::IndentedScope is{writer};
    for (auto const& enum_type : descriptor.get<kDescriptorProtoEnumTypeField>()) {
      RETURN_IF_ERROR(AppendEnum(writer, enum_type));
      writer->AppendEmptyLine();
    }
    writer->AppendLine(absl::StrCat("static ::absl::StatusOr<", *name,
                                    "> Decode(::absl::Span<uint8_t const> data);"));
    writer->AppendLine(absl::StrCat("static ::tsdb2::io::Cord Encode(", *name, " const& proto);"));
    writer->AppendEmptyLine();
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

absl::Status Generator::AppendMessageHeaders(
    FileWriter* const writer,
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
      RETURN_IF_ERROR(AppendMessageHeader(writer, *it->second));
    }
  }
  return absl::OkStatus();
}

absl::Status Generator::EmitFieldDecoding(
    internal::FileWriter* const writer,
    google::protobuf::FieldDescriptorProto const& descriptor) const {
  DEFINE_CONST_OR_RETURN(name, RequireField<kFieldDescriptorProtoNameField>(descriptor));
  DEFINE_CONST_OR_RETURN(number, RequireField<kFieldDescriptorProtoNumberField>(descriptor));
  DEFINE_CONST_OR_RETURN(label, RequireField<kFieldDescriptorProtoLabelField>(descriptor));
  writer->AppendLine(absl::StrCat("case ", *number, ": {"));
  auto const& maybe_type_name = descriptor.get<kFieldDescriptorProtoTypeNameField>();
  if (maybe_type_name.has_value()) {
    DEFINE_CONST_OR_RETURN(is_message, IsMessage(maybe_type_name.value()));
    std::string const type_name = absl::StrReplaceAll(maybe_type_name.value(), {{".", "::"}});
    FileWriter::IndentedScope is{writer};
    if (is_message) {
      writer->AppendLine(
          "DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));");
      writer->AppendLine(
          absl::StrCat("DEFINE_VAR_OR_RETURN(value, ", type_name, "::Decode(child_span));"));
      switch (*label) {
        case FieldDescriptorProto_Label::LABEL_OPTIONAL:
          writer->AppendLine(absl::StrCat("proto.", *name, ".emplace(std::move(value));"));
          break;
        case FieldDescriptorProto_Label::LABEL_REQUIRED:
          writer->AppendLine(absl::StrCat("proto.", *name, " = std::move(value);"));
          break;
        case FieldDescriptorProto_Label::LABEL_REPEATED:
          writer->AppendLine(absl::StrCat("proto.", *name, ".emplace_back(std::move(value));"));
          break;
        default:
          return absl::InvalidArgumentError("invalid field label");
      }
    } else {
      writer->AppendLine(
          absl::StrCat("DEFINE_CONST_OR_RETURN(value, decoder.DecodeUInt64(tag.wire_type));"));
      switch (*label) {
        case FieldDescriptorProto_Label::LABEL_OPTIONAL:
          writer->AppendLine(
              absl::StrCat("proto.", *name, ".emplace(static_cast<", type_name, ">(value));"));
          break;
        case FieldDescriptorProto_Label::LABEL_REQUIRED:
          writer->AppendLine(
              absl::StrCat("proto.", *name, " = static_cast<", type_name, ">(value);"));
          break;
        case FieldDescriptorProto_Label::LABEL_REPEATED:
          writer->AppendLine(
              absl::StrCat("proto.", *name, ".emplace_back(static_cast<", type_name, ">(value));"));
          break;
        default:
          return absl::InvalidArgumentError("invalid field label");
      }
    }
  } else {
    DEFINE_CONST_OR_RETURN(type, RequireField<kFieldDescriptorProtoTypeField>(descriptor));
    FileWriter::IndentedScope is{writer};
    switch (*type) {
      case FieldDescriptorProto_Type::TYPE_DOUBLE:
        writer->AppendLine("DEFINE_CONST_OR_RETURN(value, decoder.DecodeDouble(tag.wire_type));");
        break;
      case FieldDescriptorProto_Type::TYPE_FLOAT:
        writer->AppendLine("DEFINE_CONST_OR_RETURN(value, decoder.DecodeFloat(tag.wire_type));");
        break;
      case FieldDescriptorProto_Type::TYPE_INT64:
        writer->AppendLine("DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt64(tag.wire_type));");
        break;
      case FieldDescriptorProto_Type::TYPE_UINT64:
        writer->AppendLine("DEFINE_CONST_OR_RETURN(value, decoder.DecodeUInt64(tag.wire_type));");
        break;
      case FieldDescriptorProto_Type::TYPE_INT32:
        writer->AppendLine("DEFINE_CONST_OR_RETURN(value, decoder.DecodeInt32(tag.wire_type));");
        break;
      case FieldDescriptorProto_Type::TYPE_FIXED64:
        writer->AppendLine(
            "DEFINE_CONST_OR_RETURN(value, decoder.DecodeFixedUInt64(tag.wire_type));");
        break;
      case FieldDescriptorProto_Type::TYPE_FIXED32:
        writer->AppendLine(
            "DEFINE_CONST_OR_RETURN(value, decoder.DecodeFixedUInt32(tag.wire_type));");
        break;
      case FieldDescriptorProto_Type::TYPE_BOOL:
        writer->AppendLine("DEFINE_CONST_OR_RETURN(value, decoder.DecodeBool(tag.wire_type));");
        break;
      case FieldDescriptorProto_Type::TYPE_STRING:
        writer->AppendLine("DEFINE_VAR_OR_RETURN(value, decoder.DecodeString(tag.wire_type));");
        break;
      case FieldDescriptorProto_Type::TYPE_BYTES:
        writer->AppendLine("DEFINE_VAR_OR_RETURN(value, decoder.DecodeBytes(tag.wire_type));");
        break;
      case FieldDescriptorProto_Type::TYPE_UINT32:
        writer->AppendLine("DEFINE_CONST_OR_RETURN(value, decoder.DecodeUInt32(tag.wire_type));");
        break;
      case FieldDescriptorProto_Type::TYPE_SFIXED32:
        writer->AppendLine(
            "DEFINE_CONST_OR_RETURN(value, decoder.DecodeFixedInt32(tag.wire_type));");
        break;
      case FieldDescriptorProto_Type::TYPE_SFIXED64:
        writer->AppendLine(
            "DEFINE_CONST_OR_RETURN(value, decoder.DecodeFixedInt64(tag.wire_type));");
        break;
      case FieldDescriptorProto_Type::TYPE_SINT32:
        writer->AppendLine("DEFINE_CONST_OR_RETURN(value, decoder.DecodeSInt32(tag.wire_type));");
        break;
      case FieldDescriptorProto_Type::TYPE_SINT64:
        writer->AppendLine("DEFINE_CONST_OR_RETURN(value, decoder.DecodeSInt64(tag.wire_type));");
        break;
      default:
        return absl::InvalidArgumentError("invalid field type");
    }
    switch (*label) {
      case FieldDescriptorProto_Label::LABEL_OPTIONAL:
        switch (*type) {
          case FieldDescriptorProto_Type::TYPE_STRING:
          case FieldDescriptorProto_Type::TYPE_BYTES:
            writer->AppendLine(absl::StrCat("proto.", *name, ".emplace(std::move(value));"));
            break;
          default:
            writer->AppendLine(absl::StrCat("proto.", *name, ".emplace(value);"));
            break;
        }
        break;
      case FieldDescriptorProto_Label::LABEL_REQUIRED:
        switch (*type) {
          case FieldDescriptorProto_Type::TYPE_STRING:
          case FieldDescriptorProto_Type::TYPE_BYTES:
            writer->AppendLine(absl::StrCat("proto.", *name, " = std::move(value);"));
            break;
          default:
            writer->AppendLine(absl::StrCat("proto.", *name, " = value;"));
            break;
        }
        break;
      case FieldDescriptorProto_Label::LABEL_REPEATED:
        switch (*type) {
          case FieldDescriptorProto_Type::TYPE_STRING:
          case FieldDescriptorProto_Type::TYPE_BYTES:
            writer->AppendLine(absl::StrCat("proto.", *name, ".emplace_back(std::move(value));"));
            break;
          default:
            writer->AppendLine(absl::StrCat("proto.", *name, ".emplace_back(value);"));
            break;
        }
        break;
      default:
        return absl::InvalidArgumentError("invalid field label");
    }
  }
  writer->AppendLine("} break;");
  return absl::OkStatus();
}

absl::Status Generator::EmitOptionalFieldEncoding(
    internal::FileWriter* const writer, std::string_view const name, size_t const number,
    google::protobuf::FieldDescriptorProto_Type const type) {
  writer->AppendLine(absl::StrCat("if (proto.", name, ".has_value()) {"));
  {
    auto const it = kFieldEncoderNames.find(type);
    if (it == kFieldEncoderNames.end()) {
      return absl::InvalidArgumentError("invalid field type");
    }
    FileWriter::IndentedScope is{writer};
    writer->AppendLine(
        absl::StrCat("encoder.", it->second, "(", number, ", proto.", name, ".value());"));
  }
  writer->AppendLine("}");
  return absl::OkStatus();
}

absl::Status Generator::EmitRepeatedFieldEncoding(
    internal::FileWriter* const writer, std::string_view const name, size_t const number,
    google::protobuf::FieldDescriptorProto_Type const type, bool const packed) {
  if (packed) {
    auto const packed_encoder_it = kPackedFieldEncoderNames.find(type);
    if (packed_encoder_it != kPackedFieldEncoderNames.end()) {
      writer->AppendLine(
          absl::StrCat("encoder.", packed_encoder_it->second, "(", number, ", proto.", name, ");"));
      return absl::OkStatus();
    }
  }
  auto const encoder_it = kFieldEncoderNames.find(type);
  if (encoder_it != kFieldEncoderNames.end()) {
    writer->AppendLine(absl::StrCat("for (auto const& value : proto.", name, ") {"));
    {
      FileWriter::IndentedScope is{writer};
      writer->AppendLine(absl::StrCat("encoder.", encoder_it->second, "(", number, ", value);"));
    }
    writer->AppendLine("}");
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError("invalid field type");
}

absl::Status Generator::EmitRequiredFieldEncoding(
    internal::FileWriter* const writer, std::string_view const name, size_t const number,
    google::protobuf::FieldDescriptorProto_Type const type) {
  auto const it = kFieldEncoderNames.find(type);
  if (it != kFieldEncoderNames.end()) {
    writer->AppendLine(absl::StrCat("encoder.", it->second, "(", number, ", proto.", name, ");"));
    return absl::OkStatus();
  } else {
    return absl::InvalidArgumentError("invalid field type");
  }
}

absl::Status Generator::EmitEnumEncoding(internal::FileWriter* const writer,
                                         std::string_view const name, size_t const number,
                                         google::protobuf::FieldDescriptorProto_Label const label,
                                         std::string_view const proto_type_name,
                                         bool const packed) {
  switch (label) {
    case google::protobuf::FieldDescriptorProto_Label::LABEL_OPTIONAL:
      writer->AppendLine(absl::StrCat("if (proto.", name, ".has_value()) {"));
      {
        FileWriter::IndentedScope is{writer};
        writer->AppendLine(
            absl::StrCat("encoder.EncodeEnumField(", number, ", proto.", name, ".value());"));
      }
      writer->AppendLine("}");
      break;
    case google::protobuf::FieldDescriptorProto_Label::LABEL_REPEATED: {
      if (packed) {
        writer->AppendLine(
            absl::StrCat("encoder.EncodePackedEnums(", number, ", proto.", name, ");"));
      } else {
        writer->AppendLine(absl::StrCat("for (auto const& value : proto.", name, ") {"));
        {
          FileWriter::IndentedScope is{writer};
          writer->AppendLine(absl::StrCat("encoder.EncodeEnumField(", number, ", value);"));
        }
        writer->AppendLine("}");
      }
    } break;
    case google::protobuf::FieldDescriptorProto_Label::LABEL_REQUIRED:
      writer->AppendLine(absl::StrCat("encoder.EncodeEnumField(", number, ", proto.", name, ");"));
      break;
    default:
      return absl::InvalidArgumentError("invalid field label");
  }
  return absl::OkStatus();
}

absl::Status Generator::EmitObjectEncoding(internal::FileWriter* const writer,
                                           std::string_view const name, size_t const number,
                                           FieldDescriptorProto_Label const label,
                                           std::string_view const proto_type_name) {
  auto const type_name = absl::StrReplaceAll(proto_type_name, {{".", "::"}});
  switch (label) {
    case FieldDescriptorProto_Label::LABEL_OPTIONAL:
      writer->AppendLine(absl::StrCat("if (proto.", name, ".has_value()) {"));
      {
        FileWriter::IndentedScope is{writer};
        writer->AppendLine(absl::StrCat("encoder.EncodeTag({ .field_number = ", number,
                                        ", .wire_type = ::tsdb2::proto::WireType::kLength });"));
        writer->AppendLine(absl::StrCat("encoder.EncodeSubMessage(", type_name, "::Encode(proto.",
                                        name, ".value()));"));
      }
      writer->AppendLine("}");
      break;
    case FieldDescriptorProto_Label::LABEL_REPEATED:
      writer->AppendLine(absl::StrCat("for (auto const& value : proto.", name, ") {"));
      {
        FileWriter::IndentedScope is{writer};
        writer->AppendLine(absl::StrCat("encoder.EncodeTag({ .field_number = ", number,
                                        ", .wire_type = ::tsdb2::proto::WireType::kLength });"));
        writer->AppendLine(
            absl::StrCat("encoder.EncodeSubMessage(", type_name, "::Encode(value));"));
      }
      writer->AppendLine("}");
      break;
    case FieldDescriptorProto_Label::LABEL_REQUIRED:
      writer->AppendLine(absl::StrCat("encoder.EncodeTag({ .field_number = ", number,
                                      ", .wire_type = ::tsdb2::proto::WireType::kLength });"));
      writer->AppendLine(
          absl::StrCat("encoder.EncodeSubMessage(", type_name, "::Encode(proto.", name, "));"));
      break;
    default:
      return absl::InvalidArgumentError("invalid field label");
  }
  return absl::OkStatus();
}

absl::Status Generator::EmitFieldEncoding(
    internal::FileWriter* const writer,
    google::protobuf::FieldDescriptorProto const& descriptor) const {
  DEFINE_CONST_OR_RETURN(name, RequireField<kFieldDescriptorProtoNameField>(descriptor));
  DEFINE_CONST_OR_RETURN(number, RequireField<kFieldDescriptorProtoNumberField>(descriptor));
  DEFINE_CONST_OR_RETURN(label, RequireField<kFieldDescriptorProtoLabelField>(descriptor));
  auto const& maybe_type_name = descriptor.get<kFieldDescriptorProtoTypeNameField>();
  if (maybe_type_name.has_value()) {
    DEFINE_CONST_OR_RETURN(is_message, IsMessage(maybe_type_name.value()));
    if (is_message) {
      return EmitObjectEncoding(writer, *name, *number, *label, maybe_type_name.value());
    } else {
      auto const& maybe_options = descriptor.get<kFieldDescriptorProtoOptionsField>();
      bool const packed = maybe_options && maybe_options->get<kFieldOptionsPackedField>();
      return EmitEnumEncoding(writer, *name, *number, *label, maybe_type_name.value(), packed);
    }
  } else {
    DEFINE_CONST_OR_RETURN(type, RequireField<kFieldDescriptorProtoTypeField>(descriptor));
    switch (*label) {
      case FieldDescriptorProto_Label::LABEL_OPTIONAL:
        return EmitOptionalFieldEncoding(writer, *name, *number, *type);
      case FieldDescriptorProto_Label::LABEL_REPEATED: {
        auto const& maybe_options = descriptor.get<kFieldDescriptorProtoOptionsField>();
        bool const packed = maybe_options && maybe_options->get<kFieldOptionsPackedField>();
        return EmitRepeatedFieldEncoding(writer, *name, *number, *type, packed);
      }
      case FieldDescriptorProto_Label::LABEL_REQUIRED:
        return EmitRequiredFieldEncoding(writer, *name, *number, *type);
      default:
        return absl::InvalidArgumentError("invalid field label");
    }
  }
}

absl::Status Generator::EmitMessageImplementation(
    FileWriter* const writer, google::protobuf::DescriptorProto const& descriptor) const {
  DEFINE_CONST_OR_RETURN(name, RequireField<kDescriptorProtoNameField>(descriptor));
  writer->AppendLine(absl::StrCat("::absl::StatusOr<", *name, "> ", *name,
                                  "::Decode(::absl::Span<uint8_t const> const data) {"));
  {
    FileWriter::IndentedScope is{writer};
    writer->AppendLine(absl::StrCat(*name, " proto;"));
    writer->AppendLine("::tsdb2::proto::Decoder decoder{data};");
    writer->AppendLine("while (!decoder.at_end()) {");
    {
      FileWriter::IndentedScope is{writer};
      writer->AppendLine("DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());");
      writer->AppendLine("switch (tag.field_number) {");
      {
        FileWriter::IndentedScope is{writer};
        for (auto const& field : descriptor.get<kDescriptorProtoFieldField>()) {
          RETURN_IF_ERROR(EmitFieldDecoding(writer, field));
        }
        writer->AppendLine("default:");
        {
          FileWriter::IndentedScope is{writer};
          writer->AppendLine("RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));");
          writer->AppendLine("break;");
        }
      }
      writer->AppendLine("}");
    }
    writer->AppendLine("}");
    writer->AppendLine("return std::move(proto);");
  }
  writer->AppendLine("}");
  writer->AppendEmptyLine();
  writer->AppendLine(
      absl::StrCat("::tsdb2::io::Cord ", *name, "::Encode(", *name, " const& proto) {"));
  {
    FileWriter::IndentedScope is{writer};
    writer->AppendLine("::tsdb2::proto::Encoder encoder;");
    for (auto const& field : descriptor.get<kDescriptorProtoFieldField>()) {
      RETURN_IF_ERROR(EmitFieldEncoding(writer, field));
    }
    writer->AppendLine("return std::move(encoder).Finish();");
  }
  writer->AppendLine("}");
  return absl::OkStatus();
}

absl::StatusOr<bool> Generator::IsMessage(std::string_view const proto_type_name) const {
  DEFINE_CONST_OR_RETURN(path, GetTypePath(proto_type_name));
  return !enums_.contains(path);
}

}  // namespace proto
}  // namespace tsdb2
