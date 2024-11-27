#include "net/alpn.h"

#include <openssl/ssl.h>

#include "absl/log/check.h"

namespace tsdb2 {
namespace net {
namespace internal {

namespace {

unsigned char const kAlpnProtocols[] = "\x02h2";

int AlpnCallback(::SSL* const ssl, unsigned char const** const out, unsigned char* const outlen,
                 unsigned char const* const in, unsigned int const inlen, void* const arg) {
  if (SSL_select_next_proto((unsigned char**)out, outlen, kAlpnProtocols,
                            sizeof(kAlpnProtocols) - 1, in, inlen) != OPENSSL_NPN_NEGOTIATED) {
    return SSL_TLSEXT_ERR_NOACK;
  }
  return SSL_TLSEXT_ERR_OK;
}

}  // namespace

void ConfigureAlpn(::SSL_CTX* const context) {
  CHECK_EQ(SSL_CTX_set_alpn_protos(context, kAlpnProtocols, sizeof(kAlpnProtocols) - 1), 0)
      << "SSL_CTX_set_alpn_protos(\"\\x02h2\")";
  SSL_CTX_set_alpn_select_cb(context, &AlpnCallback, nullptr);
}

}  // namespace internal
}  // namespace net
}  // namespace tsdb2
