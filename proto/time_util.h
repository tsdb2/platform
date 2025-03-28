#ifndef __TSDB2_PROTO_TIME_UTIL_H__
#define __TSDB2_PROTO_TIME_UTIL_H__

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "proto/duration.pb.sync.h"
#include "proto/timestamp.pb.sync.h"

namespace tsdb2 {
namespace proto {

absl::StatusOr<absl::Time> DecodeGoogleApiProto(::google::protobuf::Timestamp const& proto);
absl::StatusOr<absl::Duration> DecodeGoogleApiProto(::google::protobuf::Duration const& proto);

::google::protobuf::Timestamp EncodeGoogleApiProto(absl::Time time);
::google::protobuf::Duration EncodeGoogleApiProto(absl::Duration duration);

}  // namespace proto
}  // namespace tsdb2

#endif  // __TSDB2_PROTO_TIME_UTIL_H__
