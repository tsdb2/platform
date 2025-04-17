#include "proto/annotations.pb.sync.h"

#include "proto/descriptor.pb.sync.h"
#include "proto/runtime.h"

namespace tsdb2::proto {

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                               FieldIndirectionType* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, FieldIndirectionType>({
          {"INDIRECTION_DIRECT", FieldIndirectionType::INDIRECTION_DIRECT},
          {"INDIRECTION_UNIQUE", FieldIndirectionType::INDIRECTION_UNIQUE},
          {"INDIRECTION_SHARED", FieldIndirectionType::INDIRECTION_SHARED},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

void Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                         FieldIndirectionType const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<FieldIndirectionType, std::string_view>({
          {FieldIndirectionType::INDIRECTION_DIRECT, "INDIRECTION_DIRECT"},
          {FieldIndirectionType::INDIRECTION_UNIQUE, "INDIRECTION_UNIQUE"},
          {FieldIndirectionType::INDIRECTION_SHARED, "INDIRECTION_SHARED"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    stringifier->AppendIdentifier(it->second);
  } else {
    stringifier->AppendInteger(::tsdb2::util::to_underlying(proto));
  }
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, MapType* const proto) {
  static auto constexpr kValuesByName =
      ::tsdb2::common::fixed_flat_map_of<std::string_view, MapType>({
          {"MAP_TYPE_STD_MAP", MapType::MAP_TYPE_STD_MAP},
          {"MAP_TYPE_STD_UNORDERED_MAP", MapType::MAP_TYPE_STD_UNORDERED_MAP},
          {"MAP_TYPE_ABSL_FLAT_HASH_MAP", MapType::MAP_TYPE_ABSL_FLAT_HASH_MAP},
          {"MAP_TYPE_ABSL_NODE_HASH_MAP", MapType::MAP_TYPE_ABSL_NODE_HASH_MAP},
          {"MAP_TYPE_ABSL_BTREE_MAP", MapType::MAP_TYPE_ABSL_BTREE_MAP},
          {"MAP_TYPE_TSDB2_FLAT_MAP", MapType::MAP_TYPE_TSDB2_FLAT_MAP},
          {"MAP_TYPE_TSDB2_TRIE_MAP", MapType::MAP_TYPE_TSDB2_TRIE_MAP},
      });
  DEFINE_CONST_OR_RETURN(name, parser->ParseIdentifier());
  auto const it = kValuesByName.find(name);
  if (it != kValuesByName.end()) {
    *proto = it->second;
    return ::absl::OkStatus();
  } else {
    return parser->InvalidFormatError();
  }
}

void Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                         MapType const& proto) {
  static auto constexpr kValueNames =
      ::tsdb2::common::fixed_flat_map_of<MapType, std::string_view>({
          {MapType::MAP_TYPE_STD_MAP, "MAP_TYPE_STD_MAP"},
          {MapType::MAP_TYPE_STD_UNORDERED_MAP, "MAP_TYPE_STD_UNORDERED_MAP"},
          {MapType::MAP_TYPE_ABSL_FLAT_HASH_MAP, "MAP_TYPE_ABSL_FLAT_HASH_MAP"},
          {MapType::MAP_TYPE_ABSL_NODE_HASH_MAP, "MAP_TYPE_ABSL_NODE_HASH_MAP"},
          {MapType::MAP_TYPE_ABSL_BTREE_MAP, "MAP_TYPE_ABSL_BTREE_MAP"},
          {MapType::MAP_TYPE_TSDB2_FLAT_MAP, "MAP_TYPE_TSDB2_FLAT_MAP"},
          {MapType::MAP_TYPE_TSDB2_TRIE_MAP, "MAP_TYPE_TSDB2_TRIE_MAP"},
      });
  auto const it = kValueNames.find(proto);
  if (it != kValueNames.end()) {
    stringifier->AppendIdentifier(it->second);
  } else {
    stringifier->AppendInteger(::tsdb2::util::to_underlying(proto));
  }
}

::absl::StatusOr<google_protobuf_FieldOptions_extension>
google_protobuf_FieldOptions_extension::Decode(::absl::Span<uint8_t const> const data) {
  google_protobuf_FieldOptions_extension proto;
  ::tsdb2::proto::Decoder decoder{data};
  while (true) {
    DEFINE_CONST_OR_RETURN(maybe_tag, decoder.DecodeTag());
    if (!maybe_tag.has_value()) {
      break;
    }
    auto const tag = maybe_tag.value();
    switch (tag.field_number) {
      case 71104: {
        DEFINE_CONST_OR_RETURN(
            value, decoder.DecodeEnumField<::tsdb2::proto::FieldIndirectionType>(tag.wire_type));
        proto.indirect.emplace(value);
      } break;
      case 71105: {
        DEFINE_CONST_OR_RETURN(value,
                               decoder.DecodeEnumField<::tsdb2::proto::MapType>(tag.wire_type));
        proto.map_type.emplace(value);
      } break;
      default:
        RETURN_IF_ERROR(decoder.SkipRecord(tag.wire_type));
        break;
    }
  }
  return std::move(proto);
}

::tsdb2::io::Cord google_protobuf_FieldOptions_extension::Encode(
    google_protobuf_FieldOptions_extension const& proto) {
  ::tsdb2::proto::Encoder encoder;
  if (proto.indirect.has_value()) {
    encoder.EncodeEnumField(71104, proto.indirect.value());
  }
  if (proto.map_type.has_value()) {
    encoder.EncodeEnumField(71105, proto.map_type.value());
  }
  return std::move(encoder).Finish();
}

::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* const parser,
                               google_protobuf_FieldOptions_extension* const proto) {
  *proto = google_protobuf_FieldOptions_extension();
  std::optional<std::string> maybe_field_name;
  while (maybe_field_name = parser->ParseFieldName(), maybe_field_name.has_value()) {
    auto const& field_name = maybe_field_name.value();
    parser->ConsumeSeparators();
    if (field_name == "indirect") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseEnum<::tsdb2::proto::FieldIndirectionType>());
      proto->indirect.emplace(value);
    } else if (field_name == "map_type") {
      RETURN_IF_ERROR(parser->RequirePrefix(":"));
      DEFINE_CONST_OR_RETURN(value, parser->ParseEnum<::tsdb2::proto::MapType>());
      proto->map_type.emplace(value);
    } else {
      RETURN_IF_ERROR(parser->SkipField());
    }
    parser->ConsumeFieldSeparators();
  }
  return ::absl::OkStatus();
}

void Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* const stringifier,
                         google_protobuf_FieldOptions_extension const& proto) {
  if (proto.indirect.has_value()) {
    stringifier->AppendField("indirect", proto.indirect.value());
  }
  if (proto.map_type.has_value()) {
    stringifier->AppendField("map_type", proto.map_type.value());
  }
}

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

}  // namespace tsdb2::proto
