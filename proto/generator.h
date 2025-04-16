#ifndef __TSDB2_PROTO_GENERATOR_H__
#define __TSDB2_PROTO_GENERATOR_H__

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "common/flat_set.h"
#include "common/utilities.h"
#include "proto/annotations.pb.sync.h"
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
  // Transparent hash & equal functors for indexing `Path`s in hash tables and looking them up by
  // `PathView`.
  struct PathHashEq {
    struct Hash {
      using is_transparent = void;
      size_t operator()(PathView const path) const { return absl::HashOf(path); }
    };
    struct Eq {
      using is_transparent = void;
      bool operator()(PathView const lhs, PathView const rhs) const { return lhs == rhs; }
    };
  };

  using EnumsByPath = absl::flat_hash_map<Path, google::protobuf::EnumDescriptorProto,
                                          PathHashEq::Hash, PathHashEq::Eq>;

  using MessagesByPath = absl::flat_hash_map<Path, google::protobuf::DescriptorProto,
                                             PathHashEq::Hash, PathHashEq::Eq>;

  struct LexicalScope {
    Path base_path;
    bool global;
    absl::Span<google::protobuf::DescriptorProto const> message_types;
    absl::Span<google::protobuf::EnumDescriptorProto const> enum_types;
    absl::Span<google::protobuf::FieldDescriptorProto const> extensions;
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

    // Checks that the (tsdb2.proto.indirect) option is applied only to optional fields of
    // sub-message type.
    absl::Status CheckFieldIndirections() const;

    // Checks that the (tsdb2.proto.map_type) option is applied only to map fields and that the keys
    // of trie maps are strings.
    absl::Status CheckMapTypes() const;

    absl::Status CheckGoogleApiType(PathView path) const;

    absl::Status AddFieldToDependencies(PathView dependee_path,
                                        google::protobuf::FieldDescriptorProto const& descriptor);

    absl::Status AddLexicalScope(LexicalScope const& scope);

    // Converts an absolute path to a partially qualified name relative to the `base_path_` (i.e.
    // the current proto package). Returns an empty optional if the absolute path is outside the
    // `base_path_`.
    std::optional<std::string> MaybeGetQualifiedName(PathView path);

    absl::Status AddFieldToFlatDependencies(
        PathView dependent_path, google::protobuf::FieldDescriptorProto const& descriptor);

    absl::Status BuildFlatDependencies(
        std::string_view scope_name,
        absl::Span<google::protobuf::DescriptorProto const> message_types,
        absl::Span<google::protobuf::EnumDescriptorProto const> enum_types);

    absl::StatusOr<Cycles> FindCycles(LexicalScope const& scope) const;

    static absl::Status GetEnumTypesByPathImpl(LexicalScope const& scope, EnumsByPath* descriptors);

    absl::StatusOr<EnumsByPath> GetEnumTypesByPath() const;

    static absl::Status GetMessageTypesByPathImpl(LexicalScope const& scope,
                                                  MessagesByPath* descriptor);

    absl::StatusOr<MessagesByPath> GetMessageTypesByPath() const;

    google::protobuf::FileDescriptorProto const* file_descriptor_;
    Path base_path_;
    bool use_raw_google_api_types_;

    EnumsByPath enum_types_by_path_;
    MessagesByPath message_types_by_path_;

    internal::DependencyManager dependencies_;
    internal::DependencyManager flat_dependencies_;
  };

  explicit Generator(google::protobuf::FileDescriptorProto const* const file_descriptor,
                     bool const emit_reflection_api, bool const use_raw_google_api_types,
                     bool const generate_definitions_for_google_api_types,
                     EnumsByPath enum_types_by_path, MessagesByPath message_types_by_path,
                     internal::DependencyManager dependencies,
                     internal::DependencyManager flat_dependencies, Path base_path)
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

  static absl::StatusOr<tsdb2::common::flat_set<std::string_view>> GetRequiredFieldNames(
      google::protobuf::DescriptorProto const& descriptor);

  static absl::StatusOr<size_t> GetNumRequiredFields(
      google::protobuf::DescriptorProto const& descriptor) {
    DEFINE_CONST_OR_RETURN(names, GetRequiredFieldNames(descriptor));
    return names.size();
  }

  static absl::StatusOr<bool> HasRequiredFields(
      google::protobuf::DescriptorProto const& descriptor) {
    DEFINE_CONST_OR_RETURN(num_fields, GetNumRequiredFields(descriptor));
    return num_fields != 0;
  }

  static absl::StatusOr<google::protobuf::OneofDescriptorProto const*> GetOneofDecl(
      google::protobuf::DescriptorProto const& message_type, size_t index);

  // Returns the number of generated fields, which may be different from the number of proto fields
  // due to the possible presence of oneof groupings, which generate a single `std::variant`.
  static size_t GetNumGeneratedFields(google::protobuf::DescriptorProto const& message_type);

  // Generates the name of an extension message. The provided descriptor must be of an extension
  // field.
  static absl::StatusOr<std::string> MakeExtensionName(
      google::protobuf::FieldDescriptorProto const& descriptor);

  static absl::StatusOr<std::vector<google::protobuf::DescriptorProto>> GetExtensionMessages(
      LexicalScope const& scope);

  static absl::StatusOr<absl::btree_map<Path, google::protobuf::DescriptorProto>>
  GetAllExtensionMessages(LexicalScope const& scope);

  void EmitIncludes(internal::TextWriter* writer) const;

  absl::StatusOr<std::pair<std::string, bool>> GetFieldType(
      google::protobuf::FieldDescriptorProto const& descriptor) const;

  static absl::StatusOr<FieldIndirectionType> GetFieldIndirection(
      google::protobuf::FieldDescriptorProto const& descriptor);

  absl::StatusOr<std::string> GetFieldInitializer(
      google::protobuf::FieldDescriptorProto const& descriptor, std::string_view type_name,
      std::string_view default_value) const;

  // REQUIRES: `proto_type_name` must identify an enum.
  absl::StatusOr<std::string> GetInitialEnumValue(std::string_view proto_type_name) const;

  absl::StatusOr<bool> FieldIsWrappedInOptional(
      google::protobuf::FieldDescriptorProto const& descriptor) const;

  static absl::StatusOr<std::optional<MapType>> GetMapType(
      google::protobuf::FieldDescriptorProto const& descriptor);

  absl::StatusOr<std::string> MakeMapSignature(
      google::protobuf::FieldDescriptorProto const& descriptor,
      google::protobuf::DescriptorProto const& entry_message_type) const;

  static absl::StatusOr<std::string_view> GetMapDescriptorName(
      google::protobuf::FieldDescriptorProto const& descriptor);

  // Returns nullptr if `path` doesn't refer to a map entry message.
  google::protobuf::DescriptorProto const* GetMapEntry(PathView path) const;

  bool IsMapEntry(PathView const path) const { return GetMapEntry(path) != nullptr; }

  // Indicates whether the specified message type has unordered map types, in which case the ordered
  // comparison operators cannot be emitted.
  absl::StatusOr<bool> HasUnorderedMaps(
      google::protobuf::DescriptorProto const& message_type) const;

  static absl::StatusOr<std::pair<google::protobuf::FieldDescriptorProto const*,
                                  google::protobuf::FieldDescriptorProto const*>>
  GetMapEntryFields(google::protobuf::DescriptorProto const& entry_message_type);

  static absl::Status EmitForwardDeclarations(internal::TextWriter* writer,
                                              LexicalScope const& scope);

  absl::Status EmitFieldDeclaration(internal::TextWriter* writer,
                                    google::protobuf::FieldDescriptorProto const& field) const;

  absl::Status EmitOneofFieldDeclaration(internal::TextWriter* writer,
                                         google::protobuf::DescriptorProto const& message_type,
                                         size_t index) const;

  absl::Status EmitMessageFields(internal::TextWriter* writer,
                                 google::protobuf::DescriptorProto const& message_type) const;

  absl::Status EmitMessageHeader(internal::TextWriter* writer, LexicalScope const& scope,
                                 google::protobuf::DescriptorProto const& message_type) const;

  absl::Status EmitHeaderForScope(internal::TextWriter* writer, LexicalScope const& scope) const;

  absl::Status EmitDescriptorSpecializationForMessage(
      internal::TextWriter* writer, LexicalScope const& scope,
      google::protobuf::DescriptorProto const& message_type) const;

  absl::Status EmitDescriptorSpecializationsForScope(internal::TextWriter* writer,
                                                     LexicalScope const& scope) const;

  static absl::Status EmitEnumImplementation(
      internal::TextWriter* writer, PathView prefix, LexicalScope const& scope,
      google::protobuf::EnumDescriptorProto const& enum_type);

  static absl::Status EmitOptionalFieldDecoding(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor);

  static absl::Status EmitRepeatedFieldDecoding(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor);

  static absl::Status EmitRawFieldDecoding(internal::TextWriter* writer,
                                           google::protobuf::FieldDescriptorProto const& descriptor,
                                           bool required);

  absl::Status EmitObjectDecoding(internal::TextWriter* writer,
                                  google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitEnumDecoding(internal::TextWriter* writer,
                                google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitGoogleApiFieldDecoding(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitFieldDecoding(internal::TextWriter* writer,
                                 google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitOneofFieldDecoding(internal::TextWriter* writer,
                                      google::protobuf::DescriptorProto const& message_type,
                                      size_t oneof_index) const;

  static absl::Status EmitOptionalFieldParsing(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor);

  static absl::Status EmitRepeatedFieldParsing(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor);

  static absl::Status EmitRawFieldParsing(internal::TextWriter* writer,
                                          google::protobuf::FieldDescriptorProto const& descriptor,
                                          bool required);

  absl::Status EmitObjectParsing(internal::TextWriter* writer,
                                 google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitEnumParsing(internal::TextWriter* writer,
                               google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitGoogleApiFieldParsing(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitFieldParsing(internal::TextWriter* writer,
                                google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitOneofFieldParsing(internal::TextWriter* writer,
                                     google::protobuf::DescriptorProto const& message_type,
                                     size_t oneof_index, bool first) const;

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

  absl::Status EmitObjectFieldEncoding(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitFieldEncoding(internal::TextWriter* writer,
                                 google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitOneofFieldEncoding(internal::TextWriter* writer,
                                      google::protobuf::DescriptorProto const& message_type,
                                      size_t oneof_index) const;

  absl::Status EmitMessageDecoding(internal::TextWriter* writer, LexicalScope const& scope,
                                   std::string_view qualified_name,
                                   google::protobuf::DescriptorProto const& message_type) const;

  absl::Status EmitMessageEncoding(internal::TextWriter* writer, LexicalScope const& scope,
                                   std::string_view qualified_name,
                                   google::protobuf::DescriptorProto const& message_type) const;

  static absl::Status EmitEnumFieldStringification(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor,
      bool is_optional);

  static absl::Status EmitGoogleApiFieldStringification(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor,
      bool is_optional);

  absl::Status EmitObjectFieldStringification(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitFieldStringification(
      internal::TextWriter* writer, google::protobuf::FieldDescriptorProto const& descriptor) const;

  absl::Status EmitOneofFieldStringification(internal::TextWriter* writer,
                                             google::protobuf::DescriptorProto const& message_type,
                                             size_t oneof_index) const;

  absl::Status EmitMessageStringification(
      internal::TextWriter* writer, LexicalScope const& scope, std::string_view qualified_name,
      google::protobuf::DescriptorProto const& message_type) const;

  absl::Status EmitMessageParsing(internal::TextWriter* writer, LexicalScope const& scope,
                                  std::string_view qualified_name,
                                  google::protobuf::DescriptorProto const& message_type) const;

  absl::Status EmitMessageImplementation(
      internal::TextWriter* writer, PathView prefix, LexicalScope const& scope,
      google::protobuf::DescriptorProto const& message_type) const;

  absl::Status EmitImplementationForScope(internal::TextWriter* writer, PathView prefix,
                                          LexicalScope const& scope) const;

  static absl::Status EmitEnumReflectionDescriptor(
      internal::TextWriter* writer, PathView path,
      google::protobuf::EnumDescriptorProto const& enum_type);

  static absl::Status EmitEnumFieldDescriptor(
      internal::TextWriter* writer, std::string_view qualified_parent_name,
      google::protobuf::FieldDescriptorProto const& descriptor, bool is_optional);

  absl::StatusOr<std::string> GetMapValueDescriptorName(PathView entry_path) const;

  absl::Status EmitObjectFieldDescriptor(
      internal::TextWriter* writer, std::string_view qualified_parent_name,
      google::protobuf::FieldDescriptorProto const& descriptor) const;

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
  EnumsByPath enum_types_by_path_;
  MessagesByPath message_types_by_path_;
  internal::DependencyManager dependencies_;
  internal::DependencyManager flat_dependencies_;
  Path base_path_;
};

}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_GENERATOR_H__
