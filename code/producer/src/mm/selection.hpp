// Exact integer Plackett-Luce selection without replacement with Lemire
// multiply-high rejection. No floating-point arithmetic.
#pragma once

#include <cstdint>

#include "mm/constants.hpp"
#include "mm/draws.hpp"
#include "mm/fatal.hpp"

namespace mm {

struct U128Parts {
  std::uint64_t high;
  std::uint64_t low;
};

// Portable 64x64 -> 128-bit product from 32-bit limbs.
inline U128Parts mul_u64_u64(std::uint64_t a, std::uint64_t b) {
  const std::uint64_t mask32 = 0xFFFFFFFFULL;
  const std::uint64_t a_lo = a & mask32, a_hi = a >> 32U;
  const std::uint64_t b_lo = b & mask32, b_hi = b >> 32U;
  const std::uint64_t p_ll = a_lo * b_lo;
  const std::uint64_t p_lh = a_lo * b_hi;
  const std::uint64_t p_hl = a_hi * b_lo;
  const std::uint64_t p_hh = a_hi * b_hi;
  const std::uint64_t mid = (p_ll >> 32U) + (p_lh & mask32) + (p_hl & mask32);
  U128Parts r;
  r.low = (mid << 32U) | (p_ll & mask32);
  r.high = p_hh + (p_lh >> 32U) + (p_hl >> 32U) + (mid >> 32U);
  return r;
}

// threshold = (2^64 - W) mod W, using defined unsigned wraparound.
inline std::uint64_t lemire_threshold(std::uint64_t w) {
  require(w != 0U, "Lemire bound must be positive");
  return (std::uint64_t{0} - w) % w;
}

struct DrawTrace {
  std::uint64_t total_weight = 0U;  // W_d
  std::uint64_t threshold = 0U;
  std::uint64_t x = 0U;             // accepted 64-bit value
  std::uint32_t retry = 0U;         // subindex of the accepted value
  std::uint64_t z = 0U;             // Z_d
  std::uint32_t selected = 0U;      // candidate index
};

inline std::uint64_t lemire_uniform(SurvivalSource& source, std::uint32_t draw, std::uint64_t w, DrawTrace& trace) {
  const std::uint64_t threshold = lemire_threshold(w);
  for (std::uint64_t retry = 0U;; ++retry) {
    if (retry > kMaxSurvivalRetry) fatal("survival retry exceeds the unsigned 32-bit subindex");
    const std::uint64_t x = source.draw_x(draw, retry);
    const U128Parts m = mul_u64_u64(x, w);
    if (m.low < threshold) continue;  // reject and retry
    trace.total_weight = w;
    trace.threshold = threshold;
    trace.x = x;
    trace.retry = static_cast<std::uint32_t>(retry);
    trace.z = m.high;
    return m.high;
  }
}

// Selects k of n candidates sequentially without replacement. At draw d the
// unselected candidates are scanned in index order; the first whose cumulative
// weight exceeds Z_d is taken. selected[d] is the survivor in slot d.
inline void plackett_luce_select(const std::uint64_t* weights, std::uint32_t n, std::uint32_t k, SurvivalSource& source,
                                 std::uint32_t* selected, DrawTrace* traces) {
  require(n <= kCandidates && k <= n, "selection size outside 1..128");
  bool taken[kCandidates] = {};
  for (std::uint32_t d = 0; d < k; ++d) {
    std::uint64_t total = 0U;
    for (std::uint32_t j = 0; j < n; ++j) {
      if (taken[j]) continue;
      require(weights[j] >= 1U && weights[j] <= kMaxWeight, "candidate weight outside 1..2^32");
      total += weights[j];
    }
    require(total >= 1U && total <= kMaxWeightSum, "weight sum outside 1..2^39");
    DrawTrace tr;
    const std::uint64_t z = lemire_uniform(source, d, total, tr);
    std::uint64_t cumulative = 0U;
    std::uint32_t pick = n;
    for (std::uint32_t j = 0; j < n; ++j) {
      if (taken[j]) continue;
      cumulative += weights[j];
      if (cumulative > z) {
        pick = j;
        break;
      }
    }
    require(pick < n, "selection scan found no candidate");
    taken[pick] = true;
    selected[d] = pick;
    tr.selected = pick;
    if (traces != nullptr) traces[d] = tr;
  }
}

}  // namespace mm
