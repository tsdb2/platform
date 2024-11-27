#include "http/http.h"

#include <string_view>

#include "absl/flags/flag.h"
#include "absl/time/time.h"
#include "common/flat_map.h"

ABSL_FLAG(absl::Duration, http2_io_timeout, absl::Seconds(60),
          "Timeout for HTTP/2 I/O operations. The timeout is reset every time some data is "
          "transferred, so it should be okay to set a low value even if we're transferring large "
          "amounts of data. The purpose of the timeout is to prevent a peer from leaving us in a "
          "pending I/O state indefinitely and causing memory leaks.");

namespace tsdb2 {
namespace http {

tsdb2::common::fixed_flat_map<int, std::string_view, kNumMethods> constexpr kMethodNames =
    tsdb2::common::fixed_flat_map_of<int, std::string_view>({
        {200, "OK"},
        {201, "Created"},
        {202, "Accepted"},
        {203, "Non-Authoritative Information"},
        {204, "No Content"},
        {205, "Reset Content"},
        {206, "Partial Content"},
        {300, "Multiple Choices"},
        {301, "Moved Permanently"},
        {302, "Found"},
        {303, "See Other"},
        {304, "Not Modified"},
        {305, "Use Proxy"},
        {307, "Temporary Redirect"},
        {308, "Permanent Redirect"},
        {400, "Bad Request"},
        {401, "Unauthorized"},
        {402, "Payment Required"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {405, "Method Not Allowed"},
        {406, "Not Acceptable"},
        {407, "Proxy Authentication Required"},
        {408, "Request Timeout"},
        {409, "Conflict"},
        {410, "Gone"},
        {411, "Length Required"},
        {412, "Precondition Failed"},
        {413, "Content Too Large"},
        {414, "URI Too Long"},
        {415, "Unsupported Media Type"},
        {416, "Range Not Satisfiable"},
        {417, "Expectation Failed"},
        {421, "Misdirected Request"},
        {422, "Unprocessable Content"},
        {426, "Upgrade Required"},
        {500, "Internal Server Error"},
        {501, "Not Implemented"},
        {502, "Bad Gateway"},
        {503, "Service Unavailable"},
        {504, "Gateway Timeout"},
        {505, "HTTP Version Not Supported"},
    });

}  // namespace http
}  // namespace tsdb2
