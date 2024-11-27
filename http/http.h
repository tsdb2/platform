#ifndef __TSDB2_HTTP_HTTP_H__
#define __TSDB2_HTTP_HTTP_H__

#include <arpa/inet.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "absl/base/attributes.h"
#include "absl/flags/declare.h"
#include "absl/time/time.h"
#include "common/flat_map.h"
#include "common/utilities.h"

ABSL_DECLARE_FLAG(absl::Duration, http2_io_timeout);

namespace tsdb2 {
namespace http {

inline std::string_view constexpr kClientPreface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

inline size_t constexpr kDefaultMaxDynamicHeaderTableSize = 4096;  // 4 KiB
inline size_t constexpr kDefaultInitialWindowSize = 65535;         // 64 KiB
inline size_t constexpr kMinFramePayloadSizeLimit = 16384;         // 16 KiB
inline size_t constexpr kDefaultMaxFramePayloadSize = kMinFramePayloadSizeLimit;
inline size_t constexpr kDefaultMaxHeaderListSize = 1048576;  // 1 MiB

enum class StreamState {
  kIdle,
  kReservedLocal,
  kReservedRemote,
  kOpen,
  kHalfClosedLocal,
  kHalfClosedRemote,
  kClosed,
};

enum class FrameType {
  kData = 0,
  kHeaders = 1,
  kPriority = 2,
  kResetStream = 3,
  kSettings = 4,
  kPushPromise = 5,
  kPing = 6,
  kGoAway = 7,
  kWindowUpdate = 8,
  kContinuation = 9,
};

// Flags used in various frame types.
inline unsigned int constexpr kFlagAck = 1;
inline unsigned int constexpr kFlagEndStream = 1;
inline unsigned int constexpr kFlagEndHeaders = 4;
inline unsigned int constexpr kFlagPadded = 8;
inline unsigned int constexpr kFlagPriority = 32;

class ABSL_ATTRIBUTE_PACKED FrameHeader {
 public:
  explicit FrameHeader() : reserved_(0) {}

  FrameHeader(FrameHeader const&) = default;
  FrameHeader& operator=(FrameHeader const&) = default;

  size_t length() const { return ::ntohl(length_ << 8); }

  FrameHeader& set_length(size_t const value) {
    length_ = ::htonl(value) >> 8;
    return *this;
  }

  FrameType frame_type() const { return static_cast<FrameType>(type_); }

  FrameHeader& set_frame_type(FrameType const value) {
    type_ = static_cast<unsigned int>(tsdb2::util::to_underlying(value));
    return *this;
  }

  uint8_t flags() const { return flags_; }

  FrameHeader& set_flags(uint8_t const value) {
    flags_ = value;
    return *this;
  }

  uint32_t stream_id() const { return ::ntohl(stream_id_); }

  FrameHeader& set_stream_id(uint32_t const id) {
    stream_id_ = ::htonl(id);
    return *this;
  }

 private:
  unsigned int length_ : 24;
  unsigned int type_ : 8;
  unsigned int flags_ : 8;
  unsigned int reserved_ : 1 ABSL_ATTRIBUTE_UNUSED;
  unsigned int stream_id_ : 31;
};

static_assert(sizeof(FrameHeader) == 9, "incorrect frame header size");

enum class ConnectionError {
  kNoError = 0,
  kProtocolError = 1,
  kInternalError = 2,
  kFlowControlError = 3,
  kSettingsTimeout = 4,
  kStreamClosed = 5,
  kFrameSizeError = 6,
  kRefusedStream = 7,
  kCancel = 8,
  kCompressionError = 9,
  kConnectError = 10,
  kEnhanceYourCalm = 11,
  kInadequateSecurity = 12,
  kHttp11Required = 13,
};

struct ABSL_ATTRIBUTE_PACKED PriorityPayload {
 public:
  explicit PriorityPayload() = default;

  PriorityPayload(PriorityPayload const&) = default;
  PriorityPayload& operator=(PriorityPayload const&) = default;

  bool exclusive() const { return exclusive_ != 0; }

  PriorityPayload& set_exclusive(bool const value) {
    exclusive_ = value ? 1 : 0;
    return *this;
  }

  uint32_t stream_dependency() const { return ::ntohl(stream_dependency_); }

  PriorityPayload& set_stream_dependency(uint32_t const value) {
    stream_dependency_ = ::htonl(value);
    return *this;
  }

  uint32_t weight() const { return weight_; }

  PriorityPayload& set_weight(uint32_t const value) {
    weight_ = value;
    return *this;
  }

 private:
  unsigned int exclusive_ : 1;
  unsigned int stream_dependency_ : 31;
  unsigned int weight_ : 8;
};

static_assert(sizeof(PriorityPayload) == 5, "incorrect PRIORITY payload size");

struct ABSL_ATTRIBUTE_PACKED ResetStreamPayload {
 public:
  explicit ResetStreamPayload() = default;

  ResetStreamPayload(ResetStreamPayload const&) = default;
  ResetStreamPayload& operator=(ResetStreamPayload const&) = default;

  ConnectionError error_code() const { return static_cast<ConnectionError>(::ntohl(error_code_)); }

  ResetStreamPayload& set_error_code(ConnectionError const error) {
    error_code_ = ::htonl(tsdb2::util::to_underlying(error));
    return *this;
  }

 private:
  unsigned int error_code_ : 32;
};

static_assert(sizeof(ResetStreamPayload) == 4, "incorrect RST_STREAM payload size");

enum class SettingsIdentifier {
  kHeaderTableSize = 1,
  kEnablePush = 2,
  kMaxConcurrentStreams = 3,
  kInitialWindowSize = 4,
  kMaxFrameSize = 5,
  kMaxHeaderListSize = 6,
};

inline size_t constexpr kNumSettings = 6;

struct ABSL_ATTRIBUTE_PACKED SettingsEntry {
 public:
  explicit SettingsEntry() = default;

  SettingsEntry(SettingsEntry const&) = default;
  SettingsEntry& operator=(SettingsEntry const&) = default;

  SettingsIdentifier identifier() const {
    return static_cast<SettingsIdentifier>(::ntohs(identifier_));
  }

  SettingsEntry& set_identifier(SettingsIdentifier const value) {
    identifier_ = ::htons(tsdb2::util::to_underlying(value));
    return *this;
  }

  uint32_t value() const { return ::ntohl(value_); }

  SettingsEntry& set_value(uint32_t const new_value) {
    value_ = ::htonl(new_value);
    return *this;
  }

 private:
  uint16_t identifier_;
  uint32_t value_;
};

static_assert(sizeof(SettingsEntry) == 6, "incorrect settings entry size");

inline size_t constexpr kPingPayloadSize = 8;

struct ABSL_ATTRIBUTE_PACKED GoAwayPayload {
 public:
  explicit GoAwayPayload() : reserved_(0) {}

  GoAwayPayload(GoAwayPayload const&) = default;
  GoAwayPayload& operator=(GoAwayPayload const&) = default;

  uint32_t last_stream_id() const { return ::ntohl(last_stream_id_); }

  GoAwayPayload& set_last_stream_id(uint32_t const id) {
    last_stream_id_ = ::htonl(id);
    return *this;
  }

  ConnectionError error_code() const { return static_cast<ConnectionError>(::ntohl(error_code_)); }

  GoAwayPayload& set_error_code(ConnectionError const error) {
    error_code_ = ::htonl(tsdb2::util::to_underlying(error));
    return *this;
  }

 private:
  unsigned int reserved_ : 1 ABSL_ATTRIBUTE_UNUSED;
  unsigned int last_stream_id_ : 31;
  unsigned int error_code_ : 32;
};

static_assert(sizeof(GoAwayPayload) == 8, "incorrect GOAWAY payload size");

struct ABSL_ATTRIBUTE_PACKED WindowUpdatePayload {
 public:
  explicit WindowUpdatePayload() : reserved_(0) {}

  WindowUpdatePayload(WindowUpdatePayload const&) = default;
  WindowUpdatePayload& operator=(WindowUpdatePayload const&) = default;

  size_t window_size_increment() const { return ::ntohl(window_size_increment_); }

  WindowUpdatePayload& set_window_size_increment(size_t const value) {
    window_size_increment_ = ::htonl(value);
    return *this;
  }

 private:
  unsigned int reserved_ : 1 ABSL_ATTRIBUTE_UNUSED;
  unsigned int window_size_increment_ : 31;
};

static_assert(sizeof(WindowUpdatePayload) == 4, "incorrect WINDOW_UPDATE payload size");

enum class Method { kGet, kHead, kPost, kPut, kDelete, kConnect, kOptions, kTrace };

enum class Status {
  k200 = 200,  // OK
  k201 = 201,  // Created
  k202 = 202,  // Accepted
  k203 = 203,  // Non-Authoritative Information
  k204 = 204,  // No Content
  k205 = 205,  // Reset Content
  k206 = 206,  // Partial Content
  k300 = 300,  // Multiple Choices
  k301 = 301,  // Moved Permanently
  k302 = 302,  // Found
  k303 = 303,  // See Other
  k304 = 304,  // Not Modified
  k305 = 305,  // Use Proxy
  k307 = 307,  // Temporary Redirect
  k308 = 308,  // Permanent Redirect
  k400 = 400,  // Bad Request
  k401 = 401,  // Unauthorized
  k402 = 402,  // Payment Required
  k403 = 403,  // Forbidden
  k404 = 404,  // Not Found
  k405 = 405,  // Method Not Allowed
  k406 = 406,  // Not Acceptable
  k407 = 407,  // Proxy Authentication Required
  k408 = 408,  // Request Timeout
  k409 = 409,  // Conflict
  k410 = 410,  // Gone
  k411 = 411,  // Length Required
  k412 = 412,  // Precondition Failed
  k413 = 413,  // Content Too Large
  k414 = 414,  // URI Too Long
  k415 = 415,  // Unsupported Media Type
  k416 = 416,  // Range Not Satisfiable
  k417 = 417,  // Expectation Failed
  k421 = 421,  // Misdirected Request
  k422 = 422,  // Unprocessable Content
  k426 = 426,  // Upgrade Required
  k500 = 500,  // Internal Server Error
  k501 = 501,  // Not Implemented
  k502 = 502,  // Bad Gateway
  k503 = 503,  // Service Unavailable
  k504 = 504,  // Gateway Timeout
  k505 = 505,  // HTTP Version Not Supported
};

inline size_t constexpr kNumMethods = 42;

extern tsdb2::common::fixed_flat_map<int, std::string_view, kNumMethods> const kMethodNames;

struct Request {
  Method method;
  std::string path;
  tsdb2::common::flat_map<std::string, std::string> cookies;
  std::string user_agent;
};

struct Response {
  // TODO
};

}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_HTTP_H__
