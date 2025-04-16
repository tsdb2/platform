#ifndef __TSDB2_PROTO_DEPENDENCY_MAPPING_PB_H__
#define __TSDB2_PROTO_DEPENDENCY_MAPPING_PB_H__

#include "proto/runtime.h"

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

namespace tsdb2::proto::internal {

struct DependencyMapping;

struct DependencyMapping : public ::tsdb2::proto::Message {
  struct Dependency;
  struct DependencyEntry;

  struct Dependency : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<Dependency> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(Dependency const& proto);

    friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser, Dependency* proto);

    friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                           Dependency const& proto);

    static auto Tie(Dependency const& proto) { return ::tsdb2::proto::Tie(proto.cc_header); }

    template <typename H>
    friend H AbslHashValue(H h, Dependency const& proto) {
      return H::combine(std::move(h), Tie(proto));
    }

    template <typename State>
    friend State Tsdb2FingerprintValue(State state, Dependency const& proto) {
      return State::Combine(std::move(state), Tie(proto));
    }

    friend bool AbslParseFlag(std::string_view const text, Dependency* const proto,
                              std::string* const error) {
      return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
    }

    friend std::string AbslUnparseFlag(Dependency const& proto) {
      return ::tsdb2::proto::text::Stringify(proto);
    }

    friend bool operator==(Dependency const& lhs, Dependency const& rhs) {
      return Tie(lhs) == Tie(rhs);
    }
    friend bool operator!=(Dependency const& lhs, Dependency const& rhs) {
      return Tie(lhs) != Tie(rhs);
    }
    friend bool operator<(Dependency const& lhs, Dependency const& rhs) {
      return Tie(lhs) < Tie(rhs);
    }
    friend bool operator<=(Dependency const& lhs, Dependency const& rhs) {
      return Tie(lhs) <= Tie(rhs);
    }
    friend bool operator>(Dependency const& lhs, Dependency const& rhs) {
      return Tie(lhs) > Tie(rhs);
    }
    friend bool operator>=(Dependency const& lhs, Dependency const& rhs) {
      return Tie(lhs) >= Tie(rhs);
    }

    std::vector<std::string> cc_header;
  };

  struct DependencyEntry : public ::tsdb2::proto::Message {
    static ::absl::StatusOr<DependencyEntry> Decode(::absl::Span<uint8_t const> data);
    static ::tsdb2::io::Cord Encode(DependencyEntry const& proto);

    friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                          DependencyEntry* proto);

    friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                           DependencyEntry const& proto);

    static auto Tie(DependencyEntry const& proto) {
      return ::tsdb2::proto::Tie(proto.key, ::tsdb2::proto::OptionalSubMessageRef(proto.value));
    }

    template <typename H>
    friend H AbslHashValue(H h, DependencyEntry const& proto) {
      return H::combine(std::move(h), Tie(proto));
    }

    template <typename State>
    friend State Tsdb2FingerprintValue(State state, DependencyEntry const& proto) {
      return State::Combine(std::move(state), Tie(proto));
    }

    friend bool AbslParseFlag(std::string_view const text, DependencyEntry* const proto,
                              std::string* const error) {
      return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
    }

    friend std::string AbslUnparseFlag(DependencyEntry const& proto) {
      return ::tsdb2::proto::text::Stringify(proto);
    }

    friend bool operator==(DependencyEntry const& lhs, DependencyEntry const& rhs) {
      return Tie(lhs) == Tie(rhs);
    }
    friend bool operator!=(DependencyEntry const& lhs, DependencyEntry const& rhs) {
      return Tie(lhs) != Tie(rhs);
    }
    friend bool operator<(DependencyEntry const& lhs, DependencyEntry const& rhs) {
      return Tie(lhs) < Tie(rhs);
    }
    friend bool operator<=(DependencyEntry const& lhs, DependencyEntry const& rhs) {
      return Tie(lhs) <= Tie(rhs);
    }
    friend bool operator>(DependencyEntry const& lhs, DependencyEntry const& rhs) {
      return Tie(lhs) > Tie(rhs);
    }
    friend bool operator>=(DependencyEntry const& lhs, DependencyEntry const& rhs) {
      return Tie(lhs) >= Tie(rhs);
    }

    std::optional<std::string> key;
    std::optional<::tsdb2::proto::internal::DependencyMapping::Dependency> value;
  };

  static ::absl::StatusOr<DependencyMapping> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(DependencyMapping const& proto);

  friend ::absl::Status Tsdb2ProtoParse(::tsdb2::proto::text::Parser* parser,
                                        DependencyMapping* proto);

  friend std::string Tsdb2ProtoStringify(::tsdb2::proto::text::Stringifier* stringifier,
                                         DependencyMapping const& proto);

  static auto Tie(DependencyMapping const& proto) { return ::tsdb2::proto::Tie(proto.dependency); }

  template <typename H>
  friend H AbslHashValue(H h, DependencyMapping const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, DependencyMapping const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool AbslParseFlag(std::string_view const text, DependencyMapping* const proto,
                            std::string* const error) {
    return ::tsdb2::proto::text::Parser::ParseFlag(text, proto, error);
  }

  friend std::string AbslUnparseFlag(DependencyMapping const& proto) {
    return ::tsdb2::proto::text::Stringify(proto);
  }

  friend bool operator==(DependencyMapping const& lhs, DependencyMapping const& rhs) {
    return Tie(lhs) == Tie(rhs);
  }
  friend bool operator!=(DependencyMapping const& lhs, DependencyMapping const& rhs) {
    return Tie(lhs) != Tie(rhs);
  }

  ::absl::flat_hash_map<std::string, ::tsdb2::proto::internal::DependencyMapping::Dependency>
      dependency;
};

}  // namespace tsdb2::proto::internal

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

#endif  // __TSDB2_PROTO_DEPENDENCY_MAPPING_PB_H__
