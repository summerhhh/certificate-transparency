#ifndef CERT_TRANS_CLIENT_SSL_CLIENT_H_
#define CERT_TRANS_CLIENT_SSL_CLIENT_H_

#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "client/client.h"
#include "client/ssl_client.h"
#include "log/log_verifier.h"
#include "proto/ct.pb.h"
#include "util/openssl_scoped_ssl_types.h"

class LogVerifier;

namespace cert_trans {


class SSLClient {
 public:
  // Takes ownership of the verifier. This client can currently
  // only verify SCTs from a single log at a time.
  // TODO(ekasper): implement a proper multi-log auditor.
  SSLClient(const std::string& server, const std::string& port,
            const std::string& ca_dir, LogVerifier* verifier);

  ~SSLClient();
  SSLClient(const SSLClient&) = delete;
  SSLClient& operator=(const SSLClient&) = delete;

  enum HandshakeResult {
    OK = 0,
    HANDSHAKE_FAILED = 1,
    SERVER_UNAVAILABLE = 2,
  };

  HandshakeResult SSLConnect() {
    return SSLConnect(false);
  }

  // Same as above but won't proceed without an SCT.
  HandshakeResult SSLConnectStrict() {
    return SSLConnect(true);
  }

  bool Connected() const;

  void Disconnect();

  void GetSSLClientCTData(ct::SSLClientCTData* data) const;

  // Need a static wrapper for the callback.
  static LogVerifier::LogVerifyResult VerifySCT(const std::string& token,
                                                LogVerifier* verifier,
                                                ct::SSLClientCTData* data);

  // Custom verification callback for verifying the SCT token
  // in a superfluous certificate. Return values:
  // With TLS extension support:
  //  1 - cert verified (SCT might still be in TLS extension which is
  //      parsed in a later callback; we record whether it was verified
  //       in the callback args)
  // other values - cert verification errors.
  // Without TLS extension support, strict mode
  // 1 - cert and SCT verified
  // other values - everything else
  // Without TLS extension support, standard mode
  // 1 - cert verified (we record whether an SCT was also verified in the
  //     callback args)
  // other values - cert verification error
  static int VerifyCallback(X509_STORE_CTX* ctx, void* arg);

  static int ExtensionCallback(SSL* s, unsigned ext_type,
                               const unsigned char* in, size_t inlen, int* al,
                               void* arg);

 private:
  Client client_;
  cert_trans::ScopedSSL_CTX ctx_;
  cert_trans::ScopedSSL ssl_;
  struct VerifyCallbackArgs {
    VerifyCallbackArgs(LogVerifier* log_verifier)
        : verifier(log_verifier),
          sct_verified(false),
          require_sct(false),
          ct_data() {
    }

    // The verifier for checking log proofs.
    std::unique_ptr<LogVerifier> verifier;
    // SCT verification result.
    bool sct_verified;
    bool require_sct;
    std::string ct_extension;
    // The resulting (partial) entry - the client reconstructs
    // the signed part of the entry (i.e., type and leaf certificate)
    // and all valid SCTs.
    ct::SSLClientCTData ct_data;
  };

  VerifyCallbackArgs verify_args_;
  bool connected_;

  // Call before each handshake.
  void ResetVerifyCallbackArgs(bool strict);

  HandshakeResult SSLConnect(bool strict);
};


}  // namespace cert_trans

#endif  // CERT_TRANS_CLIENT_SSL_CLIENT_H_
