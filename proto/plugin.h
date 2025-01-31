#ifndef __TSDB2_PROTO_PLUGIN_H__
#define __TSDB2_PROTO_PLUGIN_H__

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "io/cord.h"

// Manually translated from google/protobuf/compiler/plugin.proto and
// google/protobuf/descriptor.proto (see https://github.com/protocolbuffers/protobuf).

namespace google {
namespace protobuf {

enum class Edition {
  // A placeholder for an unknown edition value.
  EDITION_UNKNOWN = 0,

  // A placeholder edition for specifying default behaviors *before* a feature was first introduced.
  // This is effectively an "infinite past".
  EDITION_LEGACY = 900,

  // Legacy syntax "editions".  These pre-date editions, but behave much like distinct editions.
  // These can't be used to specify the edition of proto files, but feature definitions must supply
  // proto2/proto3 defaults for backwards compatibility.
  EDITION_PROTO2 = 998,
  EDITION_PROTO3 = 999,

  // Editions that have been released.  The specific values are arbitrary and should not be depended
  // on, but they will always be time-ordered for easy comparison.
  EDITION_2023 = 1000,
  EDITION_2024 = 1001,

  // Placeholder editions for testing feature resolution.  These should not be used or relied on
  // outside of tests.
  EDITION_1_TEST_ONLY = 1,
  EDITION_2_TEST_ONLY = 2,
  EDITION_99997_TEST_ONLY = 99997,
  EDITION_99998_TEST_ONLY = 99998,
  EDITION_99999_TEST_ONLY = 99999,

  // Placeholder for specifying unbounded edition support.  This should only ever be used by plugins
  // that can expect to never require any changes to support a new edition.
  EDITION_MAX = 0x7FFFFFFF,
};

// Describes an enum type.
struct EnumDescriptorProto {
  static absl::StatusOr<EnumDescriptorProto> Decode(absl::Span<uint8_t const> const& buffer);
  static tsdb2::io::Cord Encode(EnumDescriptorProto const& value);

  auto tie() const { return std::tie(name, value, options, reserved_range, reserved_name); }

  template <typename H>
  friend H AbslHashValue(H h, EnumDescriptorProto const& value) {
    return H::combine(std::move(h), value.tie());
  }

  friend bool operator==(EnumDescriptorProto const& lhs, EnumDescriptorProto const& rhs) {
    return lhs.tie() == rhs.tie();
  }

  friend bool operator!=(EnumDescriptorProto const& lhs, EnumDescriptorProto const& rhs) {
    return lhs.tie() != rhs.tie();
  }

  std::optional<std::string> name;

  std::vector<EnumValueDescriptorProto> value;

  std::optional<EnumOptions> options;

  // Range of reserved numeric values. Reserved values may not be used by
  // entries in the same enum. Reserved ranges may not overlap.
  //
  // Note that this is distinct from DescriptorProto.ReservedRange in that it
  // is inclusive such that it can appropriately represent the entire int32
  // domain.
  struct EnumReservedRange {
    static absl::StatusOr<EnumReservedRange> Decode(absl::Span<uint8_t const> const& buffer);
    static tsdb2::io::Cord Encode(EnumReservedRange const& value);

    auto tie() const { return std::tie(start, end); }

    template <typename H>
    friend H AbslHashValue(H h, EnumReservedRange const& value) {
      return H::combine(std::move(h), value.tie());
    }

    friend bool operator==(EnumReservedRange const& lhs, EnumReservedRange const& rhs) {
      return lhs.tie() == rhs.tie();
    }

    friend bool operator!=(EnumReservedRange const& lhs, EnumReservedRange const& rhs) {
      return lhs.tie() != rhs.tie();
    }

    std::optional<int32_t> start;  // Inclusive.
    std::optional<int32_t> end;    // Inclusive.
  };

  // Range of reserved numeric values. Reserved numeric values may not be used
  // by enum values in the same enum declaration. Reserved ranges may not
  // overlap.
  std::vector<EnumReservedRange> reserved_range;

  // Reserved enum value names, which may not be reused. A given name may only
  // be reserved once.
  std::vector<std::string> reserved_name;
};

// A message representing a option the parser does not recognize. This only appears in options
// protos created by the compiler::Parser class. DescriptorPool resolves these when building
// Descriptor objects. Therefore, options protos in descriptor objects (e.g. returned by
// Descriptor::options(), or produced by Descriptor::CopyTo()) will never have UninterpretedOptions
// in them.
struct UninterpretedOption {
  struct NamePart;

  static absl::StatusOr<UninterpretedOption> Decode(absl::Span<uint8_t const> const& buffer);
  static tsdb2::io::Cord Encode(UninterpretedOption const& value);

  auto tie() const {
    return std::tie(name, identifier_value, positive_int_value, negative_int_value, double_value,
                    string_value, aggregate_value);
  }

  template <typename H>
  friend H AbslHashValue(H h, UninterpretedOption const& value) {
    return H::combine(std::move(h), value.tie());
  }

  friend bool operator==(UninterpretedOption const& lhs, UninterpretedOption const& rhs) {
    return lhs.tie() == rhs.tie();
  }

  friend bool operator!=(UninterpretedOption const& lhs, UninterpretedOption const& rhs) {
    return lhs.tie() != rhs.tie();
  }

  // The name of the uninterpreted option.  Each string represents a segment in a dot-separated
  // name.  is_extension is true iff a segment represents an extension (denoted with parentheses in
  // options specs in .proto files). E.g.,{ ["foo", false], ["bar.baz", true], ["moo", false] }
  // represents "foo.(bar.baz).moo".
  struct NamePart {
    static absl::StatusOr<NamePart> Decode(absl::Span<uint8_t const> const& buffer);
    static tsdb2::io::Cord Encode(NamePart const& value);

    auto tie() const { return std::tie(name_part, is_extension); }

    template <typename H>
    friend H AbslHashValue(H h, NamePart const& value) {
      return H::combine(std::move(h), value.tie());
    }

    friend bool operator==(NamePart const& lhs, NamePart const& rhs) {
      return lhs.tie() == rhs.tie();
    }

    friend bool operator!=(NamePart const& lhs, NamePart const& rhs) {
      return lhs.tie() != rhs.tie();
    }

    std::string name_part;
    bool is_extension{};
  };

  std::vector<NamePart> name;

  // The value of the uninterpreted option, in whatever type the tokenizer identified it as during
  // parsing. Exactly one of these should be set.
  std::optional<std::string> identifier_value;
  std::optional<uint64_t> positive_int_value;
  std::optional<int64_t> negative_int_value;
  std::optional<double> double_value;
  std::optional<std::string> string_value;
  std::optional<std::string> aggregate_value;
};

struct FeatureSet {
  static absl::StatusOr<FeatureSet> Decode(absl::Span<uint8_t const> const& buffer);
  static tsdb2::io::Cord Encode(FeatureSet const& value);

  auto tie() const {
    return std::tie(field_presence, enum_type, repeated_field_encoding, utf8_validation,
                    message_encoding, json_format, enforce_naming_style);
  }

  template <typename H>
  friend H AbslHashValue(H h, FeatureSet const& value) {
    return H::combine(std::move(h), value.tie());
  }

  friend bool operator==(FeatureSet const& lhs, FeatureSet const& rhs) {
    return lhs.tie() == rhs.tie();
  }

  friend bool operator!=(FeatureSet const& lhs, FeatureSet const& rhs) {
    return lhs.tie() != rhs.tie();
  }

  enum class FieldPresence {
    FIELD_PRESENCE_UNKNOWN = 0,
    EXPLICIT = 1,
    IMPLICIT = 2,
    LEGACY_REQUIRED = 3,
  };

  std::optional<FieldPresence> field_presence;

  enum class EnumType {
    ENUM_TYPE_UNKNOWN = 0,
    OPEN = 1,
    CLOSED = 2,
  };

  std::optional<EnumType> enum_type;

  enum class RepeatedFieldEncoding {
    REPEATED_FIELD_ENCODING_UNKNOWN = 0,
    PACKED = 1,
    EXPANDED = 2,
  };

  std::optional<RepeatedFieldEncoding> repeated_field_encoding;

  enum class Utf8Validation {
    UTF8_VALIDATION_UNKNOWN = 0,
    VERIFY = 2,
    NONE = 3,
  };

  std::optional<Utf8Validation> utf8_validation;

  enum class MessageEncoding {
    MESSAGE_ENCODING_UNKNOWN = 0,
    LENGTH_PREFIXED = 1,
    DELIMITED = 2,
  };

  std::optional<MessageEncoding> message_encoding;

  enum class JsonFormat {
    JSON_FORMAT_UNKNOWN = 0,
    ALLOW = 1,
    LEGACY_BEST_EFFORT = 2,
  };

  std::optional<JsonFormat> json_format;

  enum class EnforceNamingStyle {
    ENFORCE_NAMING_STYLE_UNKNOWN = 0,
    STYLE2024 = 1,
    STYLE_LEGACY = 2,
  };

  std::optional<EnforceNamingStyle> enforce_naming_style;
};

struct ExtensionRangeOptions {
  static absl::StatusOr<ExtensionRangeOptions> Decode(absl::Span<uint8_t const> const& buffer);
  static tsdb2::io::Cord Encode(ExtensionRangeOptions const& value);

  auto tie() const { return std::tie(uninterpreted_option, declaration, features, verification); }

  template <typename H>
  friend H AbslHashValue(H h, ExtensionRangeOptions const& value) {
    return H::combine(std::move(h), value.tie());
  }

  friend bool operator==(ExtensionRangeOptions const& lhs, ExtensionRangeOptions const& rhs) {
    return lhs.tie() == rhs.tie();
  }

  friend bool operator!=(ExtensionRangeOptions const& lhs, ExtensionRangeOptions const& rhs) {
    return lhs.tie() != rhs.tie();
  }

  // The parser stores options it doesn't recognize here. See above.
  std::vector<UninterpretedOption> uninterpreted_option;

  struct Declaration {
    static absl::StatusOr<Declaration> Decode(absl::Span<uint8_t const> const& buffer);
    static tsdb2::io::Cord Encode(Declaration const& value);

    auto tie() const { return std::tie(number, full_name, type, reserved, repeated); }

    template <typename H>
    friend H AbslHashValue(H h, Declaration const& value) {
      return H::combine(std::move(h), value.tie());
    }

    friend bool operator==(Declaration const& lhs, Declaration const& rhs) {
      return lhs.tie() == rhs.tie();
    }

    friend bool operator!=(Declaration const& lhs, Declaration const& rhs) {
      return lhs.tie() != rhs.tie();
    }

    // The extension number declared within the extension range.
    std::optional<int32_t> number;

    // The fully-qualified name of the extension field. There must be a leading dot in front of the
    // full name.
    std::optional<std::string> full_name;

    // The fully-qualified type name of the extension field. Unlike Metadata.type, Declaration.type
    // must have a leading dot for messages and enums.
    std::optional<std::string> type;

    // If true, indicates that the number is reserved in the extension range, and any extension
    // field with the number will fail to compile. Set this when a declared extension field is
    // deleted.
    std::optional<bool> reserved;

    // If true, indicates that the extension must be defined as repeated. Otherwise the extension
    // must be defined as optional.
    std::optional<bool> repeated;
  };

  // For external users: DO NOT USE. We are in the process of open sourcing
  // extension declaration and executing internal cleanups before it can be
  // used externally.
  std::vector<Declaration> declaration;

  // Any features defined in the specific edition.
  std::optional<FeatureSet> features;

  // The verification state of the extension range.
  enum class VerificationState {
    // All the extensions of the range must be declared.
    DECLARATION = 0,
    UNVERIFIED = 1,
  };

  // The verification state of the range.
  // TODO: flip the default to DECLARATION once all empty ranges
  // are marked as UNVERIFIED.
  std::optional<VerificationState> verification;
};

// Describes a field within a message.
struct FieldDescriptorProto {
  static absl::StatusOr<FieldDescriptorProto> Decode(absl::Span<uint8_t const> const& buffer);
  static tsdb2::io::Cord Encode(FieldDescriptorProto const& value);

  auto tie() const {
    return std::tie(name, number, label, type, type_name, extendee, default_value, oneof_index,
                    json_name, options, proto3_optional);
  }

  template <typename H>
  friend H AbslHashValue(H h, FieldDescriptorProto const& value) {
    return H::combine(std::move(h), value.tie());
  }

  friend bool operator==(FieldDescriptorProto const& lhs, FieldDescriptorProto const& rhs) {
    return lhs.tie() == rhs.tie();
  }

  friend bool operator!=(FieldDescriptorProto const& lhs, FieldDescriptorProto const& rhs) {
    return lhs.tie() != rhs.tie();
  }

  enum class Type {
    // 0 is reserved for errors.
    // Order is weird for historical reasons.
    TYPE_DOUBLE = 1,
    TYPE_FLOAT = 2,
    // Not ZigZag encoded.  Negative numbers take 10 bytes.  Use TYPE_SINT64 if negative values are
    // likely.
    TYPE_INT64 = 3,
    TYPE_UINT64 = 4,
    // Not ZigZag encoded.  Negative numbers take 10 bytes.  Use TYPE_SINT32 if negative values are
    // likely.
    TYPE_INT32 = 5,
    TYPE_FIXED64 = 6,
    TYPE_FIXED32 = 7,
    TYPE_BOOL = 8,
    TYPE_STRING = 9,
    // Tag-delimited aggregate.
    // Group type is deprecated and not supported after google.protobuf. However, Proto3
    // implementations should still be able to parse the group wire format and treat group fields as
    // unknown fields.  In Editions, the group wire format can be enabled via the `message_encoding`
    // feature.
    TYPE_GROUP = 10,
    TYPE_MESSAGE = 11,  // Length-delimited aggregate.

    // New in version 2.
    TYPE_BYTES = 12,
    TYPE_UINT32 = 13,
    TYPE_ENUM = 14,
    TYPE_SFIXED32 = 15,
    TYPE_SFIXED64 = 16,
    TYPE_SINT32 = 17,  // Uses ZigZag encoding.
    TYPE_SINT64 = 18,  // Uses ZigZag encoding.
  };

  enum class Label {
    // 0 is reserved for errors
    LABEL_OPTIONAL = 1,
    LABEL_REPEATED = 3,
    // The required label is only allowed in google.protobuf.  In proto3 and Editions it's
    // explicitly prohibited.  In Editions, the `field_presence` feature can be used to get this
    // behavior.
    LABEL_REQUIRED = 2,
  };

  std::optional<std::string> name;
  std::optional<int32_t> number;
  std::optional<Label> label;

  // If type_name is set, this need not be set.  If both this and type_name are set, this must be
  // one of TYPE_ENUM, TYPE_MESSAGE or TYPE_GROUP.
  std::optional<Type> type;

  // For message and enum types, this is the name of the type.  If the name starts with a '.', it is
  // fully-qualified.  Otherwise, C++-like scoping rules are used to find the type (i.e. first the
  // nested types within this message are searched, then within the parent, on up to the root
  // namespace).
  std::optional<std::string> type_name;

  // For extensions, this is the name of the type being extended.  It is  resolved in the same
  // manner as type_name.
  std::optional<std::string> extendee;

  // For numeric types, contains the original text representation of the value.
  // For booleans, "true" or "false".
  // For strings, contains the default text contents (not escaped in any way).
  // For bytes, contains the C escaped value.  All bytes >= 128 are escaped.
  std::optional<std::string> default_value;

  // If set, gives the index of a oneof in the containing type's oneof_decl list.  This field is a
  // member of that oneof.
  std::optional<int32_t> oneof_index;

  // JSON name of this field. The value is set by protocol compiler. If the user has set a
  // "json_name" option on this field, that option's value will be used. Otherwise, it's deduced
  // from the field's name by converting it to camelCase.
  std::optional<std::string> json_name;

  std::optional<FieldOptions> options;

  // If true, this is a proto3 "optional". When a proto3 field is optional, it tracks presence
  // regardless of field type.
  //
  // When proto3_optional is true, this field must belong to a oneof to signal to old proto3 clients
  // that presence is tracked for this field. This oneof is known as a "synthetic" oneof, and this
  // field must be its sole member (each proto3 optional field gets its own synthetic oneof).
  // Synthetic oneofs exist in the descriptor only, and do not generate any API. Synthetic oneofs
  // must be ordered after all "real" oneofs.
  //
  // For message fields, proto3_optional doesn't create any semantic change, since non-repeated
  // message fields always track presence. However it still indicates the semantic detail of whether
  // the user wrote "optional" or not. This can be useful for round-tripping the .proto file. For
  // consistency we give message fields a synthetic oneof also, even though it is not required to
  // track presence. This is especially important because the parser can't tell if a field is a
  // message or an enum, so it must always create a synthetic oneof.
  //
  // Proto2 optional fields do not set this flag, because they already indicate optional with
  // `LABEL_OPTIONAL`.
  std::optional<bool> proto3_optional;
};

// Describes a oneof.
struct OneofDescriptorProto {
  static absl::StatusOr<OneofDescriptorProto> Decode(absl::Span<uint8_t const> const& buffer);
  static tsdb2::io::Cord Encode(OneofDescriptorProto const& value);

  auto tie() const { return std::tie(name, options); }

  template <typename H>
  friend H AbslHashValue(H h, OneofDescriptorProto const& value) {
    return H::combine(std::move(h), value.tie());
  }

  friend bool operator==(OneofDescriptorProto const& lhs, OneofDescriptorProto const& rhs) {
    return lhs.tie() == rhs.tie();
  }

  friend bool operator!=(OneofDescriptorProto const& lhs, OneofDescriptorProto const& rhs) {
    return lhs.tie() != rhs.tie();
  }

  std::optional<std::string> name;
  std::optional<OneofOptions> options;
}

struct MessageOptions {
  static absl::StatusOr<MessageOptions> Decode(absl::Span<uint8_t const> const& buffer);
  static tsdb2::io::Cord Encode(MessageOptions const& value);

  auto tie() const {
    return std::tie(message_set_wire_format, no_standard_descriptor_accessor, deprecated, map_entry,
                    deprecated_legacy_json_field_conflicts, features, uninterpreted_option);
  }

  template <typename H>
  friend H AbslHashValue(H h, MessageOptions const& value) {
    return H::combine(std::move(h), value.tie());
  }

  friend bool operator==(MessageOptions const& lhs, MessageOptions const& rhs) {
    return lhs.tie() == rhs.tie();
  }

  friend bool operator!=(MessageOptions const& lhs, MessageOptions const& rhs) {
    return lhs.tie() != rhs.tie();
  }

  // Set true to use the old proto1 MessageSet wire format for extensions. This is provided for
  // backwards-compatibility with the MessageSet wire format.  You should not use this for any other
  // reason:  It's less efficient, has fewer features, and is more complicated.
  //
  // The message must be defined exactly as follows:
  //
  //   message Foo {
  //     option message_set_wire_format = true;
  //     extensions 4 to max;
  //   }
  //
  // Note that the message cannot have any defined fields; MessageSets only have extensions.
  //
  // All extensions of your type must be singular messages; e.g. they cannot be int32s, enums, or
  // repeated messages.
  //
  // Because this is an option, the above two restrictions are not enforced by the protocol
  // compiler.
  std::optional<bool> message_set_wire_format{false};

  // Disables the generation of the standard "descriptor()" accessor, which can conflict with a
  // field of the same name.  This is meant to make migration from proto1 easier; new code should
  // avoid fields named "descriptor".
  std::optional<bool> no_standard_descriptor_accessor{false};

  // Is this message deprecated?
  // Depending on the target platform, this can emit Deprecated annotations for the message, or it
  // will be completely ignored; in the very least, this is a formalization for deprecating
  // messages.
  std::optional<bool> deprecated{false};

  // Whether the message is an automatically generated map entry type for the maps field.
  //
  // For maps fields:
  //
  //     map<KeyType, ValueType> map_field = 1;
  //
  // The parsed descriptor looks like:
  //
  //     message MapFieldEntry {
  //         option map_entry = true;
  //         optional KeyType key = 1;
  //         optional ValueType value = 2;
  //     }
  //     repeated MapFieldEntry map_field = 1;
  //
  // Implementations may choose not to generate the map_entry=true message, but use a native map in
  // the target language to hold the keys and values. The reflection APIs in such implementations
  // still need to work as if the field is a repeated message field.
  //
  // NOTE: Do not set the option in .proto files. Always use the maps syntax instead. The option
  // should only be implicitly set by the proto compiler parser.
  std::optional<bool> map_entry = 7;

  // Enable the legacy handling of JSON field name conflicts.  This lowercases and strips
  // underscored from the fields before comparison in proto3 only. The new behavior takes
  // `json_name` into account and applies to proto2 as well.
  //
  // This should only be used as a temporary measure against broken builds due to the change in
  // behavior for JSON field name conflicts.
  //
  // TODO -- This is legacy behavior we plan to remove once downstream teams have had time to
  // migrate.
  std::optional<bool> deprecated_legacy_json_field_conflicts{true};

  // Any features defined in the specific edition.
  // WARNING: This field should only be used by protobuf plugins or special cases like the proto
  // compiler. Other uses are discouraged and developers should rely on the protoreflect APIs for
  // their client language.
  std::optional<FeatureSet> features;

  // The parser stores options it doesn't recognize here. See above.
  std::vector<UninterpretedOption> uninterpreted_option;
};

// Describes a message type.
struct DescriptorProto {
  struct ExtensionRange;
  struct ReservedRange;

  static absl::StatusOr<DescriptorProto> Decode(absl::Span<uint8_t const> const& buffer);
  static tsdb2::io::Cord Encode(DescriptorProto const& value);

  auto tie() const {
    return std::tie(name, field, extension, nested_type, enum_type, extension_range, oneof_decl,
                    options, reserved_range, reserved_name);
  }

  template <typename H>
  friend H AbslHashValue(H h, DescriptorProto const& value) {
    return H::combine(std::move(h), value.tie());
  }

  friend bool operator==(DescriptorProto const& lhs, DescriptorProto const& rhs) {
    return lhs.tie() == rhs.tie();
  }

  friend bool operator!=(DescriptorProto const& lhs, DescriptorProto const& rhs) {
    return lhs.tie() != rhs.tie();
  }

  std::optional<std::string> name;

  std::vector<FieldDescriptorProto> field;
  std::vector<FieldDescriptorProto> extension;

  std::vector<DescriptorProto> nested_type;
  std::vector<EnumDescriptorProto> enum_type;

  struct ExtensionRange {
    static absl::StatusOr<ExtensionRange> Decode(absl::Span<uint8_t const> const& buffer);
    static tsdb2::io::Cord Encode(ExtensionRange const& value);

    auto tie() const { return std::tie(start, end, options); }

    template <typename H>
    friend H AbslHashValue(H h, ExtensionRange const& value) {
      return H::combine(std::move(h), value.tie());
    }

    friend bool operator==(ExtensionRange const& lhs, ExtensionRange const& rhs) {
      return lhs.tie() == rhs.tie();
    }

    friend bool operator!=(ExtensionRange const& lhs, ExtensionRange const& rhs) {
      return lhs.tie() != rhs.tie();
    }

    std::optional<int32_t> start;  // Inclusive.
    std::optional<int32_t> end;    // Exclusive.
    std::optional<ExtensionRangeOptions> options;
  };

  std::vector<ExtensionRange> extension_range;

  std::vector<OneOfDescriptorProto> oneof_decl;

  std::optional<MessageOptions> options;

  // Range of reserved tag numbers. Reserved tag numbers may not be used by fields or extension
  // ranges in the same message. Reserved ranges may not overlap.
  struct ReservedRange {
    static absl::StatusOr<ReservedRange> Decode(absl::Span<uint8_t const> const& buffer);
    static tsdb2::io::Cord Encode(ReservedRange const& value);

    auto tie() const { return std::tie(start, end); }

    template <typename H>
    friend H AbslHashValue(H h, ReservedRange const& value) {
      return H::combine(std::move(h), value.tie());
    }

    friend bool operator==(ReservedRange const& lhs, ReservedRange const& rhs) {
      return lhs.tie() == rhs.tie();
    }

    friend bool operator!=(ReservedRange const& lhs, ReservedRange const& rhs) {
      return lhs.tie() != rhs.tie();
    }

    std::optional<int32_t> start;  // Inclusive.
    std::optional<int32_t> end;    // Exclusive.
  };

  std::vector<ReservedRange> reserved_range;

  // Reserved field names, which may not be used by fields in the same message. A given name may
  // only be reserved once.
  std::vector<std::string> reserved_name;
};

// Describes a complete .proto file.
struct FileDescriptorProto {
  static absl::StatusOr<FileDescriptorProto> Decode(absl::Span<uint8_t const> const& buffer);
  static tsdb2::io::Cord Encode(FileDescriptorProto const& value);

  auto tie() const {
    return std::tie(name, package, dependency, public_dependency, weak_dependency, syntax, edition);
  }

  template <typename H>
  friend H AbslHashValue(H h, FileDescriptorProto const& value) {
    return H::combine(std::move(h), value.tie());
  }

  friend bool operator==(FileDescriptorProto const& lhs, FileDescriptorProto const& rhs) {
    return lhs.tie() == rhs.tie();
  }

  friend bool operator!=(FileDescriptorProto const& lhs, FileDescriptorProto const& rhs) {
    return lhs.tie() != rhs.tie();
  }

  std::optional<std::string> name;     // file name, relative to root of source tree
  std::optional<std::string> package;  // e.g. "foo", "foo.bar", etc.

  // Names of files imported by this file.
  std::vector<std::string> dependency;

  // Indexes of the public imported files in the dependency list above.
  std::vector<int32_t> public_dependency;

  // Indexes of the weak imported files in the dependency list.
  // For Google-internal migration only. Do not use.
  std::vector<int32_t> weak_dependency;

  // All top-level definitions in this file.
  std::vector<DescriptorProto> message_type;

  // TODO

  std::optional<std::string> syntax;
  std::optional<Edition> edition;
};

// Describes the relationship between generated code and its original source file. A
// GeneratedCodeInfo message is associated with only one generated source file, but may contain
// references to different source .proto files.
struct GeneratedCodeInfo {
  struct Annotation;

  static absl::StatusOr<GeneratedCodeInfo> Decode(absl::Span<uint8_t const> const& buffer);
  static tsdb2::io::Cord Encode(GeneratedCodeInfo const& value);

  auto tie() const { return std::tie(annotation); }

  template <typename H>
  friend H AbslHashValue(H h, GeneratedCodeInfo const& value) {
    return H::combine(std::move(h), value.tie());
  }

  friend bool operator==(GeneratedCodeInfo const& lhs, GeneratedCodeInfo const& rhs) {
    return lhs.tie() == rhs.tie();
  }

  friend bool operator!=(GeneratedCodeInfo const& lhs, GeneratedCodeInfo const& rhs) {
    return lhs.tie() != rhs.tie();
  }

  // An Annotation connects some span of text in generated code to an element of its generating
  // .proto file.
  std::vector<Annotation> annotation;

  struct Annotation {
    static absl::StatusOr<Annotation> Decode(absl::Span<uint8_t const> const& buffer);
    static tsdb2::io::Cord Encode(Annotation const& value);

    auto tie() const { return std::tie(path, source_file, begin, end, semantic); }

    template <typename H>
    friend H AbslHashValue(H h, Annotation const& value) {
      return H::combine(std::move(h), value.tie());
    }

    friend bool operator==(Annotation const& lhs, Annotation const& rhs) {
      return lhs.tie() == rhs.tie();
    }

    friend bool operator!=(Annotation const& lhs, Annotation const& rhs) {
      return lhs.tie() != rhs.tie();
    }

    // Identifies the element in the original source .proto file. This field is formatted the same
    // as SourceCodeInfo.Location.path.
    std::vector<int32_t> path;

    // Identifies the filesystem path to the original source .proto.
    std::optional<std::string> source_file;

    // Identifies the starting offset in bytes in the generated code that relates to the identified
    // object.
    std::optional<int32_t> begin;

    // Identifies the ending offset in bytes in the generated code that relates to the identified
    // object. The end offset should be one past the last relevant byte (so the length of the text =
    // end - begin).
    std::optional<int32_t> end;

    // Represents the identified object's effect on the element in the original .proto file.
    enum class Semantic {
      // There is no effect or the effect is indescribable.
      NONE = 0,
      // The element is set or otherwise mutated.
      SET = 1,
      // An alias to the element is returned.
      ALIAS = 2,
    };

    std::optional<Semantic> semantic;
  };
};

namespace compiler {

struct CodeGeneratorRequest;
struct CodeGeneratorResponse;
struct Version;

// The version number of protocol compiler.
struct Version {
  static absl::StatusOr<Version> Decode(absl::Span<uint8_t const> const& buffer);
  static tsdb2::io::Cord Encode(Version const& value);

  auto tie() const { return std::tie(major, minor, patch, suffix); }

  template <typename H>
  friend H AbslHashValue(H h, Version const& value) {
    return H::combine(std::move(h), value.tie());
  }

  friend bool operator==(Version const& lhs, Version const& rhs) { return lhs.tie() == rhs.tie(); }
  friend bool operator!=(Version const& lhs, Version const& rhs) { return lhs.tie() != rhs.tie(); }

  std::optional<int32_t> major;
  std::optional<int32_t> minor;
  std::optional<int32_t> patch;

  // A suffix for alpha, beta or rc release, e.g., "alpha-1", "rc2". It should be empty for mainline
  // stable releases.
  std::optional<std::string> suffix;
};

// An encoded CodeGeneratorRequest is written to the plugin's stdin.
struct CodeGeneratorRequest {
  static absl::StatusOr<CodeGeneratorRequest> Decode(absl::Span<uint8_t const> const& buffer);
  static tsdb2::io::Cord Encode(CodeGeneratorRequest const& value);

  auto tie() const {
    return std::tie(file_to_generate, parameter, proto_file, source_file_descriptors,
                    compiler_version);
  }

  template <typename H>
  friend H AbslHashValue(H h, CodeGeneratorRequest const& value) {
    return H::combine(std::move(h), value.tie());
  }

  friend bool operator==(CodeGeneratorRequest const& lhs, CodeGeneratorRequest const& rhs) {
    return lhs.tie() == rhs.tie();
  }

  friend bool operator!=(CodeGeneratorRequest const& lhs, CodeGeneratorRequest const& rhs) {
    return lhs.tie() != rhs.tie();
  }

  // The .proto files that were explicitly listed on the command-line.  The code generator should
  // generate code only for these files.  Each file's descriptor will be included in proto_file,
  // below.
  std::vector<std::string> file_to_generate;

  // The generator parameter passed on the command-line.
  std::optional<std::string> parameter;

  // FileDescriptorProtos for all files in files_to_generate and everything they import.  The files
  // will appear in topological order, so each file appears before any file that imports it.
  //
  // Note: the files listed in files_to_generate will include runtime-retention options only, but
  // all other files will include source-retention options. The source_file_descriptors field below
  // is available in case you need source-retention options for files_to_generate.
  //
  // protoc guarantees that all proto_files will be written after the fields above, even though this
  // is not technically guaranteed by the protobuf wire format.  This theoretically could allow a
  // plugin to stream in the FileDescriptorProtos and handle them one by one rather than read the
  // entire set into memory at once.  However, as of this writing, this is not similarly optimized
  // on protoc's end -- it will store all fields in memory at once before sending them to the
  // plugin.
  //
  // Type names of fields and extensions in the FileDescriptorProto are always fully qualified.
  std::vector<FileDescriptorProto> proto_file;

  // File descriptors with all options, including source-retention options. These descriptors are
  // only provided for the files listed in files_to_generate.
  std::vector<FileDescriptorProto> source_file_descriptors;

  // The version number of protocol compiler.
  std::optional<Version> compiler_version;
};

// The plugin writes an encoded CodeGeneratorResponse to stdout.
struct CodeGeneratorResponse {
  struct File;

  static absl::StatusOr<CodeGeneratorResponse> Decode(absl::Span<uint8_t const> const& buffer);
  static tsdb2::io::Cord Encode(CodeGeneratorResponse const& value);

  auto tie() const { return std::tie(error, supported_features, minimum_edition, maximum_edition); }

  template <typename H>
  friend H AbslHashValue(H h, CodeGeneratorResponse const& value) {
    return H::combine(std::move(h), value.tie());
  }

  friend bool operator==(CodeGeneratorResponse const& lhs, CodeGeneratorResponse const& rhs) {
    return lhs.tie() == rhs.tie();
  }

  friend bool operator!=(CodeGeneratorResponse const& lhs, CodeGeneratorResponse const& rhs) {
    return lhs.tie() != rhs.tie();
  }

  // Error message.  If non-empty, code generation failed.  The plugin process should exit with
  // status code zero even if it reports an error in this way.
  //
  // This should be used to indicate errors in .proto files which prevent the code generator from
  // generating correct code.  Errors which indicate a problem in protoc itself -- such as the input
  // CodeGeneratorRequest being unparseable -- should be reported by writing a message to stderr and
  // exiting with a non-zero status code.
  std::optional<std::string> error;

  // A bitmask of supported features that the code generator supports. This is a bitwise "or" of
  // values from the Feature enum.
  std::optional<uint64_t> supported_features;

  // Sync with code_generator.h.
  enum class Feature {
    FEATURE_NONE = 0,
    FEATURE_PROTO3_OPTIONAL = 1,
    FEATURE_SUPPORTS_EDITIONS = 2,
  };

  // The minimum edition this plugin supports.  This will be treated as an Edition enum, but we want
  // to allow unknown values.  It should be specified according the edition enum value, *not* the
  // edition number.  Only takes effect for plugins that have FEATURE_SUPPORTS_EDITIONS set.
  std::optional<int32_t> minimum_edition;

  // The maximum edition this plugin supports.  This will be treated as an Edition enum, but we want
  // to allow unknown values.  It should be specified according the edition enum value, *not* the
  // edition number.  Only takes effect for plugins that have FEATURE_SUPPORTS_EDITIONS set.
  std::optional<int32_t> maximum_edition;

  // Represents a single generated file.
  struct File {
    static absl::StatusOr<File> Decode(absl::Span<uint8_t const> const& buffer);
    static tsdb2::io::Cord Encode(File const& value);

    auto tie() const { return std::tie(name, insertion_point, content, generated_code_info); }

    template <typename H>
    friend H AbslHashValue(H h, File const& value) {
      return H::combine(std::move(h), value.tie());
    }

    friend bool operator==(File const& lhs, File const& rhs) { return lhs.tie() == rhs.tie(); }

    friend bool operator!=(File const& lhs, File const& rhs) { return lhs.tie() != rhs.tie(); }

    // The file name, relative to the output directory.  The name must not contain "." or ".."
    // components and must be relative, not be absolute (so, the file cannot lie outside the output
    // directory).  "/" must be used as the path separator, not "\".
    //
    // If the name is omitted, the content will be appended to the previous file.  This allows the
    // generator to break large files into small chunks, and allows the generated text to be
    // streamed back to protoc so that large files need not reside completely in memory at one time.
    // Note that as of this writing protoc does not optimize for this -- it will read the entire
    // CodeGeneratorResponse before writing files to disk.
    std::optional<std::string> name;

    // If non-empty, indicates that the named file should already exist, and the content here is to
    // be inserted into that file at a defined insertion point.  This feature allows a code
    // generator to extend the output produced by another code generator.  The original generator
    // may provide insertion points by placing special annotations in the file that look like:
    //   @@protoc_insertion_point(NAME)
    // The annotation can have arbitrary text before and after it on the line, which allows it to be
    // placed in a comment.  NAME should be replaced with an identifier naming the point -- this is
    // what other generators will use as the insertion_point.  Code inserted at this point will be
    // placed immediately above the line containing the insertion point (thus multiple insertions to
    // the same point will come out in the order they were added). The double-@ is intended to make
    // it unlikely that the generated code could contain things that look like insertion points by
    // accident.
    //
    // For example, the C++ code generator places the following line in the .pb.h files that it
    // generates:
    //   // @@protoc_insertion_point(namespace_scope)
    // This line appears within the scope of the file's package namespace, but outside of any
    // particular class.  Another plugin can then specify the insertion_point "namespace_scope" to
    // generate additional classes or other declarations that should be placed in this scope.
    //
    // Note that if the line containing the insertion point begins with whitespace, the same
    // whitespace will be added to every line of the inserted text.  This is useful for languages
    // like Python, where indentation matters.  In these languages, the insertion point comment
    // should be indented the same amount as any inserted code will need to be in order to work
    // correctly in that context.
    //
    // The code generator that generates the initial file and the one which inserts into it must
    // both run as part of a single invocation of protoc. Code generators are executed in the order
    // in which they appear on the command line.
    //
    // If |insertion_point| is present, |name| must also be present.
    std::optional<std::string> insertion_point;

    // The file contents.
    std::optional<std::string> content;

    // Information describing the file content being inserted. If an insertion point is used, this
    // information will be appropriately offset and inserted into the code generation metadata for
    // the generated files.
    std::optional<GeneratedCodeInfo> generated_code_info;
  };

  std::vector<File> file;
};

}  // namespace compiler

}  // namespace protobuf
}  // namespace google

#endif  // __TSDB2_PROTO_PLUGIN_H__
