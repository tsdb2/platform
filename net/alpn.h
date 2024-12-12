#ifndef __TSDB2_NET_ALPN_H__
#define __TSDB2_NET_ALPN_H__

#include <openssl/crypto.h>
#include <openssl/ssl.h>

namespace tsdb2 {
namespace net {
namespace internal {

// Configures ALPN on the provided SSL context. The configuration specifies that the server only
// supports HTTP/2.
void ConfigureAlpn(::SSL_CTX* context);

}  // namespace internal
}  // namespace net
}  // namespace tsdb2

#endif  // __TSDB2_NET_ALPN_H__
