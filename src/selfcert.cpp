#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <iostream>
int selfcert(const std::string &p) {
  OpenSSL_add_all_algorithms();
  ERR_load_crypto_strings();
  EVP_PKEY *pkey = EVP_PKEY_new();
  if (!pkey)
    return 1;
  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
  if (!ctx)
    return 1;
  if (EVP_PKEY_keygen_init(ctx) <= 0)
    return 1;
  if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0)
    return 1;
  if (EVP_PKEY_keygen(ctx, &pkey) <= 0)
    return 1;
  X509 *x509 = X509_new();
  if (!x509)
    return 1;
  X509_set_version(x509, 2);
  ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
  X509_gmtime_adj(X509_get_notBefore(x509), 0);
  X509_gmtime_adj(X509_get_notAfter(x509), 365 * 24 * 3600);
  X509_set_pubkey(x509, pkey);
  X509_NAME *name = X509_NAME_new();
  std::string cn;
  std::cout << "请输入证书的通用名（CN），如果不输入将使用默认值 "
               "'Self-Signed': ";
  std::getline(std::cin, cn);

  if (cn.empty()) {
    cn = "Self-Signed";
  }
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8,
                             (const unsigned char *)cn.c_str(), -1, -1, 0);
  X509_set_subject_name(x509, name);
  X509_set_issuer_name(x509, name);
  if (!X509_sign(x509, pkey, EVP_sha256()))
    return 1;
  auto keypath = p + "/server.pem";
  FILE *privateKeyFile = fopen(keypath.c_str(), "wb");
  if (!privateKeyFile) {
    EVP_PKEY_free(pkey);
    X509_free(x509);
    return 1;
  }

  if (PEM_write_PrivateKey(privateKeyFile, pkey, nullptr, nullptr, 0, nullptr,
                           nullptr) == 0) {
    fclose(privateKeyFile);
    EVP_PKEY_free(pkey);
    X509_free(x509);
    return 1;
  }
  fclose(privateKeyFile);
  auto certpath = p + "/server.cert";
  FILE *certFile = fopen(certpath.c_str(), "wb");
  if (!certFile) {
    EVP_PKEY_free(pkey);
    X509_free(x509);
    return 1;
  }

  if (PEM_write_X509(certFile, x509) == 0) {
    fclose(certFile);
    EVP_PKEY_free(pkey);
    X509_free(x509);
    return 1;
  }
  fclose(certFile);

  EVP_PKEY_free(pkey);
  X509_free(x509);
  X509_NAME_free(name);
  EVP_PKEY_CTX_free(ctx);

  std::cout << "自签名证书已成功,保存在 " << p << " 中!" << std::endl;

  return 0;
}
