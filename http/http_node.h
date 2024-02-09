#ifndef __TSDB2_HTTP_HTTP_NODE_H__
#define __TSDB2_HTTP_HTTP_NODE_H__

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "common/buffer.h"
#include "common/reffed_ptr.h"
#include "common/utilities.h"
#include "http/http.h"
#include "net/sockets.h"

namespace tsdb2 {
namespace http {

template <typename Socket>
class HttpConnection : public Socket {
 public:
  static bool constexpr kIsListener = false;

  static absl::StatusOr<std::unique_ptr<HttpConnection>> Create(
      tsdb2::net::SelectServer* const parent, tsdb2::net::FD fd) {
    std::unique_ptr<HttpConnection> connection{new HttpConnection(parent, std::move(fd))};
    auto preface_status = connection->ServerPreface();
    if (preface_status.ok()) {
      return connection;
    } else {
      return preface_status;
    }
  }

 private:
  explicit HttpConnection(tsdb2::net::SelectServer* const parent, tsdb2::net::FD fd)
      : Socket(parent, Socket::kConnectedTag, std::move(fd)) {}

  absl::Status ReadClientPreface(absl::StatusOr<tsdb2::net::Buffer> status_or_buffer);

  absl::Status ServerPreface();
};

template <typename Socket>
absl::Status HttpConnection<Socket>::ReadClientPreface(
    absl::StatusOr<tsdb2::net::Buffer> status_or_buffer) {
  if (!status_or_buffer.ok()) {
    return status_or_buffer.status();
  }
  auto const& buffer = status_or_buffer.value();
  std::string_view preface{buffer.as_char_array(), buffer.size()};
  if (preface != kClientPreface) {
    return absl::UnavailableError(absl::StrCat("the client sent an invalid HTTP/2 preface: \"",
                                               absl::CEscape(preface), "\""));
  }
  // TODO: start reading and processing frames.
  return absl::OkStatus();
}

template <typename Socket>
absl::Status HttpConnection<Socket>::ServerPreface() {
  RETURN_IF_ERROR(this->Read(kFrameHeaderSize,
                             absl::bind_front(&HttpConnection<Socket>::ReadClientPreface, this)));
  FrameHeader const header{
      .length = 0,
      .type = kFrameTypeSettings,
      .flags = 0,
      .reserved = 0,
      .stream_id = 0,
  };
  return this->Write(tsdb2::net::Buffer(&header, kFrameHeaderSize),
                     [](absl::Status) { return absl::OkStatus(); });
}

// An HTTP client and server supporting both HTTP/1.1 and HTTP/2.
//
// The implementation uses `SelectServer`, so the underlying sockets are dual-stack and it will be
// possible to connect to this server both via IPv4 and IPv6.
class HttpNode {
 public:
  // Constructs an HTTP server bound to the specified address and listening on the specified port.
  // If the address is an empty string the server will bind to `INADDR6_ANY`.
  static absl::StatusOr<std::unique_ptr<HttpNode>> Create(std::string_view address, uint16_t port,
                                                          tsdb2::net::SocketOptions const& options);

  // Shorthand for `Create("", port, options)`.
  static absl::StatusOr<std::unique_ptr<HttpNode>> Create(
      uint16_t const port, tsdb2::net::SocketOptions const& options) {
    return Create("", port, options);
  }

  // Returns a default singleton `HttpNode` instance.
  //
  // The singleton instance takes its local address and port from the --local_address and --port
  // command line flags respectively. It's created the first time `GetDefault()` is invoked and is
  // never destroyed.
  static HttpNode* GetDefault();

  // Returns the local address this server is bound to. An empty string indicates it was bound to
  // INADDR6_ANY.
  std::string_view local_address() const { return listener_->address(); }

  // Returns the local TCP/IP port this server is listening on.
  uint16_t port() const { return listener_->port(); }

  void WaitForTermination() { termination_.WaitForNotification(); }

 private:
  HttpNode(HttpNode const&) = delete;
  HttpNode& operator=(HttpNode const&) = delete;
  HttpNode(HttpNode&&) = delete;
  HttpNode& operator=(HttpNode&&) = delete;

  explicit HttpNode() = default;

  absl::Status Listen(std::string_view address, uint16_t port,
                      tsdb2::net::SocketOptions const& options);

  void AcceptCallback(absl::StatusOr<tsdb2::common::reffed_ptr<HttpConnection<tsdb2::net::Socket>>>
                          status_or_socket);

  tsdb2::common::reffed_ptr<tsdb2::net::ListenerSocket<HttpConnection<tsdb2::net::Socket>>>
      listener_;

  absl::Notification termination_;
};

}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_HTTP_SERVER_H__
