#ifndef __TSDB2_PROTO_DESCRIPTOR_PB_H__
#define __TSDB2_PROTO_DESCRIPTOR_PB_H__

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "io/cord.h"
#include "proto/proto.h"

namespace google::protobuf {

struct FileDescriptorSet;
struct FileDescriptorProto;
struct DescriptorProto;
struct ExtensionRangeOptions;
struct FieldDescriptorProto;
struct OneofDescriptorProto;
struct EnumDescriptorProto;
struct EnumValueDescriptorProto;
struct ServiceDescriptorProto;
struct MethodDescriptorProto;
struct FileOptions;
struct MessageOptions;
struct FieldOptions;
struct OneofOptions;
struct EnumOptions;
struct EnumValueOptions;
struct ServiceOptions;
struct MethodOptions;
struct UninterpretedOption;
struct FeatureSet;
struct FeatureSetDefaults;
struct SourceCodeInfo;
struct GeneratedCodeInfo;

enum class Edition {
  EDITION_UNKNOWN = 0,
  EDITION_LEGACY = 900,
  EDITION_PROTO2 = 998,
  EDITION_PROTO3 = 999,
  EDITION_2023 = 1000,
  EDITION_2024 = 1001,
  EDITION_1_TEST_ONLY = 1,
  EDITION_2_TEST_ONLY = 2,
  EDITION_99997_TEST_ONLY = 99997,
  EDITION_99998_TEST_ONLY = 99998,
  EDITION_99999_TEST_ONLY = 99999,
  EDITION_MAX = 2147483647,
};

struct FeatureSet : public ::tsdb2::proto::Message {
  enum class FieldPresence {
    FIELD_PRESENCE_UNKNOWN = 0,
    EXPLICIT = 1,
    IMPLICIT = 2,
    LEGACY_REQUIRED = 3,
  };

  enum class EnumType {
    ENUM_TYPE_UNKNOWN = 0,
    OPEN = 1,
    CLOSED = 2,
  };

  enum class RepeatedFieldEncoding {
    REPEATED_FIELD_ENCODING_UNKNOWN = 0,
    PACKED = 1,
    EXPANDED = 2,
  };

  enum class Utf8Validation {
    UTF8_VALIDATION_UNKNOWN = 0,
    VERIFY = 2,
    NONE = 3,
  };

  enum class MessageEncoding {
    MESSAGE_ENCODING_UNKNOWN = 0,
    LENGTH_PREFIXED = 1,
    DELIMITED = 2,
  };

  enum class JsonFormat {
    JSON_FORMAT_UNKNOWN = 0,
    ALLOW = 1,
    LEGACY_BEST_EFFORT = 2,
  };

  static ::absl::StatusOr<FeatureSet> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(FeatureSet const& proto);

  std::optional<::google::protobuf::FeatureSet::FieldPresence> field_presence;
  std::optional<::google::protobuf::FeatureSet::EnumType> enum_type;
  std::optional<::google::protobuf::FeatureSet::RepeatedFieldEncoding> repeated_field_encoding;
  std::optional<::google::protobuf::FeatureSet::Utf8Validation> utf8_validation;
  std::optional<::google::protobuf::FeatureSet::MessageEncoding> message_encoding;
  std::optional<::google::protobuf::FeatureSet::JsonFormat> json_format;
};

struct ExtensionRangeOptions : public ::tsdb2::proto::Message {
  struct Declaration;

  enum class VerificationState {
    DECLARATION = 0,
    UNVERIFIED = 1,
  };

  struct Declaration : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<Declaration> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(Declaration const& proto);

    std::optional<int32_t> number;
    std::optional<std::string> full_name;
    std::optional<std::string> type;
    std::optional<bool> reserved;
    std::optional<bool> repeated;
  };

  static ::absl::StatusOr<ExtensionRangeOptions> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(ExtensionRangeOptions const& proto);

  std::vector<::google::protobuf::UninterpretedOption> uninterpreted_option;
  std::vector<::google::protobuf::ExtensionRangeOptions::Declaration> declaration;
  std::optional<::google::protobuf::FeatureSet> features;
  ::google::protobuf::ExtensionRangeOptions::VerificationState verification{
      ::google::protobuf::ExtensionRangeOptions::VerificationState::UNVERIFIED};
};

struct MessageOptions : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<MessageOptions> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(MessageOptions const& proto);

  bool message_set_wire_format{false};
  bool no_standard_descriptor_accessor{false};
  bool deprecated{false};
  std::optional<bool> map_entry;
  std::optional<bool> deprecated_legacy_json_field_conflicts;
  std::optional<::google::protobuf::FeatureSet> features;
  std::vector<::google::protobuf::UninterpretedOption> uninterpreted_option;
};

struct DescriptorProto : public ::tsdb2::proto::Message {
  struct ExtensionRange;
  struct ReservedRange;

  struct ExtensionRange : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<ExtensionRange> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(ExtensionRange const& proto);

    std::optional<int32_t> start;
    std::optional<int32_t> end;
    std::optional<::google::protobuf::ExtensionRangeOptions> options;
  };

  struct ReservedRange : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<ReservedRange> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(ReservedRange const& proto);

    std::optional<int32_t> start;
    std::optional<int32_t> end;
  };

  static ::absl::StatusOr<DescriptorProto> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(DescriptorProto const& proto);

  std::optional<std::string> name;
  std::vector<::google::protobuf::FieldDescriptorProto> field;
  std::vector<::google::protobuf::FieldDescriptorProto> extension;
  std::vector<::google::protobuf::DescriptorProto> nested_type;
  std::vector<::google::protobuf::EnumDescriptorProto> enum_type;
  std::vector<::google::protobuf::DescriptorProto::ExtensionRange> extension_range;
  std::vector<::google::protobuf::OneofDescriptorProto> oneof_decl;
  std::optional<::google::protobuf::MessageOptions> options;
  std::vector<::google::protobuf::DescriptorProto::ReservedRange> reserved_range;
  std::vector<std::string> reserved_name;
};

struct EnumOptions : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<EnumOptions> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(EnumOptions const& proto);

  std::optional<bool> allow_alias;
  bool deprecated{false};
  std::optional<bool> deprecated_legacy_json_field_conflicts;
  std::optional<::google::protobuf::FeatureSet> features;
  std::vector<::google::protobuf::UninterpretedOption> uninterpreted_option;
};

struct EnumDescriptorProto : public ::tsdb2::proto::Message {
  struct EnumReservedRange;

  struct EnumReservedRange : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<EnumReservedRange> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(EnumReservedRange const& proto);

    std::optional<int32_t> start;
    std::optional<int32_t> end;
  };

  static ::absl::StatusOr<EnumDescriptorProto> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(EnumDescriptorProto const& proto);

  std::optional<std::string> name;
  std::vector<::google::protobuf::EnumValueDescriptorProto> value;
  std::optional<::google::protobuf::EnumOptions> options;
  std::vector<::google::protobuf::EnumDescriptorProto::EnumReservedRange> reserved_range;
  std::vector<std::string> reserved_name;
};

struct FieldOptions : public ::tsdb2::proto::Message {
  struct EditionDefault;
  struct FeatureSupport;

  enum class CType {
    STRING = 0,
    CORD = 1,
    STRING_PIECE = 2,
  };

  enum class JSType {
    JS_NORMAL = 0,
    JS_STRING = 1,
    JS_NUMBER = 2,
  };

  enum class OptionRetention {
    RETENTION_UNKNOWN = 0,
    RETENTION_RUNTIME = 1,
    RETENTION_SOURCE = 2,
  };

  enum class OptionTargetType {
    TARGET_TYPE_UNKNOWN = 0,
    TARGET_TYPE_FILE = 1,
    TARGET_TYPE_EXTENSION_RANGE = 2,
    TARGET_TYPE_MESSAGE = 3,
    TARGET_TYPE_FIELD = 4,
    TARGET_TYPE_ONEOF = 5,
    TARGET_TYPE_ENUM = 6,
    TARGET_TYPE_ENUM_ENTRY = 7,
    TARGET_TYPE_SERVICE = 8,
    TARGET_TYPE_METHOD = 9,
  };

  struct EditionDefault : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<EditionDefault> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(EditionDefault const& proto);

    std::optional<::google::protobuf::Edition> edition;
    std::optional<std::string> value;
  };

  struct FeatureSupport : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<FeatureSupport> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(FeatureSupport const& proto);

    std::optional<::google::protobuf::Edition> edition_introduced;
    std::optional<::google::protobuf::Edition> edition_deprecated;
    std::optional<std::string> deprecation_warning;
    std::optional<::google::protobuf::Edition> edition_removed;
  };

  static ::absl::StatusOr<FieldOptions> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(FieldOptions const& proto);

  ::google::protobuf::FieldOptions::CType ctype{::google::protobuf::FieldOptions::CType::STRING};
  std::optional<bool> packed;
  ::google::protobuf::FieldOptions::JSType jstype{
      ::google::protobuf::FieldOptions::JSType::JS_NORMAL};
  bool lazy{false};
  bool unverified_lazy{false};
  bool deprecated{false};
  bool weak{false};
  bool debug_redact{false};
  std::optional<::google::protobuf::FieldOptions::OptionRetention> retention;
  std::vector<::google::protobuf::FieldOptions::OptionTargetType> targets;
  std::vector<::google::protobuf::FieldOptions::EditionDefault> edition_defaults;
  std::optional<::google::protobuf::FeatureSet> features;
  std::optional<::google::protobuf::FieldOptions::FeatureSupport> feature_support;
  std::vector<::google::protobuf::UninterpretedOption> uninterpreted_option;
};

struct EnumValueOptions : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<EnumValueOptions> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(EnumValueOptions const& proto);

  bool deprecated{false};
  std::optional<::google::protobuf::FeatureSet> features;
  bool debug_redact{false};
  std::optional<::google::protobuf::FieldOptions::FeatureSupport> feature_support;
  std::vector<::google::protobuf::UninterpretedOption> uninterpreted_option;
};

struct EnumValueDescriptorProto : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<EnumValueDescriptorProto> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(EnumValueDescriptorProto const& proto);

  std::optional<std::string> name;
  std::optional<int32_t> number;
  std::optional<::google::protobuf::EnumValueOptions> options;
};

struct FeatureSetDefaults : public ::tsdb2::proto::Message {
  struct FeatureSetEditionDefault;

  struct FeatureSetEditionDefault : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<FeatureSetEditionDefault> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(FeatureSetEditionDefault const& proto);

    std::optional<::google::protobuf::Edition> edition;
    std::optional<::google::protobuf::FeatureSet> overridable_features;
    std::optional<::google::protobuf::FeatureSet> fixed_features;
  };

  static ::absl::StatusOr<FeatureSetDefaults> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(FeatureSetDefaults const& proto);

  std::vector<::google::protobuf::FeatureSetDefaults::FeatureSetEditionDefault> defaults;
  std::optional<::google::protobuf::Edition> minimum_edition;
  std::optional<::google::protobuf::Edition> maximum_edition;
};

struct FieldDescriptorProto : public ::tsdb2::proto::Message {
  enum class Type {
    TYPE_DOUBLE = 1,
    TYPE_FLOAT = 2,
    TYPE_INT64 = 3,
    TYPE_UINT64 = 4,
    TYPE_INT32 = 5,
    TYPE_FIXED64 = 6,
    TYPE_FIXED32 = 7,
    TYPE_BOOL = 8,
    TYPE_STRING = 9,
    TYPE_GROUP = 10,
    TYPE_MESSAGE = 11,
    TYPE_BYTES = 12,
    TYPE_UINT32 = 13,
    TYPE_ENUM = 14,
    TYPE_SFIXED32 = 15,
    TYPE_SFIXED64 = 16,
    TYPE_SINT32 = 17,
    TYPE_SINT64 = 18,
  };

  enum class Label {
    LABEL_OPTIONAL = 1,
    LABEL_REPEATED = 3,
    LABEL_REQUIRED = 2,
  };

  static ::absl::StatusOr<FieldDescriptorProto> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(FieldDescriptorProto const& proto);

  std::optional<std::string> name;
  std::optional<int32_t> number;
  std::optional<::google::protobuf::FieldDescriptorProto::Label> label;
  std::optional<::google::protobuf::FieldDescriptorProto::Type> type;
  std::optional<std::string> type_name;
  std::optional<std::string> extendee;
  std::optional<std::string> default_value;
  std::optional<int32_t> oneof_index;
  std::optional<std::string> json_name;
  std::optional<::google::protobuf::FieldOptions> options;
  std::optional<bool> proto3_optional;
};

struct FileOptions : public ::tsdb2::proto::Message {
  enum class OptimizeMode {
    SPEED = 1,
    CODE_SIZE = 2,
    LITE_RUNTIME = 3,
  };

  static ::absl::StatusOr<FileOptions> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(FileOptions const& proto);

  std::optional<std::string> java_package;
  std::optional<std::string> java_outer_classname;
  bool java_multiple_files{false};
  std::optional<bool> java_generate_equals_and_hash;
  bool java_string_check_utf8{false};
  ::google::protobuf::FileOptions::OptimizeMode optimize_for{
      ::google::protobuf::FileOptions::OptimizeMode::SPEED};
  std::optional<std::string> go_package;
  bool cc_generic_services{false};
  bool java_generic_services{false};
  bool py_generic_services{false};
  bool deprecated{false};
  bool cc_enable_arenas{true};
  std::optional<std::string> objc_class_prefix;
  std::optional<std::string> csharp_namespace;
  std::optional<std::string> swift_prefix;
  std::optional<std::string> php_class_prefix;
  std::optional<std::string> php_namespace;
  std::optional<std::string> php_metadata_namespace;
  std::optional<std::string> ruby_package;
  std::optional<::google::protobuf::FeatureSet> features;
  std::vector<::google::protobuf::UninterpretedOption> uninterpreted_option;
};

struct SourceCodeInfo : public ::tsdb2::proto::Message {
  struct Location;

  struct Location : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<Location> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(Location const& proto);

    std::vector<int32_t> path;
    std::vector<int32_t> span;
    std::optional<std::string> leading_comments;
    std::optional<std::string> trailing_comments;
    std::vector<std::string> leading_detached_comments;
  };

  static ::absl::StatusOr<SourceCodeInfo> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(SourceCodeInfo const& proto);

  std::vector<::google::protobuf::SourceCodeInfo::Location> location;
};

struct FileDescriptorProto : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<FileDescriptorProto> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(FileDescriptorProto const& proto);

  std::optional<std::string> name;
  std::optional<std::string> package;
  std::vector<std::string> dependency;
  std::vector<int32_t> public_dependency;
  std::vector<int32_t> weak_dependency;
  std::vector<::google::protobuf::DescriptorProto> message_type;
  std::vector<::google::protobuf::EnumDescriptorProto> enum_type;
  std::vector<::google::protobuf::ServiceDescriptorProto> service;
  std::vector<::google::protobuf::FieldDescriptorProto> extension;
  std::optional<::google::protobuf::FileOptions> options;
  std::optional<::google::protobuf::SourceCodeInfo> source_code_info;
  std::optional<std::string> syntax;
  std::optional<::google::protobuf::Edition> edition;
};

struct FileDescriptorSet : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<FileDescriptorSet> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(FileDescriptorSet const& proto);

  std::vector<::google::protobuf::FileDescriptorProto> file;
};

struct GeneratedCodeInfo : public ::tsdb2::proto::Message {
  struct Annotation;

  struct Annotation : public ::tsdb2::proto::Message {
    enum class Semantic {
      NONE = 0,
      SET = 1,
      ALIAS = 2,
    };

    static ::absl::StatusOr<Annotation> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(Annotation const& proto);

    std::vector<int32_t> path;
    std::optional<std::string> source_file;
    std::optional<int32_t> begin;
    std::optional<int32_t> end;
    std::optional<::google::protobuf::GeneratedCodeInfo::Annotation::Semantic> semantic;
  };

  static ::absl::StatusOr<GeneratedCodeInfo> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(GeneratedCodeInfo const& proto);

  std::vector<::google::protobuf::GeneratedCodeInfo::Annotation> annotation;
};

struct MethodOptions : public ::tsdb2::proto::Message {
  enum class IdempotencyLevel {
    IDEMPOTENCY_UNKNOWN = 0,
    NO_SIDE_EFFECTS = 1,
    IDEMPOTENT = 2,
  };

  static ::absl::StatusOr<MethodOptions> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(MethodOptions const& proto);

  bool deprecated{false};
  ::google::protobuf::MethodOptions::IdempotencyLevel idempotency_level{
      ::google::protobuf::MethodOptions::IdempotencyLevel::IDEMPOTENCY_UNKNOWN};
  std::optional<::google::protobuf::FeatureSet> features;
  std::vector<::google::protobuf::UninterpretedOption> uninterpreted_option;
};

struct MethodDescriptorProto : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<MethodDescriptorProto> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(MethodDescriptorProto const& proto);

  std::optional<std::string> name;
  std::optional<std::string> input_type;
  std::optional<std::string> output_type;
  std::optional<::google::protobuf::MethodOptions> options;
  bool client_streaming{false};
  bool server_streaming{false};
};

struct OneofOptions : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<OneofOptions> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(OneofOptions const& proto);

  std::optional<::google::protobuf::FeatureSet> features;
  std::vector<::google::protobuf::UninterpretedOption> uninterpreted_option;
};

struct OneofDescriptorProto : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<OneofDescriptorProto> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(OneofDescriptorProto const& proto);

  std::optional<std::string> name;
  std::optional<::google::protobuf::OneofOptions> options;
};

struct ServiceOptions : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<ServiceOptions> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(ServiceOptions const& proto);

  std::optional<::google::protobuf::FeatureSet> features;
  bool deprecated{false};
  std::vector<::google::protobuf::UninterpretedOption> uninterpreted_option;
};

struct ServiceDescriptorProto : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<ServiceDescriptorProto> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(ServiceDescriptorProto const& proto);

  std::optional<std::string> name;
  std::vector<::google::protobuf::MethodDescriptorProto> method;
  std::optional<::google::protobuf::ServiceOptions> options;
};

struct UninterpretedOption : public ::tsdb2::proto::Message {
  struct NamePart;

  struct NamePart : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<NamePart> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(NamePart const& proto);

    std::string name_part{};
    bool is_extension{};
  };

  static ::absl::StatusOr<UninterpretedOption> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(UninterpretedOption const& proto);

  std::vector<::google::protobuf::UninterpretedOption::NamePart> name;
  std::optional<std::string> identifier_value;
  std::optional<uint64_t> positive_int_value;
  std::optional<int64_t> negative_int_value;
  std::optional<double> double_value;
  std::optional<std::vector<uint8_t>> string_value;
  std::optional<std::string> aggregate_value;
};

}  // namespace google::protobuf

#endif  // __TSDB2_PROTO_DESCRIPTOR_PB_H__
