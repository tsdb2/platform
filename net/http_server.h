#include <cstdint>
#include <memory>
#include <string_view>

#include "absl/status/statusor.h"
#include "net/fd.h"
#include "net/select_server.h"

namespace tsdb2 {
namespace net {

// An HTTP server supporting both HTTP/1.1 and HTTP/2.
//
// The implementation uses `SelectServer`, so the underlying sockets are dual-stack and it will be
// possible to connect to this server both via IPv4 and IPv6.
class HttpServer {
 public:
  // Constructs an HTTP server bound to the specified address and listening on the specified port.
  // If the address is an empty string the server will bind to `INADDR6_ANY`.
  static absl::StatusOr<std::unique_ptr<HttpServer>> Create(std::string_view address,
                                                            uint16_t port);

  // Shorthand for `Create("", port)`.
  static absl::StatusOr<std::unique_ptr<HttpServer>> Create(uint16_t const port) {
    return Create("", port);
  }

  // Returns a default singleton `HttpServer` instance.
  //
  // The singleton instance takes its local address and port from the --local_address and --port
  // command line flags respectively. It's created the first time `GetDefault()` is invoked and is
  // never destroyed.
  static HttpServer* GetDefault();

  // Returns the local address this server is bound to. An empty string indicates it was bound to
  // INADDR6_ANY.
  std::string_view local_address() const { return listener_->address(); }

  // Returns the local TCP/IP port this server is listening on.
  uint16_t port() const { return listener_->port(); }

 private:
  HttpServer(HttpServer const&) = delete;
  HttpServer& operator=(HttpServer const&) = delete;

  explicit HttpServer(ListenerSocket* const listener) : listener_(listener) { Accept(); }

  void Accept();

  void AcceptCallback(absl::StatusOr<FD> fd);

  ListenerSocket* const listener_;
};

}  // namespace net
}  // namespace tsdb2
