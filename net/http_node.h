#ifndef __TSDB2_NET_HTTP_NODE_H__
#define __TSDB2_NET_HTTP_NODE_H__

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "common/buffer.h"
#include "common/reffed_ptr.h"
#include "net/sockets.h"

namespace tsdb2 {
namespace net {

// An HTTP client and server supporting both HTTP/1.1 and HTTP/2.
//
// The implementation uses `SelectServer`, so the underlying sockets are dual-stack and it will be
// possible to connect to this server both via IPv4 and IPv6.
class HttpNode {
 public:
  // Constructs an HTTP server bound to the specified address and listening on the specified port.
  // If the address is an empty string the server will bind to `INADDR6_ANY`.
  static absl::StatusOr<std::unique_ptr<HttpNode>> Create(std::string_view address, uint16_t port,
                                                          SocketOptions const& options);

  // Shorthand for `Create("", port, options)`.
  static absl::StatusOr<std::unique_ptr<HttpNode>> Create(uint16_t const port,
                                                          SocketOptions const& options) {
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
  // Handles a single network connection. This class is used for both accepted and initiated
  // connections.
  class Connection {
   public:
    // Custom hash & eq functors to store connections in hash table data structures indexing them by
    // socket pointer (our socket classes guarantee pointer stability).
    struct HashEq {
      static ::tsdb2::common::reffed_ptr<Socket> const& GetSocket(Connection const& connection) {
        return connection.socket_;
      }

      static ::tsdb2::common::reffed_ptr<Socket> const& GetSocket(Connection* const connection) {
        return connection->socket_;
      }

      static ::tsdb2::common::reffed_ptr<Socket> const& GetSocket(
          std::unique_ptr<Connection> const& connection) {
        return connection->socket_;
      }

      struct Hash {
        using is_transparent = void;
        template <typename Value>
        size_t operator()(Value&& value) const {
          return absl::HashOf(GetSocket(std::forward<Value>(value)));
        }
      };

      struct Eq {
        using is_transparent = void;
        template <typename LHS, typename RHS>
        bool operator()(LHS&& lhs, RHS&& rhs) const {
          return GetSocket(std::forward<LHS>(lhs)) == GetSocket(std::forward<RHS>(rhs));
        }
      };
    };

    explicit Connection(HttpNode* const parent, ::tsdb2::common::reffed_ptr<Socket> socket)
        : parent_(parent), socket_(std::move(socket)) {}

    absl::Status ServerPreface();

   private:
    using ReadCallback = absl::AnyInvocable<void(Buffer const& buffer)>;

    void Destroy() { parent_->RemoveConnection(*this); }

    absl::Status Read(size_t length, ReadCallback callback);

    Connection(Connection const&) = delete;
    Connection& operator=(Connection const&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(Connection&&) = delete;

    HttpNode* const parent_;

    ::tsdb2::common::reffed_ptr<Socket> socket_;
  };

  using ConnectionSet = absl::flat_hash_set<std::unique_ptr<Connection>, Connection::HashEq::Hash,
                                            Connection::HashEq::Eq>;

  HttpNode(HttpNode const&) = delete;
  HttpNode& operator=(HttpNode const&) = delete;

  explicit HttpNode() = default;

  absl::Status Listen(std::string_view address, uint16_t port, SocketOptions const& options);

  void AcceptCallback(absl::StatusOr<::tsdb2::common::reffed_ptr<Socket>> status_or_socket)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void RemoveConnection(Connection const& connection) ABSL_LOCKS_EXCLUDED(mutex_);

  ::tsdb2::common::reffed_ptr<ListenerSocket> listener_;

  absl::Mutex mutable mutex_;
  ConnectionSet connections_ ABSL_GUARDED_BY(mutex_);

  absl::Notification termination_;
};

}  // namespace net
}  // namespace tsdb2

#endif  // __TSDB2_NET_HTTP_SERVER_H__
