#include "http/hpack.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "absl/status/statusor.h"
#include "absl/types/span.h"

namespace tsdb2 {
namespace http {

// See https://httpwg.org/specs/rfc7541.html#static.table.definition for the static header table
// definition.

size_t constexpr kNumStaticHeaders = 61;
std::string_view constexpr kStaticHeaders[kNumStaticHeaders][2]{
    {":authority", ""},
    {":method", "GET"},
    {":method", "POST"},
    {":path", "/"},
    {":path", "/index.html"},
    {":scheme", "http"},
    {":scheme", "https"},
    {":status", "200"},
    {":status", "204"},
    {":status", "206"},
    {":status", "304"},
    {":status", "400"},
    {":status", "404"},
    {":status", "500"},
    {"accept-charset", ""},
    {"accept-encoding", "gzip,deflate"},
    {"accept-language", ""},
    {"accept-ranges", ""},
    {"accept", ""},
    {"access-control-allow-origin", ""},
    {"age", ""},
    {"allow", ""},
    {"authorization", ""},
    {"cache-control", ""},
    {"content-disposition", ""},
    {"content-encoding", ""},
    {"content-language", ""},
    {"content-length", ""},
    {"content-location", ""},
    {"content-range", ""},
    {"content-type", ""},
    {"cookie", ""},
    {"date", ""},
    {"etag", ""},
    {"expect", ""},
    {"expires", ""},
    {"from", ""},
    {"host", ""},
    {"if-match", ""},
    {"if-modified-since", ""},
    {"if-none-match", ""},
    {"if-range", ""},
    {"if-unmodified-since", ""},
    {"last-modified", ""},
    {"link", ""},
    {"location", ""},
    {"max-forwards", ""},
    {"proxy-authenticate", ""},
    {"proxy-authorization", ""},
    {"range", ""},
    {"referer", ""},
    {"refresh", ""},
    {"retry-after", ""},
    {"server", ""},
    {"set-cookie", ""},
    {"strict-transport-security", ""},
    {"transfer-encoding", ""},
    {"user-agent", ""},
    {"vary", ""},
    {"via", ""},
    {"www-authenticate", ""},
};

absl::StatusOr<HeaderSet> HPACKDecoder::Decode(absl::Span<uint8_t const> const data) {
  HeaderSet headers;
  for (size_t offset = 0; offset < data.size(); ++offset) {
    switch (data[offset]) {
      // TODO
      default:
        return absl::InvalidArgumentError("invalid HPACK encoding");
    }
  }
  return headers;
}

}  // namespace http
}  // namespace tsdb2
