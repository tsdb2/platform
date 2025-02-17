#include "proto/generator.h"

#include <cerrno>
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
#include "common/flat_set.h"
#include "common/no_destructor.h"
#include "common/re.h"
#include "common/utilities.h"
#include "proto/dependencies.h"
#include "proto/descriptor.pb.sync.h"
#include "proto/plugin.pb.sync.h"
#include "proto/proto.h"
#include "proto/text_writer.h"

namespace tsdb2 {
namespace proto {

namespace {

using ::google::protobuf::FieldDescriptorProto;
using ::google::protobuf::compiler::CodeGeneratorResponse;
using ::tsdb2::proto::internal::DependencyManager;
using ::tsdb2::proto::internal::TextWriter;

tsdb2::common::NoDestructor<tsdb2::common::RE> const kFileExtensionPattern{
    tsdb2::common::RE::CreateOrDie("(\\.[^./\\\\]*)$")};

tsdb2::common::NoDestructor<tsdb2::common::RE> const kPackagePattern{
    tsdb2::common::RE::CreateOrDie("^[_A-Za-z][_A-Za-z0-9]*(?:\\.[_A-Za-z][_A-Za-z0-9]*)*$")};

tsdb2::common::NoDestructor<tsdb2::common::RE> const kIdentifierPattern{
    tsdb2::common::RE::CreateOrDie("^[_A-Za-z][_A-Za-z0-9]*$")};

auto constexpr kDefaultHeaderIncludes = tsdb2::common::fixed_flat_set_of<std::string_view>({
    "absl/base/attributes.h",
    "absl/status/statusor.h",
    "absl/types/span.h",
    "io/cord.h",
    "proto/proto.h",
});

auto constexpr kDefaultSourceIncludes = tsdb2::common::fixed_flat_set_of<std::string_view>({
    "absl/status/statusor.h",
    "absl/types/span.h",
    "common/utilities.h",
    "io/cord.h",
    "proto/wire_format.h",
});

auto constexpr kFieldTypeNames =
    tsdb2::common::fixed_flat_map_of<FieldDescriptorProto::Type, std::string_view>({
        {FieldDescriptorProto::Type::TYPE_DOUBLE, "double"},
        {FieldDescriptorProto::Type::TYPE_FLOAT, "float"},
        {FieldDescriptorProto::Type::TYPE_INT64, "int64_t"},
        {FieldDescriptorProto::Type::TYPE_UINT64, "uint64_t"},
        {FieldDescriptorProto::Type::TYPE_INT32, "int32_t"},
        {FieldDescriptorProto::Type::TYPE_FIXED64, "uint64_t"},
        {FieldDescriptorProto::Type::TYPE_FIXED32, "uint32_t"},
        {FieldDescriptorProto::Type::TYPE_BOOL, "bool"},
        {FieldDescriptorProto::Type::TYPE_STRING, "std::string"},
        {FieldDescriptorProto::Type::TYPE_BYTES, "std::vector<uint8_t>"},
        {FieldDescriptorProto::Type::TYPE_UINT32, "uint32_t"},
        {FieldDescriptorProto::Type::TYPE_SFIXED32, "int32_t"},
        {FieldDescriptorProto::Type::TYPE_SFIXED64, "int64_t"},
        {FieldDescriptorProto::Type::TYPE_SINT32, "int32_t"},
        {FieldDescriptorProto::Type::TYPE_SINT64, "int64_t"},
    });

tsdb2::common::NoDestructor<tsdb2::common::RE> const kBooleanPattern{
    tsdb2::common::RE::CreateOrDie("^true|false$")};

tsdb2::common::NoDestructor<tsdb2::common::RE> const kSignedIntegerPattern{
    tsdb2::common::RE::CreateOrDie("^[+-]?(?:0|[1-9][0-9]*)$")};

tsdb2::common::NoDestructor<tsdb2::common::RE> const kUnsignedIntegerPattern{
    tsdb2::common::RE::CreateOrDie("^\\+?(?:0|[1-9][0-9]*)$")};

tsdb2::common::NoDestructor<tsdb2::common::RE> const kFloatNumberPattern{
    tsdb2::common::RE::CreateOrDie("^[+-]?(?:0|[1-9][0-9]*)(?:\\.[0-9]+)?$")};

auto const kFieldInitializerPatterns =
    tsdb2::common::fixed_flat_map_of<FieldDescriptorProto::Type, tsdb2::common::RE const*>({
        {FieldDescriptorProto::Type::TYPE_DOUBLE, kFloatNumberPattern.Get()},
        {FieldDescriptorProto::Type::TYPE_FLOAT, kFloatNumberPattern.Get()},
        {FieldDescriptorProto::Type::TYPE_INT64, kSignedIntegerPattern.Get()},
        {FieldDescriptorProto::Type::TYPE_UINT64, kUnsignedIntegerPattern.Get()},
        {FieldDescriptorProto::Type::TYPE_INT32, kSignedIntegerPattern.Get()},
        {FieldDescriptorProto::Type::TYPE_FIXED64, kUnsignedIntegerPattern.Get()},
        {FieldDescriptorProto::Type::TYPE_FIXED32, kUnsignedIntegerPattern.Get()},
        {FieldDescriptorProto::Type::TYPE_BOOL, kBooleanPattern.Get()},
        {FieldDescriptorProto::Type::TYPE_UINT32, kUnsignedIntegerPattern.Get()},
        {FieldDescriptorProto::Type::TYPE_ENUM, kIdentifierPattern.Get()},
        {FieldDescriptorProto::Type::TYPE_SFIXED32, kSignedIntegerPattern.Get()},
        {FieldDescriptorProto::Type::TYPE_SFIXED64, kSignedIntegerPattern.Get()},
        {FieldDescriptorProto::Type::TYPE_SINT32, kSignedIntegerPattern.Get()},
        {FieldDescriptorProto::Type::TYPE_SINT64, kSignedIntegerPattern.Get()},
    });

auto constexpr kFieldDecoderNames =
    tsdb2::common::fixed_flat_map_of<FieldDescriptorProto::Type, std::string_view>({
        {FieldDescriptorProto::Type::TYPE_DOUBLE, "DecodeDouble"},
        {FieldDescriptorProto::Type::TYPE_FLOAT, "DecodeFloat"},
        {FieldDescriptorProto::Type::TYPE_INT64, "DecodeInt64"},
        {FieldDescriptorProto::Type::TYPE_UINT64, "DecodeUInt64"},
        {FieldDescriptorProto::Type::TYPE_INT32, "DecodeInt32"},
        {FieldDescriptorProto::Type::TYPE_FIXED64, "DecodeFixedUInt64"},
        {FieldDescriptorProto::Type::TYPE_FIXED32, "DecodeFixedUInt32"},
        {FieldDescriptorProto::Type::TYPE_BOOL, "DecodeBool"},
        {FieldDescriptorProto::Type::TYPE_STRING, "DecodeString"},
        {FieldDescriptorProto::Type::TYPE_BYTES, "DecodeBytes"},
        {FieldDescriptorProto::Type::TYPE_UINT32, "DecodeUInt32"},
        {FieldDescriptorProto::Type::TYPE_SFIXED32, "DecodeFixedInt32"},
        {FieldDescriptorProto::Type::TYPE_SFIXED64, "DecodeFixedInt64"},
        {FieldDescriptorProto::Type::TYPE_SINT32, "DecodeSInt32"},
        {FieldDescriptorProto::Type::TYPE_SINT64, "DecodeSInt64"},
    });

auto constexpr kFieldEncoderNames =
    tsdb2::common::fixed_flat_map_of<FieldDescriptorProto::Type, std::string_view>({
        {FieldDescriptorProto::Type::TYPE_DOUBLE, "EncodeDoubleField"},
        {FieldDescriptorProto::Type::TYPE_FLOAT, "EncodeFloatField"},
        {FieldDescriptorProto::Type::TYPE_INT64, "EncodeInt64Field"},
        {FieldDescriptorProto::Type::TYPE_UINT64, "EncodeUInt64Field"},
        {FieldDescriptorProto::Type::TYPE_INT32, "EncodeInt32Field"},
        {FieldDescriptorProto::Type::TYPE_FIXED64, "EncodeFixedUInt64Field"},
        {FieldDescriptorProto::Type::TYPE_FIXED32, "EncodeFixedUInt32Field"},
        {FieldDescriptorProto::Type::TYPE_BOOL, "EncodeBoolField"},
        {FieldDescriptorProto::Type::TYPE_STRING, "EncodeStringField"},
        {FieldDescriptorProto::Type::TYPE_BYTES, "EncodeBytesField"},
        {FieldDescriptorProto::Type::TYPE_UINT32, "EncodeUInt32Field"},
        {FieldDescriptorProto::Type::TYPE_SFIXED32, "EncodeFixedInt32Field"},
        {FieldDescriptorProto::Type::TYPE_SFIXED64, "EncodeFixedInt64Field"},
        {FieldDescriptorProto::Type::TYPE_SINT32, "EncodeSInt32Field"},
        {FieldDescriptorProto::Type::TYPE_SINT64, "EncodeSInt64Field"},
    });

auto constexpr kPackedFieldEncoderNames =
    tsdb2::common::fixed_flat_map_of<FieldDescriptorProto::Type, std::string_view>({
        {FieldDescriptorProto::Type::TYPE_DOUBLE, "EncodePackedDoubles"},
        {FieldDescriptorProto::Type::TYPE_FLOAT, "EncodePackedFloats"},
        {FieldDescriptorProto::Type::TYPE_INT64, "EncodePackedInt64s"},
        {FieldDescriptorProto::Type::TYPE_UINT64, "EncodePackedUInt64s"},
        {FieldDescriptorProto::Type::TYPE_INT32, "EncodePackedInt32s"},
        {FieldDescriptorProto::Type::TYPE_FIXED64, "EncodePackedFixedUInt64s"},
        {FieldDescriptorProto::Type::TYPE_FIXED32, "EncodePackedFixedUInt32s"},
        {FieldDescriptorProto::Type::TYPE_BOOL, "EncodePackedBools"},
        {FieldDescriptorProto::Type::TYPE_UINT32, "EncodePackedUInt32s"},
        {FieldDescriptorProto::Type::TYPE_SFIXED32, "EncodePackedFixedInt32s"},
        {FieldDescriptorProto::Type::TYPE_SFIXED64, "EncodePackedFixedInt64s"},
        {FieldDescriptorProto::Type::TYPE_SINT32, "EncodePackedSInt32s"},
        {FieldDescriptorProto::Type::TYPE_SINT64, "EncodePackedSInt64s"},
    });

DependencyManager::Path JoinPath(DependencyManager::PathView const lhs,
                                 std::string_view const rhs) {
  DependencyManager::Path result;
  result.reserve(lhs.size() + 1);
  result.insert(result.end(), lhs.begin(), lhs.end());
  result.emplace_back(rhs);
  return result;
}

absl::StatusOr<DependencyManager::Path> GetTypePath(std::string_view const proto_type_name) {
  auto const splitter = absl::StrSplit(proto_type_name, '.');
  std::vector<std::string> const components{splitter.begin(), splitter.end()};
  if (components.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid type name: \"", absl::CEscape(proto_type_name), "\""));
  }
  if (components.front().empty()) {
    // fully qualified name
    return DependencyManager::Path{components.begin() + 1, components.end()};
  } else {
    // TODO: C++ semantics search algorithm
    return absl::UnimplementedError(
        absl::StrCat("cannot resolve \"", absl::CEscape(proto_type_name),
                     "\": partially qualified types not yet implemented"));
  }
}

std::string MakeCycleMessage(DependencyManager::PathView const base_path,
                             DependencyManager::Cycle const& cycle) {
  auto const prefix = absl::StrJoin(base_path, ".");
  std::vector<std::string> entries;
  entries.reserve(cycle.size() + 1);
  for (auto const& item : cycle) {
    entries.emplace_back(
        absl::StrCat(prefix, ".", absl::StrJoin(item.first, "."), ".", item.second));
  }
  if (!cycle.empty()) {
    auto const& first_path = cycle.front().first;
    entries.emplace_back(absl::StrCat(prefix, ".", absl::StrJoin(first_path, ".")));
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
  DEFINE_VAR_OR_RETURN(builder, Builder::Create(file_descriptor));
  return std::move(builder).Build();
}

absl::StatusOr<std::string> Generator::GenerateHeaderFileContent() {
  TextWriter writer;
  DEFINE_CONST_OR_RETURN(header_guard_name, GetHeaderGuardName());
  writer.AppendUnindentedLine(absl::StrCat("#ifndef ", header_guard_name));
  writer.AppendUnindentedLine(absl::StrCat("#define ", header_guard_name));
  writer.AppendEmptyLine();
  writer.AppendUnindentedLine("#include <cstdint>");
  writer.AppendUnindentedLine("#include <optional>");
  writer.AppendUnindentedLine("#include <string>");
  writer.AppendUnindentedLine("#include <vector>");
  writer.AppendEmptyLine();
  EmitHeaderIncludes(&writer);
  writer.AppendEmptyLine();
  DEFINE_CONST_OR_RETURN(package, GetCppPackage());
  if (!package.empty()) {
    writer.AppendLine(absl::StrCat("namespace ", package, " {"));
    writer.AppendEmptyLine();
  }
  RETURN_IF_ERROR(EmitHeaderForScope(&writer, LexicalScope{
                                                  .base_path = base_path_,
                                                  .message_types = file_descriptor_->message_type,
                                                  .enum_types = file_descriptor_->enum_type,
                                              }));
  if (!package.empty()) {
    writer.AppendLine(absl::StrCat("}  // namespace ", package));
    writer.AppendEmptyLine();
  }
  writer.AppendUnindentedLine(absl::StrCat("#endif  // ", header_guard_name));
  return std::move(writer).Finish();
}

absl::StatusOr<std::string> Generator::GenerateSourceFileContent() {
  TextWriter writer;
  REQUIRE_FIELD_OR_RETURN(name, *file_descriptor_, name);
  auto const header_path = generator::MakeHeaderFileName(name);
  writer.AppendUnindentedLine(absl::StrCat("#include \"", header_path, "\""));
  writer.AppendEmptyLine();
  writer.AppendUnindentedLine("#include <cstdint>");
  writer.AppendUnindentedLine("#include <utility>");
  writer.AppendEmptyLine();
  EmitSourceIncludes(&writer);
  DEFINE_CONST_OR_RETURN(package, GetCppPackage());
  if (!package.empty()) {
    writer.AppendEmptyLine();
    writer.AppendLine(absl::StrCat("namespace ", package, " {"));
  }
  RETURN_IF_ERROR(EmitImplementationForScope(&writer, /*prefix=*/{},
                                             LexicalScope{
                                                 .base_path = base_path_,
                                                 .message_types = file_descriptor_->message_type,
                                                 .enum_types = file_descriptor_->enum_type,
                                             }));
  if (!package.empty()) {
    writer.AppendEmptyLine();
    writer.AppendLine(absl::StrCat("}  // namespace ", package));
  }
  return std::move(writer).Finish();
}

absl::StatusOr<CodeGeneratorResponse::File> Generator::GenerateHeaderFile() {
  REQUIRE_FIELD_OR_RETURN(name, *file_descriptor_, name);
  DEFINE_VAR_OR_RETURN(content, GenerateHeaderFileContent());
  CodeGeneratorResponse::File file;
  file.name = generator::MakeHeaderFileName(name);
  file.content = std::move(content);
  return std::move(file);
}

absl::StatusOr<CodeGeneratorResponse::File> Generator::GenerateSourceFile() {
  REQUIRE_FIELD_OR_RETURN(name, *file_descriptor_, name);
  DEFINE_VAR_OR_RETURN(content, GenerateSourceFileContent());
  CodeGeneratorResponse::File file;
  file.name = generator::MakeSourceFileName(name);
  file.content = std::move(content);
  return std::move(file);
}

absl::StatusOr<Generator::Builder> Generator::Builder::Create(
    google::protobuf::FileDescriptorProto const& file_descriptor) {
  REQUIRE_FIELD_OR_RETURN(package_name, file_descriptor, package);
  if (!kPackagePattern->Test(package_name)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid package name: \"", absl::CEscape(package_name), "\""));
  }
  auto const splitter = absl::StrSplit(package_name, '.');
  std::vector<std::string_view> const package_components{splitter.begin(), splitter.end()};
  return Builder(file_descriptor, Path(package_components.begin(), package_components.end()));
}

absl::StatusOr<Generator> Generator::Builder::Build() && {
  Generator::LexicalScope const global_scope{
      .base_path = base_path_,
      .message_types = file_descriptor_->message_type,
      .enum_types = file_descriptor_->enum_type,
  };
  RETURN_IF_ERROR(AddLexicalScope(global_scope));
  DEFINE_CONST_OR_RETURN(cycle_info, FindCycles(global_scope));
  if (!cycle_info.cycles.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("message dependency cycle detected: ",
                     MakeCycleMessage(cycle_info.base_path, cycle_info.cycles.front())));
  }
  return Generator(file_descriptor_, std::move(enums_), std::move(dependencies_),
                   std::move(base_path_));
}

absl::Status Generator::Builder::AddLexicalScope(Generator::LexicalScope const& scope) {
  for (auto const& enum_type : scope.enum_types) {
    REQUIRE_FIELD_OR_RETURN(name, enum_type, name);
    if (!kIdentifierPattern->Test(name)) {
      return absl::InvalidArgumentError(
          absl::StrCat("invalid enum type name: \"", absl::CEscape(name), "\""));
    }
    Path const path = JoinPath(scope.base_path, name);
    enums_.emplace(path);
    dependencies_.AddNode(path);
  }
  for (auto const& message_type : scope.message_types) {
    REQUIRE_FIELD_OR_RETURN(name, message_type, name);
    if (!kIdentifierPattern->Test(name)) {
      return absl::InvalidArgumentError(
          absl::StrCat("invalid message type name: \"", absl::CEscape(name), "\""));
    }
    DependencyManager::Path const path = JoinPath(scope.base_path, name);
    dependencies_.AddNode(path);
    RETURN_IF_ERROR(AddLexicalScope(Generator::LexicalScope{
        .base_path = path,
        .message_types = message_type.nested_type,
        .enum_types = message_type.enum_type,
    }));
  }
  for (auto const& message_type : scope.message_types) {
    REQUIRE_FIELD_OR_RETURN(message_name, message_type, name);
    DependencyManager::Path const path = JoinPath(scope.base_path, message_name);
    for (auto const& field : message_type.field) {
      REQUIRE_FIELD_OR_RETURN(field_name, field, name);
      if (!kIdentifierPattern->Test(field_name)) {
        return absl::InvalidArgumentError(
            absl::StrCat("invalid message field name: \"", absl::CEscape(field_name), "\""));
      }
      if (field.type_name.has_value()) {
        REQUIRE_FIELD_OR_RETURN(label, field, label);
        if (label != FieldDescriptorProto::Label::LABEL_REPEATED) {
          DEFINE_CONST_OR_RETURN(type_path, GetTypePath(field.type_name.value()));
          dependencies_.AddDependency(path, type_path, field_name);
        }
      }
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<Generator::Builder::CycleInfo> Generator::Builder::FindCycles(
    LexicalScope const& scope) const {
  auto cycles = dependencies_.FindCycles(scope.base_path);
  if (!cycles.empty()) {
    return CycleInfo{
        .base_path = scope.base_path,
        .cycles = std::move(cycles),
    };
  }
  for (auto const& message_type : scope.message_types) {
    REQUIRE_FIELD_OR_RETURN(name, message_type, name);
    Path path = JoinPath(scope.base_path, name);
    DEFINE_VAR_OR_RETURN(cycle_info, FindCycles(LexicalScope{
                                         .base_path = path,
                                         .message_types = message_type.nested_type,
                                         .enum_types = message_type.enum_type,
                                     }));
    if (!cycle_info.cycles.empty()) {
      return CycleInfo{
          .base_path = std::move(path),
          .cycles = std::move(cycle_info.cycles),
      };
    }
  }
  return CycleInfo{.base_path = scope.base_path};
}

absl::StatusOr<std::string> Generator::GetHeaderGuardName() const {
  REQUIRE_FIELD_OR_RETURN(name, *file_descriptor_, name);
  // TODO: we are replacing only path separators here, but file paths may contain many more symbols.
  std::string converted = absl::StrReplaceAll(name, {{"/", "_"}, {"\\", "_"}});
  std::string_view extension;
  if (kFileExtensionPattern->PartialMatchArgs(converted, &extension)) {
    converted.erase(converted.size() - extension.size());
  }
  absl::AsciiStrToUpper(&converted);
  return absl::StrCat("__TSDB2_", converted, "_PB_H__");
}

absl::StatusOr<std::string> Generator::GetCppPackage() const {
  if (!file_descriptor_->package.has_value()) {
    return "";
  }
  auto const& package_name = file_descriptor_->package.value();
  if (!kPackagePattern->Test(package_name)) {
    return absl::InvalidArgumentError(
        absl::StrCat("package name \"", absl::CEscape(package_name), "\" has an invalid format"));
  }
  return absl::StrReplaceAll(package_name, {{".", "::"}});
}

absl::StatusOr<bool> Generator::IsMessage(std::string_view proto_type_name) const {
  DEFINE_CONST_OR_RETURN(path, GetTypePath(proto_type_name));
  return !enums_.contains(path);
}

absl::StatusOr<bool> Generator::IsEnum(std::string_view proto_type_name) const {
  DEFINE_CONST_OR_RETURN(is_message, IsMessage(proto_type_name));
  return !is_message;
}

namespace {

template <typename DefaultHeaderSet>
void EmitIncludes(google::protobuf::FileDescriptorProto const& file_descriptor,
                  TextWriter* const writer, DefaultHeaderSet const& default_headers) {
  tsdb2::common::flat_map<std::string, bool> headers;
  headers.reserve(default_headers.size() + file_descriptor.dependency.size());
  for (auto const default_dependency : default_headers) {
    headers.try_emplace(std::string(default_dependency), false);
  }
  tsdb2::common::flat_set<size_t> const public_dependency_indexes{
      file_descriptor.public_dependency.begin(), file_descriptor.public_dependency.end()};
  for (size_t i = 0; i < file_descriptor.dependency.size(); ++i) {
    headers.try_emplace(generator::MakeHeaderFileName(file_descriptor.dependency[i]),
                        /*public=*/public_dependency_indexes.contains(i));
  }
  for (auto const& [header, is_public] : headers) {
    if (is_public) {
      writer->AppendUnindentedLine(
          absl::StrCat("#include \"", header, "\"  // IWYU pragma: export"));
    } else {
      writer->AppendUnindentedLine(absl::StrCat("#include \"", header, "\""));
    }
  }
}

}  // namespace

void Generator::EmitHeaderIncludes(internal::TextWriter* const writer) const {
  EmitIncludes(*file_descriptor_, writer, kDefaultHeaderIncludes);
}

void Generator::EmitSourceIncludes(internal::TextWriter* const writer) const {
  EmitIncludes(*file_descriptor_, writer, kDefaultSourceIncludes);
}

absl::StatusOr<std::pair<std::string, bool>> Generator::GetFieldType(
    FieldDescriptorProto const& descriptor) const {
  if (descriptor.type_name.has_value()) {
    auto const& type_name = descriptor.type_name.value();
    DEFINE_CONST_OR_RETURN(is_enum, IsEnum(type_name));
    return std::make_pair(absl::StrReplaceAll(type_name, {{".", "::"}}), /*primitive=*/is_enum);
  }
  REQUIRE_FIELD_OR_RETURN(type, descriptor, type);
  auto const it = kFieldTypeNames.find(type);
  if (it == kFieldTypeNames.end()) {
    return absl::InvalidArgumentError("invalid field type");
  }
  return std::make_pair(std::string(it->second), /*primitive=*/true);
}

absl::StatusOr<std::string> Generator::GetFieldInitializer(
    FieldDescriptorProto const& descriptor, std::string_view const type_name,
    std::string_view const default_value) const {
  REQUIRE_FIELD_OR_RETURN(type, descriptor, type);
  if (type == FieldDescriptorProto::Type::TYPE_STRING) {
    return absl::StrCat("\"", absl::CEscape(default_value), "\"");
  }
  auto const it = kFieldInitializerPatterns.find(type);
  if (it == kFieldInitializerPatterns.end()) {
    return absl::InvalidArgumentError("invalid type for initialized field");
  }
  if (!it->second->Test(default_value)) {
    return absl::InvalidArgumentError("invalid field initializer");
  }
  if (!descriptor.type_name.has_value()) {
    return std::string(default_value);
  }
  DEFINE_CONST_OR_RETURN(is_enum, IsEnum(descriptor.type_name.value()));
  if (is_enum) {
    return absl::StrCat(type_name, "::", default_value);
  } else {
    return std::string(default_value);
  }
}

absl::StatusOr<bool> Generator::FieldIsWrappedInOptional(
    FieldDescriptorProto const& descriptor) const {
  REQUIRE_FIELD_OR_RETURN(label, descriptor, label);
  if (label != FieldDescriptorProto::Label::LABEL_OPTIONAL) {
    return false;
  }
  DEFINE_CONST_OR_RETURN(type_pair, GetFieldType(descriptor));
  auto const& [type, primitive] = type_pair;
  return !primitive || !descriptor.default_value.has_value();
}

absl::Status Generator::EmitHeaderForScope(internal::TextWriter* const writer,
                                           LexicalScope const& scope) const {
  for (auto const& message_type : scope.message_types) {
    REQUIRE_FIELD_OR_RETURN(name, message_type, name);
    writer->AppendLine(absl::StrCat("struct ", name, ";"));
  }
  if (!scope.message_types.empty()) {
    writer->AppendEmptyLine();
  }
  for (auto const& enum_type : scope.enum_types) {
    REQUIRE_FIELD_OR_RETURN(name, enum_type, name);
    writer->AppendLine(absl::StrCat("enum class ", name, " {"));
    {
      TextWriter::IndentedScope is{writer};
      for (auto const& value : enum_type.value) {
        REQUIRE_FIELD_OR_RETURN(name, value, name);
        if (!kIdentifierPattern->Test(name)) {
          return absl::InvalidArgumentError(
              absl::StrCat("invalid enum value name: \"", name, "\""));
        }
        REQUIRE_FIELD_OR_RETURN(number, value, number);
        writer->AppendLine(absl::StrCat(name, " = ", number, ","));
      }
    }
    writer->AppendLine("};");
    writer->AppendEmptyLine();
  }
  absl::flat_hash_map<std::string, google::protobuf::DescriptorProto const*> descriptors_by_name;
  descriptors_by_name.reserve(scope.message_types.size());
  for (auto const& message_type : scope.message_types) {
    REQUIRE_FIELD_OR_RETURN(name, message_type, name);
    descriptors_by_name.try_emplace(name, &message_type);
  }
  for (auto const& name : dependencies_.MakeOrder(scope.base_path)) {
    auto const it = descriptors_by_name.find(name);
    if (it == descriptors_by_name.end()) {
      // If not found it means `name` refers to an enum, otherwise it's a regular message. We don't
      // need to process enums here because they're always defined at the beginning of every lexical
      // scope.
      continue;
    }
    auto const& message_type = *(it->second);
    writer->AppendLine(absl::StrCat("struct ", name, " : public ::tsdb2::proto::Message {"));
    {
      TextWriter::IndentedScope is{writer};
      RETURN_IF_ERROR(EmitHeaderForScope(writer, LexicalScope{
                                                     .base_path = JoinPath(scope.base_path, name),
                                                     .message_types = message_type.nested_type,
                                                     .enum_types = message_type.enum_type,
                                                 }));
      writer->AppendLine(absl::StrCat("static ::absl::StatusOr<", name,
                                      "> Decode(::absl::Span<uint8_t const> data);"));
      writer->AppendLine(absl::StrCat("static ::tsdb2::io::Cord Encode(", name, " const& proto);"));
      if (!message_type.field.empty()) {
        writer->AppendEmptyLine();
      }
      for (auto const& field : message_type.field) {
        REQUIRE_FIELD_OR_RETURN(name, field, name);
        REQUIRE_FIELD_OR_RETURN(label, field, label);
        DEFINE_CONST_OR_RETURN(type_pair, GetFieldType(field));
        auto const& [type, primitive] = type_pair;
        switch (label) {
          case FieldDescriptorProto::Label::LABEL_OPTIONAL:
            if (primitive && field.default_value.has_value()) {
              DEFINE_CONST_OR_RETURN(initializer,
                                     GetFieldInitializer(field, type, field.default_value.value()));
              writer->AppendLine(absl::StrCat(type, " ", name, "{", initializer, "};"));
            } else {
              writer->AppendLine(absl::StrCat("std::optional<", type, "> ", name, ";"));
            }
            break;
          case FieldDescriptorProto::Label::LABEL_REPEATED:
            writer->AppendLine(absl::StrCat("std::vector<", type, "> ", name, ";"));
            break;
          case FieldDescriptorProto::Label::LABEL_REQUIRED:
            if (field.default_value.has_value()) {
              DEFINE_CONST_OR_RETURN(initializer,
                                     GetFieldInitializer(field, type, field.default_value.value()));
              writer->AppendLine(absl::StrCat(type, " ", name, "{", initializer, "};"));
            } else if (primitive) {
              writer->AppendLine(absl::StrCat(type, " ", name, "{};"));
            } else {
              writer->AppendLine(absl::StrCat(type, " ", name, ";"));
            }
            break;
          default:
            return absl::InvalidArgumentError("unknown value for field label");
        }
      }
    }
    writer->AppendLine("};");
    writer->AppendEmptyLine();
  }
  return absl::OkStatus();
}

absl::Status Generator::EmitFieldDecoding(TextWriter* const writer,
                                          FieldDescriptorProto const& descriptor) const {
  REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
  REQUIRE_FIELD_OR_RETURN(number, descriptor, number);
  REQUIRE_FIELD_OR_RETURN(label, descriptor, label);
  DEFINE_CONST_OR_RETURN(is_optional, FieldIsWrappedInOptional(descriptor));
  writer->AppendLine(absl::StrCat("case ", number, ": {"));
  if (descriptor.type_name.has_value()) {
    DEFINE_CONST_OR_RETURN(is_message, IsMessage(descriptor.type_name.value()));
    std::string const type_name = absl::StrReplaceAll(descriptor.type_name.value(), {{".", "::"}});
    TextWriter::IndentedScope is{writer};
    if (is_message) {
      writer->AppendLine(
          "DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));");
      writer->AppendLine(
          absl::StrCat("DEFINE_VAR_OR_RETURN(value, ", type_name, "::Decode(child_span));"));
      switch (label) {
        case FieldDescriptorProto::Label::LABEL_OPTIONAL:
          writer->AppendLine(absl::StrCat("proto.", name, ".emplace(std::move(value));"));
          break;
        case FieldDescriptorProto::Label::LABEL_REQUIRED:
          writer->AppendLine(absl::StrCat("proto.", name, " = std::move(value);"));
          break;
        case FieldDescriptorProto::Label::LABEL_REPEATED:
          writer->AppendLine(absl::StrCat("proto.", name, ".emplace_back(std::move(value));"));
          break;
        default:
          return absl::InvalidArgumentError("invalid field label");
      }
    } else {
      writer->AppendLine(
          absl::StrCat("DEFINE_CONST_OR_RETURN(value, decoder.DecodeUInt64(tag.wire_type));"));
      switch (label) {
        case FieldDescriptorProto::Label::LABEL_OPTIONAL:
          if (is_optional) {
            writer->AppendLine(
                absl::StrCat("proto.", name, ".emplace(static_cast<", type_name, ">(value));"));
          } else {
            writer->AppendLine(
                absl::StrCat("proto.", name, " = static_cast<", type_name, ">(value);"));
          }
          break;
        case FieldDescriptorProto::Label::LABEL_REQUIRED:
          writer->AppendLine(
              absl::StrCat("proto.", name, " = static_cast<", type_name, ">(value);"));
          break;
        case FieldDescriptorProto::Label::LABEL_REPEATED:
          writer->AppendLine(
              absl::StrCat("proto.", name, ".emplace_back(static_cast<", type_name, ">(value));"));
          break;
        default:
          return absl::InvalidArgumentError("invalid field label");
      }
    }
  } else {
    REQUIRE_FIELD_OR_RETURN(type, descriptor, type);
    auto const it = kFieldDecoderNames.find(type);
    if (it == kFieldDecoderNames.end()) {
      return absl::InvalidArgumentError("invalid field type");
    }
    TextWriter::IndentedScope is{writer};
    bool const move = type == FieldDescriptorProto::Type::TYPE_STRING ||
                      type == FieldDescriptorProto::Type::TYPE_BYTES;
    if (move) {
      writer->AppendLine(
          absl::StrCat("DEFINE_VAR_OR_RETURN(value, decoder.", it->second, "(tag.wire_type));"));
    } else {
      writer->AppendLine(
          absl::StrCat("DEFINE_CONST_OR_RETURN(value, decoder.", it->second, "(tag.wire_type));"));
    }
    switch (label) {
      case FieldDescriptorProto::Label::LABEL_OPTIONAL:
        if (is_optional) {
          if (move) {
            writer->AppendLine(absl::StrCat("proto.", name, ".emplace(std::move(value));"));
          } else {
            writer->AppendLine(absl::StrCat("proto.", name, ".emplace(value);"));
          }
        } else {
          if (move) {
            writer->AppendLine(absl::StrCat("proto.", name, " = std::move(value);"));
          } else {
            writer->AppendLine(absl::StrCat("proto.", name, " = value;"));
          }
        }
        break;
      case FieldDescriptorProto::Label::LABEL_REQUIRED:
        if (move) {
          writer->AppendLine(absl::StrCat("proto.", name, " = std::move(value);"));
        } else {
          writer->AppendLine(absl::StrCat("proto.", name, " = value;"));
        }
        break;
      case FieldDescriptorProto::Label::LABEL_REPEATED:
        if (move) {
          writer->AppendLine(absl::StrCat("proto.", name, ".emplace_back(std::move(value));"));
        } else {
          writer->AppendLine(absl::StrCat("proto.", name, ".emplace_back(value);"));
        }
        break;
      default:
        return absl::InvalidArgumentError("invalid field label");
    }
  }
  writer->AppendLine("} break;");
  return absl::OkStatus();
}

absl::Status Generator::EmitOptionalFieldEncoding(TextWriter* const writer,
                                                  FieldDescriptorProto const& descriptor) {
  REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
  REQUIRE_FIELD_OR_RETURN(number, descriptor, number);
  REQUIRE_FIELD_OR_RETURN(type, descriptor, type);
  writer->AppendLine(absl::StrCat("if (proto.", name, ".has_value()) {"));
  {
    auto const it = kFieldEncoderNames.find(type);
    if (it == kFieldEncoderNames.end()) {
      return absl::InvalidArgumentError("invalid field type");
    }
    TextWriter::IndentedScope is{writer};
    writer->AppendLine(
        absl::StrCat("encoder.", it->second, "(", number, ", proto.", name, ".value());"));
  }
  writer->AppendLine("}");
  return absl::OkStatus();
}

absl::Status Generator::EmitRepeatedFieldEncoding(TextWriter* const writer,
                                                  FieldDescriptorProto const& descriptor) {
  REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
  REQUIRE_FIELD_OR_RETURN(number, descriptor, number);
  REQUIRE_FIELD_OR_RETURN(type, descriptor, type);
  bool const packed = descriptor.options && descriptor.options->packed.value_or(true);
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
      TextWriter::IndentedScope is{writer};
      writer->AppendLine(absl::StrCat("encoder.", encoder_it->second, "(", number, ", value);"));
    }
    writer->AppendLine("}");
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError("invalid field type");
}

absl::Status Generator::EmitRequiredFieldEncoding(TextWriter* const writer,
                                                  FieldDescriptorProto const& descriptor) {
  REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
  REQUIRE_FIELD_OR_RETURN(number, descriptor, number);
  REQUIRE_FIELD_OR_RETURN(type, descriptor, type);
  auto const it = kFieldEncoderNames.find(type);
  if (it != kFieldEncoderNames.end()) {
    writer->AppendLine(absl::StrCat("encoder.", it->second, "(", number, ", proto.", name, ");"));
    return absl::OkStatus();
  } else {
    return absl::InvalidArgumentError("invalid field type");
  }
}

absl::Status Generator::EmitEnumEncoding(TextWriter* const writer,
                                         FieldDescriptorProto const& descriptor,
                                         bool const is_optional) {
  REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
  REQUIRE_FIELD_OR_RETURN(number, descriptor, number);
  REQUIRE_FIELD_OR_RETURN(label, descriptor, label);
  bool const packed = descriptor.options && descriptor.options->packed.value_or(true);
  switch (label) {
    case FieldDescriptorProto::Label::LABEL_OPTIONAL:
      if (is_optional) {
        writer->AppendLine(absl::StrCat("if (proto.", name, ".has_value()) {"));
        {
          TextWriter::IndentedScope is{writer};
          writer->AppendLine(
              absl::StrCat("encoder.EncodeEnumField(", number, ", proto.", name, ".value());"));
        }
        writer->AppendLine("}");
      } else {
        writer->AppendLine(
            absl::StrCat("encoder.EncodeEnumField(", number, ", proto.", name, ");"));
      }
      break;
    case FieldDescriptorProto::Label::LABEL_REPEATED: {
      if (packed) {
        writer->AppendLine(
            absl::StrCat("encoder.EncodePackedEnums(", number, ", proto.", name, ");"));
      } else {
        writer->AppendLine(absl::StrCat("for (auto const& value : proto.", name, ") {"));
        {
          TextWriter::IndentedScope is{writer};
          writer->AppendLine(absl::StrCat("encoder.EncodeEnumField(", number, ", value);"));
        }
        writer->AppendLine("}");
      }
    } break;
    case FieldDescriptorProto::Label::LABEL_REQUIRED:
      writer->AppendLine(absl::StrCat("encoder.EncodeEnumField(", number, ", proto.", name, ");"));
      break;
    default:
      return absl::InvalidArgumentError("invalid field label");
  }
  return absl::OkStatus();
}

absl::Status Generator::EmitObjectEncoding(TextWriter* const writer,
                                           FieldDescriptorProto const& descriptor) {
  REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
  REQUIRE_FIELD_OR_RETURN(number, descriptor, number);
  REQUIRE_FIELD_OR_RETURN(label, descriptor, label);
  REQUIRE_FIELD_OR_RETURN(proto_type_name, descriptor, type_name);
  auto const type_name = absl::StrReplaceAll(proto_type_name, {{".", "::"}});
  switch (label) {
    case FieldDescriptorProto::Label::LABEL_OPTIONAL:
      writer->AppendLine(absl::StrCat("if (proto.", name, ".has_value()) {"));
      {
        TextWriter::IndentedScope is{writer};
        writer->AppendLine(absl::StrCat("encoder.EncodeTag({ .field_number = ", number,
                                        ", .wire_type = ::tsdb2::proto::WireType::kLength });"));
        writer->AppendLine(absl::StrCat("encoder.EncodeSubMessage(", type_name, "::Encode(proto.",
                                        name, ".value()));"));
      }
      writer->AppendLine("}");
      break;
    case FieldDescriptorProto::Label::LABEL_REPEATED:
      writer->AppendLine(absl::StrCat("for (auto const& value : proto.", name, ") {"));
      {
        TextWriter::IndentedScope is{writer};
        writer->AppendLine(absl::StrCat("encoder.EncodeTag({ .field_number = ", number,
                                        ", .wire_type = ::tsdb2::proto::WireType::kLength });"));
        writer->AppendLine(
            absl::StrCat("encoder.EncodeSubMessage(", type_name, "::Encode(value));"));
      }
      writer->AppendLine("}");
      break;
    case FieldDescriptorProto::Label::LABEL_REQUIRED:
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

absl::Status Generator::EmitFieldEncoding(TextWriter* const writer,
                                          FieldDescriptorProto const& descriptor) const {
  DEFINE_CONST_OR_RETURN(is_optional, FieldIsWrappedInOptional(descriptor));
  if (descriptor.type_name.has_value()) {
    DEFINE_CONST_OR_RETURN(is_message, IsMessage(descriptor.type_name.value()));
    if (is_message) {
      return EmitObjectEncoding(writer, descriptor);
    } else {
      return EmitEnumEncoding(writer, descriptor, is_optional);
    }
  } else {
    REQUIRE_FIELD_OR_RETURN(label, descriptor, label);
    switch (label) {
      case FieldDescriptorProto::Label::LABEL_OPTIONAL:
        if (is_optional) {
          return EmitOptionalFieldEncoding(writer, descriptor);
        } else {
          return EmitRequiredFieldEncoding(writer, descriptor);
        }
      case FieldDescriptorProto::Label::LABEL_REPEATED: {
        return EmitRepeatedFieldEncoding(writer, descriptor);
      }
      case FieldDescriptorProto::Label::LABEL_REQUIRED:
        return EmitRequiredFieldEncoding(writer, descriptor);
      default:
        return absl::InvalidArgumentError("invalid field label");
    }
  }
}

absl::Status Generator::EmitImplementationForScope(internal::TextWriter* const writer,
                                                   PathView const prefix,
                                                   LexicalScope const& scope) const {
  for (auto const& message_type : scope.message_types) {
    REQUIRE_FIELD_OR_RETURN(name, message_type, name);
    Path const path = JoinPath(prefix, name);
    RETURN_IF_ERROR(EmitImplementationForScope(writer, /*prefix=*/path,
                                               LexicalScope{
                                                   .base_path = JoinPath(scope.base_path, name),
                                                   .message_types = message_type.nested_type,
                                                   .enum_types = message_type.enum_type,
                                               }));
    auto const qualified_name = absl::StrJoin(path, "::");
    writer->AppendEmptyLine();
    writer->AppendLine(absl::StrCat("::absl::StatusOr<", qualified_name, "> ", qualified_name,
                                    "::Decode(::absl::Span<uint8_t const> const data) {"));
    {
      TextWriter::IndentedScope is{writer};
      writer->AppendLine(absl::StrCat(qualified_name, " proto;"));
      writer->AppendLine("::tsdb2::proto::Decoder decoder{data};");
      writer->AppendLine("while (!decoder.at_end()) {");
      {
        TextWriter::IndentedScope is{writer};
        writer->AppendLine("DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());");
        writer->AppendLine("switch (tag.field_number) {");
        {
          TextWriter::IndentedScope is{writer};
          for (auto const& field : message_type.field) {
            RETURN_IF_ERROR(EmitFieldDecoding(writer, field));
          }
          writer->AppendLine("default:");
          {
            TextWriter::IndentedScope is{writer};
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
    writer->AppendLine(absl::StrCat("::tsdb2::io::Cord ", qualified_name, "::Encode(",
                                    qualified_name, " const& proto) {"));
    {
      TextWriter::IndentedScope is{writer};
      writer->AppendLine("::tsdb2::proto::Encoder encoder;");
      for (auto const& field : message_type.field) {
        RETURN_IF_ERROR(EmitFieldEncoding(writer, field));
      }
      writer->AppendLine("return std::move(encoder).Finish();");
    }
    writer->AppendLine("}");
  }
  return absl::OkStatus();
}

}  // namespace proto
}  // namespace tsdb2
