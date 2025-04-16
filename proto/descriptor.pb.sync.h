#ifndef __TSDB2_PROTO_DESCRIPTOR_PB_H__
#define __TSDB2_PROTO_DESCRIPTOR_PB_H__

#include "proto/runtime.h"

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

namespace google::protobuf {

struct DescriptorProto;
struct EnumDescriptorProto;
struct EnumOptions;
struct EnumValueDescriptorProto;
struct EnumValueOptions;
struct ExtensionRangeOptions;
struct FeatureSet;
struct FeatureSetDefaults;
struct FieldDescriptorProto;
struct FieldOptions;
struct FileDescriptorProto;
struct FileDescriptorSet;
struct FileOptions;
struct GeneratedCodeInfo;
struct MessageOptions;
struct MethodDescriptorProto;
struct MethodOptions;
struct OneofDescriptorProto;
struct OneofOptions;
struct ServiceDescriptorProto;
struct ServiceOptions;
struct SourceCodeInfo;
struct UninterpretedOption;

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

template <typename H>
inline H AbslHashValue(H h, Edition const& value) {
  return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
}

template <typename State>
inline State Tsdb2FingerprintValue(State state, Edition const& value) {
  return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, Edition* proto);

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                Edition const& proto);

inline bool AbslParseFlag(std::string_view const text, Edition* proto, std::string* const error) {
  return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
}

inline std::string AbslUnparseFlag(Edition const& proto) {
  return ::tsdb2::proto::text::Stringify(proto);
}

struct FeatureSet : public ::tsdb2::proto::Message {
  enum class FieldPresence {
    FIELD_PRESENCE_UNKNOWN = 0,
    EXPLICIT = 1,
    IMPLICIT = 2,
    LEGACY_REQUIRED = 3,
  };

  template <typename H>
  friend H AbslHashValue(H h, FieldPresence const& value) {
    return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, FieldPresence const& value) {
    return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
  }

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, FieldPresence* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         FieldPresence const& proto);

  friend bool AbslParseFlag(std::string_view const text, FieldPresence* proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(FieldPresence const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  enum class EnumType {
    ENUM_TYPE_UNKNOWN = 0,
    OPEN = 1,
    CLOSED = 2,
  };

  template <typename H>
  friend H AbslHashValue(H h, EnumType const& value) {
    return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, EnumType const& value) {
    return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
  }

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, EnumType* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         EnumType const& proto);

  friend bool AbslParseFlag(std::string_view const text, EnumType* proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(EnumType const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  enum class RepeatedFieldEncoding {
    REPEATED_FIELD_ENCODING_UNKNOWN = 0,
    PACKED = 1,
    EXPANDED = 2,
  };

  template <typename H>
  friend H AbslHashValue(H h, RepeatedFieldEncoding const& value) {
    return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, RepeatedFieldEncoding const& value) {
    return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
  }

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        RepeatedFieldEncoding* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         RepeatedFieldEncoding const& proto);

  friend bool AbslParseFlag(std::string_view const text, RepeatedFieldEncoding* proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(RepeatedFieldEncoding const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  enum class Utf8Validation {
    UTF8_VALIDATION_UNKNOWN = 0,
    VERIFY = 2,
    NONE = 3,
  };

  template <typename H>
  friend H AbslHashValue(H h, Utf8Validation const& value) {
    return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, Utf8Validation const& value) {
    return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
  }

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        Utf8Validation* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         Utf8Validation const& proto);

  friend bool AbslParseFlag(std::string_view const text, Utf8Validation* proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(Utf8Validation const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  enum class MessageEncoding {
    MESSAGE_ENCODING_UNKNOWN = 0,
    LENGTH_PREFIXED = 1,
    DELIMITED = 2,
  };

  template <typename H>
  friend H AbslHashValue(H h, MessageEncoding const& value) {
    return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, MessageEncoding const& value) {
    return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
  }

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        MessageEncoding* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         MessageEncoding const& proto);

  friend bool AbslParseFlag(std::string_view const text, MessageEncoding* proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(MessageEncoding const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  enum class JsonFormat {
    JSON_FORMAT_UNKNOWN = 0,
    ALLOW = 1,
    LEGACY_BEST_EFFORT = 2,
  };

  template <typename H>
  friend H AbslHashValue(H h, JsonFormat const& value) {
    return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, JsonFormat const& value) {
    return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
  }

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, JsonFormat* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         JsonFormat const& proto);

  friend bool AbslParseFlag(std::string_view const text, JsonFormat* proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(JsonFormat const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  static ::absl::StatusOr<FeatureSet> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(FeatureSet const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, FeatureSet* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         FeatureSet const& proto);

  static auto Tie(FeatureSet const& proto) {
    return ::tsdb2::proto::Tie(proto.field_presence, proto.enum_type, proto.repeated_field_encoding,
                               proto.utf8_validation, proto.message_encoding, proto.json_format,
                               proto.extension_data);
  }

  template <typename H>
  friend H AbslHashValue(H h, FeatureSet const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, FeatureSet const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, FeatureSet* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(FeatureSet const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(FeatureSet const& lhs, FeatureSet const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(FeatureSet const& lhs, FeatureSet const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(FeatureSet const& lhs, FeatureSet const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(FeatureSet const& lhs, FeatureSet const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(FeatureSet const& lhs, FeatureSet const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(FeatureSet const& lhs, FeatureSet const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::optional<::google::protobuf::FeatureSet::FieldPresence> field_presence;
  std::optional<::google::protobuf::FeatureSet::EnumType> enum_type;
  std::optional<::google::protobuf::FeatureSet::RepeatedFieldEncoding> repeated_field_encoding;
  std::optional<::google::protobuf::FeatureSet::Utf8Validation> utf8_validation;
  std::optional<::google::protobuf::FeatureSet::MessageEncoding> message_encoding;
  std::optional<::google::protobuf::FeatureSet::JsonFormat> json_format;
  ::tsdb2::proto::ExtensionData extension_data;
};

struct ExtensionRangeOptions : public ::tsdb2::proto::Message {
  struct Declaration;

  enum class VerificationState {
    DECLARATION = 0,
    UNVERIFIED = 1,
  };

  template <typename H>
  friend H AbslHashValue(H h, VerificationState const& value) {
    return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, VerificationState const& value) {
    return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
  }

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        VerificationState* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         VerificationState const& proto);

  friend bool AbslParseFlag(std::string_view const text, VerificationState* proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(VerificationState const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  struct Declaration : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<Declaration> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(Declaration const& proto);

    friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, Declaration* proto);

    friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                           Declaration const& proto);

    static auto Tie(Declaration const& proto) {
      return ::tsdb2::proto::Tie(proto.number, proto.full_name, proto.type, proto.reserved,
                                 proto.repeated);
    }

    template <typename H>
    friend H AbslHashValue(H h, Declaration const& proto) {
      return H::combine(std::move(h), Tie(proto));
    }

    template <typename State>
    friend State Tsdb2FingerprintValue(State state, Declaration const& proto) {
      return State::Combine(std::move(state), Tie(proto));
    }

    friend bool AbslParseFlag(std::string_view const text, Declaration* const proto,
                              std::string* const error) {
      return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
    }

    friend std::string AbslUnparseFlag(Declaration const& proto) {
      return ::tsdb2::proto::text::Stringify(proto);
    }

    friend bool operator==(Declaration const& lhs, Declaration const& rhs) {
      return Tie(lhs) == Tie(rhs);
    }
    friend bool operator!=(Declaration const& lhs, Declaration const& rhs) {
      return Tie(lhs) != Tie(rhs);
    }
    friend bool operator<(Declaration const& lhs, Declaration const& rhs) {
      return Tie(lhs) < Tie(rhs);
    }
    friend bool operator<=(Declaration const& lhs, Declaration const& rhs) {
      return Tie(lhs) <= Tie(rhs);
    }
    friend bool operator>(Declaration const& lhs, Declaration const& rhs) {
      return Tie(lhs) > Tie(rhs);
    }
    friend bool operator>=(Declaration const& lhs, Declaration const& rhs) {
      return Tie(lhs) >= Tie(rhs);
    }

    std::optional<int32_t> number;
    std::optional<std::string> full_name;
    std::optional<std::string> type;
    std::optional<bool> reserved;
    std::optional<bool> repeated;
  };

  static ::absl::StatusOr<ExtensionRangeOptions> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(ExtensionRangeOptions const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        ExtensionRangeOptions* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         ExtensionRangeOptions const& proto);

  static auto Tie(ExtensionRangeOptions const& proto) {
    return ::tsdb2::proto::Tie(proto.uninterpreted_option, proto.declaration,
                               ::tsdb2::proto::OptionalSubMessageRef(proto.features),
                               proto.verification, proto.extension_data);
  }

  template <typename H>
  friend H AbslHashValue(H h, ExtensionRangeOptions const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, ExtensionRangeOptions const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, ExtensionRangeOptions* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(ExtensionRangeOptions const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(ExtensionRangeOptions const& lhs, ExtensionRangeOptions const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(ExtensionRangeOptions const& lhs, ExtensionRangeOptions const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(ExtensionRangeOptions const& lhs, ExtensionRangeOptions const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(ExtensionRangeOptions const& lhs, ExtensionRangeOptions const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(ExtensionRangeOptions const& lhs, ExtensionRangeOptions const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(ExtensionRangeOptions const& lhs, ExtensionRangeOptions const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::vector<::google::protobuf::UninterpretedOption> uninterpreted_option;
  std::vector<::google::protobuf::ExtensionRangeOptions::Declaration> declaration;
  std::optional<::google::protobuf::FeatureSet> features;
  ::google::protobuf::ExtensionRangeOptions::VerificationState verification{
      ::google::protobuf::ExtensionRangeOptions::VerificationState::UNVERIFIED};
  ::tsdb2::proto::ExtensionData extension_data;
};

struct MessageOptions : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<MessageOptions> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(MessageOptions const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        MessageOptions* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         MessageOptions const& proto);

  static auto Tie(MessageOptions const& proto) {
    return ::tsdb2::proto::Tie(proto.message_set_wire_format, proto.no_standard_descriptor_accessor,
                               proto.deprecated, proto.map_entry,
                               proto.deprecated_legacy_json_field_conflicts,
                               ::tsdb2::proto::OptionalSubMessageRef(proto.features),
                               proto.uninterpreted_option, proto.extension_data);
  }

  template <typename H>
  friend H AbslHashValue(H h, MessageOptions const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, MessageOptions const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, MessageOptions* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(MessageOptions const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(MessageOptions const& lhs, MessageOptions const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(MessageOptions const& lhs, MessageOptions const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(MessageOptions const& lhs, MessageOptions const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(MessageOptions const& lhs, MessageOptions const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(MessageOptions const& lhs, MessageOptions const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(MessageOptions const& lhs, MessageOptions const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  bool message_set_wire_format{false};
  bool no_standard_descriptor_accessor{false};
  bool deprecated{false};
  std::optional<bool> map_entry;
  ABSL_DEPRECATED("") std::optional<bool> deprecated_legacy_json_field_conflicts;
  std::optional<::google::protobuf::FeatureSet> features;
  std::vector<::google::protobuf::UninterpretedOption> uninterpreted_option;
  ::tsdb2::proto::ExtensionData extension_data;
};

struct DescriptorProto : public ::tsdb2::proto::Message {
  struct ExtensionRange;
  struct ReservedRange;

  struct ExtensionRange : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<ExtensionRange> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(ExtensionRange const& proto);

    friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                          ExtensionRange* proto);

    friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                           ExtensionRange const& proto);

    static auto Tie(ExtensionRange const& proto) {
      return ::tsdb2::proto::Tie(proto.start, proto.end,
                                 ::tsdb2::proto::OptionalSubMessageRef(proto.options));
    }

    template <typename H>
    friend H AbslHashValue(H h, ExtensionRange const& proto) {
      return H::combine(std::move(h), Tie(proto));
    }

    template <typename State>
    friend State Tsdb2FingerprintValue(State state, ExtensionRange const& proto) {
      return State::Combine(std::move(state), Tie(proto));
    }

    friend bool AbslParseFlag(std::string_view const text, ExtensionRange* const proto,
                              std::string* const error) {
      return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
    }

    friend std::string AbslUnparseFlag(ExtensionRange const& proto) {
      return ::tsdb2::proto::text::Stringify(proto);
    }

    friend bool operator==(ExtensionRange const& lhs, ExtensionRange const& rhs) {
      return Tie(lhs) == Tie(rhs);
    }
    friend bool operator!=(ExtensionRange const& lhs, ExtensionRange const& rhs) {
      return Tie(lhs) != Tie(rhs);
    }
    friend bool operator<(ExtensionRange const& lhs, ExtensionRange const& rhs) {
      return Tie(lhs) < Tie(rhs);
    }
    friend bool operator<=(ExtensionRange const& lhs, ExtensionRange const& rhs) {
      return Tie(lhs) <= Tie(rhs);
    }
    friend bool operator>(ExtensionRange const& lhs, ExtensionRange const& rhs) {
      return Tie(lhs) > Tie(rhs);
    }
    friend bool operator>=(ExtensionRange const& lhs, ExtensionRange const& rhs) {
      return Tie(lhs) >= Tie(rhs);
    }

    std::optional<int32_t> start;
    std::optional<int32_t> end;
    std::optional<::google::protobuf::ExtensionRangeOptions> options;
  };

  struct ReservedRange : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<ReservedRange> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(ReservedRange const& proto);

    friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                          ReservedRange* proto);

    friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                           ReservedRange const& proto);

    static auto Tie(ReservedRange const& proto) {
      return ::tsdb2::proto::Tie(proto.start, proto.end);
    }

    template <typename H>
    friend H AbslHashValue(H h, ReservedRange const& proto) {
      return H::combine(std::move(h), Tie(proto));
    }

    template <typename State>
    friend State Tsdb2FingerprintValue(State state, ReservedRange const& proto) {
      return State::Combine(std::move(state), Tie(proto));
    }

    friend bool AbslParseFlag(std::string_view const text, ReservedRange* const proto,
                              std::string* const error) {
      return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
    }

    friend std::string AbslUnparseFlag(ReservedRange const& proto) {
      return ::tsdb2::proto::text::Stringify(proto);
    }

    friend bool operator==(ReservedRange const& lhs, ReservedRange const& rhs) {
      return Tie(lhs) == Tie(rhs);
    }
    friend bool operator!=(ReservedRange const& lhs, ReservedRange const& rhs) {
      return Tie(lhs) != Tie(rhs);
    }
    friend bool operator<(ReservedRange const& lhs, ReservedRange const& rhs) {
      return Tie(lhs) < Tie(rhs);
    }
    friend bool operator<=(ReservedRange const& lhs, ReservedRange const& rhs) {
      return Tie(lhs) <= Tie(rhs);
    }
    friend bool operator>(ReservedRange const& lhs, ReservedRange const& rhs) {
      return Tie(lhs) > Tie(rhs);
    }
    friend bool operator>=(ReservedRange const& lhs, ReservedRange const& rhs) {
      return Tie(lhs) >= Tie(rhs);
    }

    std::optional<int32_t> start;
    std::optional<int32_t> end;
  };

  static ::absl::StatusOr<DescriptorProto> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(DescriptorProto const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        DescriptorProto* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         DescriptorProto const& proto);

  static auto Tie(DescriptorProto const& proto) {
    return ::tsdb2::proto::Tie(proto.name, proto.field, proto.extension, proto.nested_type,
                               proto.enum_type, proto.extension_range, proto.oneof_decl,
                               ::tsdb2::proto::OptionalSubMessageRef(proto.options),
                               proto.reserved_range, proto.reserved_name);
  }

  template <typename H>
  friend H AbslHashValue(H h, DescriptorProto const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, DescriptorProto const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, DescriptorProto* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(DescriptorProto const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(DescriptorProto const& lhs, DescriptorProto const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(DescriptorProto const& lhs, DescriptorProto const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(DescriptorProto const& lhs, DescriptorProto const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(DescriptorProto const& lhs, DescriptorProto const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(DescriptorProto const& lhs, DescriptorProto const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(DescriptorProto const& lhs, DescriptorProto const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

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

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, EnumOptions* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         EnumOptions const& proto);

  static auto Tie(EnumOptions const& proto) {
    return ::tsdb2::proto::Tie(proto.allow_alias, proto.deprecated,
                               proto.deprecated_legacy_json_field_conflicts,
                               ::tsdb2::proto::OptionalSubMessageRef(proto.features),
                               proto.uninterpreted_option, proto.extension_data);
  }

  template <typename H>
  friend H AbslHashValue(H h, EnumOptions const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, EnumOptions const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, EnumOptions* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(EnumOptions const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(EnumOptions const& lhs, EnumOptions const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(EnumOptions const& lhs, EnumOptions const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(EnumOptions const& lhs, EnumOptions const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(EnumOptions const& lhs, EnumOptions const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(EnumOptions const& lhs, EnumOptions const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(EnumOptions const& lhs, EnumOptions const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::optional<bool> allow_alias;
  bool deprecated{false};
  ABSL_DEPRECATED("") std::optional<bool> deprecated_legacy_json_field_conflicts;
  std::optional<::google::protobuf::FeatureSet> features;
  std::vector<::google::protobuf::UninterpretedOption> uninterpreted_option;
  ::tsdb2::proto::ExtensionData extension_data;
};

struct EnumDescriptorProto : public ::tsdb2::proto::Message {
  struct EnumReservedRange;

  struct EnumReservedRange : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<EnumReservedRange> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(EnumReservedRange const& proto);

    friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                          EnumReservedRange* proto);

    friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                           EnumReservedRange const& proto);

    static auto Tie(EnumReservedRange const& proto) {
      return ::tsdb2::proto::Tie(proto.start, proto.end);
    }

    template <typename H>
    friend H AbslHashValue(H h, EnumReservedRange const& proto) {
      return H::combine(std::move(h), Tie(proto));
    }

    template <typename State>
    friend State Tsdb2FingerprintValue(State state, EnumReservedRange const& proto) {
      return State::Combine(std::move(state), Tie(proto));
    }

    friend bool AbslParseFlag(std::string_view const text, EnumReservedRange* const proto,
                              std::string* const error) {
      return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
    }

    friend std::string AbslUnparseFlag(EnumReservedRange const& proto) {
      return ::tsdb2::proto::text::Stringify(proto);
    }

    friend bool operator==(EnumReservedRange const& lhs, EnumReservedRange const& rhs) {
      return Tie(lhs) == Tie(rhs);
    }
    friend bool operator!=(EnumReservedRange const& lhs, EnumReservedRange const& rhs) {
      return Tie(lhs) != Tie(rhs);
    }
    friend bool operator<(EnumReservedRange const& lhs, EnumReservedRange const& rhs) {
      return Tie(lhs) < Tie(rhs);
    }
    friend bool operator<=(EnumReservedRange const& lhs, EnumReservedRange const& rhs) {
      return Tie(lhs) <= Tie(rhs);
    }
    friend bool operator>(EnumReservedRange const& lhs, EnumReservedRange const& rhs) {
      return Tie(lhs) > Tie(rhs);
    }
    friend bool operator>=(EnumReservedRange const& lhs, EnumReservedRange const& rhs) {
      return Tie(lhs) >= Tie(rhs);
    }

    std::optional<int32_t> start;
    std::optional<int32_t> end;
  };

  static ::absl::StatusOr<EnumDescriptorProto> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(EnumDescriptorProto const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        EnumDescriptorProto* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         EnumDescriptorProto const& proto);

  static auto Tie(EnumDescriptorProto const& proto) {
    return ::tsdb2::proto::Tie(proto.name, proto.value,
                               ::tsdb2::proto::OptionalSubMessageRef(proto.options),
                               proto.reserved_range, proto.reserved_name);
  }

  template <typename H>
  friend H AbslHashValue(H h, EnumDescriptorProto const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, EnumDescriptorProto const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, EnumDescriptorProto* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(EnumDescriptorProto const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(EnumDescriptorProto const& lhs, EnumDescriptorProto const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(EnumDescriptorProto const& lhs, EnumDescriptorProto const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(EnumDescriptorProto const& lhs, EnumDescriptorProto const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(EnumDescriptorProto const& lhs, EnumDescriptorProto const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(EnumDescriptorProto const& lhs, EnumDescriptorProto const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(EnumDescriptorProto const& lhs, EnumDescriptorProto const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

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

  template <typename H>
  friend H AbslHashValue(H h, CType const& value) {
    return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, CType const& value) {
    return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
  }

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, CType* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         CType const& proto);

  friend bool AbslParseFlag(std::string_view const text, CType* proto, std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(CType const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  enum class JSType {
    JS_NORMAL = 0,
    JS_STRING = 1,
    JS_NUMBER = 2,
  };

  template <typename H>
  friend H AbslHashValue(H h, JSType const& value) {
    return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, JSType const& value) {
    return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
  }

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, JSType* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         JSType const& proto);

  friend bool AbslParseFlag(std::string_view const text, JSType* proto, std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(JSType const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  enum class OptionRetention {
    RETENTION_UNKNOWN = 0,
    RETENTION_RUNTIME = 1,
    RETENTION_SOURCE = 2,
  };

  template <typename H>
  friend H AbslHashValue(H h, OptionRetention const& value) {
    return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, OptionRetention const& value) {
    return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
  }

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        OptionRetention* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         OptionRetention const& proto);

  friend bool AbslParseFlag(std::string_view const text, OptionRetention* proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(OptionRetention const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

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

  template <typename H>
  friend H AbslHashValue(H h, OptionTargetType const& value) {
    return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, OptionTargetType const& value) {
    return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
  }

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        OptionTargetType* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         OptionTargetType const& proto);

  friend bool AbslParseFlag(std::string_view const text, OptionTargetType* proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(OptionTargetType const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  struct EditionDefault : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<EditionDefault> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(EditionDefault const& proto);

    friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                          EditionDefault* proto);

    friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                           EditionDefault const& proto);

    static auto Tie(EditionDefault const& proto) {
      return ::tsdb2::proto::Tie(proto.edition, proto.value);
    }

    template <typename H>
    friend H AbslHashValue(H h, EditionDefault const& proto) {
      return H::combine(std::move(h), Tie(proto));
    }

    template <typename State>
    friend State Tsdb2FingerprintValue(State state, EditionDefault const& proto) {
      return State::Combine(std::move(state), Tie(proto));
    }

    friend bool AbslParseFlag(std::string_view const text, EditionDefault* const proto,
                              std::string* const error) {
      return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
    }

    friend std::string AbslUnparseFlag(EditionDefault const& proto) {
      return ::tsdb2::proto::text::Stringify(proto);
    }

    friend bool operator==(EditionDefault const& lhs, EditionDefault const& rhs) {
      return Tie(lhs) == Tie(rhs);
    }
    friend bool operator!=(EditionDefault const& lhs, EditionDefault const& rhs) {
      return Tie(lhs) != Tie(rhs);
    }
    friend bool operator<(EditionDefault const& lhs, EditionDefault const& rhs) {
      return Tie(lhs) < Tie(rhs);
    }
    friend bool operator<=(EditionDefault const& lhs, EditionDefault const& rhs) {
      return Tie(lhs) <= Tie(rhs);
    }
    friend bool operator>(EditionDefault const& lhs, EditionDefault const& rhs) {
      return Tie(lhs) > Tie(rhs);
    }
    friend bool operator>=(EditionDefault const& lhs, EditionDefault const& rhs) {
      return Tie(lhs) >= Tie(rhs);
    }

    std::optional<::google::protobuf::Edition> edition;
    std::optional<std::string> value;
  };

  struct FeatureSupport : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<FeatureSupport> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(FeatureSupport const& proto);

    friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                          FeatureSupport* proto);

    friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                           FeatureSupport const& proto);

    static auto Tie(FeatureSupport const& proto) {
      return ::tsdb2::proto::Tie(proto.edition_introduced, proto.edition_deprecated,
                                 proto.deprecation_warning, proto.edition_removed);
    }

    template <typename H>
    friend H AbslHashValue(H h, FeatureSupport const& proto) {
      return H::combine(std::move(h), Tie(proto));
    }

    template <typename State>
    friend State Tsdb2FingerprintValue(State state, FeatureSupport const& proto) {
      return State::Combine(std::move(state), Tie(proto));
    }

    friend bool AbslParseFlag(std::string_view const text, FeatureSupport* const proto,
                              std::string* const error) {
      return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
    }

    friend std::string AbslUnparseFlag(FeatureSupport const& proto) {
      return ::tsdb2::proto::text::Stringify(proto);
    }

    friend bool operator==(FeatureSupport const& lhs, FeatureSupport const& rhs) {
      return Tie(lhs) == Tie(rhs);
    }
    friend bool operator!=(FeatureSupport const& lhs, FeatureSupport const& rhs) {
      return Tie(lhs) != Tie(rhs);
    }
    friend bool operator<(FeatureSupport const& lhs, FeatureSupport const& rhs) {
      return Tie(lhs) < Tie(rhs);
    }
    friend bool operator<=(FeatureSupport const& lhs, FeatureSupport const& rhs) {
      return Tie(lhs) <= Tie(rhs);
    }
    friend bool operator>(FeatureSupport const& lhs, FeatureSupport const& rhs) {
      return Tie(lhs) > Tie(rhs);
    }
    friend bool operator>=(FeatureSupport const& lhs, FeatureSupport const& rhs) {
      return Tie(lhs) >= Tie(rhs);
    }

    std::optional<::google::protobuf::Edition> edition_introduced;
    std::optional<::google::protobuf::Edition> edition_deprecated;
    std::optional<std::string> deprecation_warning;
    std::optional<::google::protobuf::Edition> edition_removed;
  };

  static ::absl::StatusOr<FieldOptions> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(FieldOptions const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, FieldOptions* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         FieldOptions const& proto);

  static auto Tie(FieldOptions const& proto) {
    return ::tsdb2::proto::Tie(
        proto.ctype, proto.packed, proto.jstype, proto.lazy, proto.unverified_lazy,
        proto.deprecated, proto.weak, proto.debug_redact, proto.retention, proto.targets,
        proto.edition_defaults, ::tsdb2::proto::OptionalSubMessageRef(proto.features),
        ::tsdb2::proto::OptionalSubMessageRef(proto.feature_support), proto.uninterpreted_option,
        proto.extension_data);
  }

  template <typename H>
  friend H AbslHashValue(H h, FieldOptions const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, FieldOptions const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, FieldOptions* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(FieldOptions const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(FieldOptions const& lhs, FieldOptions const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(FieldOptions const& lhs, FieldOptions const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(FieldOptions const& lhs, FieldOptions const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(FieldOptions const& lhs, FieldOptions const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(FieldOptions const& lhs, FieldOptions const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(FieldOptions const& lhs, FieldOptions const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

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
  ::tsdb2::proto::ExtensionData extension_data;
};

struct EnumValueOptions : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<EnumValueOptions> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(EnumValueOptions const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        EnumValueOptions* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         EnumValueOptions const& proto);

  static auto Tie(EnumValueOptions const& proto) {
    return ::tsdb2::proto::Tie(
        proto.deprecated, ::tsdb2::proto::OptionalSubMessageRef(proto.features), proto.debug_redact,
        ::tsdb2::proto::OptionalSubMessageRef(proto.feature_support), proto.uninterpreted_option,
        proto.extension_data);
  }

  template <typename H>
  friend H AbslHashValue(H h, EnumValueOptions const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, EnumValueOptions const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, EnumValueOptions* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(EnumValueOptions const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(EnumValueOptions const& lhs, EnumValueOptions const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(EnumValueOptions const& lhs, EnumValueOptions const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(EnumValueOptions const& lhs, EnumValueOptions const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(EnumValueOptions const& lhs, EnumValueOptions const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(EnumValueOptions const& lhs, EnumValueOptions const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(EnumValueOptions const& lhs, EnumValueOptions const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  bool deprecated{false};
  std::optional<::google::protobuf::FeatureSet> features;
  bool debug_redact{false};
  std::optional<::google::protobuf::FieldOptions::FeatureSupport> feature_support;
  std::vector<::google::protobuf::UninterpretedOption> uninterpreted_option;
  ::tsdb2::proto::ExtensionData extension_data;
};

struct EnumValueDescriptorProto : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<EnumValueDescriptorProto> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(EnumValueDescriptorProto const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        EnumValueDescriptorProto* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         EnumValueDescriptorProto const& proto);

  static auto Tie(EnumValueDescriptorProto const& proto) {
    return ::tsdb2::proto::Tie(proto.name, proto.number,
                               ::tsdb2::proto::OptionalSubMessageRef(proto.options));
  }

  template <typename H>
  friend H AbslHashValue(H h, EnumValueDescriptorProto const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, EnumValueDescriptorProto const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, EnumValueDescriptorProto* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(EnumValueDescriptorProto const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(EnumValueDescriptorProto const& lhs, EnumValueDescriptorProto const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(EnumValueDescriptorProto const& lhs, EnumValueDescriptorProto const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(EnumValueDescriptorProto const& lhs, EnumValueDescriptorProto const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(EnumValueDescriptorProto const& lhs, EnumValueDescriptorProto const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(EnumValueDescriptorProto const& lhs, EnumValueDescriptorProto const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(EnumValueDescriptorProto const& lhs, EnumValueDescriptorProto const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::optional<std::string> name;
  std::optional<int32_t> number;
  std::optional<::google::protobuf::EnumValueOptions> options;
};

struct FeatureSetDefaults : public ::tsdb2::proto::Message {
  struct FeatureSetEditionDefault;

  struct FeatureSetEditionDefault : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<FeatureSetEditionDefault> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(FeatureSetEditionDefault const& proto);

    friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                          FeatureSetEditionDefault* proto);

    friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                           FeatureSetEditionDefault const& proto);

    static auto Tie(FeatureSetEditionDefault const& proto) {
      return ::tsdb2::proto::Tie(proto.edition,
                                 ::tsdb2::proto::OptionalSubMessageRef(proto.overridable_features),
                                 ::tsdb2::proto::OptionalSubMessageRef(proto.fixed_features));
    }

    template <typename H>
    friend H AbslHashValue(H h, FeatureSetEditionDefault const& proto) {
      return H::combine(std::move(h), Tie(proto));
    }

    template <typename State>
    friend State Tsdb2FingerprintValue(State state, FeatureSetEditionDefault const& proto) {
      return State::Combine(std::move(state), Tie(proto));
    }

    friend bool AbslParseFlag(std::string_view const text, FeatureSetEditionDefault* const proto,
                              std::string* const error) {
      return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
    }

    friend std::string AbslUnparseFlag(FeatureSetEditionDefault const& proto) {
      return ::tsdb2::proto::text::Stringify(proto);
    }

    friend bool operator==(FeatureSetEditionDefault const& lhs,
                           FeatureSetEditionDefault const& rhs) {
      return Tie(lhs) == Tie(rhs);
    }
    friend bool operator!=(FeatureSetEditionDefault const& lhs,
                           FeatureSetEditionDefault const& rhs) {
      return Tie(lhs) != Tie(rhs);
    }
    friend bool operator<(FeatureSetEditionDefault const& lhs,
                          FeatureSetEditionDefault const& rhs) {
      return Tie(lhs) < Tie(rhs);
    }
    friend bool operator<=(FeatureSetEditionDefault const& lhs,
                           FeatureSetEditionDefault const& rhs) {
      return Tie(lhs) <= Tie(rhs);
    }
    friend bool operator>(FeatureSetEditionDefault const& lhs,
                          FeatureSetEditionDefault const& rhs) {
      return Tie(lhs) > Tie(rhs);
    }
    friend bool operator>=(FeatureSetEditionDefault const& lhs,
                           FeatureSetEditionDefault const& rhs) {
      return Tie(lhs) >= Tie(rhs);
    }

    std::optional<::google::protobuf::Edition> edition;
    std::optional<::google::protobuf::FeatureSet> overridable_features;
    std::optional<::google::protobuf::FeatureSet> fixed_features;
  };

  static ::absl::StatusOr<FeatureSetDefaults> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(FeatureSetDefaults const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        FeatureSetDefaults* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         FeatureSetDefaults const& proto);

  static auto Tie(FeatureSetDefaults const& proto) {
    return ::tsdb2::proto::Tie(proto.defaults, proto.minimum_edition, proto.maximum_edition);
  }

  template <typename H>
  friend H AbslHashValue(H h, FeatureSetDefaults const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, FeatureSetDefaults const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, FeatureSetDefaults* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(FeatureSetDefaults const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(FeatureSetDefaults const& lhs, FeatureSetDefaults const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(FeatureSetDefaults const& lhs, FeatureSetDefaults const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(FeatureSetDefaults const& lhs, FeatureSetDefaults const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(FeatureSetDefaults const& lhs, FeatureSetDefaults const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(FeatureSetDefaults const& lhs, FeatureSetDefaults const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(FeatureSetDefaults const& lhs, FeatureSetDefaults const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

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

  template <typename H>
  friend H AbslHashValue(H h, Type const& value) {
    return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, Type const& value) {
    return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
  }

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, Type* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         Type const& proto);

  friend bool AbslParseFlag(std::string_view const text, Type* proto, std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(Type const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  enum class Label {
    LABEL_OPTIONAL = 1,
    LABEL_REPEATED = 3,
    LABEL_REQUIRED = 2,
  };

  template <typename H>
  friend H AbslHashValue(H h, Label const& value) {
    return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, Label const& value) {
    return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
  }

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, Label* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         Label const& proto);

  friend bool AbslParseFlag(std::string_view const text, Label* proto, std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(Label const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  static ::absl::StatusOr<FieldDescriptorProto> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(FieldDescriptorProto const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        FieldDescriptorProto* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         FieldDescriptorProto const& proto);

  static auto Tie(FieldDescriptorProto const& proto) {
    return ::tsdb2::proto::Tie(
        proto.name, proto.number, proto.label, proto.type, proto.type_name, proto.extendee,
        proto.default_value, proto.oneof_index, proto.json_name,
        ::tsdb2::proto::OptionalSubMessageRef(proto.options), proto.proto3_optional);
  }

  template <typename H>
  friend H AbslHashValue(H h, FieldDescriptorProto const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, FieldDescriptorProto const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, FieldDescriptorProto* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(FieldDescriptorProto const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(FieldDescriptorProto const& lhs, FieldDescriptorProto const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(FieldDescriptorProto const& lhs, FieldDescriptorProto const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(FieldDescriptorProto const& lhs, FieldDescriptorProto const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(FieldDescriptorProto const& lhs, FieldDescriptorProto const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(FieldDescriptorProto const& lhs, FieldDescriptorProto const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(FieldDescriptorProto const& lhs, FieldDescriptorProto const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

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

  template <typename H>
  friend H AbslHashValue(H h, OptimizeMode const& value) {
    return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, OptimizeMode const& value) {
    return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
  }

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, OptimizeMode* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         OptimizeMode const& proto);

  friend bool AbslParseFlag(std::string_view const text, OptimizeMode* proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(OptimizeMode const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  static ::absl::StatusOr<FileOptions> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(FileOptions const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, FileOptions* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         FileOptions const& proto);

  static auto Tie(FileOptions const& proto) {
    return ::tsdb2::proto::Tie(
        proto.java_package, proto.java_outer_classname, proto.java_multiple_files,
        proto.java_generate_equals_and_hash, proto.java_string_check_utf8, proto.optimize_for,
        proto.go_package, proto.cc_generic_services, proto.java_generic_services,
        proto.py_generic_services, proto.deprecated, proto.cc_enable_arenas,
        proto.objc_class_prefix, proto.csharp_namespace, proto.swift_prefix, proto.php_class_prefix,
        proto.php_namespace, proto.php_metadata_namespace, proto.ruby_package,
        ::tsdb2::proto::OptionalSubMessageRef(proto.features), proto.uninterpreted_option,
        proto.extension_data);
  }

  template <typename H>
  friend H AbslHashValue(H h, FileOptions const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, FileOptions const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, FileOptions* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(FileOptions const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(FileOptions const& lhs, FileOptions const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(FileOptions const& lhs, FileOptions const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(FileOptions const& lhs, FileOptions const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(FileOptions const& lhs, FileOptions const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(FileOptions const& lhs, FileOptions const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(FileOptions const& lhs, FileOptions const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::optional<std::string> java_package;
  std::optional<std::string> java_outer_classname;
  bool java_multiple_files{false};
  ABSL_DEPRECATED("") std::optional<bool> java_generate_equals_and_hash;
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
  ::tsdb2::proto::ExtensionData extension_data;
};

struct SourceCodeInfo : public ::tsdb2::proto::Message {
  struct Location;

  struct Location : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<Location> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(Location const& proto);

    friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, Location* proto);

    friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                           Location const& proto);

    static auto Tie(Location const& proto) {
      return ::tsdb2::proto::Tie(proto.path, proto.span, proto.leading_comments,
                                 proto.trailing_comments, proto.leading_detached_comments);
    }

    template <typename H>
    friend H AbslHashValue(H h, Location const& proto) {
      return H::combine(std::move(h), Tie(proto));
    }

    template <typename State>
    friend State Tsdb2FingerprintValue(State state, Location const& proto) {
      return State::Combine(std::move(state), Tie(proto));
    }

    friend bool AbslParseFlag(std::string_view const text, Location* const proto,
                              std::string* const error) {
      return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
    }

    friend std::string AbslUnparseFlag(Location const& proto) {
      return ::tsdb2::proto::text::Stringify(proto);
    }

    friend bool operator==(Location const& lhs, Location const& rhs) {
      return Tie(lhs) == Tie(rhs);
    }
    friend bool operator!=(Location const& lhs, Location const& rhs) {
      return Tie(lhs) != Tie(rhs);
    }
    friend bool operator<(Location const& lhs, Location const& rhs) { return Tie(lhs) < Tie(rhs); }
    friend bool operator<=(Location const& lhs, Location const& rhs) {
      return Tie(lhs) <= Tie(rhs);
    }
    friend bool operator>(Location const& lhs, Location const& rhs) { return Tie(lhs) > Tie(rhs); }
    friend bool operator>=(Location const& lhs, Location const& rhs) {
      return Tie(lhs) >= Tie(rhs);
    }

    std::vector<int32_t> path;
    std::vector<int32_t> span;
    std::optional<std::string> leading_comments;
    std::optional<std::string> trailing_comments;
    std::vector<std::string> leading_detached_comments;
  };

  static ::absl::StatusOr<SourceCodeInfo> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(SourceCodeInfo const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        SourceCodeInfo* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         SourceCodeInfo const& proto);

  static auto Tie(SourceCodeInfo const& proto) { return ::tsdb2::proto::Tie(proto.location); }

  template <typename H>
  friend H AbslHashValue(H h, SourceCodeInfo const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, SourceCodeInfo const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, SourceCodeInfo* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(SourceCodeInfo const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(SourceCodeInfo const& lhs, SourceCodeInfo const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(SourceCodeInfo const& lhs, SourceCodeInfo const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(SourceCodeInfo const& lhs, SourceCodeInfo const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(SourceCodeInfo const& lhs, SourceCodeInfo const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(SourceCodeInfo const& lhs, SourceCodeInfo const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(SourceCodeInfo const& lhs, SourceCodeInfo const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::vector<::google::protobuf::SourceCodeInfo::Location> location;
};

struct FileDescriptorProto : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<FileDescriptorProto> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(FileDescriptorProto const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        FileDescriptorProto* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         FileDescriptorProto const& proto);

  static auto Tie(FileDescriptorProto const& proto) {
    return ::tsdb2::proto::Tie(
        proto.name, proto.package, proto.dependency, proto.public_dependency, proto.weak_dependency,
        proto.message_type, proto.enum_type, proto.service, proto.extension,
        ::tsdb2::proto::OptionalSubMessageRef(proto.options),
        ::tsdb2::proto::OptionalSubMessageRef(proto.source_code_info), proto.syntax, proto.edition);
  }

  template <typename H>
  friend H AbslHashValue(H h, FileDescriptorProto const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, FileDescriptorProto const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, FileDescriptorProto* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(FileDescriptorProto const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(FileDescriptorProto const& lhs, FileDescriptorProto const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(FileDescriptorProto const& lhs, FileDescriptorProto const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(FileDescriptorProto const& lhs, FileDescriptorProto const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(FileDescriptorProto const& lhs, FileDescriptorProto const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(FileDescriptorProto const& lhs, FileDescriptorProto const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(FileDescriptorProto const& lhs, FileDescriptorProto const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

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

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        FileDescriptorSet* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         FileDescriptorSet const& proto);

  static auto Tie(FileDescriptorSet const& proto) { return ::tsdb2::proto::Tie(proto.file); }

  template <typename H>
  friend H AbslHashValue(H h, FileDescriptorSet const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, FileDescriptorSet const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, FileDescriptorSet* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(FileDescriptorSet const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(FileDescriptorSet const& lhs, FileDescriptorSet const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(FileDescriptorSet const& lhs, FileDescriptorSet const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(FileDescriptorSet const& lhs, FileDescriptorSet const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(FileDescriptorSet const& lhs, FileDescriptorSet const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(FileDescriptorSet const& lhs, FileDescriptorSet const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(FileDescriptorSet const& lhs, FileDescriptorSet const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

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

    template <typename H>
    friend H AbslHashValue(H h, Semantic const& value) {
      return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
    }

    template <typename State>
    friend State Tsdb2FingerprintValue(State state, Semantic const& value) {
      return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
    }

    friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, Semantic* proto);

    friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                           Semantic const& proto);

    friend bool AbslParseFlag(std::string_view const text, Semantic* proto,
                              std::string* const error) {
      return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
    }

    friend std::string AbslUnparseFlag(Semantic const& proto) {
      return ::tsdb2::proto::text::Stringify(proto);
    }

    static ::absl::StatusOr<Annotation> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(Annotation const& proto);

    friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, Annotation* proto);

    friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                           Annotation const& proto);

    static auto Tie(Annotation const& proto) {
      return ::tsdb2::proto::Tie(proto.path, proto.source_file, proto.begin, proto.end,
                                 proto.semantic);
    }

    template <typename H>
    friend H AbslHashValue(H h, Annotation const& proto) {
      return H::combine(std::move(h), Tie(proto));
    }

    template <typename State>
    friend State Tsdb2FingerprintValue(State state, Annotation const& proto) {
      return State::Combine(std::move(state), Tie(proto));
    }

    friend bool AbslParseFlag(std::string_view const text, Annotation* const proto,
                              std::string* const error) {
      return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
    }

    friend std::string AbslUnparseFlag(Annotation const& proto) {
      return ::tsdb2::proto::text::Stringify(proto);
    }

    friend bool operator==(Annotation const& lhs, Annotation const& rhs) {
      return Tie(lhs) == Tie(rhs);
    }
    friend bool operator!=(Annotation const& lhs, Annotation const& rhs) {
      return Tie(lhs) != Tie(rhs);
    }
    friend bool operator<(Annotation const& lhs, Annotation const& rhs) {
      return Tie(lhs) < Tie(rhs);
    }
    friend bool operator<=(Annotation const& lhs, Annotation const& rhs) {
      return Tie(lhs) <= Tie(rhs);
    }
    friend bool operator>(Annotation const& lhs, Annotation const& rhs) {
      return Tie(lhs) > Tie(rhs);
    }
    friend bool operator>=(Annotation const& lhs, Annotation const& rhs) {
      return Tie(lhs) >= Tie(rhs);
    }

    std::vector<int32_t> path;
    std::optional<std::string> source_file;
    std::optional<int32_t> begin;
    std::optional<int32_t> end;
    std::optional<::google::protobuf::GeneratedCodeInfo::Annotation::Semantic> semantic;
  };

  static ::absl::StatusOr<GeneratedCodeInfo> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(GeneratedCodeInfo const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        GeneratedCodeInfo* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         GeneratedCodeInfo const& proto);

  static auto Tie(GeneratedCodeInfo const& proto) { return ::tsdb2::proto::Tie(proto.annotation); }

  template <typename H>
  friend H AbslHashValue(H h, GeneratedCodeInfo const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, GeneratedCodeInfo const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, GeneratedCodeInfo* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(GeneratedCodeInfo const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(GeneratedCodeInfo const& lhs, GeneratedCodeInfo const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(GeneratedCodeInfo const& lhs, GeneratedCodeInfo const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(GeneratedCodeInfo const& lhs, GeneratedCodeInfo const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(GeneratedCodeInfo const& lhs, GeneratedCodeInfo const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(GeneratedCodeInfo const& lhs, GeneratedCodeInfo const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(GeneratedCodeInfo const& lhs, GeneratedCodeInfo const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::vector<::google::protobuf::GeneratedCodeInfo::Annotation> annotation;
};

struct MethodOptions : public ::tsdb2::proto::Message {
  enum class IdempotencyLevel {
    IDEMPOTENCY_UNKNOWN = 0,
    NO_SIDE_EFFECTS = 1,
    IDEMPOTENT = 2,
  };

  template <typename H>
  friend H AbslHashValue(H h, IdempotencyLevel const& value) {
    return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, IdempotencyLevel const& value) {
    return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
  }

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        IdempotencyLevel* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         IdempotencyLevel const& proto);

  friend bool AbslParseFlag(std::string_view const text, IdempotencyLevel* proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(IdempotencyLevel const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  static ::absl::StatusOr<MethodOptions> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(MethodOptions const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, MethodOptions* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         MethodOptions const& proto);

  static auto Tie(MethodOptions const& proto) {
    return ::tsdb2::proto::Tie(proto.deprecated, proto.idempotency_level,
                               ::tsdb2::proto::OptionalSubMessageRef(proto.features),
                               proto.uninterpreted_option, proto.extension_data);
  }

  template <typename H>
  friend H AbslHashValue(H h, MethodOptions const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, MethodOptions const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, MethodOptions* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(MethodOptions const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(MethodOptions const& lhs, MethodOptions const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(MethodOptions const& lhs, MethodOptions const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(MethodOptions const& lhs, MethodOptions const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(MethodOptions const& lhs, MethodOptions const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(MethodOptions const& lhs, MethodOptions const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(MethodOptions const& lhs, MethodOptions const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  bool deprecated{false};
  ::google::protobuf::MethodOptions::IdempotencyLevel idempotency_level{
      ::google::protobuf::MethodOptions::IdempotencyLevel::IDEMPOTENCY_UNKNOWN};
  std::optional<::google::protobuf::FeatureSet> features;
  std::vector<::google::protobuf::UninterpretedOption> uninterpreted_option;
  ::tsdb2::proto::ExtensionData extension_data;
};

struct MethodDescriptorProto : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<MethodDescriptorProto> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(MethodDescriptorProto const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        MethodDescriptorProto* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         MethodDescriptorProto const& proto);

  static auto Tie(MethodDescriptorProto const& proto) {
    return ::tsdb2::proto::Tie(proto.name, proto.input_type, proto.output_type,
                               ::tsdb2::proto::OptionalSubMessageRef(proto.options),
                               proto.client_streaming, proto.server_streaming);
  }

  template <typename H>
  friend H AbslHashValue(H h, MethodDescriptorProto const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, MethodDescriptorProto const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, MethodDescriptorProto* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(MethodDescriptorProto const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(MethodDescriptorProto const& lhs, MethodDescriptorProto const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(MethodDescriptorProto const& lhs, MethodDescriptorProto const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(MethodDescriptorProto const& lhs, MethodDescriptorProto const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(MethodDescriptorProto const& lhs, MethodDescriptorProto const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(MethodDescriptorProto const& lhs, MethodDescriptorProto const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(MethodDescriptorProto const& lhs, MethodDescriptorProto const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

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

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, OneofOptions* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         OneofOptions const& proto);

  static auto Tie(OneofOptions const& proto) {
    return ::tsdb2::proto::Tie(::tsdb2::proto::OptionalSubMessageRef(proto.features),
                               proto.uninterpreted_option, proto.extension_data);
  }

  template <typename H>
  friend H AbslHashValue(H h, OneofOptions const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, OneofOptions const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, OneofOptions* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(OneofOptions const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(OneofOptions const& lhs, OneofOptions const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(OneofOptions const& lhs, OneofOptions const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(OneofOptions const& lhs, OneofOptions const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(OneofOptions const& lhs, OneofOptions const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(OneofOptions const& lhs, OneofOptions const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(OneofOptions const& lhs, OneofOptions const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::optional<::google::protobuf::FeatureSet> features;
  std::vector<::google::protobuf::UninterpretedOption> uninterpreted_option;
  ::tsdb2::proto::ExtensionData extension_data;
};

struct OneofDescriptorProto : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<OneofDescriptorProto> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(OneofDescriptorProto const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        OneofDescriptorProto* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         OneofDescriptorProto const& proto);

  static auto Tie(OneofDescriptorProto const& proto) {
    return ::tsdb2::proto::Tie(proto.name, ::tsdb2::proto::OptionalSubMessageRef(proto.options));
  }

  template <typename H>
  friend H AbslHashValue(H h, OneofDescriptorProto const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, OneofDescriptorProto const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, OneofDescriptorProto* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(OneofDescriptorProto const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(OneofDescriptorProto const& lhs, OneofDescriptorProto const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(OneofDescriptorProto const& lhs, OneofDescriptorProto const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(OneofDescriptorProto const& lhs, OneofDescriptorProto const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(OneofDescriptorProto const& lhs, OneofDescriptorProto const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(OneofDescriptorProto const& lhs, OneofDescriptorProto const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(OneofDescriptorProto const& lhs, OneofDescriptorProto const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::optional<std::string> name;
  std::optional<::google::protobuf::OneofOptions> options;
};

struct ServiceOptions : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<ServiceOptions> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(ServiceOptions const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        ServiceOptions* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         ServiceOptions const& proto);

  static auto Tie(ServiceOptions const& proto) {
    return ::tsdb2::proto::Tie(::tsdb2::proto::OptionalSubMessageRef(proto.features),
                               proto.deprecated, proto.uninterpreted_option, proto.extension_data);
  }

  template <typename H>
  friend H AbslHashValue(H h, ServiceOptions const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, ServiceOptions const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, ServiceOptions* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(ServiceOptions const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(ServiceOptions const& lhs, ServiceOptions const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(ServiceOptions const& lhs, ServiceOptions const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(ServiceOptions const& lhs, ServiceOptions const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(ServiceOptions const& lhs, ServiceOptions const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(ServiceOptions const& lhs, ServiceOptions const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(ServiceOptions const& lhs, ServiceOptions const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::optional<::google::protobuf::FeatureSet> features;
  bool deprecated{false};
  std::vector<::google::protobuf::UninterpretedOption> uninterpreted_option;
  ::tsdb2::proto::ExtensionData extension_data;
};

struct ServiceDescriptorProto : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<ServiceDescriptorProto> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(ServiceDescriptorProto const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        ServiceDescriptorProto* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         ServiceDescriptorProto const& proto);

  static auto Tie(ServiceDescriptorProto const& proto) {
    return ::tsdb2::proto::Tie(proto.name, proto.method,
                               ::tsdb2::proto::OptionalSubMessageRef(proto.options));
  }

  template <typename H>
  friend H AbslHashValue(H h, ServiceDescriptorProto const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, ServiceDescriptorProto const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, ServiceDescriptorProto* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(ServiceDescriptorProto const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(ServiceDescriptorProto const& lhs, ServiceDescriptorProto const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(ServiceDescriptorProto const& lhs, ServiceDescriptorProto const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(ServiceDescriptorProto const& lhs, ServiceDescriptorProto const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(ServiceDescriptorProto const& lhs, ServiceDescriptorProto const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(ServiceDescriptorProto const& lhs, ServiceDescriptorProto const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(ServiceDescriptorProto const& lhs, ServiceDescriptorProto const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::optional<std::string> name;
  std::vector<::google::protobuf::MethodDescriptorProto> method;
  std::optional<::google::protobuf::ServiceOptions> options;
};

struct UninterpretedOption : public ::tsdb2::proto::Message {
  struct NamePart;

  struct NamePart : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<NamePart> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(NamePart const& proto);

    friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, NamePart* proto);

    friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                           NamePart const& proto);

    static auto Tie(NamePart const& proto) {
      return ::tsdb2::proto::Tie(proto.name_part, proto.is_extension);
    }

    template <typename H>
    friend H AbslHashValue(H h, NamePart const& proto) {
      return H::combine(std::move(h), Tie(proto));
    }

    template <typename State>
    friend State Tsdb2FingerprintValue(State state, NamePart const& proto) {
      return State::Combine(std::move(state), Tie(proto));
    }

    friend bool AbslParseFlag(std::string_view const text, NamePart* const proto,
                              std::string* const error) {
      return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
    }

    friend std::string AbslUnparseFlag(NamePart const& proto) {
      return ::tsdb2::proto::text::Stringify(proto);
    }

    friend bool operator==(NamePart const& lhs, NamePart const& rhs) {
      return Tie(lhs) == Tie(rhs);
    }
    friend bool operator!=(NamePart const& lhs, NamePart const& rhs) {
      return Tie(lhs) != Tie(rhs);
    }
    friend bool operator<(NamePart const& lhs, NamePart const& rhs) { return Tie(lhs) < Tie(rhs); }
    friend bool operator<=(NamePart const& lhs, NamePart const& rhs) {
      return Tie(lhs) <= Tie(rhs);
    }
    friend bool operator>(NamePart const& lhs, NamePart const& rhs) { return Tie(lhs) > Tie(rhs); }
    friend bool operator>=(NamePart const& lhs, NamePart const& rhs) {
      return Tie(lhs) >= Tie(rhs);
    }

    std::string name_part;
    bool is_extension{};
  };

  static ::absl::StatusOr<UninterpretedOption> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(UninterpretedOption const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        UninterpretedOption* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         UninterpretedOption const& proto);

  static auto Tie(UninterpretedOption const& proto) {
    return ::tsdb2::proto::Tie(proto.name, proto.identifier_value, proto.positive_int_value,
                               proto.negative_int_value, proto.double_value, proto.string_value,
                               proto.aggregate_value);
  }

  template <typename H>
  friend H AbslHashValue(H h, UninterpretedOption const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, UninterpretedOption const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, UninterpretedOption* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(UninterpretedOption const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(UninterpretedOption const& lhs, UninterpretedOption const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(UninterpretedOption const& lhs, UninterpretedOption const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(UninterpretedOption const& lhs, UninterpretedOption const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(UninterpretedOption const& lhs, UninterpretedOption const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(UninterpretedOption const& lhs, UninterpretedOption const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(UninterpretedOption const& lhs, UninterpretedOption const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::vector<::google::protobuf::UninterpretedOption::NamePart> name;
  std::optional<std::string> identifier_value;
  std::optional<uint64_t> positive_int_value;
  std::optional<int64_t> negative_int_value;
  std::optional<double> double_value;
  std::optional<std::vector<uint8_t>> string_value;
  std::optional<std::string> aggregate_value;
};

}  // namespace google::protobuf

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

#endif  // __TSDB2_PROTO_DESCRIPTOR_PB_H__
