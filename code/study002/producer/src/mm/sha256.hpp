// Self-contained SHA-256 (FIPS 180-4). Used for purpose keys, final-state
// hashes and output manifests. Known-answer tests are in fixtures.hpp (S0).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace mm {

using Sha256Digest = std::array<std::uint8_t, 32>;

class Sha256 {
 public:
  Sha256() { reset(); }

  void reset() {
    static const std::uint32_t kInit[8] = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                                           0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    for (int i = 0; i < 8; ++i) h_[i] = kInit[i];
    total_bytes_ = 0U;
    buffer_len_ = 0U;
  }

  void update(const void* data, std::size_t n) {
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    total_bytes_ += static_cast<std::uint64_t>(n);
    while (n > 0U) {
      std::size_t take = 64U - buffer_len_;
      if (take > n) take = n;
      std::memcpy(buffer_ + buffer_len_, p, take);
      buffer_len_ += take;
      p += take;
      n -= take;
      if (buffer_len_ == 64U) {
        compress(buffer_);
        buffer_len_ = 0U;
      }
    }
  }

  Sha256Digest finish() {
    const std::uint64_t bit_len = total_bytes_ * 8U;
    buffer_[buffer_len_++] = 0x80U;
    if (buffer_len_ > 56U) {
      while (buffer_len_ < 64U) buffer_[buffer_len_++] = 0U;
      compress(buffer_);
      buffer_len_ = 0U;
    }
    while (buffer_len_ < 56U) buffer_[buffer_len_++] = 0U;
    for (int i = 7; i >= 0; --i) buffer_[buffer_len_++] = static_cast<std::uint8_t>(bit_len >> (8 * i));
    compress(buffer_);
    Sha256Digest out{};
    for (int i = 0; i < 8; ++i) {
      out[4 * i + 0] = static_cast<std::uint8_t>(h_[i] >> 24U);
      out[4 * i + 1] = static_cast<std::uint8_t>(h_[i] >> 16U);
      out[4 * i + 2] = static_cast<std::uint8_t>(h_[i] >> 8U);
      out[4 * i + 3] = static_cast<std::uint8_t>(h_[i]);
    }
    reset();
    return out;
  }

 private:
  static std::uint32_t rotr(std::uint32_t x, unsigned n) { return (x >> n) | (x << (32U - n)); }

  void compress(const std::uint8_t* block) {
    static const std::uint32_t k[64] = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      w[i] = (static_cast<std::uint32_t>(block[4 * i]) << 24U) | (static_cast<std::uint32_t>(block[4 * i + 1]) << 16U) |
             (static_cast<std::uint32_t>(block[4 * i + 2]) << 8U) | static_cast<std::uint32_t>(block[4 * i + 3]);
    }
    for (int i = 16; i < 64; ++i) {
      const std::uint32_t s0 = rotr(w[i - 15], 7U) ^ rotr(w[i - 15], 18U) ^ (w[i - 15] >> 3U);
      const std::uint32_t s1 = rotr(w[i - 2], 17U) ^ rotr(w[i - 2], 19U) ^ (w[i - 2] >> 10U);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
    for (int i = 0; i < 64; ++i) {
      const std::uint32_t big_s1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
      const std::uint32_t ch = (e & f) ^ (~e & g);
      const std::uint32_t t1 = h + big_s1 + ch + k[i] + w[i];
      const std::uint32_t big_s0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
      const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t t2 = big_s0 + maj;
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h_[0] += a;
    h_[1] += b;
    h_[2] += c;
    h_[3] += d;
    h_[4] += e;
    h_[5] += f;
    h_[6] += g;
    h_[7] += h;
  }

  std::uint32_t h_[8];
  std::uint64_t total_bytes_;
  std::uint8_t buffer_[64];
  std::size_t buffer_len_;
};

inline Sha256Digest sha256_bytes(const void* data, std::size_t n) {
  Sha256 h;
  h.update(data, n);
  return h.finish();
}

inline std::string to_hex(const std::uint8_t* p, std::size_t n) {
  static const char kHex[] = "0123456789abcdef";
  std::string s;
  s.reserve(2U * n);
  for (std::size_t i = 0; i < n; ++i) {
    s.push_back(kHex[p[i] >> 4U]);
    s.push_back(kHex[p[i] & 15U]);
  }
  return s;
}

inline std::string to_hex(const Sha256Digest& d) { return to_hex(d.data(), d.size()); }

inline bool sha256_file_hex(const std::string& path, std::string& hex_out) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) return false;
  Sha256 h;
  std::vector<std::uint8_t> buf(1U << 20U);
  for (;;) {
    const std::size_t n = std::fread(buf.data(), 1U, buf.size(), f);
    if (n > 0U) h.update(buf.data(), n);
    if (n < buf.size()) {
      const bool bad = std::ferror(f) != 0;
      std::fclose(f);
      if (bad) return false;
      break;
    }
  }
  hex_out = to_hex(h.finish());
  return true;
}

}  // namespace mm
