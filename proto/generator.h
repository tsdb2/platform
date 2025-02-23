#ifndef __TSDB2_PROTO_GENERATOR_H__
#define __TSDB2_PROTO_GENERATOR_H__

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "proto/dependencies.h"
#include "proto/descriptor.pb.sync.h"
#include "proto/plugin.pb.sync.h"
#include "proto/text_writer.h"

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
  using Path = internal::DependencyManager::Path;
  using PathView = internal::DependencyManager::PathView;

  static absl::StatusOr<Generator> Create(
      google::protobuf::FileDescriptorProto const& file_descriptor);

  ~Generator() = default;

  Generator(Generator&&) noexcept = default;
  Generator& operator=(Generator&&) noexcept = default;

  absl::StatusOr<std::string> GenerateHeaderFileContent();
  absl::StatusOr<std::string> GenerateSourceFileContent();

  absl::StatusOr<google::protobuf::compiler::CodeGeneratorResponse::File> GenerateHeaderFile();
  absl::StatusOr<google::protobuf::compiler::CodeGeneratorResponse::File> GenerateSourceFile();

 private:
  struct LexicalScope {
    Path base_path;
    bool global;
    absl::Span<google::protobuf::DescriptorProto const> message_types;
    absl::Span<google::protobuf::EnumDescriptorProto const> enum_types;
  };

  class Builder {
   public:
    static absl::StatusOr<Builder> Create(
        google::protobuf::FileDescriptorProto const& file_descriptor);

    ~Builder() = default;

    Builder(Builder&&) noexcept = default;
    Builder& operator=(Builder&&) noexcept = default;

    absl::StatusOr<Generator> Build() &&;

   private:
    using Cycle = internal::DependencyManager::Cycle;
    using Cycles = std::vector<Cycle>;

    explicit Builder(google::protobuf::FileDescriptorProto const& file_descriptor, Path base_path)
        : file_descriptor_(&file_descriptor), base_path_(std::move(base_path)) {}

    Builder(Builder const&) = delete;
    Builder& operator=(Builder const&) = delete;

    absl::Status AddLexicalScope(LexicalScope const& scope);

    std::optional<std::string> MaybeGetQualifiedName(PathView path);

    absl::Status BuildFlatDependencies(
        std::string_view scope_name,
        absl::Span<google::protobuf::DescriptorProto const> message_types,
        absl::Span<google::protobuf::EnumDescriptorProto const> enum_types);

    absl::StatusOr<Cycles> FindCycles(LexicalScope const& scope) const;

    google::protobuf::FileDescriptorProto const* file_descriptor_;
    Path base_path_;

    absl::flat_hash_set<Path> enums_;
    internal::DependencyManager dependencies_;
    internal::DependencyManager flat_dependencies_;
  };

  explicit Generator(google::protobuf::FileDescriptorProto const* const file_descriptor,
                     absl::flat_hash_set<Path> enums, internal::DependencyManager dependencies,
                     internal::DependencyManager flat_dependencies, Path base_path)
      : file_descriptor_(file_descriptor),
        enums_(std::move(enums)),
        dependencies_(std::move(dependencies)),
        flat_dependencies_(std::move(flat_dependencies)),
        base_path_(std::move(base_path)) {}

  Generator(Generator const&) = delete;
  Generator& operator=(Generator const&) = delete;

  absl::StatusOr<std::string> GetHeaderGuardName() const;
  absl::StatusOr<std::string> GetCppPackage() const;

  absl::StatusOr<bool> IsMessage(std::string_view proto_type_name) const;
  absl::StatusOr<bool> IsEnum(std::string_view proto_type_name) const;

  static absl::StatusOr<bool> HasRequiredFields(
      google::protobuf::DescriptorProto const& descriptor);

  void EmitHeaderIncludes(internal::TextWriter* writer) const;
  void EmitSourceIncludes(internal::TextWriter* writer) const;

  absl::StatusOr<std::pair<std::string, bool>> GetFieldType(
      google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::StatusOr<std::string> GetFieldInitializer(
      google::protobuf::FieldDescriptorProto const& descriptor, std::string_view type_name,
      std::string_view default_value) const;

  absl::StatusOr<bool> FieldIsWrappedInOptional(
      google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitHeaderForScope(internal::TextWriter* writer, LexicalScope const& scope) const;

  static absl::Status EmitOptionalFieldDecoding(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor);

  static absl::Status EmitRepeatedFieldDecoding(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor);

  static absl::Status EmitRawFieldDecoding(internal::TextWriter* writer,
                                           google::protobuf::FieldDescriptorProto const& descriptor,
                                           bool required);

  static absl::Status EmitObjectDecoding(internal::TextWriter* writer,
                                         google::protobuf::FieldDescriptorProto const& descriptor);

  absl::Status EmitEnumDecoding(internal::TextWriter* writer,
                                google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitFieldDecoding(internal::TextWriter* writer,
                                 google::protobuf::FieldDescriptorProto const& descriptor,
                                 bool track_decoded_fields) const;

  static absl::Status EmitOptionalFieldEncoding(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor);

  static absl::Status EmitRepeatedFieldEncoding(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor);

  static absl::Status EmitRequiredFieldEncoding(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor);

  static absl::Status EmitEnumEncoding(internal::TextWriter* writer,
                                       google::protobuf::FieldDescriptorProto const& descriptor,
                                       bool is_optional);

  static absl::Status EmitObjectEncoding(internal::TextWriter* writer,
                                         google::protobuf::FieldDescriptorProto const& descriptor);

  absl::Status EmitFieldEncoding(internal::TextWriter* writer,
                                 google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitImplementationForScope(internal::TextWriter* writer, PathView prefix,
                                          LexicalScope const& scope) const;

  static absl::Status GetEnumTypesByPathImpl(
      LexicalScope const& scope,
      absl::flat_hash_map<Path, google::protobuf::EnumDescriptorProto>* descriptors);

  absl::StatusOr<absl::flat_hash_map<Path, google::protobuf::EnumDescriptorProto>>
  GetEnumTypesByPath() const;

  static absl::Status GetMessageTypesByPathImpl(
      LexicalScope const& scope,
      absl::flat_hash_map<Path, google::protobuf::DescriptorProto>* descriptor);

  absl::StatusOr<absl::flat_hash_map<Path, google::protobuf::DescriptorProto>>
  GetMessageTypesByPath() const;

  static absl::Status EmitEnumReflectionDescriptor(
      internal::TextWriter* writer, PathView path,
      google::protobuf::EnumDescriptorProto const& enum_type);

  static absl::Status EmitEnumFieldDescriptor(
      internal::TextWriter* writer, std::string_view qualified_parent_name,
      google::protobuf::FieldDescriptorProto const& descriptor, bool is_optional);

  static absl::Status EmitObjectFieldDescriptor(
      internal::TextWriter* writer, std::string_view qualified_parent_name,
      google::protobuf::FieldDescriptorProto const& descriptor);

  absl::Status EmitFieldDescriptor(internal::TextWriter* writer,
                                   std::string_view qualified_parent_name,
                                   google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitMessageReflectionDescriptor(
      internal::TextWriter* writer, PathView path,
      google::protobuf::DescriptorProto const& message_type) const;

  absl::Status EmitReflectionDescriptors(
      internal::TextWriter* writer,
      absl::flat_hash_map<Path, google::protobuf::EnumDescriptorProto> const& enum_types_by_path,
      absl::flat_hash_map<Path, google::protobuf::DescriptorProto> const& message_types_by_path)
      const;

  google::protobuf::FileDescriptorProto const* file_descriptor_;
  absl::flat_hash_set<Path> enums_;
  internal::DependencyManager dependencies_;
  internal::DependencyManager flat_dependencies_;
  Path base_path_;
};

}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_GENERATOR_H__
