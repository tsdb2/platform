#ifndef __TSDB2_PROTO_GENERATOR_H__
#define __TSDB2_PROTO_GENERATOR_H__

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/utilities.h"
#include "proto/dependencies.h"
#include "proto/descriptor.h"
#include "proto/file_writer.h"
#include "proto/object.h"

namespace tsdb2 {
namespace proto {

namespace generator {

absl::StatusOr<std::vector<uint8_t>> ReadFile(FILE* fp);
absl::Status WriteFile(FILE* fp, absl::Span<uint8_t const> data);

std::string MakeHeaderFileName(std::string_view proto_file_name);
std::string MakeSourceFileName(std::string_view proto_file_name);

}  // namespace generator

class Generator {
 public:
  static absl::StatusOr<Generator> Create(
      google::protobuf::FileDescriptorProto const& file_descriptor);

  absl::StatusOr<std::string> GenerateHeaderFileContent();
  absl::StatusOr<std::string> GenerateSourceFileContent();

  absl::StatusOr<google::protobuf::compiler::CodeGeneratorResponse_File> GenerateHeaderFile();
  absl::StatusOr<google::protobuf::compiler::CodeGeneratorResponse_File> GenerateSourceFile();

 private:
  static internal::DependencyManager::Path JoinPath(internal::DependencyManager::PathView lhs,
                                                    std::string_view rhs);

  static internal::DependencyManager::Path GetPackagePath(
      google::protobuf::FileDescriptorProto const& file_descriptor);

  template <typename DescriptorProto>
  static absl::Status RecurseAddScopeDependencies(
      internal::DependencyManager::PathView base_path,
      absl::flat_hash_set<internal::DependencyManager::Path>* enums,
      internal::DependencyManager* dependencies,
      google::protobuf::FileDescriptorProto const& file_descriptor,
      absl::Span<DescriptorProto const> message_types,
      absl::Span<google::protobuf::EnumDescriptorProto const> enum_types);

  template <typename DescriptorProto>
  static absl::Status AddScopeDependencies(
      internal::DependencyManager::PathView base_path,
      absl::flat_hash_set<internal::DependencyManager::Path>* enums,
      internal::DependencyManager* dependencies,
      google::protobuf::FileDescriptorProto const& file_descriptor,
      absl::Span<DescriptorProto const> message_types,
      absl::Span<google::protobuf::EnumDescriptorProto const> enum_types);

  template <>
  absl::Status RecurseAddScopeDependencies<google::protobuf::DescriptorProto>(
      internal::DependencyManager::PathView const base_path,
      absl::flat_hash_set<internal::DependencyManager::Path>* const enums,
      internal::DependencyManager* const dependencies,
      google::protobuf::FileDescriptorProto const& file_descriptor,
      absl::Span<google::protobuf::DescriptorProto const> const message_types,
      absl::Span<google::protobuf::EnumDescriptorProto const> const enum_types) {
    return AddScopeDependencies(base_path, enums, dependencies, file_descriptor, message_types,
                                enum_types);
  }

  template <>
  absl::Status RecurseAddScopeDependencies<google::protobuf::NestedDescriptorProto>(
      internal::DependencyManager::PathView const base_path,
      absl::flat_hash_set<internal::DependencyManager::Path>* const enums,
      internal::DependencyManager* const dependencies,
      google::protobuf::FileDescriptorProto const& file_descriptor,
      absl::Span<google::protobuf::NestedDescriptorProto const> const message_types,
      absl::Span<google::protobuf::EnumDescriptorProto const> const enum_types) {
    return AddScopeDependencies(base_path, enums, dependencies, file_descriptor, message_types,
                                enum_types);
  }

  template <>
  absl::Status RecurseAddScopeDependencies<google::protobuf::NestedNestedDescriptorProto>(
      internal::DependencyManager::PathView const base_path,
      absl::flat_hash_set<internal::DependencyManager::Path>* const enums,
      internal::DependencyManager* const dependencies,
      google::protobuf::FileDescriptorProto const& file_descriptor,
      absl::Span<google::protobuf::NestedNestedDescriptorProto const> const message_types,
      absl::Span<google::protobuf::EnumDescriptorProto const> const enum_types) {
    return absl::OkStatus();
  }

  static absl::StatusOr<internal::DependencyManager::Path> GetTypePath(
      google::protobuf::FileDescriptorProto const& descriptor, std::string_view proto_type_name);

  explicit Generator(google::protobuf::FileDescriptorProto const& file_descriptor,
                     absl::flat_hash_set<internal::DependencyManager::Path> enums,
                     internal::DependencyManager dependencies)
      : file_descriptor_(file_descriptor),
        enums_(std::move(enums)),
        dependencies_(std::move(dependencies)) {}

  absl::StatusOr<std::string> GetHeaderPath();
  absl::StatusOr<std::string> GetHeaderGuardName();
  absl::StatusOr<std::string> GetCppPackage();

  static absl::Status AppendEnum(internal::FileWriter* writer,
                                 google::protobuf::EnumDescriptorProto const& enum_descriptor);

  absl::StatusOr<std::pair<std::string, bool>> GetFieldType(
      google::protobuf::FieldDescriptorProto const& descriptor) const;

  static absl::Status AppendForwardDeclaration(internal::FileWriter* writer,
                                               google::protobuf::DescriptorProto const& descriptor);

  absl::StatusOr<std::string> GetFieldInitializer(
      google::protobuf::FieldDescriptorProto const& descriptor, std::string_view type_name,
      std::string_view default_value) const;

  absl::StatusOr<bool> FieldIsWrappedInOptional(
      google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status AppendMessageHeader(internal::FileWriter* writer,
                                   google::protobuf::DescriptorProto const& descriptor);

  absl::Status AppendMessageHeaders(
      internal::FileWriter* writer,
      absl::Span<google::protobuf::DescriptorProto const> descriptors);

  absl::Status EmitFieldDecoding(internal::FileWriter* writer,
                                 google::protobuf::FieldDescriptorProto const& descriptor) const;

  static absl::Status EmitOptionalFieldEncoding(internal::FileWriter* writer, std::string_view name,
                                                size_t number,
                                                google::protobuf::FieldDescriptorProto_Type type);

  static absl::Status EmitRepeatedFieldEncoding(internal::FileWriter* writer, std::string_view name,
                                                size_t number,
                                                google::protobuf::FieldDescriptorProto_Type type,
                                                bool packed);

  static absl::Status EmitRequiredFieldEncoding(internal::FileWriter* writer, std::string_view name,
                                                size_t number,
                                                google::protobuf::FieldDescriptorProto_Type type);

  static absl::Status EmitEnumEncoding(internal::FileWriter* writer, std::string_view name,
                                       size_t number,
                                       google::protobuf::FieldDescriptorProto_Label label,
                                       std::string_view proto_type_name, bool packed,
                                       bool is_optional);

  static absl::Status EmitObjectEncoding(internal::FileWriter* writer, std::string_view name,
                                         size_t number,
                                         google::protobuf::FieldDescriptorProto_Label label,
                                         std::string_view proto_type_name);

  absl::Status EmitFieldEncoding(internal::FileWriter* writer,
                                 google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitMessageImplementation(internal::FileWriter* writer,
                                         google::protobuf::DescriptorProto const& descriptor) const;

  absl::StatusOr<internal::DependencyManager::Path> GetTypePath(
      std::string_view proto_type_name) const {
    return GetTypePath(file_descriptor_, proto_type_name);
  }

  absl::StatusOr<bool> IsMessage(std::string_view proto_type_name) const;
  absl::StatusOr<bool> IsEnum(std::string_view proto_type_name) const;

  google::protobuf::FileDescriptorProto const& file_descriptor_;
  absl::flat_hash_set<internal::DependencyManager::Path> enums_;
  internal::DependencyManager const dependencies_;
};

template <typename DescriptorProto>
absl::Status Generator::AddScopeDependencies(
    internal::DependencyManager::PathView const base_path,
    absl::flat_hash_set<internal::DependencyManager::Path>* const enums,
    internal::DependencyManager* const dependencies,
    google::protobuf::FileDescriptorProto const& file_descriptor,
    absl::Span<DescriptorProto const> const message_types,
    absl::Span<google::protobuf::EnumDescriptorProto const> const enum_types) {
  for (auto const& enum_type : enum_types) {
    DEFINE_CONST_OR_RETURN(
        name, RequireField<google::protobuf::kEnumDescriptorProtoNameField>(enum_type));
    internal::DependencyManager::Path const path = JoinPath(base_path, *name);
    enums->emplace(path);
    dependencies->AddNode(path);
  }
  for (auto const& message_type : message_types) {
    DEFINE_CONST_OR_RETURN(
        message_name, RequireField<google::protobuf::kEnumDescriptorProtoNameField>(message_type));
    internal::DependencyManager::Path const path = JoinPath(base_path, *message_name);
    dependencies->AddNode(path);
    RETURN_IF_ERROR(RecurseAddScopeDependencies(
        path, enums, dependencies, file_descriptor,
        absl::MakeConstSpan(
            message_type.template get<google::protobuf::kDescriptorProtoNestedTypeField>()),
        message_type.template get<google::protobuf::kDescriptorProtoEnumTypeField>()));
  }
  for (auto const& message_type : message_types) {
    DEFINE_CONST_OR_RETURN(
        message_name, RequireField<google::protobuf::kEnumDescriptorProtoNameField>(message_type));
    internal::DependencyManager::Path const path = JoinPath(base_path, *message_name);
    for (auto const& field :
         message_type.template get<google::protobuf::kDescriptorProtoFieldField>()) {
      auto const& maybe_type_name =
          field.template get<google::protobuf::kFieldDescriptorProtoTypeNameField>();
      if (maybe_type_name.has_value()) {
        DEFINE_CONST_OR_RETURN(
            label, RequireField<google::protobuf::kFieldDescriptorProtoLabelField>(field));
        if (*label != google::protobuf::FieldDescriptorProto_Label::LABEL_REPEATED) {
          DEFINE_CONST_OR_RETURN(type_path, GetTypePath(file_descriptor, maybe_type_name.value()));
          DEFINE_CONST_OR_RETURN(
              field_name, RequireField<google::protobuf::kFieldDescriptorProtoNameField>(field));
          dependencies->AddDependency(path, type_path, *field_name);
        }
      }
    }
  }
  return absl::OkStatus();
}

}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_GENERATOR_H__
