// Self-contained FIPS 180-4 SHA-256 used for purpose-key derivation,
// file identities and trajectory digests.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace torus {

using Digest = std::array<std::uint8_t, 32>;

class Sha256 {
 public:
  Sha256() { reset(); }
  void reset();
  void update(const void* data, std::size_t len);
  Digest finish();

 private:
  void block(const std::uint8_t* p);
  std::uint32_t h_[8];
  std::uint64_t total_;
  std::uint8_t buf_[64];
  std::size_t used_;
};

Digest sha256_bytes(const void* data, std::size_t len);
Digest sha256_string(const std::string& s);
std::string digest_hex(const Digest& d);

// Known-answer self-test; returns false on any mismatch.
bool sha256_self_test();

}  // namespace torus
