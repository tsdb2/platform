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
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/flat_map.h"
#include "common/flat_set.h"
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

    explicit Builder(google::protobuf::FileDescriptorProto const& file_descriptor, Path base_path,
                     bool const use_raw_google_api_types)
        : file_descriptor_(&file_descriptor),
          base_path_(std::move(base_path)),
          use_raw_google_api_types_(use_raw_google_api_types) {}

    Builder(Builder const&) = delete;
    Builder& operator=(Builder const&) = delete;

    absl::Status CheckGoogleApiType(PathView path) const;

    absl::Status AddLexicalScope(LexicalScope const& scope);

    std::optional<std::string> MaybeGetQualifiedName(PathView path);

    absl::Status BuildFlatDependencies(
        std::string_view scope_name,
        absl::Span<google::protobuf::DescriptorProto const> message_types,
        absl::Span<google::protobuf::EnumDescriptorProto const> enum_types);

    absl::StatusOr<Cycles> FindCycles(LexicalScope const& scope) const;

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

    google::protobuf::FileDescriptorProto const* file_descriptor_;
    Path base_path_;
    bool use_raw_google_api_types_;

    absl::flat_hash_map<Path, google::protobuf::EnumDescriptorProto> enum_types_by_path_;
    absl::flat_hash_map<Path, google::protobuf::DescriptorProto> message_types_by_path_;

    internal::DependencyManager dependencies_;
    internal::DependencyManager flat_dependencies_;
  };

  explicit Generator(
      google::protobuf::FileDescriptorProto const* const file_descriptor,
      bool const emit_reflection_api, bool const use_raw_google_api_types,
      bool const generate_definitions_for_google_api_types,
      absl::flat_hash_map<Path, google::protobuf::EnumDescriptorProto> enum_types_by_path,
      absl::flat_hash_map<Path, google::protobuf::DescriptorProto> message_types_by_path,
      internal::DependencyManager dependencies, internal::DependencyManager flat_dependencies,
      Path base_path)
      : file_descriptor_(file_descriptor),
        emit_reflection_api_(emit_reflection_api),
        use_raw_google_api_types_(use_raw_google_api_types),
        generate_definitions_for_google_api_types_(generate_definitions_for_google_api_types),
        enum_types_by_path_(std::move(enum_types_by_path)),
        message_types_by_path_(std::move(message_types_by_path)),
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

  static absl::StatusOr<google::protobuf::OneofDescriptorProto const*> GetOneofDecl(
      google::protobuf::DescriptorProto const& message_type, size_t index);

  // Returns the number of generated fields, which may be different from the number of proto fields
  // due to the possible presence of oneof groupings, which generate a single `std::variant`.
  static size_t GetNumGeneratedFields(google::protobuf::DescriptorProto const& message_type);

  void EmitIncludes(google::protobuf::FileDescriptorProto const& file_descriptor,
                    internal::TextWriter* writer,
                    tsdb2::common::flat_map<std::string, bool> headers) const;

  template <typename DefaultHeaderSet>
  void EmitIncludes(google::protobuf::FileDescriptorProto const& file_descriptor,
                    internal::TextWriter* const writer,
                    DefaultHeaderSet const& default_headers) const {
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
    EmitIncludes(file_descriptor, writer, std::move(headers));
  }

  void EmitHeaderIncludes(internal::TextWriter* writer) const;
  void EmitSourceIncludes(internal::TextWriter* writer) const;

  absl::StatusOr<std::pair<std::string, bool>> GetFieldType(
      google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::StatusOr<std::string> GetFieldInitializer(
      google::protobuf::FieldDescriptorProto const& descriptor, std::string_view type_name,
      std::string_view default_value) const;

  // REQUIRES: `proto_type_name` must identify an enum.
  absl::StatusOr<std::string> GetInitialEnumValue(std::string_view proto_type_name) const;

  absl::StatusOr<bool> FieldIsWrappedInOptional(
      google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitOneofField(internal::TextWriter* writer,
                              google::protobuf::DescriptorProto const& message_type,
                              size_t index) const;

  absl::Status EmitMessageFields(internal::TextWriter* writer,
                                 google::protobuf::DescriptorProto const& message_type) const;

  absl::Status EmitHeaderForScope(internal::TextWriter* writer, LexicalScope const& scope) const;

  absl::Status EmitDescriptorSpecializationsForScope(internal::TextWriter* writer,
                                                     LexicalScope const& scope) const;

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

  absl::Status EmitGoogleApiFieldDecoding(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitFieldDecoding(internal::TextWriter* writer,
                                 google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitOneofFieldDecoding(internal::TextWriter* writer,
                                      google::protobuf::DescriptorProto const& message_type,
                                      size_t index) const;

  static absl::Status EmitOptionalFieldEncoding(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor);

  static absl::Status EmitRepeatedFieldEncoding(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor);

  static absl::Status EmitRequiredFieldEncoding(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor);

  static absl::Status EmitEnumFieldEncoding(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor,
      bool is_optional);

  static absl::Status EmitGoogleApiFieldEncoding(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor,
      bool is_optional);

  static absl::Status EmitObjectFieldEncoding(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor);

  absl::Status EmitFieldEncoding(internal::TextWriter* writer,
                                 google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitOneofFieldEncoding(internal::TextWriter* writer,
                                      google::protobuf::DescriptorProto const& message_type,
                                      size_t index) const;

  absl::Status EmitImplementationForScope(internal::TextWriter* writer, PathView prefix,
                                          LexicalScope const& scope) const;

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

  absl::Status EmitOneofFieldDescriptor(internal::TextWriter* writer,
                                        google::protobuf::DescriptorProto const& message_type,
                                        std::string_view qualified_parent_name, size_t index) const;

  absl::Status EmitMessageReflectionDescriptor(
      internal::TextWriter* writer, PathView path,
      google::protobuf::DescriptorProto const& message_type) const;

  absl::Status EmitReflectionDescriptors(internal::TextWriter* writer) const;

  google::protobuf::FileDescriptorProto const* file_descriptor_;
  bool emit_reflection_api_;
  bool use_raw_google_api_types_;
  bool generate_definitions_for_google_api_types_;
  absl::flat_hash_map<Path, google::protobuf::EnumDescriptorProto> enum_types_by_path_;
  absl::flat_hash_map<Path, google::protobuf::DescriptorProto> message_types_by_path_;
  internal::DependencyManager dependencies_;
  internal::DependencyManager flat_dependencies_;
  Path base_path_;
};

}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_GENERATOR_H__
