#include <openssl/evp.h>
#include <openssl/sha.h>

#include <fstream>
#include <iomanip>
#include <sstream>

std::string str_sha256(const std::string &str) {
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char *>(str.c_str()), str.size(),
         hash);

  std::stringstream ss;
  for (const auto &byte : hash) {
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(byte);
  }
  return ss.str();
}
std::string file_sha256(const std::string &filename) {
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int length = 0;

  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  if (mdctx == nullptr) {
    throw std::runtime_error("Failed to create MD context");
  }

  if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
    EVP_MD_CTX_free(mdctx);
    throw std::runtime_error("Failed to initialize digest");
  }

  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    EVP_MD_CTX_free(mdctx);
    throw std::runtime_error("Could not open file: " + filename);
  }

  const size_t bufferSize = 32768; // 32 KB buffer
  char buffer[bufferSize];

  while (file.read(buffer, bufferSize)) {
    EVP_DigestUpdate(mdctx, buffer, file.gcount());
  }
  // Handle the remaining bytes in the buffer
  EVP_DigestUpdate(mdctx, buffer, file.gcount());

  if (EVP_DigestFinal_ex(mdctx, hash, &length) != 1) {
    EVP_MD_CTX_free(mdctx);
    throw std::runtime_error("Failed to finalize digest");
  }

  EVP_MD_CTX_free(mdctx);
  std::ostringstream oss;
  for (unsigned int i = 0; i < length; ++i) {
    oss << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(hash[i]);
  }

  return oss.str();
}
