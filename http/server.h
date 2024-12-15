#ifndef __TSDB2_HTTP_SERVER_H__
#define __TSDB2_HTTP_SERVER_H__

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "common/reffed_ptr.h"
#include "common/trie_map.h"
#include "http/channel.h"
#include "http/http.h"
#include "net/base_sockets.h"
#include "net/sockets.h"
#include "net/ssl_sockets.h"

namespace tsdb2 {
namespace http {

class Handler {
 public:
  explicit Handler() = default;
  virtual ~Handler() = default;
  virtual absl::Status operator()(Request const& request, Response* response) = 0;

 private:
  Handler(Handler const&) = delete;
  Handler& operator=(Handler const&) = delete;
  Handler(Handler&&) = delete;
  Handler& operator=(Handler&&) = delete;
};

// An HTTP/2 server.
//
// TODO: consider adding support for HTTP/1.1 (which gRPC can still use via gRPC-Web). Simpler
// microcontrollers and embedded devices may not have enough resources for a full-fledged HTTP/2
// implementation.
//
// The implementation uses `EpollServer` and the underlying sockets are dual-stack, so it will be
// possible to connect to this server both via IPv4 and IPv6.
class Server final : public ChannelManager {
 public:
  struct Binding {
    std::string address;
    uint16_t port;
  };

  // Constructs an HTTP server bound to the specified address and listening on the specified port.
  // If the address is an empty string the server will bind to `INADDR6_ANY`.
  static absl::StatusOr<std::unique_ptr<Server>> Create(std::string_view address, uint16_t port,
                                                        bool use_ssl,
                                                        tsdb2::net::SocketOptions const& options);

  // Shorthand for `Create("", port, use_ssl, options)`.
  static absl::StatusOr<std::unique_ptr<Server>> Create(uint16_t const port, bool const use_ssl,
                                                        tsdb2::net::SocketOptions const& options) {
    return Create("", port, use_ssl, options);
  }

  // Returns a default singleton `Server` instance.
  //
  // The singleton instance takes its local address and port from the --local_address and --port
  // command line flags respectively. It's created the first time `GetDefault()` is invoked and is
  // never destroyed.
  static Server* GetDefault();

  // Returns the local address & TCP port this server is bound to. An empty string for the address
  // indicates it was bound to INADDR6_ANY.
  Binding local_binding() const;

  // Blocks while the server is running and either returns an error status when the underlying
  // listener socket fails for any reason or it returns OK when the server receives /quitquitquit.
  absl::Status WaitForTermination() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  struct HashChannel {
    using is_transparent = void;
    size_t operator()(BaseChannel* const channel) const { return absl::HashOf(channel); }
    size_t operator()(tsdb2::common::reffed_ptr<BaseChannel> const& channel) const {
      return absl::HashOf(channel.get());
    }
  };

  struct CompareChannels {
    using is_transparent = void;

    static BaseChannel* ToRawPtr(BaseChannel* const channel) { return channel; }
    static BaseChannel* ToRawPtr(tsdb2::common::reffed_ptr<BaseChannel> const& channel) {
      return channel.get();
    }

    template <typename LHS, typename RHS>
    bool operator()(LHS&& lhs, RHS&& rhs) const {
      return ToRawPtr(std::forward<LHS>(lhs)) == ToRawPtr(std::forward<RHS>(rhs));
    }
  };

  using ChannelSet =
      absl::flat_hash_set<tsdb2::common::reffed_ptr<BaseChannel>, HashChannel, CompareChannels>;

  explicit Server() = default;

  tsdb2::common::reffed_ptr<tsdb2::net::BaseListenerSocket> listener() const
      ABSL_LOCKS_EXCLUDED(listener_mutex_);

  void RemoveChannel(BaseChannel* channel) override ABSL_LOCKS_EXCLUDED(mutex_);

  absl::Status AcceptInternal(
      absl::StatusOr<tsdb2::common::reffed_ptr<BaseChannel>> status_or_socket)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void AcceptCallbackImpl(
      absl::StatusOr<tsdb2::common::reffed_ptr<Channel<tsdb2::net::Socket>>> status_or_socket);

  static void AcceptCallback(
      void* arg,
      absl::StatusOr<tsdb2::common::reffed_ptr<Channel<tsdb2::net::Socket>>> status_or_socket);

  void AcceptSSLCallbackImpl(
      absl::StatusOr<tsdb2::common::reffed_ptr<Channel<tsdb2::net::SSLSocket>>> status_or_socket);

  static void AcceptSSLCallback(
      void* arg,
      absl::StatusOr<tsdb2::common::reffed_ptr<Channel<tsdb2::net::SSLSocket>>> status_or_socket);

  absl::Status Listen(std::string_view address, uint16_t port, bool use_ssl,
                      tsdb2::net::SocketOptions const& options);

  tsdb2::common::trie_map<std::unique_ptr<Handler>> const handlers_;

  absl::Mutex mutable mutex_;

  ChannelSet channels_ ABSL_GUARDED_BY(mutex_);

  std::optional<absl::Status> termination_status_ ABSL_GUARDED_BY(mutex_) = std::nullopt;

  absl::Mutex mutable listener_mutex_;
  tsdb2::common::reffed_ptr<tsdb2::net::BaseListenerSocket> listener_
      ABSL_GUARDED_BY(listener_mutex_);
};

}  // namespace http
}  // namespace tsdb2

#endif  // __TSDB2_HTTP_SERVER_H__
