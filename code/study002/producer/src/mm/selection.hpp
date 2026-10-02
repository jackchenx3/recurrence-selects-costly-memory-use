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

// ---------------------------------------------------------------------------
// Study-002 addition (everything above is the accepted study-001 code).
// Exact uniform index by the same Lemire multiply-high rejection, and the
// DECOY_PERMUTATION Fisher-Yates shuffle (spec sections 5 and 7).

// Accepts x for bound n iff low64(x*n) >= (2^64 - n) mod n; then j = high64(x*n) < n.
inline bool lemire_accept(std::uint64_t x, std::uint64_t n, std::uint64_t& j) {
  const U128Parts m = mul_u64_u64(x, n);
  if (m.low < lemire_threshold(n)) return false;
  j = m.high;
  return true;
}

struct PermutationTrace {
  std::uint64_t x[kGenotypeBits] = {};      // accepted value at step i (index i; 0 unused)
  std::uint32_t retry[kGenotypeBits] = {};  // accepted retry at step i
  std::uint32_t j[kGenotypeBits] = {};      // chosen swap index at step i
  std::uint32_t retry_total = 0U;
  std::uint32_t retry_steps = 0U;
};

// Fisher-Yates over positions 0..n-1 (1 <= n <= 32). perm[s] = s for s < n;
// for i = n-1, ..., 1: retry r = 0, 1, ... draws x = source.draw_x(i, r) until
// Lemire accepts x for bound i+1, giving j uniform in [0, i]; then swap perm[i]
// and perm[j]. Entries perm[n..31] are not touched. Production uses n = 32, so
// source bit s maps to destination perm[s]. Every step is always generated.
inline void fisher_yates_permutation(DecoySource& source, std::uint32_t n, std::uint8_t* perm, PermutationTrace* trace) {
  require(n >= 1U && n <= kGenotypeBits, "permutation size outside 1..32");
  PermutationTrace local;
  PermutationTrace& tr = (trace != nullptr) ? *trace : local;
  tr = PermutationTrace{};
  for (std::uint32_t s = 0; s < n; ++s) perm[s] = static_cast<std::uint8_t>(s);
  for (std::uint32_t i = n - 1U; i >= 1U; --i) {
    const std::uint64_t bound = static_cast<std::uint64_t>(i) + 1U;
    std::uint64_t j = 0U;
    std::uint64_t retry = 0U;
    for (;; ++retry) {
      if (retry > kMaxDecoyRetry) fatal("decoy permutation retry exceeds the unsigned 32-bit subindex");
      const std::uint64_t x = source.draw_x(i, retry);
      if (lemire_accept(x, bound, j)) {
        tr.x[i] = x;
        break;
      }
    }
    require(j <= i, "Fisher-Yates index outside [0, i]");
    if (retry > static_cast<std::uint64_t>(0xFFFFFFFFU - tr.retry_total)) fatal("decoy retry total overflow");
    tr.retry[i] = static_cast<std::uint32_t>(retry);
    tr.j[i] = static_cast<std::uint32_t>(j);
    tr.retry_total += static_cast<std::uint32_t>(retry);
    if (retry != 0U) ++tr.retry_steps;
    const std::uint8_t tmp = perm[i];
    perm[i] = perm[j];
    perm[j] = tmp;
  }
}

inline void install_decoy_permutation(DecoySource& source, UpdateDraws& d) {
  PermutationTrace tr;
  fisher_yates_permutation(source, kGenotypeBits, d.perm, &tr);
  require(is_bit_permutation(d.perm), "decoy permutation is not a bijection");
  for (std::uint32_t i = 0; i < kGenotypeBits; ++i) {
    d.perm_x[i] = tr.x[i];
    d.perm_retry[i] = tr.retry[i];
  }
  d.perm_retry_total = tr.retry_total;
  d.perm_retry_steps = tr.retry_steps;
  d.perm_fnv1a = perm_fnv1a(d.perm);
  d.perm_ready = 1U;
}

// All exogenous draws of one (block, update): the accepted purposes, then the
// decoy permutation, unconditionally and before any cell is stepped.
inline void fill_update_all(const DrawSource& source, std::uint32_t t, UpdateDraws& d) {
  source.fill_update_base(t, d);
  PhiloxDecoySource decoy(source, t);
  install_decoy_permutation(decoy, d);
}

}  // namespace mm
