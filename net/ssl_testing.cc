#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "common/no_destructor.h"
#include "common/scoped_override.h"
#include "common/singleton.h"
#include "net/alpn.h"
#include "net/ssl.h"
#include "net/ssl_sockets.h"
#include "server/base_module.h"
#include "server/init_tsdb2.h"

namespace tsdb2 {
namespace net {
namespace internal {

namespace {

char constexpr kCertificate[] = R"pem(
-----BEGIN CERTIFICATE-----
MIIDkzCCAnugAwIBAgIUJoATFqPfrOaKXpffYcu/hVS5XBowDQYJKoZIhvcNAQEL
BQAwWTELMAkGA1UEBhMCVVMxDTALBgNVBAgMBFRlc3QxDTALBgNVBAcMBFRlc3Qx
ETAPBgNVBAoMCFRlc3QgT3JnMRkwFwYDVQQDDBBUZXN0IENlcnRpZmljYXRlMB4X
DTI0MDkxMjE4Mzg1MloXDTI1MDkxMjE4Mzg1MlowWTELMAkGA1UEBhMCVVMxDTAL
BgNVBAgMBFRlc3QxDTALBgNVBAcMBFRlc3QxETAPBgNVBAoMCFRlc3QgT3JnMRkw
FwYDVQQDDBBUZXN0IENlcnRpZmljYXRlMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8A
MIIBCgKCAQEAoy5zdO8nfJm8/NQejZIJffGp3enPvECOTuXB3gzfSC2JNTUUm4pe
jCIzsMabaLWerj46v8Y6E7tA4rKqrfN6s/i9zfsfI2sVX2Eidu4Pd+wjvgqY1KJw
24eQDZg1LsOqdCSy1FJ2uferxBhRcRlOU35cLTGJsnDj/sGRifcfgDYmAJyMcs93
NSD/VEdlNkKCo5RnLz6rIQ9Y1YE+akO1BtDgVcnax/SBbsOYi0QoJNdSbeFKvwWO
5GHWehql7vuT/ZMnaz9/mT0UW/JgMl7E+DJBg+egnlP1HzTKWKU4YElDOcleByzq
+UC0jSTVU3YnCTzSeSbR2597bzdgCpMIfQIDAQABo1MwUTAdBgNVHQ4EFgQUoOrM
h6ZebvX+VqCAyQrWHK6rQRYwHwYDVR0jBBgwFoAUoOrMh6ZebvX+VqCAyQrWHK6r
QRYwDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAckoTC6l9JViS
GuLnhs73No9PBD5VkeTexpcbblH4cAovGTUOkCkt16yRJgeNgFxN0mGVCJyVSpz0
e/qMjChceJRz//qhJvLAu2tcKNdsRSXhE8D0Zk/IPGQiuySvTKTxLX18RlYwxAnF
6XHuOiqe5cdR8b2ewAlpDIj/28QFizRfh4R0h9aq1eWMPw6eVJ5Z5nZ9pR2JXi3i
7DY2qg39f/LOss3XEAz1fkoWfWycnXCfUYSHzMOuOJFSh9TfNZBtLbBGethAUmYV
NKQ6XD2F1Fj4yLKE/pkwocqvIl0xs/kCzaZixH/0FFv8OxGDAi9XEF1f86tTQL3V
d8uUlDCspg==
-----END CERTIFICATE-----
)pem";

// WARNING: this key is obviously leaked and is NOT SAFE. USE ONLY IN UNIT TESTS.
char constexpr kUnsafePrivateKey[] = R"pem(
-----BEGIN PRIVATE KEY-----
MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQCjLnN07yd8mbz8
1B6Nkgl98and6c+8QI5O5cHeDN9ILYk1NRSbil6MIjOwxptotZ6uPjq/xjoTu0Di
sqqt83qz+L3N+x8jaxVfYSJ27g937CO+CpjUonDbh5ANmDUuw6p0JLLUUna596vE
GFFxGU5TflwtMYmycOP+wZGJ9x+ANiYAnIxyz3c1IP9UR2U2QoKjlGcvPqshD1jV
gT5qQ7UG0OBVydrH9IFuw5iLRCgk11Jt4Uq/BY7kYdZ6GqXu+5P9kydrP3+ZPRRb
8mAyXsT4MkGD56CeU/UfNMpYpThgSUM5yV4HLOr5QLSNJNVTdicJPNJ5JtHbn3tv
N2AKkwh9AgMBAAECggEBAIUacaDTJoFTIb2VBGrz0PxDMAzMF991RN8EOBN4qnRE
eGPHLU0GG8LwbvtltQQ/CPUV23PsLBlGJ1IC7IfBT7gXUDMBAFAym5dWMveRjVqD
alAscqcW7TmUJSOBoPfJMHNWL+xSa3eblyb3sw6u1p3yNxvCCZBc1wxJNf60baR0
uLyrcaEfEWHQOST2dPGtInyX2aEUBCneOEaKlH4tZBhuYloqu3gIHJESiidSWLr8
atSEbkBatCem2wmtVmkdIogmoGm0U+zSkCmZDiYfNDRS1NDiY9rTI/mETPpZ7b74
gT0An9zSpFpXVefC0h8RWn4oZfVaxgRx9tIHzxIjEeECgYEA11+Egpuajp3272d8
bQTlTek++fK9vLg+tbmWuxsvqfbNW51T0D6ZPMVTwLyz/xYUXprar1M5usizS/j1
aV8bNJ71Mc6C1AagsTztSxNMh359Eh4/Juwpy6E8fpT8pfAqp0bbPDIBOZBJeTp6
U3VexYzRUlwq95QMBsSHotW20+UCgYEAwfaR7cPztJCCdyYTIkICiXpvEL0531vF
4qipk0krWrA5VRGCnR75JBAesHUKuiUutBbXEaBdN4w/4XamtDXd1gvw1pBzLnxr
7a1yAw6LB2WNFkkWKVeQSA05PtEYuQNUOryZ0JFaSvw2H4SjtScBOYUt6O8KWMvf
xmw8e/e1yLkCgYEAlqjd1FOvgbak6AIXe1fiZGcWw9h3vA2S6KLD+21gIWBhdYYP
/Gvd3DIZjYkzzOyQIUHoWp84kh4Vtr6YRjbenCfaVBYnVNSyEmoRgOQmM95a9ZKt
ELhB4I2Q+OeV+SqRW+ToNiqwyqjRjPlIWxuOyVjhkOCiugAZjZ5rV5ByzbUCgYAF
Hr8naZ7LS21GO+kRZHCwtFyuMnCOptuIai9fxfSxBindRwMNpr2o6AwHWG+aDlU3
R8sRmgwb5UXia8FmzG04s0P+Rf3kYkBvG78AuaeN4G5jAjbljHwwirjSIa7nY2Eb
09KzoMKjbBj5qASySX9Mx2k41uaNBYS4ti66jwVrcQKBgQCaU6MHlJJKpaFCR8/Z
4jAT5StXbOhMbhAO1o6k0M+U81kwipe32jyaSdsX/m4ma8y3folU8SMXccbSMpXZ
JTa68JN2XswhkMTzbKJn4t5vAxpt46ppeLAf0cR3zJ2jDl7D2pB/IlIRxIXkxKGD
TgipNYf1CBN5DQ+vwKTpwnPXUQ==
-----END PRIVATE KEY-----
)pem";

}  // namespace

void SSLContext::SetUpForTesting() {
  static tsdb2::common::NoDestructor<
      tsdb2::common::ScopedOverride<tsdb2::common::Singleton<SSLContext const>>> const
      override_instance{&SSLContext::server_context_, CreateUnsafeServerContextForTesting()};
}

SSLContext *SSLContext::CreateUnsafeServerContextForTesting() {
  auto const method = TLS_server_method();
  CHECK(method != nullptr) << "TLS_server_method";

  ::SSL_CTX *const context = SSL_CTX_new(method);
  CHECK(context != nullptr) << "SSL_CTX_new";

  CHECK_GT(SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION), 0)
      << "SSL_CTX_set_min_proto_version(TLS1_2_VERSION)";
  CHECK_GT(SSL_CTX_set_max_proto_version(context, TLS1_3_VERSION), 0)
      << "SSL_CTX_set_max_proto_version(TLS1_3_VERSION)";

  BIO *const certificate_bio = BIO_new_mem_buf(kCertificate, -1);
  CHECK(certificate_bio != nullptr) << "BIO_new_mem_buf";

  X509 *const certificate = PEM_read_bio_X509(certificate_bio, nullptr, 0, nullptr);
  CHECK(certificate != nullptr) << "PEM_read_bio_X509";

  BIO *const private_key_bio = BIO_new_mem_buf(kUnsafePrivateKey, -1);
  CHECK(private_key_bio != nullptr) << "BIO_new_mem_buf";

  EVP_PKEY *const private_key = PEM_read_bio_PrivateKey(private_key_bio, nullptr, 0, nullptr);
  CHECK(private_key != nullptr) << "PEM_read_bio_PrivateKey";

  CHECK_GT(SSL_CTX_use_certificate(context, certificate), 0);
  CHECK_GT(SSL_CTX_use_PrivateKey(context, private_key), 0);
  CHECK_EQ(SSL_CTX_check_private_key(context), 1);

  BIO_free(certificate_bio);
  BIO_free(private_key_bio);

  SSL_CTX_set_session_cache_mode(context, SSL_SESS_CACHE_OFF);
  SSL_CTX_set_options(context, SSL_OP_NO_RENEGOTIATION);
  SSL_CTX_set_options(context, SSL_OP_NO_TICKET);

  ConfigureAlpn(context);

  return new SSLContext(context);
}

class SSLTestingModule : public tsdb2::init::BaseModule {
 public:
  static SSLTestingModule *Get() { return instance_.Get(); };

  absl::Status InitializeForTesting() override {
    SSLContext::SetUpForTesting();
    return absl::OkStatus();
  }

 private:
  friend class tsdb2::common::NoDestructor<SSLTestingModule>;
  static tsdb2::common::NoDestructor<SSLTestingModule> instance_;

  explicit SSLTestingModule() : BaseModule("ssl_testing") {
    tsdb2::init::RegisterModule(this, SSLModule::Get());
  }
};

tsdb2::common::NoDestructor<SSLTestingModule> instance_;

}  // namespace internal
}  // namespace net
}  // namespace tsdb2
