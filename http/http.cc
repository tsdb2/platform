#include "http/http.h"

#include <cstddef>
#include <optional>
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "common/flat_map.h"
#include "common/no_destructor.h"
#include "server/base_module.h"
#include "server/init_tsdb2.h"

ABSL_FLAG(absl::Duration, http2_io_timeout, absl::Seconds(60),
          "Timeout for HTTP/2 I/O operations. The timeout is reset every time some data is "
          "transferred, so it should be okay to set a low value even if we're transferring large "
          "amounts of data. The purpose of the timeout is to prevent a peer from leaving us in a "
          "pending I/O state indefinitely and causing memory leaks.");

ABSL_FLAG(size_t, http2_max_dynamic_header_table_size,
          tsdb2::http::kDefaultMaxDynamicHeaderTableSize,
          "Maximum HPACK table size. The table is maintained on a per-connection basis. Note that "
          "this is the limit for the size of the local table used for *decoding*; the encoder's "
          "table must mirror the peer's, so we can only learn its size at runtime and cannot "
          "enforce a limit on it.");

ABSL_FLAG(std::optional<size_t>, http2_max_concurrent_streams, std::nullopt,
          "Maximum number of streams in a single HTTP/2 channel. No limit if unspecified.");

ABSL_FLAG(size_t, http2_initial_stream_window_size, tsdb2::http::kDefaultInitialWindowSize,
          "Initial flow control window size fore newly created streams.");

ABSL_FLAG(size_t, http2_max_frame_payload_size, tsdb2::http::kDefaultMaxFramePayloadSize,
          "Maximum frame payload size. Must be at least 16 KiB as per the specs, so we'll "
          "check-fail at startup if a lower value is specified in this flag.");

ABSL_FLAG(size_t, http2_max_header_list_size, tsdb2::http::kDefaultMaxHeaderListSize,
          "Maximum size of a single uncompressed HTTP/2 field section (\"field\" means \"header\" "
          "or \"trailer\").");

namespace tsdb2 {
namespace http {

tsdb2::common::fixed_flat_map<StreamState, std::string_view, kNumStreamStates> const
    kStreamStateNames = tsdb2::common::fixed_flat_map_of<StreamState, std::string_view>({
        {StreamState::kIdle, "idle"},
        {StreamState::kReservedLocal, "reserved-local"},
        {StreamState::kReservedRemote, "reserved-remote"},
        {StreamState::kOpen, "open"},
        {StreamState::kHalfClosedLocal, "half-closed-local"},
        {StreamState::kHalfClosedRemote, "half-closed-remote"},
        {StreamState::kClosed, "closed"},
    });

tsdb2::common::fixed_flat_map<std::string_view, Method, kNumMethods> constexpr kMethodsByName =
    tsdb2::common::fixed_flat_map_of<std::string_view, Method>({
        {"GET", Method::kGet},
        {"HEAD", Method::kHead},
        {"POST", Method::kPost},
        {"PUT", Method::kPut},
        {"DELETE", Method::kDelete},
        {"CONNECT", Method::kConnect},
        {"OPTIONS", Method::kOptions},
        {"TRACE", Method::kTrace},
    });

tsdb2::common::fixed_flat_map<Method, std::string_view, kNumMethods> constexpr kMethodNames =
    tsdb2::common::fixed_flat_map_of<Method, std::string_view>({
        {Method::kGet, "GET"},
        {Method::kHead, "HEAD"},
        {Method::kPost, "POST"},
        {Method::kPut, "PUT"},
        {Method::kDelete, "DELETE"},
        {Method::kConnect, "CONNECT"},
        {Method::kOptions, "OPTIONS"},
        {Method::kTrace, "TRACE"},
    });

tsdb2::common::fixed_flat_map<int, std::string_view, kNumStatuses> constexpr kStatusNames =
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

absl::Status HttpModule::Initialize() {
  if (absl::GetFlag(FLAGS_http2_max_frame_payload_size) < kMinFramePayloadSizeLimit) {
    return absl::InvalidArgumentError(
        "the --http2_max_frame_payload_size must be at least 16384 (= 16 KiB).");
  }
  return absl::OkStatus();
}

HttpModule::HttpModule() : BaseModule("http") { tsdb2::init::RegisterModule(this); }

tsdb2::common::NoDestructor<HttpModule> HttpModule::instance_;

}  // namespace http
}  // namespace tsdb2
