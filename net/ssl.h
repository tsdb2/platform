#ifndef __TSDB2_NET_SSL_H__
#define __TSDB2_NET_SSL_H__

#include <openssl/crypto.h>
#include <openssl/ssl.h>

#include <algorithm>
#include <string_view>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/no_destructor.h"
#include "common/singleton.h"
#include "common/utilities.h"
#include "io/fd.h"
#include "server/base_module.h"
#include "server/init_tsdb2.h"

namespace tsdb2 {
namespace net {
namespace internal {

// Empties the OpenSSL error stack and logs all errors to the ERROR trail. Don't call this directly,
// use the `TSDB2_SSL_LOG_ERRORS` macro which adds the source location automatically.
void LogSSLErrors(std::string_view file, int line);

#define TSDB2_SSL_LOG_ERRORS() (::tsdb2::net::internal::LogSSLErrors(__FILE__, __LINE__))

// Smart pointer object for `SSL` objects provided by OpenSSL.
class SSL final {
 public:
  explicit SSL() : ssl_(nullptr) {}
  ~SSL() { Free(); }

  SSL(SSL const& other) : ssl_(other.ssl_) { UpRef(); }

  SSL& operator=(SSL const& other) {
    if (this != &other) {
      Free();
      ssl_ = other.ssl_;
      UpRef();
    }
    return *this;
  }

  SSL(SSL&& other) noexcept : ssl_(other.ssl_) { other.ssl_ = nullptr; }

  SSL& operator=(SSL&& other) noexcept {
    if (this != &other) {
      Free();
      ssl_ = other.ssl_;
      other.ssl_ = nullptr;
    }
    return *this;
  }

  void swap(SSL& other) noexcept { std::swap(ssl_, other.ssl_); }
  friend void swap(SSL& lhs, SSL& rhs) noexcept { std::swap(lhs.ssl_, rhs.ssl_); }

  [[nodiscard]] bool empty() const { return ssl_ == nullptr; }
  explicit operator bool() const { return ssl_ != nullptr; }

  ::SSL* get() const { return ssl_; }
  ::SSL* operator->() const { return get(); }
  ::SSL& operator*() const { return *ssl_; }

  // Calls `SSL_set_fd`.
  absl::Status SetFD(tsdb2::io::FD const& fd) {
    if (SSL_set_fd(ssl_, *fd) > 0) {
      return absl::OkStatus();
    } else {
      return absl::UnknownError("SSL_set_fd");
    }
  }

 private:
  friend class SSLContext;

  explicit SSL(gsl::owner<::SSL*> const ssl) : ssl_(ssl) {}

  void Free() {
    if (ssl_ != nullptr) {
      SSL_free(ssl_);
    }
  }

  void UpRef() {
    if (ssl_ != nullptr) {
      SSL_up_ref(ssl_);
    }
  }

  gsl::owner<::SSL*> ssl_;
};

// This singleton manages a `SSL_CTX` object.
class SSLContext {
 public:
  // Returns the server-side `SSLContext` instance. Use it to accept SSL connections from a remote
  // client.
  static SSLContext const& GetServerContext() { return *server_context_; }

  // Returns the client-side `SSLContext` instance. Use it to connect SSL sockets to a remote
  // server.
  static SSLContext const& GetClientContext() { return *client_context_; }

  // Configures the SSL context for testing by loading a hard-coded self-signed certificate and a
  // hard-coded private key.
  //
  // WARNING: do NOT use this function in production, the testing setup is completely unsafe because
  // the hard-coded private key is leaked.
  static void SetUpForTesting();

  SSLContext(SSLContext&& other) noexcept : context_(other.context_) { other.context_ = nullptr; }

  SSLContext& operator=(SSLContext&& other) noexcept {
    Free();
    context_ = other.context_;
    other.context_ = nullptr;
    return *this;
  }

  void swap(SSLContext& other) noexcept { std::swap(context_, other.context_); }

  friend void swap(SSLContext& lhs, SSLContext& rhs) noexcept {
    std::swap(lhs.context_, rhs.context_);
  }

  // Constructs an `SSL` object from this SSL context.
  absl::StatusOr<SSL> MakeSSL(tsdb2::io::FD const& fd) const;

 private:
  // Constructs the server-side `SSLContext` instance. Invoked by `GetServerContext` only the first
  // time.
  static gsl::owner<SSLContext*> CreateServerContext();

  // Constructs the client-side `SSLContext` instance. Invoked by `GetClientContext` only the first
  // time.
  static gsl::owner<SSLContext*> CreateClientContext();

  // Creates a server-side `SSLContext` with an unsafe self-signed certificate and an utterly leaked
  // private key.
  //
  // WARNING: this is UNSAFE for production usage, use only in unit tests.
  static gsl::owner<SSLContext*> CreateUnsafeServerContextForTesting();

  // The singleton server-side `SSLContext` instance.
  static tsdb2::common::Singleton<SSLContext const> server_context_;

  // The singleton client-side `SSLContext` instance.
  static tsdb2::common::Singleton<SSLContext const> client_context_;

  explicit SSLContext(gsl::owner<::SSL_CTX*> const context) : context_(context) {}

  ~SSLContext() { Free(); }

  SSLContext(SSLContext const&) = delete;
  SSLContext& operator=(SSLContext const&) = delete;

  void Free() {
    if (context_ != nullptr) {
      SSL_CTX_free(context_);
    }
  }

  gsl::owner<::SSL_CTX*> context_;
};

class SSLModule : public tsdb2::init::BaseModule {
 public:
  static SSLModule* Get() { return instance_.Get(); };
  absl::Status Initialize() override;
  absl::Status InitializeForTesting() override;

 private:
  friend class tsdb2::common::NoDestructor<SSLModule>;
  static tsdb2::common::NoDestructor<SSLModule> instance_;
  explicit SSLModule() : BaseModule("ssl_lib") { tsdb2::init::RegisterModule(this); }

  static absl::Status InitializeInternal();
};

}  // namespace internal
}  // namespace net
}  // namespace tsdb2

#endif  // __TSDB2_NET_SSL_H__
