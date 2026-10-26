// Canonical Random123 Philox4x32-10, written from the published algorithm
// (Salmon, Moraes, Dror, Shaw, "Parallel random numbers: as easy as 1, 2, 3",
// SC'11). No Random123 source is included or linked. Conformance is checked
// against the official known-answer vectors in kat.hpp (fixture K0).
#pragma once

#include <array>
#include <cstdint>

namespace mm {

using PhiloxCounter = std::array<std::uint32_t, 4>;
using PhiloxOutput = std::array<std::uint32_t, 4>;

struct PhiloxKey {
  std::uint32_t k0;
  std::uint32_t k1;
};

constexpr std::uint32_t kPhiloxM0 = 0xD2511F53U;
constexpr std::uint32_t kPhiloxM1 = 0xCD9E8D57U;
constexpr std::uint32_t kPhiloxW0 = 0x9E3779B9U;
constexpr std::uint32_t kPhiloxW1 = 0xBB67AE85U;
constexpr int kPhiloxRounds = 10;

inline void philox_mulhilo32(std::uint32_t a, std::uint32_t b, std::uint32_t& hi, std::uint32_t& lo) {
  const std::uint64_t product = static_cast<std::uint64_t>(a) * static_cast<std::uint64_t>(b);
  hi = static_cast<std::uint32_t>(product >> 32U);
  lo = static_cast<std::uint32_t>(product);
}

inline PhiloxCounter philox4x32_round(const PhiloxCounter& c, std::uint32_t k0, std::uint32_t k1) {
  std::uint32_t hi0 = 0U, lo0 = 0U, hi1 = 0U, lo1 = 0U;
  philox_mulhilo32(kPhiloxM0, c[0], hi0, lo0);
  philox_mulhilo32(kPhiloxM1, c[2], hi1, lo1);
  return PhiloxCounter{{hi1 ^ c[1] ^ k0, lo1, hi0 ^ c[3] ^ k1, lo0}};
}

// Round 1 uses the supplied key; each later round first bumps the key by the
// Weyl constants (unsigned 32-bit wraparound), as in Random123.
inline PhiloxOutput philox4x32_10(PhiloxCounter ctr, PhiloxKey key) {
  std::uint32_t k0 = key.k0;
  std::uint32_t k1 = key.k1;
  ctr = philox4x32_round(ctr, k0, k1);
  for (int r = 1; r < kPhiloxRounds; ++r) {
    k0 += kPhiloxW0;
    k1 += kPhiloxW1;
    ctr = philox4x32_round(ctr, k0, k1);
  }
  return ctr;
}

}  // namespace mm
