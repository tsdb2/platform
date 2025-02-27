#include "proto/generator.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
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

ABSL_FLAG(
    bool, emit_reflection_api, false,
    "Whether to emit the reflection API for the generated types. The reflection API is very heavy "
    "to compile so it's disabled by default, but you'll need it if you use TextFormat.");

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
    "common/utilities.h",
    "io/cord.h",
    "proto/proto.h",
});

auto constexpr kDefaultSourceIncludes = tsdb2::common::fixed_flat_set_of<std::string_view>({
    "absl/status/status.h",
    "absl/status/statusor.h",
    "absl/types/span.h",
    "common/flat_set.h",
    "common/utilities.h",
    "io/cord.h",
    "proto/proto.h",
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
        {FieldDescriptorProto::Type::TYPE_DOUBLE, "DecodeDoubleField"},
        {FieldDescriptorProto::Type::TYPE_FLOAT, "DecodeFloatField"},
        {FieldDescriptorProto::Type::TYPE_INT64, "DecodeInt64Field"},
        {FieldDescriptorProto::Type::TYPE_UINT64, "DecodeUInt64Field"},
        {FieldDescriptorProto::Type::TYPE_INT32, "DecodeInt32Field"},
        {FieldDescriptorProto::Type::TYPE_FIXED64, "DecodeFixedUInt64Field"},
        {FieldDescriptorProto::Type::TYPE_FIXED32, "DecodeFixedUInt32Field"},
        {FieldDescriptorProto::Type::TYPE_BOOL, "DecodeBoolField"},
        {FieldDescriptorProto::Type::TYPE_STRING, "DecodeStringField"},
        {FieldDescriptorProto::Type::TYPE_BYTES, "DecodeBytesField"},
        {FieldDescriptorProto::Type::TYPE_UINT32, "DecodeUInt32Field"},
        {FieldDescriptorProto::Type::TYPE_SFIXED32, "DecodeFixedInt32Field"},
        {FieldDescriptorProto::Type::TYPE_SFIXED64, "DecodeFixedInt64Field"},
        {FieldDescriptorProto::Type::TYPE_SINT32, "DecodeSInt32Field"},
        {FieldDescriptorProto::Type::TYPE_SINT64, "DecodeSInt64Field"},
    });

auto constexpr kRepeatedFieldDecoderNames =
    tsdb2::common::fixed_flat_map_of<FieldDescriptorProto::Type, std::string_view>({
        {FieldDescriptorProto::Type::TYPE_DOUBLE, "DecodeRepeatedDoubles"},
        {FieldDescriptorProto::Type::TYPE_FLOAT, "DecodeRepeatedFloats"},
        {FieldDescriptorProto::Type::TYPE_INT64, "DecodeRepeatedInt64s"},
        {FieldDescriptorProto::Type::TYPE_UINT64, "DecodeRepeatedUInt64s"},
        {FieldDescriptorProto::Type::TYPE_INT32, "DecodeRepeatedInt32s"},
        {FieldDescriptorProto::Type::TYPE_FIXED64, "DecodeRepeatedFixedUInt64s"},
        {FieldDescriptorProto::Type::TYPE_FIXED32, "DecodeRepeatedFixedUInt32s"},
        {FieldDescriptorProto::Type::TYPE_BOOL, "DecodeRepeatedBools"},
        {FieldDescriptorProto::Type::TYPE_UINT32, "DecodeRepeatedUInt32s"},
        {FieldDescriptorProto::Type::TYPE_SFIXED32, "DecodeRepeatedFixedInt32s"},
        {FieldDescriptorProto::Type::TYPE_SFIXED64, "DecodeRepeatedFixedInt64s"},
        {FieldDescriptorProto::Type::TYPE_SINT32, "DecodeRepeatedSInt32s"},
        {FieldDescriptorProto::Type::TYPE_SINT64, "DecodeRepeatedSInt64s"},
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

Generator::Path JoinPath(Generator::PathView const lhs, std::string_view const rhs) {
  Generator::Path result;
  result.reserve(lhs.size() + 1);
  result.insert(result.end(), lhs.begin(), lhs.end());
  result.emplace_back(rhs);
  return result;
}

Generator::Path JoinPath(Generator::PathView const lhs, Generator::PathView const rhs) {
  Generator::Path result;
  result.reserve(lhs.size() + rhs.size());
  result.insert(result.end(), lhs.begin(), lhs.end());
  result.insert(result.end(), rhs.begin(), rhs.end());
  return result;
}

Generator::Path SplitPath(std::string_view const proto_type_name) {
  auto const splitter = absl::StrSplit(proto_type_name, '.');
  std::vector<std::string> const components{splitter.begin(), splitter.end()};
  return {components.begin(), components.end()};
}

absl::StatusOr<Generator::Path> GetTypePath(std::string_view const proto_type_name) {
  auto const splitter = absl::StrSplit(proto_type_name, '.');
  std::vector<std::string> const components{splitter.begin(), splitter.end()};
  if (components.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid type name: \"", absl::CEscape(proto_type_name), "\""));
  }
  if (components.front().empty()) {
    // fully qualified name
    return Generator::Path{components.begin() + 1, components.end()};
  } else {
    // TODO: C++ semantics search algorithm
    return absl::UnimplementedError(
        absl::StrCat("cannot resolve \"", absl::CEscape(proto_type_name),
                     "\": partially qualified types not yet implemented"));
  }
}

std::string MakeCycleMessage(DependencyManager::Cycle const& cycle) {
  std::vector<std::string> entries;
  entries.reserve(cycle.size() + 1);
  for (auto const& item : cycle) {
    entries.emplace_back(absl::StrCat(absl::StrJoin(item.first, "."), ".", item.second));
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
  DEFINE_VAR_OR_RETURN(builder, Builder::Create(file_descriptor));
  return std::move(builder).Build();
}

absl::StatusOr<std::string> Generator::GenerateHeaderFileContent() {
  TextWriter writer;
  DEFINE_CONST_OR_RETURN(header_guard_name, GetHeaderGuardName());
  writer.AppendUnindentedLine("#ifndef ", header_guard_name);
  writer.AppendUnindentedLine("#define ", header_guard_name);
  writer.AppendEmptyLine();
  writer.AppendUnindentedLine("#include <cstdint>");
  writer.AppendUnindentedLine("#include <optional>");
  writer.AppendUnindentedLine("#include <string>");
  writer.AppendUnindentedLine("#include <tuple>");
  writer.AppendUnindentedLine("#include <utility>");
  writer.AppendUnindentedLine("#include <variant>");
  writer.AppendUnindentedLine("#include <vector>");
  writer.AppendEmptyLine();
  EmitHeaderIncludes(&writer);
  writer.AppendEmptyLine();
  writer.AppendLine("TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();");
  writer.AppendEmptyLine();
  DEFINE_CONST_OR_RETURN(package, GetCppPackage());
  if (!package.empty()) {
    writer.AppendLine("namespace ", package, " {");
    writer.AppendEmptyLine();
  }
  LexicalScope const global_scope{
      .base_path = base_path_,
      .global = true,
      .message_types = file_descriptor_->message_type,
      .enum_types = file_descriptor_->enum_type,
  };
  RETURN_IF_ERROR(EmitHeaderForScope(&writer, global_scope));
  if (!package.empty()) {
    writer.AppendLine("}  // namespace ", package);
    writer.AppendEmptyLine();
  }
  if (emit_reflection_api_) {
    writer.AppendLine("namespace tsdb2::proto {");
    RETURN_IF_ERROR(EmitDescriptorSpecializationsForScope(&writer, global_scope));
    writer.AppendEmptyLine();
    writer.AppendLine("}  // namespace tsdb2::proto");
    writer.AppendEmptyLine();
  }
  writer.AppendLine("TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();");
  writer.AppendEmptyLine();
  writer.AppendUnindentedLine("#endif  // ", header_guard_name);
  return std::move(writer).Finish();
}

absl::StatusOr<std::string> Generator::GenerateSourceFileContent() {
  TextWriter writer;
  REQUIRE_FIELD_OR_RETURN(name, *file_descriptor_, name);
  auto const header_path = generator::MakeHeaderFileName(name);
  writer.AppendUnindentedLine("#include \"", header_path, "\"");
  writer.AppendEmptyLine();
  writer.AppendUnindentedLine("#include <cstddef>");
  writer.AppendUnindentedLine("#include <cstdint>");
  writer.AppendUnindentedLine("#include <functional>");
  writer.AppendUnindentedLine("#include <tuple>");
  writer.AppendUnindentedLine("#include <utility>");
  writer.AppendUnindentedLine("#include <variant>");
  writer.AppendEmptyLine();
  EmitSourceIncludes(&writer);
  DEFINE_CONST_OR_RETURN(package, GetCppPackage());
  if (!package.empty()) {
    writer.AppendEmptyLine();
    writer.AppendLine("namespace ", package, " {");
  }
  writer.AppendEmptyLine();
  writer.AppendLine("TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();");
  LexicalScope const global_scope{
      .base_path = base_path_,
      .global = true,
      .message_types = file_descriptor_->message_type,
      .enum_types = file_descriptor_->enum_type,
  };
  RETURN_IF_ERROR(EmitImplementationForScope(&writer, /*prefix=*/{}, global_scope));
  if (emit_reflection_api_) {
    RETURN_IF_ERROR(EmitReflectionDescriptors(&writer));
  }
  writer.AppendEmptyLine();
  writer.AppendLine("TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();");
  if (!package.empty()) {
    writer.AppendEmptyLine();
    writer.AppendLine("}  // namespace ", package);
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
  return Builder(file_descriptor, SplitPath(package_name));
}

absl::StatusOr<Generator> Generator::Builder::Build() && {
  Generator::LexicalScope const global_scope{
      .base_path = base_path_,
      .global = true,
      .message_types = file_descriptor_->message_type,
      .enum_types = file_descriptor_->enum_type,
  };
  RETURN_IF_ERROR(AddLexicalScope(global_scope));
  RETURN_IF_ERROR(
      BuildFlatDependencies("", file_descriptor_->message_type, file_descriptor_->enum_type));
  DEFINE_CONST_OR_RETURN(cycles, FindCycles(global_scope));
  if (!cycles.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("message dependency cycle detected: ", MakeCycleMessage(cycles.front())));
  }
  auto const flat_cycles = flat_dependencies_.FindCycles(base_path_);
  if (!flat_cycles.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("message dependency cycle detected: ", MakeCycleMessage(flat_cycles.front())));
  }
  ASSIGN_OR_RETURN(enum_types_by_path_, GetEnumTypesByPath());
  ASSIGN_OR_RETURN(message_types_by_path_, GetMessageTypesByPath());
  return Generator(file_descriptor_, absl::GetFlag(FLAGS_emit_reflection_api),
                   std::move(enum_types_by_path_), std::move(message_types_by_path_),
                   std::move(dependencies_), std::move(flat_dependencies_), std::move(base_path_));
}

absl::Status Generator::Builder::AddLexicalScope(Generator::LexicalScope const& scope) {
  for (auto const& enum_type : scope.enum_types) {
    REQUIRE_FIELD_OR_RETURN(name, enum_type, name);
    if (!kIdentifierPattern->Test(name)) {
      return absl::InvalidArgumentError(
          absl::StrCat("invalid enum type name: \"", absl::CEscape(name), "\""));
    }
    Path const path = JoinPath(scope.base_path, name);
    dependencies_.AddNode(path);
  }
  for (auto const& message_type : scope.message_types) {
    REQUIRE_FIELD_OR_RETURN(name, message_type, name);
    if (!kIdentifierPattern->Test(name)) {
      return absl::InvalidArgumentError(
          absl::StrCat("invalid message type name: \"", absl::CEscape(name), "\""));
    }
    Path const path = JoinPath(scope.base_path, name);
    dependencies_.AddNode(path);
    RETURN_IF_ERROR(AddLexicalScope(Generator::LexicalScope{
        .base_path = path,
        .global = false,
        .message_types = message_type.nested_type,
        .enum_types = message_type.enum_type,
    }));
  }
  for (auto const& message_type : scope.message_types) {
    REQUIRE_FIELD_OR_RETURN(message_name, message_type, name);
    Path const path = JoinPath(scope.base_path, message_name);
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

std::optional<std::string> Generator::Builder::MaybeGetQualifiedName(PathView const path) {
  if (path.size() < base_path_.size()) {
    return std::nullopt;
  }
  size_t offset = 0;
  for (; offset < base_path_.size(); ++offset) {
    if (path[offset] != base_path_[offset]) {
      return std::nullopt;
    }
  }
  return absl::StrJoin(path.begin() + offset, path.end(), ".");
}

absl::Status Generator::Builder::BuildFlatDependencies(
    std::string_view const scope_name,
    absl::Span<google::protobuf::DescriptorProto const> const message_types,
    absl::Span<google::protobuf::EnumDescriptorProto const> const enum_types) {
  for (auto const& enum_type : enum_types) {
    REQUIRE_FIELD_OR_RETURN(name, enum_type, name);
    auto const qualified_name = scope_name.empty() ? name : absl::StrCat(scope_name, ".", name);
    flat_dependencies_.AddNode(JoinPath(base_path_, qualified_name));
  }
  for (auto const& message_type : message_types) {
    REQUIRE_FIELD_OR_RETURN(name, message_type, name);
    auto const qualified_name = scope_name.empty() ? name : absl::StrCat(scope_name, ".", name);
    Path const child_path = JoinPath(base_path_, qualified_name);
    flat_dependencies_.AddNode(child_path);
    RETURN_IF_ERROR(
        BuildFlatDependencies(qualified_name, message_type.nested_type, message_type.enum_type));
    for (auto const& field : message_type.field) {
      if (field.type_name.has_value()) {
        REQUIRE_FIELD_OR_RETURN(label, field, label);
        if (label != FieldDescriptorProto::Label::LABEL_REPEATED) {
          DEFINE_CONST_OR_RETURN(dependee_path, GetTypePath(field.type_name.value()));
          auto const maybe_qualified_dependee_name = MaybeGetQualifiedName(dependee_path);
          if (maybe_qualified_dependee_name.has_value()) {
            REQUIRE_FIELD_OR_RETURN(field_name, field, name);
            flat_dependencies_.AddDependency(
                child_path, JoinPath(base_path_, maybe_qualified_dependee_name.value()),
                field_name);
          }
        }
      }
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<Generator::Builder::Cycles> Generator::Builder::FindCycles(
    LexicalScope const& scope) const {
  auto cycles = dependencies_.FindCycles(scope.base_path);
  if (!cycles.empty()) {
    return std::move(cycles);
  }
  for (auto const& message_type : scope.message_types) {
    REQUIRE_FIELD_OR_RETURN(name, message_type, name);
    Path path = JoinPath(scope.base_path, name);
    DEFINE_VAR_OR_RETURN(cycles, FindCycles(LexicalScope{
                                     .base_path = path,
                                     .global = false,
                                     .message_types = message_type.nested_type,
                                     .enum_types = message_type.enum_type,
                                 }));
    if (!cycles.empty()) {
      return std::move(cycles);
    }
  }
  return Cycles();
}

absl::Status Generator::Builder::GetEnumTypesByPathImpl(
    LexicalScope const& scope,
    absl::flat_hash_map<Path, google::protobuf::EnumDescriptorProto>* const descriptors) {
  for (auto const& enum_type : scope.enum_types) {
    REQUIRE_FIELD_OR_RETURN(name, enum_type, name);
    descriptors->try_emplace(JoinPath(scope.base_path, name), enum_type);
  }
  for (auto const& message_type : scope.message_types) {
    REQUIRE_FIELD_OR_RETURN(name, message_type, name);
    LexicalScope const child_scope{
        .base_path = JoinPath(scope.base_path, name),
        .global = false,
        .message_types = message_type.nested_type,
        .enum_types = message_type.enum_type,
    };
    RETURN_IF_ERROR(GetEnumTypesByPathImpl(child_scope, descriptors));
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::flat_hash_map<Generator::Path, google::protobuf::EnumDescriptorProto>>
Generator::Builder::GetEnumTypesByPath() const {
  absl::flat_hash_map<Path, google::protobuf::EnumDescriptorProto> descriptors;
  LexicalScope const global_scope{
      .base_path = base_path_,
      .global = true,
      .message_types = file_descriptor_->message_type,
      .enum_types = file_descriptor_->enum_type,
  };
  RETURN_IF_ERROR(GetEnumTypesByPathImpl(global_scope, &descriptors));
  return std::move(descriptors);
}

absl::Status Generator::Builder::GetMessageTypesByPathImpl(
    LexicalScope const& scope,
    absl::flat_hash_map<Path, google::protobuf::DescriptorProto>* const descriptors) {
  for (auto const& message_type : scope.message_types) {
    REQUIRE_FIELD_OR_RETURN(name, message_type, name);
    descriptors->try_emplace(JoinPath(scope.base_path, name), message_type);
    LexicalScope const child_scope{
        .base_path = JoinPath(scope.base_path, name),
        .global = false,
        .message_types = message_type.nested_type,
        .enum_types = message_type.enum_type,
    };
    RETURN_IF_ERROR(GetMessageTypesByPathImpl(child_scope, descriptors));
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::flat_hash_map<Generator::Path, google::protobuf::DescriptorProto>>
Generator::Builder::GetMessageTypesByPath() const {
  absl::flat_hash_map<Path, google::protobuf::DescriptorProto> descriptors;
  LexicalScope const global_scope{
      .base_path = base_path_,
      .global = true,
      .message_types = file_descriptor_->message_type,
      .enum_types = file_descriptor_->enum_type,
  };
  RETURN_IF_ERROR(GetMessageTypesByPathImpl(global_scope, &descriptors));
  return std::move(descriptors);
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
  return !enum_types_by_path_.contains(path);
}

absl::StatusOr<bool> Generator::IsEnum(std::string_view proto_type_name) const {
  DEFINE_CONST_OR_RETURN(is_message, IsMessage(proto_type_name));
  return !is_message;
}

absl::StatusOr<bool> Generator::HasRequiredFields(
    google::protobuf::DescriptorProto const& descriptor) {
  for (auto const& field : descriptor.field) {
    if (!field.oneof_index.has_value()) {
      REQUIRE_FIELD_OR_RETURN(label, field, label);
      if (label == FieldDescriptorProto::Label::LABEL_REQUIRED) {
        return true;
      }
    }
  }
  return false;
}

absl::StatusOr<google::protobuf::OneofDescriptorProto const*> Generator::GetOneofDecl(
    google::protobuf::DescriptorProto const& message_type, size_t const index) {
  if (index >= message_type.oneof_decl.size()) {
    return absl::InvalidArgumentError(absl::StrCat("invalid oneof index ", index,
                                                   ", there are only ",
                                                   message_type.oneof_decl.size(), " oneofs"));
  }
  return &(message_type.oneof_decl[index]);
}

size_t Generator::GetNumGeneratedFields(google::protobuf::DescriptorProto const& message_type) {
  absl::flat_hash_set<size_t> oneof_indices;
  oneof_indices.reserve(message_type.oneof_decl.size());
  size_t num_regular_fields = 0;
  for (auto const& field : message_type.field) {
    if (field.oneof_index.has_value()) {
      oneof_indices.emplace(field.oneof_index.value());
    } else {
      ++num_regular_fields;
    }
  }
  return num_regular_fields + oneof_indices.size();
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
      writer->AppendUnindentedLine("#include \"", header, "\"  // IWYU pragma: export");
    } else {
      writer->AppendUnindentedLine("#include \"", header, "\"");
    }
  }
}

}  // namespace

void Generator::EmitHeaderIncludes(TextWriter* const writer) const {
  EmitIncludes(*file_descriptor_, writer, kDefaultHeaderIncludes);
}

void Generator::EmitSourceIncludes(TextWriter* const writer) const {
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

absl::StatusOr<std::string> Generator::GetInitialEnumValue(
    std::string_view const proto_type_name) const {
  DEFINE_CONST_OR_RETURN(path, GetTypePath(proto_type_name));
  auto const it = enum_types_by_path_.find(path);
  if (it == enum_types_by_path_.end()) {
    return absl::InternalError(
        absl::StrCat("\"", absl::CEscape(proto_type_name), "\" doesn\'t refer to an enum type"));
  }
  auto const& descriptor = it->second;
  for (auto const& value : descriptor.value) {
    REQUIRE_FIELD_OR_RETURN(number, value, number);
    if (number == 0) {
      REQUIRE_FIELD_OR_RETURN(name, value, name);
      return absl::StrCat("::", absl::StrJoin(path, "::"), "::", name);
    }
  }
  for (auto const& value : descriptor.value) {
    REQUIRE_FIELD_OR_RETURN(name, value, name);
    return absl::StrCat("::", absl::StrJoin(path, "::"), "::", name);
  }
  REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
  return absl::FailedPreconditionError(absl::StrCat("enum \"", name, "\" is empty"));
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

absl::Status Generator::EmitOneofField(internal::TextWriter* const writer,
                                       google::protobuf::DescriptorProto const& message_type,
                                       size_t const index) const {
  DEFINE_CONST_OR_RETURN(oneof_decl, GetOneofDecl(message_type, index));
  REQUIRE_FIELD_OR_RETURN(name, *oneof_decl, name);
  std::vector<std::string> types;
  for (auto const& field : message_type.field) {
    if (field.oneof_index.has_value() && field.oneof_index.value() == index) {
      DEFINE_VAR_OR_RETURN(type_pair, GetFieldType(field));
      types.emplace_back(std::move(type_pair.first));
    }
  }
  if (types.empty()) {
    writer->AppendLine("std::variant<std::monostate> ", name, ";");
  } else {
    writer->AppendLine("std::variant<std::monostate, ", absl::StrJoin(types, ", "), "> ", name,
                       ";");
  }
  return absl::OkStatus();
}

absl::Status Generator::EmitMessageFields(
    internal::TextWriter* const writer,
    google::protobuf::DescriptorProto const& message_type) const {
  absl::flat_hash_set<size_t> oneof_indices;
  oneof_indices.reserve(message_type.oneof_decl.size());
  for (auto const& field : message_type.field) {
    REQUIRE_FIELD_OR_RETURN(name, field, name);
    if (field.oneof_index.has_value()) {
      size_t const index = field.oneof_index.value();
      auto const [unused_it, inserted] = oneof_indices.emplace(index);
      if (inserted) {
        RETURN_IF_ERROR(EmitOneofField(writer, message_type, index));
      }
    } else {
      REQUIRE_FIELD_OR_RETURN(label, field, label);
      DEFINE_CONST_OR_RETURN(type_pair, GetFieldType(field));
      auto const& [type, primitive] = type_pair;
      std::string deprecation;
      if (field.options && field.options->deprecated) {
        deprecation = "ABSL_DEPRECATED(\"\") ";
      }
      switch (label) {
        case FieldDescriptorProto::Label::LABEL_OPTIONAL:
          if (primitive && field.default_value.has_value()) {
            DEFINE_CONST_OR_RETURN(initializer,
                                   GetFieldInitializer(field, type, field.default_value.value()));
            writer->AppendLine(deprecation, type, " ", name, "{", initializer, "};");
          } else {
            writer->AppendLine(deprecation, "std::optional<", type, "> ", name, ";");
          }
          break;
        case FieldDescriptorProto::Label::LABEL_REPEATED:
          writer->AppendLine(deprecation, "std::vector<", type, "> ", name, ";");
          break;
        case FieldDescriptorProto::Label::LABEL_REQUIRED:
          if (field.default_value.has_value()) {
            DEFINE_CONST_OR_RETURN(initializer,
                                   GetFieldInitializer(field, type, field.default_value.value()));
            writer->AppendLine(deprecation, type, " ", name, "{", initializer, "};");
          } else if (primitive) {
            REQUIRE_FIELD_OR_RETURN(primitive_type, field, type);
            switch (primitive_type) {
              case FieldDescriptorProto::Type::TYPE_STRING:
              case FieldDescriptorProto::Type::TYPE_BYTES:
                writer->AppendLine(deprecation, type, " ", name, ";");
                break;
              case FieldDescriptorProto::Type::TYPE_ENUM: {
                REQUIRE_FIELD_OR_RETURN(type_name, field, type_name);
                DEFINE_CONST_OR_RETURN(initial_value, GetInitialEnumValue(type_name));
                writer->AppendLine(deprecation, type, " ", name, " = ", initial_value, ";");
              } break;
              default:
                writer->AppendLine(deprecation, type, " ", name, "{};");
                break;
            }
          } else {
            writer->AppendLine(deprecation, type, " ", name, ";");
          }
          break;
        default:
          return absl::InvalidArgumentError("unknown value for field label");
      }
    }
  }
  return absl::OkStatus();
}

absl::Status Generator::EmitHeaderForScope(TextWriter* const writer,
                                           LexicalScope const& scope) const {
  for (auto const& message_type : scope.message_types) {
    REQUIRE_FIELD_OR_RETURN(name, message_type, name);
    writer->AppendLine("struct ", name, ";");
  }
  if (!scope.message_types.empty()) {
    writer->AppendEmptyLine();
  }
  for (auto const& enum_type : scope.enum_types) {
    REQUIRE_FIELD_OR_RETURN(name, enum_type, name);
    if (enum_type.options && enum_type.options->deprecated) {
      writer->AppendLine("ABSL_DEPRECATED(\"\")");
    }
    writer->AppendLine("enum class ", name, " {");
    {
      TextWriter::IndentedScope is{writer};
      for (auto const& value : enum_type.value) {
        REQUIRE_FIELD_OR_RETURN(name, value, name);
        if (!kIdentifierPattern->Test(name)) {
          return absl::InvalidArgumentError(
              absl::StrCat("invalid enum value name: \"", name, "\""));
        }
        REQUIRE_FIELD_OR_RETURN(number, value, number);
        std::string deprecation;
        if (value.options && value.options->deprecated) {
          deprecation = " ABSL_DEPRECATED(\"\")";
        }
        writer->AppendLine(name, deprecation, " = ", number, ",");
      }
    }
    writer->AppendLine("};");
    if (emit_reflection_api_) {
      writer->AppendEmptyLine();
      if (scope.global) {
        writer->AppendLine("extern ::tsdb2::proto::EnumDescriptor<", name, ", ",
                           enum_type.value.size(), "> const ", name, "_ENUM_DESCRIPTOR;");
      } else {
        writer->AppendLine("static ::tsdb2::proto::EnumDescriptor<", name, ", ",
                           enum_type.value.size(), "> const ", name, "_ENUM_DESCRIPTOR;");
      }
    }
    writer->AppendEmptyLine();
    writer->AppendLine("template <typename H>");
    if (scope.global) {
      writer->AppendLine("inline H AbslHashValue(H h, ", name, " const& value) {");
    } else {
      writer->AppendLine("friend H AbslHashValue(H h, ", name, " const& value) {");
    }
    {
      TextWriter::IndentedScope is{writer};
      writer->AppendLine("return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));");
    }
    writer->AppendLine("}");
    writer->AppendEmptyLine();
    writer->AppendLine("template <typename State>");
    if (scope.global) {
      writer->AppendLine("inline State Tsdb2FingerprintValue(State state, ", name,
                         " const& value) {");
    } else {
      writer->AppendLine("friend State Tsdb2FingerprintValue(State state, ", name,
                         " const& value) {");
    }
    {
      TextWriter::IndentedScope is{writer};
      writer->AppendLine(
          "return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));");
    }
    writer->AppendLine("}");
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
    if (message_type.options && message_type.options->deprecated) {
      writer->AppendLine("ABSL_DEPRECATED(\"\")");
    }
    writer->AppendLine("struct ", name, " : public ::tsdb2::proto::Message {");
    {
      TextWriter::IndentedScope is{writer};
      if (emit_reflection_api_) {
        writer->AppendLine("static ::tsdb2::proto::MessageDescriptor<", name, ", ",
                           GetNumGeneratedFields(message_type), "> const MESSAGE_DESCRIPTOR;");
        writer->AppendEmptyLine();
      }
      RETURN_IF_ERROR(EmitHeaderForScope(writer, LexicalScope{
                                                     .base_path = JoinPath(scope.base_path, name),
                                                     .global = false,
                                                     .message_types = message_type.nested_type,
                                                     .enum_types = message_type.enum_type,
                                                 }));
      writer->AppendLine("static ::absl::StatusOr<", name,
                         "> Decode(::absl::Span<uint8_t const> data);");
      writer->AppendLine("static ::tsdb2::io::Cord Encode(", name, " const& proto);");
      writer->AppendEmptyLine();
      writer->AppendLine("static auto Tie(", name, " const& proto) {");
      {
        TextWriter::IndentedScope is{writer};
        std::vector<std::string> params;
        params.reserve(message_type.field.size());
        absl::flat_hash_set<size_t> oneof_indices;
        oneof_indices.reserve(message_type.oneof_decl.size());
        for (auto const& field : message_type.field) {
          if (field.oneof_index.has_value()) {
            size_t const index = field.oneof_index.value();
            auto const [unused_it, inserted] = oneof_indices.emplace(index);
            if (inserted) {
              DEFINE_CONST_OR_RETURN(oneof_decl, GetOneofDecl(message_type, index));
              REQUIRE_FIELD_OR_RETURN(name, *oneof_decl, name);
              params.emplace_back(absl::StrCat("proto.", name));
            }
          } else {
            REQUIRE_FIELD_OR_RETURN(name, field, name);
            params.emplace_back(absl::StrCat("proto.", name));
          }
        }
        writer->AppendLine("return std::tie(", absl::StrJoin(params, ", "), ");");
      }
      writer->AppendLine("}");
      writer->AppendEmptyLine();
      writer->AppendLine("template <typename H>");
      writer->AppendLine("friend H AbslHashValue(H h, ", name, " const& proto) {");
      {
        TextWriter::IndentedScope is{writer};
        writer->AppendLine("return H::combine(std::move(h), Tie(proto));");
      }
      writer->AppendLine("}");
      writer->AppendEmptyLine();
      writer->AppendLine("template <typename State>");
      writer->AppendLine("friend State Tsdb2FingerprintValue(State state, ", name,
                         " const& proto) {");
      {
        TextWriter::IndentedScope is{writer};
        writer->AppendLine("return State::Combine(std::move(state), Tie(proto));");
      }
      writer->AppendLine("}");
      writer->AppendEmptyLine();
      for (auto const& op : {"==", "!=", "<", "<=", ">", ">="}) {
        writer->AppendLine("friend bool operator", op, "(", name, " const& lhs, ", name,
                           " const& rhs) { return Tie(lhs) ", op, " Tie(rhs); }");
      }
      if (!message_type.field.empty()) {
        writer->AppendEmptyLine();
      }
      RETURN_IF_ERROR(EmitMessageFields(writer, message_type));
    }
    writer->AppendLine("};");
    writer->AppendEmptyLine();
  }
  return absl::OkStatus();
}

absl::Status Generator::EmitDescriptorSpecializationsForScope(internal::TextWriter* const writer,
                                                              LexicalScope const& scope) const {
  for (auto const& enum_type : scope.enum_types) {
    REQUIRE_FIELD_OR_RETURN(name, enum_type, name);
    auto const path = JoinPath(scope.base_path, name);
    auto const fully_qualified_name = absl::StrCat("::", absl::StrJoin(path, "::"));
    writer->AppendEmptyLine();
    writer->AppendLine("template <>");
    writer->AppendLine("inline auto const& GetEnumDescriptor<", fully_qualified_name, ">() {");
    {
      TextWriter::IndentedScope is{writer};
      writer->AppendLine("return ", fully_qualified_name, "_ENUM_DESCRIPTOR;");
    }
    writer->AppendLine("};");
  }
  for (auto const& message_type : scope.message_types) {
    REQUIRE_FIELD_OR_RETURN(name, message_type, name);
    auto const path = JoinPath(scope.base_path, name);
    auto const fully_qualified_name = absl::StrCat("::", absl::StrJoin(path, "::"));
    LexicalScope const child_scope{
        .base_path = path,
        .global = false,
        .message_types = message_type.nested_type,
        .enum_types = message_type.enum_type,
    };
    RETURN_IF_ERROR(EmitDescriptorSpecializationsForScope(writer, child_scope));
    writer->AppendEmptyLine();
    writer->AppendLine("template <>");
    writer->AppendLine("inline auto const& GetMessageDescriptor<", fully_qualified_name, ">() {");
    {
      TextWriter::IndentedScope is{writer};
      writer->AppendLine("return ", fully_qualified_name, "::MESSAGE_DESCRIPTOR;");
    }
    writer->AppendLine("};");
  }
  return absl::OkStatus();
}

absl::Status Generator::EmitOptionalFieldDecoding(TextWriter* const writer,
                                                  FieldDescriptorProto const& descriptor) {
  REQUIRE_FIELD_OR_RETURN(number, descriptor, number);
  writer->AppendLine("case ", number, ": {");
  {
    REQUIRE_FIELD_OR_RETURN(type, descriptor, type);
    auto const it = kFieldDecoderNames.find(type);
    if (it == kFieldDecoderNames.end()) {
      return absl::InvalidArgumentError("invalid field type");
    }
    TextWriter::IndentedScope is{writer};
    REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
    if (type != FieldDescriptorProto::Type::TYPE_STRING &&
        type != FieldDescriptorProto::Type::TYPE_BYTES) {
      writer->AppendLine("DEFINE_CONST_OR_RETURN(value, decoder.", it->second, "(tag.wire_type));");
      writer->AppendLine("proto.", name, ".emplace(value);");
    } else {
      writer->AppendLine("DEFINE_VAR_OR_RETURN(value, decoder.", it->second, "(tag.wire_type));");
      writer->AppendLine("proto.", name, ".emplace(std::move(value));");
    }
  }
  writer->AppendLine("} break;");
  return absl::OkStatus();
}

absl::Status Generator::EmitRepeatedFieldDecoding(TextWriter* const writer,
                                                  FieldDescriptorProto const& descriptor) {
  REQUIRE_FIELD_OR_RETURN(number, descriptor, number);
  writer->AppendLine("case ", number, ": {");
  {
    TextWriter::IndentedScope is{writer};
    REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
    REQUIRE_FIELD_OR_RETURN(type, descriptor, type);
    auto const it1 = kRepeatedFieldDecoderNames.find(type);
    auto const it2 = kFieldDecoderNames.find(type);
    if (it1 != kRepeatedFieldDecoderNames.end()) {
      writer->AppendLine("RETURN_IF_ERROR(decoder.", it1->second, "(tag.wire_type, &proto.", name,
                         "));");
    } else if (it2 != kFieldDecoderNames.end()) {
      if (type != FieldDescriptorProto::Type::TYPE_STRING &&
          type != FieldDescriptorProto::Type::TYPE_BYTES) {
        writer->AppendLine("DEFINE_CONST_OR_RETURN(value, decoder.", it2->second,
                           "(tag.wire_type));");
        writer->AppendLine("proto.", name, ".emplace_back(value);");
      } else {
        writer->AppendLine("DEFINE_VAR_OR_RETURN(value, decoder.", it2->second,
                           "(tag.wire_type));");
        writer->AppendLine("proto.", name, ".emplace_back(std::move(value));");
      }
    } else {
      return absl::InvalidArgumentError("invalid field type");
    }
  }
  writer->AppendLine("} break;");
  return absl::OkStatus();
}

absl::Status Generator::EmitRawFieldDecoding(TextWriter* const writer,
                                             FieldDescriptorProto const& descriptor,
                                             bool const required) {
  REQUIRE_FIELD_OR_RETURN(number, descriptor, number);
  writer->AppendLine("case ", number, ": {");
  {
    REQUIRE_FIELD_OR_RETURN(type, descriptor, type);
    auto const it = kFieldDecoderNames.find(type);
    if (it == kFieldDecoderNames.end()) {
      return absl::InvalidArgumentError("invalid field type");
    }
    TextWriter::IndentedScope is{writer};
    REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
    if (type != FieldDescriptorProto::Type::TYPE_STRING &&
        type != FieldDescriptorProto::Type::TYPE_BYTES) {
      writer->AppendLine("DEFINE_CONST_OR_RETURN(value, decoder.", it->second, "(tag.wire_type));");
      writer->AppendLine("proto.", name, " = value;");
    } else {
      writer->AppendLine("DEFINE_VAR_OR_RETURN(value, decoder.", it->second, "(tag.wire_type));");
      writer->AppendLine("proto.", name, " = std::move(value);");
    }
    if (required) {
      writer->AppendLine("decoded.emplace(", number, ");");
    }
  }
  writer->AppendLine("} break;");
  return absl::OkStatus();
}

absl::Status Generator::EmitObjectDecoding(TextWriter* const writer,
                                           FieldDescriptorProto const& descriptor) {
  REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
  REQUIRE_FIELD_OR_RETURN(number, descriptor, number);
  REQUIRE_FIELD_OR_RETURN(label, descriptor, label);
  writer->AppendLine("case ", number, ": {");
  {
    TextWriter::IndentedScope is{writer};
    std::string const type_name = absl::StrReplaceAll(descriptor.type_name.value(), {{".", "::"}});
    writer->AppendLine("DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));");
    writer->AppendLine("DEFINE_VAR_OR_RETURN(value, ", type_name, "::Decode(child_span));");
    switch (label) {
      case FieldDescriptorProto::Label::LABEL_OPTIONAL:
        writer->AppendLine("proto.", name, ".emplace(std::move(value));");
        break;
      case FieldDescriptorProto::Label::LABEL_REQUIRED: {
        writer->AppendLine("proto.", name, " = std::move(value);");
        writer->AppendLine("decoded.emplace(", number, ");");
      } break;
      case FieldDescriptorProto::Label::LABEL_REPEATED:
        writer->AppendLine("proto.", name, ".emplace_back(std::move(value));");
        break;
      default:
        return absl::InvalidArgumentError("invalid field label");
    }
  }
  writer->AppendLine("} break;");
  return absl::OkStatus();
}

absl::Status Generator::EmitEnumDecoding(TextWriter* const writer,
                                         FieldDescriptorProto const& descriptor) const {
  REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
  REQUIRE_FIELD_OR_RETURN(number, descriptor, number);
  REQUIRE_FIELD_OR_RETURN(label, descriptor, label);
  writer->AppendLine("case ", number, ": {");
  {
    TextWriter::IndentedScope is{writer};
    std::string const type_name = absl::StrReplaceAll(descriptor.type_name.value(), {{".", "::"}});
    switch (label) {
      case FieldDescriptorProto::Label::LABEL_OPTIONAL: {
        writer->AppendLine("DEFINE_CONST_OR_RETURN(value, decoder.DecodeEnumField<", type_name,
                           ">(tag.wire_type));");
        DEFINE_CONST_OR_RETURN(is_optional, FieldIsWrappedInOptional(descriptor));
        if (is_optional) {
          writer->AppendLine("proto.", name, ".emplace(value);");
        } else {
          writer->AppendLine("proto.", name, " = value;");
        }
      } break;
      case FieldDescriptorProto::Label::LABEL_REQUIRED:
        writer->AppendLine("DEFINE_CONST_OR_RETURN(value, decoder.DecodeEnumField<", type_name,
                           ">(tag.wire_type));");
        writer->AppendLine("proto.", name, " = value;");
        writer->AppendLine("decoded.emplace(", number, ");");
        break;
      case FieldDescriptorProto::Label::LABEL_REPEATED:
        writer->AppendLine("RETURN_IF_ERROR(decoder.DecodeRepeatedEnums<", type_name,
                           ">(tag.wire_type, &proto.", name, "));");
        break;
      default:
        return absl::InvalidArgumentError("invalid field label");
    }
  }
  writer->AppendLine("} break;");
  return absl::OkStatus();
}

absl::Status Generator::EmitFieldDecoding(TextWriter* const writer,
                                          FieldDescriptorProto const& descriptor) const {
  if (descriptor.type_name.has_value()) {
    DEFINE_CONST_OR_RETURN(is_message, IsMessage(descriptor.type_name.value()));
    if (is_message) {
      return EmitObjectDecoding(writer, descriptor);
    } else {
      return EmitEnumDecoding(writer, descriptor);
    }
  } else {
    REQUIRE_FIELD_OR_RETURN(label, descriptor, label);
    switch (label) {
      case FieldDescriptorProto::Label::LABEL_OPTIONAL: {
        DEFINE_CONST_OR_RETURN(is_optional, FieldIsWrappedInOptional(descriptor));
        if (is_optional) {
          return EmitOptionalFieldDecoding(writer, descriptor);
        } else {
          return EmitRawFieldDecoding(writer, descriptor, /*required=*/false);
        }
      }
      case FieldDescriptorProto::Label::LABEL_REQUIRED:
        return EmitRawFieldDecoding(writer, descriptor, /*required=*/true);
      case FieldDescriptorProto::Label::LABEL_REPEATED:
        return EmitRepeatedFieldDecoding(writer, descriptor);
      default:
        return absl::InvalidArgumentError("invalid field label");
    }
  }
}

absl::Status Generator::EmitOneofFieldDecoding(
    internal::TextWriter* const writer, google::protobuf::DescriptorProto const& message_type,
    size_t const oneof_index) const {
  DEFINE_CONST_OR_RETURN(oneof_decl, GetOneofDecl(message_type, oneof_index));
  REQUIRE_FIELD_OR_RETURN(name, *oneof_decl, name);
  size_t field_index = 1;
  for (auto const& field : message_type.field) {
    if (field.oneof_index.has_value() && field.oneof_index.value() == oneof_index) {
      REQUIRE_FIELD_OR_RETURN(number, field, number);
      writer->AppendLine("case ", number, ": {");
      {
        TextWriter::IndentedScope is{writer};
        if (field.type_name.has_value()) {
          std::string const type_name = absl::StrReplaceAll(field.type_name.value(), {{".", "::"}});
          DEFINE_CONST_OR_RETURN(is_message, IsMessage(field.type_name.value()));
          if (is_message) {
            writer->AppendLine(
                "DEFINE_CONST_OR_RETURN(child_span, decoder.GetChildSpan(tag.wire_type));");
            writer->AppendLine("DEFINE_VAR_OR_RETURN(value, ", type_name, "::Decode(child_span));");
            writer->AppendLine("proto.", name, ".emplace<", field_index, ">(std::move(value));");
          } else {
            writer->AppendLine("DEFINE_CONST_OR_RETURN(value, decoder.DecodeEnumField<", type_name,
                               ">(tag.wire_type));");
            writer->AppendLine("proto.", name, ".emplace<", field_index, ">(value);");
          }
        } else {
          REQUIRE_FIELD_OR_RETURN(type, field, type);
          auto const it = kFieldDecoderNames.find(type);
          if (it == kFieldDecoderNames.end()) {
            return absl::InvalidArgumentError("invalid field type");
          }
          if (type != FieldDescriptorProto::Type::TYPE_STRING &&
              type != FieldDescriptorProto::Type::TYPE_BYTES) {
            writer->AppendLine("DEFINE_CONST_OR_RETURN(value, decoder.", it->second,
                               "(tag.wire_type));");
            writer->AppendLine("proto.", name, ".emplace<", field_index, ">(value);");
          } else {
            writer->AppendLine("DEFINE_VAR_OR_RETURN(value, decoder.", it->second,
                               "(tag.wire_type));");
            writer->AppendLine("proto.", name, ".emplace<", field_index, ">(std::move(value));");
          }
        }
      }
      writer->AppendLine("} break;");
      ++field_index;
    }
  }
  return absl::OkStatus();
}

absl::Status Generator::EmitOptionalFieldEncoding(TextWriter* const writer,
                                                  FieldDescriptorProto const& descriptor) {
  REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
  REQUIRE_FIELD_OR_RETURN(number, descriptor, number);
  REQUIRE_FIELD_OR_RETURN(type, descriptor, type);
  writer->AppendLine("if (proto.", name, ".has_value()) {");
  {
    auto const it = kFieldEncoderNames.find(type);
    if (it == kFieldEncoderNames.end()) {
      return absl::InvalidArgumentError("invalid field type");
    }
    TextWriter::IndentedScope is{writer};
    writer->AppendLine("encoder.", it->second, "(", number, ", proto.", name, ".value());");
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
      writer->AppendLine("encoder.", packed_encoder_it->second, "(", number, ", proto.", name,
                         ");");
      return absl::OkStatus();
    }
  }
  auto const encoder_it = kFieldEncoderNames.find(type);
  if (encoder_it != kFieldEncoderNames.end()) {
    writer->AppendLine("for (auto const& value : proto.", name, ") {");
    {
      TextWriter::IndentedScope is{writer};
      writer->AppendLine("encoder.", encoder_it->second, "(", number, ", value);");
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
    writer->AppendLine("encoder.", it->second, "(", number, ", proto.", name, ");");
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
        writer->AppendLine("if (proto.", name, ".has_value()) {");
        {
          TextWriter::IndentedScope is{writer};
          writer->AppendLine("encoder.EncodeEnumField(", number, ", proto.", name, ".value());");
        }
        writer->AppendLine("}");
      } else {
        writer->AppendLine("encoder.EncodeEnumField(", number, ", proto.", name, ");");
      }
      break;
    case FieldDescriptorProto::Label::LABEL_REPEATED: {
      if (packed) {
        writer->AppendLine("encoder.EncodePackedEnums(", number, ", proto.", name, ");");
      } else {
        writer->AppendLine("for (auto const& value : proto.", name, ") {");
        {
          TextWriter::IndentedScope is{writer};
          writer->AppendLine("encoder.EncodeEnumField(", number, ", value);");
        }
        writer->AppendLine("}");
      }
    } break;
    case FieldDescriptorProto::Label::LABEL_REQUIRED:
      writer->AppendLine("encoder.EncodeEnumField(", number, ", proto.", name, ");");
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
      writer->AppendLine("if (proto.", name, ".has_value()) {");
      {
        TextWriter::IndentedScope is{writer};
        writer->AppendLine("encoder.EncodeTag({.field_number = ", number,
                           ", .wire_type = ::tsdb2::proto::WireType::kLength});");
        writer->AppendLine("encoder.EncodeSubMessage(", type_name, "::Encode(proto.", name,
                           ".value()));");
      }
      writer->AppendLine("}");
      break;
    case FieldDescriptorProto::Label::LABEL_REPEATED:
      writer->AppendLine("for (auto const& value : proto.", name, ") {");
      {
        TextWriter::IndentedScope is{writer};
        writer->AppendLine("encoder.EncodeTag({.field_number = ", number,
                           ", .wire_type = ::tsdb2::proto::WireType::kLength});");
        writer->AppendLine("encoder.EncodeSubMessage(", type_name, "::Encode(value));");
      }
      writer->AppendLine("}");
      break;
    case FieldDescriptorProto::Label::LABEL_REQUIRED:
      writer->AppendLine("encoder.EncodeTag({.field_number = ", number,
                         ", .wire_type = ::tsdb2::proto::WireType::kLength});");
      writer->AppendLine("encoder.EncodeSubMessage(", type_name, "::Encode(proto.", name, "));");
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

absl::Status Generator::EmitOneofFieldEncoding(
    internal::TextWriter* const writer, google::protobuf::DescriptorProto const& message_type,
    size_t const oneof_index) const {
  DEFINE_CONST_OR_RETURN(oneof_decl, GetOneofDecl(message_type, oneof_index));
  REQUIRE_FIELD_OR_RETURN(name, *oneof_decl, name);
  writer->AppendLine("switch (proto.", name, ".index()) {");
  {
    TextWriter::IndentedScope is{writer};
    size_t field_index = 1;
    for (auto const& field : message_type.field) {
      if (field.oneof_index.has_value() && field.oneof_index.value() == oneof_index) {
        REQUIRE_FIELD_OR_RETURN(number, field, number);
        writer->AppendLine("case ", field_index, ":");
        TextWriter::IndentedScope is{writer};
        if (field.type_name.has_value()) {
          std::string const type_name = absl::StrReplaceAll(field.type_name.value(), {{".", "::"}});
          DEFINE_CONST_OR_RETURN(is_message, IsMessage(field.type_name.value()));
          if (is_message) {
            writer->AppendLine("encoder.EncodeTag({.field_number = ", number,
                               ", .wire_type = ::tsdb2::proto::WireType::kLength});");
            writer->AppendLine("encoder.EncodeSubMessage(", type_name, "::Encode(std::get<",
                               field_index, ">(proto.", name, ")));");
          } else {
            writer->AppendLine("encoder.EncodeEnumField(", number, ", std::get<", field_index,
                               ">(proto.", name, "));");
          }
        } else {
          REQUIRE_FIELD_OR_RETURN(type, field, type);
          auto const it = kFieldEncoderNames.find(type);
          if (it == kFieldEncoderNames.end()) {
            return absl::InvalidArgumentError("invalid field type");
          }
          writer->AppendLine("encoder.", it->second, "(", number, ", std::get<", field_index,
                             ">(proto.", name, "));");
        }
        writer->AppendLine("break;");
        ++field_index;
      }
    }
  }
  writer->AppendLine("}");
  return absl::OkStatus();
}

absl::Status Generator::EmitImplementationForScope(TextWriter* const writer, PathView const prefix,
                                                   LexicalScope const& scope) const {
  for (auto const& message_type : scope.message_types) {
    REQUIRE_FIELD_OR_RETURN(name, message_type, name);
    Path const path = JoinPath(prefix, name);
    RETURN_IF_ERROR(EmitImplementationForScope(writer, /*prefix=*/path,
                                               LexicalScope{
                                                   .base_path = JoinPath(scope.base_path, name),
                                                   .global = false,
                                                   .message_types = message_type.nested_type,
                                                   .enum_types = message_type.enum_type,
                                               }));
    auto const qualified_name = absl::StrJoin(path, "::");
    DEFINE_CONST_OR_RETURN(has_required_fields, HasRequiredFields(message_type));
    writer->AppendEmptyLine();
    writer->AppendLine("::absl::StatusOr<", qualified_name, "> ", qualified_name,
                       "::Decode(::absl::Span<uint8_t const> const data) {");
    {
      TextWriter::IndentedScope is{writer};
      writer->AppendLine(qualified_name, " proto;");
      if (has_required_fields) {
        writer->AppendLine("::tsdb2::common::flat_set<size_t> decoded;");
      }
      writer->AppendLine("::tsdb2::proto::Decoder decoder{data};");
      writer->AppendLine("while (!decoder.at_end()) {");
      {
        TextWriter::IndentedScope is{writer};
        writer->AppendLine("DEFINE_CONST_OR_RETURN(tag, decoder.DecodeTag());");
        writer->AppendLine("switch (tag.field_number) {");
        {
          TextWriter::IndentedScope is{writer};
          absl::flat_hash_set<size_t> oneof_indices;
          oneof_indices.reserve(message_type.oneof_decl.size());
          for (auto const& field : message_type.field) {
            if (field.oneof_index.has_value()) {
              size_t const index = field.oneof_index.value();
              auto const [unused_it, inserted] = oneof_indices.emplace(index);
              if (inserted) {
                RETURN_IF_ERROR(EmitOneofFieldDecoding(writer, message_type, index));
              }
            } else {
              RETURN_IF_ERROR(EmitFieldDecoding(writer, field));
            }
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
      if (has_required_fields) {
        for (auto const& field : message_type.field) {
          if (!field.oneof_index.has_value()) {
            REQUIRE_FIELD_OR_RETURN(label, field, label);
            if (label == FieldDescriptorProto::Label::LABEL_REQUIRED) {
              REQUIRE_FIELD_OR_RETURN(number, field, number);
              writer->AppendLine("if (!decoded.contains(", number, ")) {");
              {
                TextWriter::IndentedScope is{writer};
                REQUIRE_FIELD_OR_RETURN(name, field, name);
                writer->AppendLine(
                    "return absl::InvalidArgumentError(\"missing required field \\\"", name,
                    "\\\"\");");
              }
              writer->AppendLine("}");
            }
          }
        }
      }
      writer->AppendLine("return std::move(proto);");
    }
    writer->AppendLine("}");
    writer->AppendEmptyLine();
    writer->AppendLine("::tsdb2::io::Cord ", qualified_name, "::Encode(", qualified_name,
                       " const& proto) {");
    {
      TextWriter::IndentedScope is{writer};
      writer->AppendLine("::tsdb2::proto::Encoder encoder;");
      absl::flat_hash_set<size_t> oneof_indices;
      oneof_indices.reserve(message_type.oneof_decl.size());
      for (auto const& field : message_type.field) {
        if (field.oneof_index.has_value()) {
          size_t const index = field.oneof_index.value();
          auto const [unused_it, inserted] = oneof_indices.emplace(index);
          if (inserted) {
            RETURN_IF_ERROR(EmitOneofFieldEncoding(writer, message_type, index));
          }
        } else {
          RETURN_IF_ERROR(EmitFieldEncoding(writer, field));
        }
      }
      writer->AppendLine("return std::move(encoder).Finish();");
    }
    writer->AppendLine("}");
  }
  return absl::OkStatus();
}

absl::Status Generator::EmitEnumReflectionDescriptor(
    TextWriter* const writer, PathView const path,
    google::protobuf::EnumDescriptorProto const& enum_type) {
  writer->AppendEmptyLine();
  auto const qualified_name = absl::StrJoin(path, "::");
  if (enum_type.value.empty()) {
    writer->AppendLine("::tsdb2::proto::EnumDescriptor<", qualified_name, ", 0> const ",
                       qualified_name, "_ENUM_DESCRIPTOR{};");
  } else {
    writer->AppendLine("::tsdb2::proto::EnumDescriptor<", qualified_name, ", ",
                       enum_type.value.size(), "> const ", qualified_name, "_ENUM_DESCRIPTOR{{");
    {
      TextWriter::IndentedScope is1{writer};
      TextWriter::IndentedScope is2{writer};
      for (auto const& value : enum_type.value) {
        REQUIRE_FIELD_OR_RETURN(name, value, name);
        REQUIRE_FIELD_OR_RETURN(number, value, number);
        writer->AppendLine("{\"", name, "\", ", number, "},");
      }
    }
    writer->AppendLine("}};");
  }
  return absl::OkStatus();
}

absl::Status Generator::EmitEnumFieldDescriptor(
    TextWriter* const writer, std::string_view const qualified_parent_name,
    google::protobuf::FieldDescriptorProto const& descriptor, bool const is_optional) {
  REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
  REQUIRE_FIELD_OR_RETURN(label, descriptor, label);
  REQUIRE_FIELD_OR_RETURN(proto_type_name, descriptor, type_name);
  DEFINE_CONST_OR_RETURN(path, GetTypePath(proto_type_name));
  auto const type_name = absl::StrCat("::", absl::StrJoin(path, "::"));
  switch (label) {
    case FieldDescriptorProto::Label::LABEL_OPTIONAL:
      if (is_optional) {
        writer->AppendLine("{\"", name, "\", ::tsdb2::proto::OptionalEnumField<",
                           qualified_parent_name, ">(&", qualified_parent_name, "::", name, ", ",
                           type_name, "_ENUM_DESCRIPTOR)},");
      } else {
        writer->AppendLine("{\"", name, "\", ::tsdb2::proto::RawEnumField<", qualified_parent_name,
                           ">(&", qualified_parent_name, "::", name, ", ", type_name,
                           "_ENUM_DESCRIPTOR)},");
      }
      break;
    case FieldDescriptorProto::Label::LABEL_REPEATED:
      writer->AppendLine("{\"", name, "\", ::tsdb2::proto::RepeatedEnumField<",
                         qualified_parent_name, ">(&", qualified_parent_name, "::", name, ", ",
                         type_name, "_ENUM_DESCRIPTOR)},");
      break;
    case FieldDescriptorProto::Label::LABEL_REQUIRED:
      writer->AppendLine("{\"", name, "\", ::tsdb2::proto::RawEnumField<", qualified_parent_name,
                         ">(&", qualified_parent_name, "::", name, ", ", type_name,
                         "_ENUM_DESCRIPTOR)},");
      break;
    default:
      return absl::InvalidArgumentError("invalid field label");
  }
  return absl::OkStatus();
}

absl::Status Generator::EmitObjectFieldDescriptor(
    TextWriter* const writer, std::string_view const qualified_parent_name,
    google::protobuf::FieldDescriptorProto const& descriptor) {
  REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
  REQUIRE_FIELD_OR_RETURN(label, descriptor, label);
  REQUIRE_FIELD_OR_RETURN(proto_type_name, descriptor, type_name);
  DEFINE_CONST_OR_RETURN(path, GetTypePath(proto_type_name))
  auto const type_name = absl::StrCat("::", absl::StrJoin(path, "::"));
  switch (label) {
    case FieldDescriptorProto::Label::LABEL_OPTIONAL:
      writer->AppendLine("{\"", name, "\", ::tsdb2::proto::OptionalSubMessageField<",
                         qualified_parent_name, ">(&", qualified_parent_name, "::", name, ", ",
                         type_name, "::MESSAGE_DESCRIPTOR)},");
      break;
    case FieldDescriptorProto::Label::LABEL_REPEATED:
      writer->AppendLine("{\"", name, "\", ::tsdb2::proto::RepeatedSubMessageField<",
                         qualified_parent_name, ">(&", qualified_parent_name, "::", name, ", ",
                         type_name, "::MESSAGE_DESCRIPTOR)},");
      break;
    case FieldDescriptorProto::Label::LABEL_REQUIRED:
      writer->AppendLine("{\"", name, "\", ::tsdb2::proto::RawSubMessageField<",
                         qualified_parent_name, ">(&", qualified_parent_name, "::", name, ", ",
                         type_name, "::MESSAGE_DESCRIPTOR)},");
      break;
    default:
      return absl::InvalidArgumentError("invalid field label");
  }
  return absl::OkStatus();
}

absl::Status Generator::EmitFieldDescriptor(
    TextWriter* writer, std::string_view const qualified_parent_name,
    google::protobuf::FieldDescriptorProto const& descriptor) const {
  DEFINE_CONST_OR_RETURN(is_optional, FieldIsWrappedInOptional(descriptor));
  if (descriptor.type_name.has_value()) {
    DEFINE_CONST_OR_RETURN(is_message, IsMessage(descriptor.type_name.value()));
    if (is_message) {
      return EmitObjectFieldDescriptor(writer, qualified_parent_name, descriptor);
    } else {
      return EmitEnumFieldDescriptor(writer, qualified_parent_name, descriptor, is_optional);
    }
  } else {
    REQUIRE_FIELD_OR_RETURN(name, descriptor, name);
    writer->AppendLine("{\"", name, "\", &", qualified_parent_name, "::", name, "},");
    return absl::OkStatus();
  }
}

absl::Status Generator::EmitOneofFieldDescriptor(
    internal::TextWriter* const writer, google::protobuf::DescriptorProto const& message_type,
    std::string_view const qualified_parent_name, size_t const index) const {
  DEFINE_CONST_OR_RETURN(oneof_decl, GetOneofDecl(message_type, index));
  std::vector<std::string> descriptors;
  for (auto const& field : message_type.field) {
    if (field.oneof_index.has_value() && field.oneof_index.value() == index) {
      if (field.type_name.has_value()) {
        std::string_view const proto_type_name = field.type_name.value();
        DEFINE_CONST_OR_RETURN(is_message, IsMessage(proto_type_name));
        DEFINE_CONST_OR_RETURN(path, GetTypePath(proto_type_name));
        auto const type_name = absl::StrCat("::", absl::StrJoin(path, "::"));
        if (is_message) {
          descriptors.emplace_back(absl::StrCat("std::cref(", type_name, "::MESSAGE_DESCRIPTOR)"));
        } else {
          descriptors.emplace_back(absl::StrCat("std::cref(", type_name, "_ENUM_DESCRIPTOR)"));
        }
      } else {
        descriptors.emplace_back("std::monostate()");
      }
    }
  }
  REQUIRE_FIELD_OR_RETURN(name, *oneof_decl, name);
  writer->AppendLine("{\"", name, "\", ::tsdb2::proto::OneOfField<", qualified_parent_name, ">(&",
                     qualified_parent_name, "::", name, ", std::make_tuple(",
                     absl::StrJoin(descriptors, ", "), "))},");
  return absl::OkStatus();
}

absl::Status Generator::EmitMessageReflectionDescriptor(
    TextWriter* const writer, PathView const path,
    google::protobuf::DescriptorProto const& message_type) const {
  writer->AppendEmptyLine();
  auto const qualified_name = absl::StrJoin(path, "::");
  if (message_type.field.empty()) {
    writer->AppendLine("::tsdb2::proto::MessageDescriptor<", qualified_name, ", 0> const ",
                       qualified_name, "::MESSAGE_DESCRIPTOR{};");
  } else {
    size_t const num_fields = GetNumGeneratedFields(message_type);
    writer->AppendLine("::tsdb2::proto::MessageDescriptor<", qualified_name, ", ", num_fields,
                       "> const ", qualified_name, "::MESSAGE_DESCRIPTOR{{");
    {
      absl::flat_hash_set<size_t> oneof_indices;
      oneof_indices.reserve(message_type.oneof_decl.size());
      TextWriter::IndentedScope is1{writer};
      TextWriter::IndentedScope is2{writer};
      for (auto const& field : message_type.field) {
        if (field.oneof_index.has_value()) {
          size_t const index = field.oneof_index.value();
          auto const [unused_it, inserted] = oneof_indices.emplace(index);
          if (inserted) {
            RETURN_IF_ERROR(EmitOneofFieldDescriptor(writer, message_type, qualified_name, index));
          }
        } else {
          RETURN_IF_ERROR(EmitFieldDescriptor(writer, qualified_name, field));
        }
      }
    }
    writer->AppendLine("}};");
  }
  return absl::OkStatus();
}

absl::Status Generator::EmitReflectionDescriptors(TextWriter* writer) const {
  auto const ordered_names = flat_dependencies_.MakeOrder(base_path_);
  for (auto const& proto_type_name : ordered_names) {
    Path const path = SplitPath(proto_type_name);
    auto const it = enum_types_by_path_.find(JoinPath(base_path_, path));
    if (it != enum_types_by_path_.end()) {
      RETURN_IF_ERROR(EmitEnumReflectionDescriptor(writer, path, it->second));
    }
  }
  for (auto const& proto_type_name : ordered_names) {
    Path const path = SplitPath(proto_type_name);
    auto const it = message_types_by_path_.find(JoinPath(base_path_, path));
    if (it != message_types_by_path_.end()) {
      RETURN_IF_ERROR(EmitMessageReflectionDescriptor(writer, path, it->second));
    }
  }
  return absl::OkStatus();
}

}  // namespace proto
}  // namespace tsdb2
