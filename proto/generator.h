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
#include "proto/dependencies.h"
#include "proto/descriptor.h"
#include "proto/file_writer.h"

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
  static internal::DependencyManager::Path GetPackagePath(
      google::protobuf::FileDescriptorProto const& file_descriptor);

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

  static absl::StatusOr<std::string> GetFieldType(
      google::protobuf::FieldDescriptorProto const& descriptor);

  static absl::Status AppendForwardDeclaration(internal::FileWriter* writer,
                                               google::protobuf::DescriptorProto const& descriptor);

  static absl::Status AppendMessageHeader(internal::FileWriter* writer,
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
                                       std::string_view proto_type_name, bool packed);

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

  google::protobuf::FileDescriptorProto const& file_descriptor_;
  absl::flat_hash_set<internal::DependencyManager::Path> enums_;
  internal::DependencyManager const dependencies_;
};

}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_GENERATOR_H__
