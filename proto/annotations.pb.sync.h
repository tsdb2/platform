#ifndef __TSDB2_PROTO_ANNOTATIONS_PB_H__
#define __TSDB2_PROTO_ANNOTATIONS_PB_H__

#include "proto/descriptor.pb.sync.h"
#include "proto/runtime.h"

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

namespace tsdb2::proto {

enum class FieldIndirectionType {
  INDIRECTION_DIRECT = 0,
  INDIRECTION_UNIQUE = 1,
  INDIRECTION_SHARED = 2,
};

template <typename H>
inline H AbslHashValue(H h, FieldIndirectionType const& value) {
  return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
}

template <typename State>
inline State Tsdb2FingerprintValue(State state, FieldIndirectionType const& value) {
  return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, FieldIndirectionType* proto);

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                FieldIndirectionType const& proto);

inline bool AbslParseFlag(std::string_view const text, FieldIndirectionType* proto,
                          std::string* const error) {
  return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
}

inline std::string AbslUnparseFlag(FieldIndirectionType const& proto) {
  return ::tsdb2::proto::text::Stringify(proto);
}

enum class MapType {
  MAP_TYPE_STD_MAP = 0,
  MAP_TYPE_STD_UNORDERED_MAP = 1,
  MAP_TYPE_ABSL_FLAT_HASH_MAP = 2,
  MAP_TYPE_ABSL_NODE_HASH_MAP = 3,
  MAP_TYPE_ABSL_BTREE_MAP = 4,
  MAP_TYPE_TSDB2_FLAT_MAP = 5,
  MAP_TYPE_TSDB2_TRIE_MAP = 6,
};

template <typename H>
inline H AbslHashValue(H h, MapType const& value) {
  return H::combine(std::move(h), ::tsdb2::util::to_underlying(value));
}

template <typename State>
inline State Tsdb2FingerprintValue(State state, MapType const& value) {
  return State::Combine(std::move(state), ::tsdb2::util::to_underlying(value));
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, MapType* proto);

std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                MapType const& proto);

inline bool AbslParseFlag(std::string_view const text, MapType* proto, std::string* const error) {
  return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
}

inline std::string AbslUnparseFlag(MapType const& proto) {
  return ::tsdb2::proto::text::Stringify(proto);
}

struct google_protobuf_FieldOptions_extension : public ::tsdb2::proto::Message {
  static ::absl::StatusOr<google_protobuf_FieldOptions_extension> Decode(
      ::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(google_protobuf_FieldOptions_extension const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        google_protobuf_FieldOptions_extension* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         google_protobuf_FieldOptions_extension const& proto);

  static auto Tie(google_protobuf_FieldOptions_extension const& proto) {
    return ::tsdb2::proto::Tie(proto.indirect, proto.map_type);
  }

  template <typename H>
  friend H AbslHashValue(H h, google_protobuf_FieldOptions_extension const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state,
                                     google_protobuf_FieldOptions_extension const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text,
                            google_protobuf_FieldOptions_extension* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(google_protobuf_FieldOptions_extension const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(google_protobuf_FieldOptions_extension const& lhs,
                         google_protobuf_FieldOptions_extension const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(google_protobuf_FieldOptions_extension const& lhs,
                         google_protobuf_FieldOptions_extension const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }
  friend bool operator<(google_protobuf_FieldOptions_extension const& lhs,
                        google_protobuf_FieldOptions_extension const& rhs) {
    return Tie(lhs) < Tie(rhs);
  }
  friend bool operator<=(google_protobuf_FieldOptions_extension const& lhs,
                         google_protobuf_FieldOptions_extension const& rhs) {
    return Tie(lhs) <= Tie(rhs);
  }
  friend bool operator>(google_protobuf_FieldOptions_extension const& lhs,
                        google_protobuf_FieldOptions_extension const& rhs) {
    return Tie(lhs) > Tie(rhs);
  }
  friend bool operator>=(google_protobuf_FieldOptions_extension const& lhs,
                         google_protobuf_FieldOptions_extension const& rhs) {
    return Tie(lhs) >= Tie(rhs);
  }

  std::optional<::tsdb2::proto::FieldIndirectionType> indirect;
  std::optional<::tsdb2::proto::MapType> map_type;
};

}  // namespace tsdb2::proto

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

#endif  // __TSDB2_PROTO_ANNOTATIONS_PB_H__
