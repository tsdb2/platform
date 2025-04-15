#ifndef __TSDB2_PROTO_DURATION_PB_H__
#define __TSDB2_PROTO_DURATION_PB_H__

#include "proto/runtime.h"

TSDB2_DISABLE_DEPRECATED_DECLARATION_WARNING();

namespace google::protobuf {

struct Duration;

struct Duration : public ::tsdb2::proto::Message {
  static ::tsdb2::proto::MessageDescriptor<Duration, 2, 0> const MESSAGE_DESCRIPTOR;

  static ::absl::StatusOr<Duration> Decode(::absl::Span<uint8_t const> data);
  static ::tsdb2::io::Cord Encode(Duration const& proto);

  static ::absl::StatusOr<Duration> Parse(::absl::Nonnull<std::string_view*> text);
  static std::string Stringify(Duration const& proto);

  static auto Tie(Duration const& proto) { return ::tsdb2::proto::Tie(proto.seconds, proto.nanos); }

  template <typename H>
  friend H AbslHashValue(H h, Duration const& proto) {
    return H::combine(std::move(h), Tie(proto));
  }

  template <typename State>
  friend State Tsdb2FingerprintValue(State state, Duration const& proto) {
    return State::Combine(std::move(state), Tie(proto));
  }

  friend bool operator==(Duration const& lhs, Duration const& rhs) { return Tie(lhs) == Tie(rhs); }
  friend bool operator!=(Duration const& lhs, Duration const& rhs) { return Tie(lhs) != Tie(rhs); }
  friend bool operator<(Duration const& lhs, Duration const& rhs) { return Tie(lhs) < Tie(rhs); }
  friend bool operator<=(Duration const& lhs, Duration const& rhs) { return Tie(lhs) <= Tie(rhs); }
  friend bool operator>(Duration const& lhs, Duration const& rhs) { return Tie(lhs) > Tie(rhs); }
  friend bool operator>=(Duration const& lhs, Duration const& rhs) { return Tie(lhs) >= Tie(rhs); }

  std::optional<int64_t> seconds;
  std::optional<int32_t> nanos;
};

}  // namespace google::protobuf

namespace tsdb2::proto {

template <>
inline auto const& GetMessageDescriptor<::google::protobuf::Duration>() {
  return ::google::protobuf::Duration::MESSAGE_DESCRIPTOR;
};

}  // namespace tsdb2::proto

TSDB2_RESTORE_DEPRECATED_DECLARATION_WARNING();

#endif  // __TSDB2_PROTO_DURATION_PB_H__
