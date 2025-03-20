#include "net/ssl.h"

#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/prov_ssl.h>
#include <openssl/ssl.h>
#include <openssl/types.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "common/env.h"
#include "common/no_destructor.h"
#include "common/singleton.h"
#include "common/utilities.h"
#include "io/fd.h"
#include "net/alpn.h"
#include "server/module.h"

namespace tsdb2 {
namespace net {
namespace internal {

namespace {

std::string_view constexpr kCertificateEnvVarKey = "SSL_CERTIFICATE_PATH";
std::string_view constexpr kPrivateKeyEnvVarKey = "SSL_PRIVATE_KEY_PATH";
std::string_view constexpr kPassphraseEnvVarKey = "SSL_PASSPHRASE";

int PassphraseCallback(char* const buffer, int const size, int const rwflag,
                       void* const user_data) {
  char const* const passphrase = reinterpret_cast<char const*>(user_data);
  if (passphrase == nullptr) {
    return 0;
  }
  int length = std::strlen(passphrase);
  length = std::min(length, size);
  std::memcpy(buffer, passphrase, length + 1);
  return length;
}

}  // namespace

void LogSSLErrors(std::string_view const file, int const line) {
  static tsdb2::common::NoDestructor<absl::Mutex> mutex;
  if (!mutex->TryLock()) {
    // another thread is already emptying the error queue.
    return;
  }
  size_t constexpr kMaxErrorStringLength = 1024;
  char buffer[kMaxErrorStringLength];
  while (auto const error = ERR_get_error()) {
    ERR_error_string_n(error, buffer, kMaxErrorStringLength);
    LOG(ERROR).AtLocation(file, line) << buffer;
  }
  mutex->Unlock();
}

tsdb2::common::Singleton<SSLContext const> SSLContext::server_context_{
    &SSLContext::CreateServerContext};

tsdb2::common::Singleton<SSLContext const> SSLContext::client_context_{
    &SSLContext::CreateClientContext};

absl::StatusOr<SSL> SSLContext::MakeSSL(tsdb2::io::FD const& fd) const {
  ::SSL* const native_ssl = SSL_new(context_);
  if (native_ssl == nullptr) {
    return absl::UnknownError("SSL_new");
  }
  SSL ssl{native_ssl};
  RETURN_IF_ERROR(ssl.SetFD(fd));
  SSL_set_options(ssl.get(), SSL_OP_NO_RENEGOTIATION);
  SSL_set_options(ssl.get(), SSL_OP_NO_TICKET);
  return std::move(ssl);
}

gsl::owner<SSLContext*> SSLContext::CreateServerContext() {
  auto const* const method = TLS_server_method();
  CHECK(method != nullptr) << "TLS_server_method";

  ::SSL_CTX* const context = SSL_CTX_new(method);
  CHECK(context != nullptr) << "SSL_CTX_new";

  CHECK_GT(SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION), 0)
      << "SSL_CTX_set_min_proto_version(TLS1_2_VERSION)";
  CHECK_GT(SSL_CTX_set_max_proto_version(context, TLS1_3_VERSION), 0)
      << "SSL_CTX_set_max_proto_version(TLS1_3_VERSION)";

  auto const maybe_certificate_path = tsdb2::common::GetEnv(std::string(kCertificateEnvVarKey));
  CHECK(maybe_certificate_path.has_value())
      << kCertificateEnvVarKey
      << " environment variable not set, cannot find the SSL certificate file.";
  CHECK_GT(SSL_CTX_use_certificate_file(context, maybe_certificate_path->c_str(), SSL_FILETYPE_PEM),
           0)
      << "SSL_CTX_use_certificate_file";

  auto const maybe_passphrase = tsdb2::common::GetEnv(std::string(kPassphraseEnvVarKey));
  if (maybe_passphrase) {
    static tsdb2::common::NoDestructor<std::string> const passphrase{maybe_passphrase.value()};
    SSL_CTX_set_default_passwd_cb(context, &PassphraseCallback);
    SSL_CTX_set_default_passwd_cb_userdata(context, const_cast<char*>(passphrase->c_str()));
  } else {
    LOG(INFO)
        << kPassphraseEnvVarKey
        << " environment variable not set, will try to read the private key without a passphrase";
  }

  auto const maybe_private_key_path = tsdb2::common::GetEnv(std::string(kPrivateKeyEnvVarKey));
  CHECK(maybe_private_key_path.has_value())
      << kPrivateKeyEnvVarKey
      << " environment variable not set, cannot find the SSL private key file.";
  CHECK_GT(SSL_CTX_use_PrivateKey_file(context, maybe_private_key_path->c_str(), SSL_FILETYPE_PEM),
           0)
      << "SSL_CTX_use_PrivateKey_file";

  SSL_CTX_set_session_cache_mode(context, SSL_SESS_CACHE_OFF);
  SSL_CTX_set_options(context, SSL_OP_NO_RENEGOTIATION);
  SSL_CTX_set_options(context, SSL_OP_NO_TICKET);

  ConfigureAlpn(context);

  return new SSLContext(context);
}

gsl::owner<SSLContext*> SSLContext::CreateClientContext() {
  auto const* const method = TLS_client_method();
  CHECK(method != nullptr) << "TLS_client_method";

  ::SSL_CTX* const context = SSL_CTX_new(method);
  CHECK(context != nullptr) << "SSL_CTX_new";

  CHECK_GT(SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION), 0)
      << "SSL_CTX_set_min_proto_version(TLS1_2_VERSION)";
  CHECK_GT(SSL_CTX_set_max_proto_version(context, TLS1_3_VERSION), 0)
      << "SSL_CTX_set_max_proto_version(TLS1_3_VERSION)";

  SSL_CTX_set_session_cache_mode(context, SSL_SESS_CACHE_OFF);
  SSL_CTX_set_options(context, SSL_OP_NO_RENEGOTIATION);
  SSL_CTX_set_options(context, SSL_OP_NO_TICKET);

  ConfigureAlpn(context);

  return new SSLContext(context);
}

static tsdb2::init::Module<SSLModule> const ssl_module;

absl::Status SSLModule::Initialize() {  // NOLINT(readability-convert-member-functions-to-static)
  auto status = InitializeInternal();
  if (!status.ok()) {
    TSDB2_SSL_LOG_ERRORS();
  }
  return status;
}

absl::Status
SSLModule::InitializeForTesting() {  // NOLINT(readability-convert-member-functions-to-static)
  if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS, nullptr) != 0) {
    return absl::OkStatus();
  } else {
    return absl::FailedPreconditionError("OpenSSL initialization failed");
  }
}

absl::Status SSLModule::InitializeInternal() {
  if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS, nullptr) == 0) {
    return absl::FailedPreconditionError("OpenSSL initialization failed");
  }
  SSLContext::GetServerContext();
  SSLContext::GetClientContext();
  return absl::OkStatus();
}

}  // namespace internal
}  // namespace net
}  // namespace tsdb2
