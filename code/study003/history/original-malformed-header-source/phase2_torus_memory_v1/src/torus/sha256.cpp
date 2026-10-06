#include "sha256.hpp"

#include <cstring>

namespace torus {

namespace {

constexpr std::uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

inline std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

}  // namespace

void Sha256::reset() {
  h_[0] = 0x6a09e667u;
  h_[1] = 0xbb67ae85u;
  h_[2] = 0x3c6ef372u;
  h_[3] = 0xa54ff53au;
  h_[4] = 0x510e527fu;
  h_[5] = 0x9b05688cu;
  h_[6] = 0x1f83d9abu;
  h_[7] = 0x5be0cd19u;
  total_ = 0;
  used_ = 0;
}

void Sha256::block(const std::uint8_t* p) {
  std::uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = (std::uint32_t(p[4 * i]) << 24) | (std::uint32_t(p[4 * i + 1]) << 16) |
           (std::uint32_t(p[4 * i + 2]) << 8) | std::uint32_t(p[4 * i + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  std::uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3];
  std::uint32_t e = h_[4], f = h_[5], g = h_[6], h = h_[7];
  for (int i = 0; i < 64; ++i) {
    std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    std::uint32_t ch = (e & f) ^ (~e & g);
    std::uint32_t t1 = h + S1 + ch + K[i] + w[i];
    std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    std::uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
    std::uint32_t t2 = S0 + mj;
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

void Sha256::update(const void* data, std::size_t len) {
  const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
  total_ += len;
  if (used_ > 0) {
    std::size_t take = 64 - used_;
    if (take > len) take = len;
    std::memcpy(buf_ + used_, p, take);
    used_ += take;
    p += take;
    len -= take;
    if (used_ == 64) {
      block(buf_);
      used_ = 0;
    }
  }
  while (len >= 64) {
    block(p);
    p += 64;
    len -= 64;
  }
  if (len > 0) {
    std::memcpy(buf_, p, len);
    used_ = len;
  }
}

Digest Sha256::finish() {
  std::uint64_t bits = total_ * 8;
  std::uint8_t pad = 0x80;
  update(&pad, 1);
  std::uint8_t zero = 0;
  while (used_ != 56) update(&zero, 1);
  std::uint8_t len_be[8];
  for (int i = 0; i < 8; ++i) len_be[i] = std::uint8_t(bits >> (56 - 8 * i));
  update(len_be, 8);
  Digest out;
  for (int i = 0; i < 8; ++i) {
    out[4 * i] = std::uint8_t(h_[i] >> 24);
    out[4 * i + 1] = std::uint8_t(h_[i] >> 16);
    out[4 * i + 2] = std::uint8_t(h_[i] >> 8);
    out[4 * i + 3] = std::uint8_t(h_[i]);
  }
  reset();
  return out;
}

Digest sha256_bytes(const void* data, std::size_t len) {
  Sha256 s;
  s.update(data, len);
  return s.finish();
}

Digest sha256_string(const std::string& s) { return sha256_bytes(s.data(), s.size()); }

std::string digest_hex(const Digest& d) {
  static const char* hex = "0123456789abcdef";
  std::string out;
  out.reserve(64);
  for (std::uint8_t b : d) {
    out.push_back(hex[b >> 4]);
    out.push_back(hex[b & 15]);
  }
  return out;
}

bool sha256_self_test() {
  struct Case {
    const char* msg;
    const char* hex;
  };
  const Case cases[] = {
      {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
      {"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
      {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
       "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
  };
  for (const Case& c : cases) {
    if (digest_hex(sha256_string(c.msg)) != c.hex) return false;
  }
  // One million 'a' characters, fed in irregular pieces to exercise buffering.
  Sha256 s;
  std::string piece(997, 'a');
  std::size_t left = 1000000;
  while (left > 0) {
    std::size_t n = left < piece.size() ? left : piece.size();
    s.update(piece.data(), n);
    left -= n;
  }
  return digest_hex(s.finish()) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";
}

}  // namespace torus
