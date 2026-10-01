// Independent SHA-256 (FIPS 180-4) for the PHASE2-MUTABLE-MEMORY-001 r1 replay auditor.
// Written from the published standard only; shares no code with the producer.
#ifndef MMEM_AUDIT_SHA256_HPP
#define MMEM_AUDIT_SHA256_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace mmaudit {

class Sha256 {
 public:
  using Digest = std::array<std::uint8_t, 32>;

  Sha256() { reset(); }

  void reset() {
    static const std::uint32_t kInitial[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                              0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    for (std::size_t i = 0; i < 8; ++i) state_[i] = kInitial[i];
    buffered_ = 0;
    length_bytes_ = 0;
  }

  void update(const void* data, std::size_t len) {
    const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
    length_bytes_ += static_cast<std::uint64_t>(len);
    while (len > 0) {
      if (buffered_ == 0 && len >= 64) {
        compress(p);
        p += 64;
        len -= 64;
        continue;
      }
      std::size_t take = 64 - buffered_;
      if (take > len) take = len;
      std::memcpy(block_ + buffered_, p, take);
      buffered_ += take;
      p += take;
      len -= take;
      if (buffered_ == 64) {
        compress(block_);
        buffered_ = 0;
      }
    }
  }

  // Finalises the digest. The object must be reset() before reuse.
  Digest finish() {
    const std::uint64_t bit_length = length_bytes_ * 8u;
    const std::uint8_t pad_one = 0x80u;
    const std::uint8_t pad_zero = 0x00u;
    update(&pad_one, 1);
    while (buffered_ != 56) update(&pad_zero, 1);
    std::uint8_t length_be[8];
    for (std::size_t i = 0; i < 8; ++i) {
      length_be[i] = static_cast<std::uint8_t>(bit_length >> (56 - 8 * i));
    }
    update(length_be, 8);
    Digest out{};
    for (std::size_t i = 0; i < 8; ++i) {
      out[4 * i + 0] = static_cast<std::uint8_t>(state_[i] >> 24);
      out[4 * i + 1] = static_cast<std::uint8_t>(state_[i] >> 16);
      out[4 * i + 2] = static_cast<std::uint8_t>(state_[i] >> 8);
      out[4 * i + 3] = static_cast<std::uint8_t>(state_[i]);
    }
    return out;
  }

 private:
  static std::uint32_t rotr(std::uint32_t x, unsigned n) { return (x >> n) | (x << (32u - n)); }

  void compress(const std::uint8_t* p) {
    static const std::uint32_t kRound[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
    std::uint32_t w[64];
    for (std::size_t i = 0; i < 16; ++i) {
      w[i] = (static_cast<std::uint32_t>(p[4 * i]) << 24) | (static_cast<std::uint32_t>(p[4 * i + 1]) << 16) |
             (static_cast<std::uint32_t>(p[4 * i + 2]) << 8) | static_cast<std::uint32_t>(p[4 * i + 3]);
    }
    for (std::size_t i = 16; i < 64; ++i) {
      const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
    for (std::size_t i = 0; i < 64; ++i) {
      const std::uint32_t big_s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const std::uint32_t choose = (e & f) ^ (~e & g);
      const std::uint32_t t1 = h + big_s1 + choose + kRound[i] + w[i];
      const std::uint32_t big_s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t t2 = big_s0 + majority;
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
  }

  std::uint32_t state_[8];
  std::uint8_t block_[64];
  std::size_t buffered_;
  std::uint64_t length_bytes_;
};

// Little-endian field feeders used by the documented hash preimages.
inline void hash_u8(Sha256& h, std::uint32_t v) {
  const std::uint8_t b = static_cast<std::uint8_t>(v);
  h.update(&b, 1);
}

inline void hash_u16(Sha256& h, std::uint32_t v) {
  const std::uint8_t b[2] = {static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v >> 8)};
  h.update(b, 2);
}

inline void hash_u32(Sha256& h, std::uint32_t v) {
  const std::uint8_t b[4] = {static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v >> 8),
                             static_cast<std::uint8_t>(v >> 16), static_cast<std::uint8_t>(v >> 24)};
  h.update(b, 4);
}

inline void hash_text(Sha256& h, const char* text) { h.update(text, std::strlen(text)); }

inline std::string to_hex(const std::uint8_t* bytes, std::size_t n) {
  static const char kDigits[] = "0123456789abcdef";
  std::string out;
  out.reserve(2 * n);
  for (std::size_t i = 0; i < n; ++i) {
    out.push_back(kDigits[bytes[i] >> 4]);
    out.push_back(kDigits[bytes[i] & 0x0fu]);
  }
  return out;
}

inline std::string to_hex(const Sha256::Digest& d) { return to_hex(d.data(), d.size()); }

}  // namespace mmaudit

#endif  // MMEM_AUDIT_SHA256_HPP
