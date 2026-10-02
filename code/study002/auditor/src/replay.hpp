// Independent block replay for PHASE2-PERFORMANCE-CONVERSION-002 r1, written from specification
// sections 2-8, frozen_config.json and the record definitions in OUTPUT_FORMATS.md. Shares no code
// with the producer.
#ifndef PCONV_AUDIT_REPLAY_HPP
#define PCONV_AUDIT_REPLAY_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "philox.hpp"
#include "records.hpp"
#include "sha256.hpp"

namespace pcaudit {

constexpr std::uint32_t kSlots = 32;
constexpr std::uint32_t kCandidates = 128;
constexpr std::uint32_t kUpdatesPerPath = 256;
constexpr std::uint32_t kLateFirst = 193;
constexpr std::uint32_t kCells = 6;  // cell = 2*arm + start
constexpr std::uint32_t kArmInfo = 0;
constexpr std::uint32_t kArmNoninfo = 1;
constexpr std::uint32_t kArmSham = 2;
constexpr std::uint32_t kPermSteps = 31;  // Fisher-Yates steps i = 31, ..., 1
constexpr std::uint8_t kNotApplicable = 255;
constexpr std::uint8_t kSourceFresh = 0;
constexpr std::uint8_t kSourceTrueCache = 1;
constexpr std::uint8_t kSourceDecoy = 2;

inline std::uint32_t popcount32(std::uint32_t v) {
  v = v - ((v >> 1) & 0x55555555u);
  v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
  v = (v + (v >> 4)) & 0x0F0F0F0Fu;
  return (v * 0x01010101u) >> 24;
}

// Full 64x64 -> 128-bit unsigned product from 32-bit limbs (no compiler extensions).
inline void mul_64x64(std::uint64_t a, std::uint64_t b, std::uint64_t& hi, std::uint64_t& lo) {
  const std::uint64_t a0 = a & 0xFFFFFFFFull, a1 = a >> 32;
  const std::uint64_t b0 = b & 0xFFFFFFFFull, b1 = b >> 32;
  const std::uint64_t p00 = a0 * b0;
  const std::uint64_t p01 = a0 * b1;
  const std::uint64_t p10 = a1 * b0;
  const std::uint64_t p11 = a1 * b1;
  const std::uint64_t middle = (p00 >> 32) + (p01 & 0xFFFFFFFFull) + (p10 & 0xFFFFFFFFull);
  lo = (p00 & 0xFFFFFFFFull) | (middle << 32);
  hi = p11 + (p01 >> 32) + (p10 >> 32) + (middle >> 32);
}

// threshold = (2^64 - n) mod n, computed in unsigned 64-bit arithmetic.
inline std::uint64_t lemire_threshold(std::uint64_t range) { return (std::uint64_t{0} - range) % range; }

// Lemire multiply-high: reject when low64(x*n) < threshold, else result = high64(x*n). Used with
// n = W for survival (section 6) and n = i+1 for the Fisher-Yates step i (section 5 / section 7).
inline bool lemire_accept(std::uint64_t x, std::uint64_t range, std::uint64_t& z) {
  std::uint64_t hi = 0, lo = 0;
  mul_64x64(x, range, hi, lo);
  if (lo < lemire_threshold(range)) return false;
  z = hi;
  return true;
}

inline std::uint64_t join64(const Word4& w) {
  return static_cast<std::uint64_t>(w[0]) | (static_cast<std::uint64_t>(w[1]) << 32);
}

// FNV-1a 32 over the given bytes (offset basis 0x811C9DC5, prime 0x01000193; binary_records.json).
inline std::uint32_t fnv1a32(const std::uint8_t* data, std::size_t n) {
  std::uint32_t h = 0x811C9DC5u;
  for (std::size_t i = 0; i < n; ++i) {
    h ^= data[i];
    h *= 0x01000193u;
  }
  return h;
}

// Fisher-Yates over N positions (specification section 5; frozen_config decoy_permutation.construction).
// Starting from [0..N-1], for i = N-1, ..., 1 request source(i, retry) for retry = 0, 1, ... until the
// Lemire test with bound i+1 accepts, set j = high64(x*(i+1)) and swap perm[i], perm[j].
// accepted_x[k] and accepted_retry[k] belong to step i = N-1-k (the order of the audit record).
template <unsigned N, typename Source>
void fisher_yates(const Source& source, std::uint8_t (&perm)[N], std::uint64_t (&accepted_x)[N - 1],
                  std::uint32_t (&accepted_retry)[N - 1]) {
  static_assert(N >= 2 && N <= 32, "Fisher-Yates size out of range");
  for (unsigned s = 0; s < N; ++s) perm[s] = static_cast<std::uint8_t>(s);
  for (unsigned i = N - 1; i >= 1; --i) {
    const std::uint64_t bound = static_cast<std::uint64_t>(i) + 1u;
    std::uint64_t retry = 0;
    std::uint64_t x = 0;
    std::uint64_t j = 0;
    for (;;) {
      if (retry > kMaxSubindex) {
        throw CoordinateError("Fisher-Yates retry exceeds the unsigned 32-bit subindex at step " + std::to_string(i));
      }
      x = source(i, retry);
      if (lemire_accept(x, bound, j)) break;
      ++retry;
    }
    if (j > i) throw std::logic_error("Lemire index exceeds the Fisher-Yates step");
    accepted_x[N - 1 - i] = x;
    accepted_retry[N - 1 - i] = static_cast<std::uint32_t>(retry);
    const std::uint8_t tmp = perm[i];
    perm[i] = perm[static_cast<unsigned>(j)];
    perm[static_cast<unsigned>(j)] = tmp;
  }
}

// Source bit s of d moves to destination bit perm[s] (specification section 5).
template <unsigned N>
std::uint32_t apply_permutation(std::uint32_t d, const std::uint8_t (&perm)[N]) {
  static_assert(N >= 1 && N <= 32, "permutation size out of range");
  std::uint32_t out = 0;
  for (unsigned s = 0; s < N; ++s) {
    if ((d >> s) & 1u) out |= (1u << perm[s]);
  }
  return out;
}

// HALF law (section 4): T_1 = I_1, T_2 = I_2; for t >= 3, T_t = T_(t-2) iff R_t = 1, else I_t.
inline bool recurrence_applied_at(std::uint32_t t, std::uint32_t copy_bit) { return t >= 3 && copy_bit == 1u; }

// Arrays are indexed 1..last; index 0 is unused.
inline void half_targets(const std::uint32_t* innovation, const std::uint32_t* copy_bit, std::uint32_t last,
                         std::uint32_t* target, std::uint8_t* applied) {
  for (std::uint32_t t = 1; t <= last; ++t) {
    if (recurrence_applied_at(t, copy_bit[t])) {
      target[t] = target[t - 2];
      applied[t] = 1;
    } else {
      target[t] = innovation[t];
      applied[t] = 0;
    }
  }
}

struct Probe {
  std::uint32_t genotype;
  std::uint32_t mask;  // genotype = parent XOR mask
  std::uint8_t source;
};

// Policy probe (section 5 item 2; frozen_config candidates.policy_probe_rule). `recurrent` is the
// operator's recurrence indicator (t >= 3 and R_t = 1); the allele never reads it. SHAM never reads
// the label or the cache.
inline Probe policy_probe(std::uint32_t arm, std::uint8_t label, std::uint8_t cache_valid, std::uint32_t parent,
                          std::uint32_t cache, std::uint32_t fresh_mask, bool recurrent,
                          const std::uint8_t (&perm)[32]) {
  if (arm != kArmSham && label == 1 && cache_valid == 1) {
    const std::uint32_t displacement = cache ^ parent;
    if (arm == kArmNoninfo && recurrent) {
      const std::uint32_t m = apply_permutation(displacement, perm);
      return Probe{parent ^ m, m, kSourceDecoy};
    }
    return Probe{cache, displacement, kSourceTrueCache};
  }
  return Probe{parent ^ fresh_mask, fresh_mask, kSourceFresh};
}

// All per-update draws of one block. Every declared coordinate is generated, whatever the cell, arm,
// label or recurrence event; survival retries beyond index 0 are generated on demand. The decoy
// permutation of every (block, update) is generated once, unconditionally, and shared by all six cells.
struct UpdateDraws {
  std::uint32_t innovation = 0;
  std::uint32_t copy_bit = 0;
  std::uint32_t fresh_mask[kSlots] = {};
  std::uint32_t scout_mask[kSlots] = {};
  std::uint32_t local_mask[kSlots] = {};
  std::uint64_t donor_key[3 * kSlots] = {};
  std::uint8_t policy_flip[kSlots] = {};
  std::uint64_t survival_x0[kSlots] = {};
  std::uint8_t perm[32] = {};
  std::uint64_t perm_x[kPermSteps] = {};
  std::uint32_t perm_retry[kPermSteps] = {};
  std::uint32_t perm_retry_total = 0;
  std::uint32_t perm_retry_steps = 0;
  std::uint32_t perm_fnv = 0;
};

struct BlockDraws {
  BlockDraws(const KeyedStream& ks, std::uint32_t block_id)
      : stream(ks), block(block_id), initial_genotype(), per_update(kUpdatesPerPath + 1), target(),
        recurrence_applied() {
    for (std::uint32_t s = 0; s < kSlots; ++s) {
      initial_genotype[s] = stream.draw(Purpose::InitialGenotype, block, 0, s, 0)[0];
    }
    std::uint32_t innovation[kUpdatesPerPath + 1] = {};
    std::uint32_t copy_bit[kUpdatesPerPath + 1] = {};
    for (std::uint32_t t = 1; t <= kUpdatesPerPath; ++t) {
      UpdateDraws& d = per_update[t];
      d.innovation = stream.draw(Purpose::TargetInnovation, block, t, 0, 0)[0];
      d.copy_bit = stream.draw(Purpose::TargetCopy, block, t, 0, 0)[0] & 1u;
      innovation[t] = d.innovation;
      copy_bit[t] = d.copy_bit;
      for (std::uint32_t s = 0; s < kSlots; ++s) {
        d.fresh_mask[s] = stream.draw(Purpose::FreshMask, block, t, s, 0)[0];
        d.scout_mask[s] = stream.draw(Purpose::ScoutMask, block, t, s, 0)[0];
        std::uint32_t mask = 0;
        for (std::uint32_t bit = 0; bit < 32; ++bit) {
          if ((stream.draw(Purpose::LocalBit, block, t, s, bit)[0] & 31u) == 0u) mask |= (1u << bit);
        }
        d.local_mask[s] = mask;
        d.policy_flip[s] = (stream.draw(Purpose::PolicyMutation, block, t, s, 0)[0] & 31u) == 0u ? 1 : 0;
        d.survival_x0[s] = join64(stream.draw(Purpose::SurvivalUniform, block, t, s, 0));
      }
      for (std::uint32_t slot = 0; slot < kSlots; ++slot) {
        for (std::uint32_t family = 0; family < 3; ++family) {
          const std::uint32_t entity = 3 * slot + family;
          d.donor_key[entity] = join64(stream.draw(Purpose::DonorKey, block, t, entity, 0));
        }
      }
      // DECOY_PERMUTATION: counter (block, update, entity = step i, subindex = retry); words 2-3 unused.
      const KeyedStream& ks_ref = stream;
      const std::uint32_t block_ref = block;
      const std::uint32_t update_ref = t;
      auto source = [&ks_ref, block_ref, update_ref](unsigned step, std::uint64_t retry) -> std::uint64_t {
        return join64(ks_ref.draw(Purpose::DecoyPermutation, block_ref, update_ref, step, retry));
      };
      fisher_yates(source, d.perm, d.perm_x, d.perm_retry);
      std::uint64_t total = 0;
      std::uint32_t steps = 0;
      for (std::uint32_t k = 0; k < kPermSteps; ++k) {
        total += d.perm_retry[k];
        if (d.perm_retry[k] != 0) ++steps;
      }
      if (total > 0xFFFFFFFFull) {
        throw CoordinateError("DECOY_PERMUTATION retry total exceeds its unsigned 32-bit record field at block " +
                              std::to_string(block) + " update " + std::to_string(t));
      }
      d.perm_retry_total = static_cast<std::uint32_t>(total);
      d.perm_retry_steps = steps;
      d.perm_fnv = fnv1a32(d.perm, 32);
    }
    half_targets(innovation, copy_bit, kUpdatesPerPath, target, recurrence_applied);
  }

  std::uint64_t survival_x(std::uint32_t update, std::uint32_t draw_index, std::uint64_t retry) const {
    if (retry == 0) return per_update[update].survival_x0[draw_index];
    return join64(stream.draw(Purpose::SurvivalUniform, block, update, draw_index, retry));
  }

  const KeyedStream& stream;
  std::uint32_t block;
  std::uint32_t initial_genotype[kSlots];
  std::vector<UpdateDraws> per_update;  // index 1..256; index 0 unused
  std::uint32_t target[kUpdatesPerPath + 1];
  std::uint8_t recurrence_applied[kUpdatesPerPath + 1];
};

struct CellState {
  std::uint32_t genotype[kSlots];
  std::uint8_t label[kSlots];  // 1 = M, 0 = F
  std::uint8_t cache_valid[kSlots];
  std::uint32_t cache[kSlots];  // 0 when invalid
};

struct CellTotals {
  std::uint32_t late_m_sum = 0;
  std::uint32_t late_mismatch_sum = 0;
  std::uint32_t queries = 0;
  std::uint32_t f_to_m = 0;
  std::uint32_t m_to_f = 0;
  std::uint32_t cache_probe_use = 0;
  std::uint32_t cache_probe_survivors = 0;
  std::uint64_t survival_retries = 0;
  std::uint32_t fixation = 0;
  std::uint32_t extinction = 0;
  std::uint32_t late_fixation = 0;
  std::uint32_t late_extinction = 0;
  std::uint32_t final_m_count = 0;
  std::uint32_t final_total_mismatch = 0;
  std::uint32_t true_cache_use = 0;
  std::uint32_t decoy_use = 0;
  std::uint32_t true_cache_survivors = 0;
  std::uint32_t decoy_survivors = 0;
  std::uint32_t valid_m_disp_w0 = 0;
  std::uint32_t valid_m_disp_w32 = 0;
  std::uint32_t decoy_w0 = 0;
  std::uint32_t decoy_w32 = 0;
  std::uint32_t decoy_identical = 0;
  std::uint32_t recurrent_updates = 0;
  std::uint64_t perm_retry_total = 0;
};

// Per-update evidence used by the auditor's own N1, N2 and C1 identities.
struct UpdateSnapshot {
  std::uint32_t candidate[kCandidates];
  std::uint8_t donor[kSlots];
  std::uint8_t survivor[kSlots];
  std::uint32_t genotype[kSlots];  // post-update
  std::uint32_t cache[kSlots];     // post-update
  std::uint32_t label_mask;        // post-update, raw: bit i = 1 iff slot i is M
  std::uint32_t valid_mask;        // post-update cache validity
  std::uint32_t decoy_differs_mask;  // slots whose decoy probe differs from the true cache
  std::uint32_t total_mismatch;
  std::uint64_t retries;
  std::uint32_t valid_m_cache;  // pre-update diagnostics
  std::uint32_t w0;
  std::uint32_t w32;
};

// INFO/NONINFO state identity used by C1 (the decoy bookkeeping mask is excluded).
inline bool same_state(const UpdateSnapshot& a, const UpdateSnapshot& b) {
  return std::memcmp(a.candidate, b.candidate, sizeof a.candidate) == 0 &&
         std::memcmp(a.donor, b.donor, sizeof a.donor) == 0 &&
         std::memcmp(a.survivor, b.survivor, sizeof a.survivor) == 0 &&
         std::memcmp(a.genotype, b.genotype, sizeof a.genotype) == 0 &&
         std::memcmp(a.cache, b.cache, sizeof a.cache) == 0 && a.label_mask == b.label_mask &&
         a.valid_mask == b.valid_mask && a.total_mismatch == b.total_mismatch && a.retries == b.retries &&
         a.valid_m_cache == b.valid_m_cache && a.w0 == b.w0 && a.w32 == b.w32;
}

struct StepOutput {
  AuditRow rows[kCandidates];
  UpdateRecord update;
};

struct BlockIdentities {
  bool n1 = false;
  bool n2 = false;
  bool sham_paired_hash_equal = false;
  bool c1 = false;
};

class BlockReplay {
 public:
  BlockReplay(const KeyedStream& ks, std::uint32_t block)
      : draws_(ks, block), block_(block), snaps_(static_cast<std::size_t>(kCells) * (kUpdatesPerPath + 1)) {
    for (std::uint32_t c = 0; c < kCells; ++c) {
      late_m_[c] = 0;
      late_mismatch_[c] = 0;
    }
  }

  // The shared audit permutation record of (block, update).
  PermRecord permutation_record(std::uint32_t t) const {
    if (t < 1 || t > kUpdatesPerPath) throw std::logic_error("permutation record update out of range");
    const UpdateDraws& d = draws_.per_update[t];
    PermRecord r;
    r.block = block_;
    r.update = static_cast<std::uint16_t>(t);
    r.retry_steps = static_cast<std::uint8_t>(d.perm_retry_steps);
    r.retry_total = d.perm_retry_total;
    std::memcpy(r.perm, d.perm, sizeof r.perm);
    for (std::uint32_t k = 0; k < kPermSteps; ++k) {
      r.accepted_x[k] = d.perm_x[k];
      r.accepted_retry[k] = d.perm_retry[k];
    }
    return r;
  }

  void begin_cell(std::uint32_t cell) {
    if (cell >= kCells || cell != next_cell_) throw std::logic_error("replay cell sequencing violated");
    cell_ = cell;
    arm_ = cell >> 1;
    start_ = cell & 1u;
    for (std::uint32_t s = 0; s < kSlots; ++s) {
      state_.genotype[s] = draws_.initial_genotype[s];
      state_.label[s] = static_cast<std::uint8_t>(start_);
      state_.cache_valid[s] = 0;
      state_.cache[s] = 0;
    }
    totals_ = CellTotals();
    paired_.reset();
    hash_text(paired_, "PCONV-PAIRED-TRAJECTORY-V1");
    hash_u32(paired_, block_);
    hash_u8(paired_, arm_);
    next_update_ = 1;
  }

  void step(std::uint32_t t, StepOutput& out) {
    if (t != next_update_ || t < 1 || t > kUpdatesPerPath) throw std::logic_error("replay update sequencing violated");
    const UpdateDraws& d = draws_.per_update[t];
    const std::uint32_t target = draws_.target[t];
    const std::uint8_t recurrence_bit = static_cast<std::uint8_t>(d.copy_bit);
    const std::uint8_t applied = draws_.recurrence_applied[t];
    const CellState pre = state_;

    std::uint32_t genotype[kCandidates];
    std::uint32_t mask[kCandidates];
    std::uint32_t mismatch[kCandidates];
    std::uint64_t weight[kCandidates];
    std::uint8_t source[kSlots];
    std::uint8_t donor_family[kSlots];
    std::uint32_t valid_m_cache = 0, w0 = 0, w32 = 0;
    std::uint32_t true_cache_use = 0, decoy_use = 0, decoy_w0 = 0, decoy_w32 = 0, decoy_identical = 0;
    std::uint32_t decoy_differs_mask = 0;
    std::uint32_t queries = 0;

    // Candidate construction (section 5); index = 32*family + slot; no genotype deduplication.
    for (std::uint32_t i = 0; i < kSlots; ++i) {
      const std::uint32_t x = pre.genotype[i];
      const bool valid_m = pre.label[i] == 1 && pre.cache_valid[i] == 1;
      const std::uint32_t displacement = pre.cache_valid[i] == 1 ? (pre.cache[i] ^ x) : 0u;
      const std::uint32_t displacement_weight = popcount32(displacement);
      if (valid_m) {
        ++valid_m_cache;
        if (displacement_weight == 0) ++w0;
        if (displacement_weight == 32) ++w32;
      }
      const Probe probe = policy_probe(arm_, pre.label[i], pre.cache_valid[i], x, pre.cache[i], d.fresh_mask[i],
                                       applied == 1, d.perm);
      source[i] = probe.source;
      if (probe.source == kSourceTrueCache) ++true_cache_use;
      if (probe.source == kSourceDecoy) {
        ++decoy_use;
        if (displacement_weight == 0) ++decoy_w0;
        if (displacement_weight == 32) ++decoy_w32;
        if (probe.genotype == pre.cache[i]) {
          ++decoy_identical;
        } else {
          decoy_differs_mask |= (1u << i);
        }
      }
      genotype[i] = x;
      mask[i] = 0;
      genotype[32 + i] = probe.genotype;
      mask[32 + i] = probe.mask;
      genotype[64 + i] = x ^ d.scout_mask[i];
      mask[64 + i] = d.scout_mask[i];
      for (std::uint32_t f = 0; f < 3; ++f) {
        mismatch[32 * f + i] = popcount32(genotype[32 * f + i] ^ target);
        ++queries;
      }
      // Donor: lowest mismatch among families 0-2; tie: lowest 64-bit donor key; tie: lowest family.
      std::uint32_t best = 0;
      for (std::uint32_t f = 1; f < 3; ++f) {
        const std::uint32_t mf = mismatch[32 * f + i];
        const std::uint32_t mb = mismatch[32 * best + i];
        const std::uint64_t kf = d.donor_key[3 * i + f];
        const std::uint64_t kb = d.donor_key[3 * i + best];
        if (mf < mb || (mf == mb && kf < kb)) best = f;
      }
      donor_family[i] = static_cast<std::uint8_t>(best);
      genotype[96 + i] = genotype[32 * best + i] ^ d.local_mask[i];
      mask[96 + i] = d.local_mask[i];
      mismatch[96 + i] = popcount32(genotype[96 + i] ^ target);
      ++queries;
    }
    for (std::uint32_t j = 0; j < kCandidates; ++j) weight[j] = std::uint64_t{1} << (32u - mismatch[j]);

    // Sequential integer-weighted survival without replacement (section 6).
    bool taken[kCandidates] = {};
    std::uint8_t rank[kCandidates];
    std::uint64_t draw_w[kCandidates] = {};
    std::uint64_t draw_x[kCandidates] = {};
    std::uint64_t draw_z[kCandidates] = {};
    std::uint32_t draw_retry[kCandidates] = {};
    std::memset(rank, kNotApplicable, sizeof rank);
    std::uint32_t survivor[kSlots];
    std::uint64_t retry_sum = 0;
    for (std::uint32_t dr = 0; dr < kSlots; ++dr) {
      std::uint64_t total = 0;
      for (std::uint32_t j = 0; j < kCandidates; ++j) {
        if (!taken[j]) total += weight[j];
      }
      std::uint64_t retry = 0;
      std::uint64_t x = 0;
      std::uint64_t z = 0;
      for (;;) {
        if (retry > kMaxSubindex) {
          throw CoordinateError("survival retry exceeds unsigned 32-bit subindex at block " + std::to_string(block_) +
                                " update " + std::to_string(t) + " draw " + std::to_string(dr));
        }
        x = draws_.survival_x(t, dr, retry);
        if (lemire_accept(x, total, z)) break;
        ++retry;
      }
      std::uint64_t cumulative = 0;
      std::uint32_t pick = kCandidates;
      for (std::uint32_t j = 0; j < kCandidates; ++j) {
        if (taken[j]) continue;
        cumulative += weight[j];
        if (cumulative > z) {
          pick = j;
          break;
        }
      }
      if (pick == kCandidates) throw std::logic_error("survivor selection fell through");
      taken[pick] = true;
      rank[pick] = static_cast<std::uint8_t>(dr);
      draw_w[pick] = total;
      draw_x[pick] = x;
      draw_z[pick] = z;
      draw_retry[pick] = static_cast<std::uint32_t>(retry);
      survivor[dr] = pick;
      retry_sum += retry;
    }

    // Inheritance, symmetric policy mutation and cache transition (section 6, section 2).
    CellState post{};
    std::uint8_t post_label_of[kCandidates];
    std::uint8_t flip_of[kCandidates];
    std::memset(post_label_of, kNotApplicable, sizeof post_label_of);
    std::memset(flip_of, kNotApplicable, sizeof flip_of);
    std::uint32_t m_count = 0, total_mismatch = 0, f_to_m = 0, m_to_f = 0;
    std::uint32_t cache_probe_survivors = 0, true_cache_survivors = 0, decoy_survivors = 0;
    std::uint32_t label_mask = 0, valid_mask = 0;
    for (std::uint32_t s = 0; s < kSlots; ++s) {
      const std::uint32_t j = survivor[s];
      const std::uint32_t parent = j & 31u;
      const std::uint8_t inherited = pre.label[parent];
      const std::uint8_t flip = d.policy_flip[s];
      const std::uint8_t label = static_cast<std::uint8_t>(inherited ^ flip);
      post.genotype[s] = genotype[j];
      post.label[s] = label;
      post.cache_valid[s] = label;
      post.cache[s] = label == 1 ? pre.genotype[parent] : 0u;
      if (label == 1) {
        ++m_count;
        label_mask |= (1u << s);
        valid_mask |= (1u << s);
      }
      total_mismatch += mismatch[j];
      if (inherited == 0 && label == 1) ++f_to_m;
      if (inherited == 1 && label == 0) ++m_to_f;
      if ((j >> 5) == 1u) {
        if (source[parent] == kSourceTrueCache || source[parent] == kSourceDecoy) ++cache_probe_survivors;
        if (source[parent] == kSourceTrueCache) ++true_cache_survivors;
        if (source[parent] == kSourceDecoy) ++decoy_survivors;
      }
      post_label_of[j] = label;
      flip_of[j] = flip;
    }
    state_ = post;

    const std::uint32_t perm_ref = block_ * 256u + (t - 1u);
    for (std::uint32_t j = 0; j < kCandidates; ++j) {
      const std::uint32_t family = j >> 5;
      const std::uint32_t parent = j & 31u;
      const std::uint32_t x = pre.genotype[parent];
      const bool cache_valid = pre.cache_valid[parent] == 1;
      const std::uint32_t displacement = cache_valid ? (pre.cache[parent] ^ x) : 0u;
      AuditRow& r = out.rows[j];
      r.block = block_;
      r.update = static_cast<std::uint16_t>(t);
      r.cell = static_cast<std::uint8_t>(cell_);
      r.candidate = static_cast<std::uint8_t>(j);
      r.family = static_cast<std::uint8_t>(family);
      r.parent = static_cast<std::uint8_t>(parent);
      r.parent_label_pre = pre.label[parent];
      r.parent_cache_valid = pre.cache_valid[parent];
      r.parent_cache = cache_valid ? pre.cache[parent] : 0u;
      r.genotype = genotype[j];
      r.target = target;
      r.mismatch = static_cast<std::uint8_t>(mismatch[j]);
      r.recurrence_bit = recurrence_bit;
      r.recurrence_applied = applied;
      r.probe_source = family == 1 ? source[parent] : kNotApplicable;
      r.weight = weight[j];
      r.selected_rank = rank[j];
      r.post_label = post_label_of[j];
      r.policy_flip = flip_of[j];
      r.donor_family = family == 3 ? donor_family[parent] : kNotApplicable;
      r.W = draw_w[j];
      r.x = draw_x[j];
      r.Z = draw_z[j];
      r.retry = draw_retry[j];
      r.perm_ref = perm_ref;
      r.parent_genotype = x;
      r.true_displacement = displacement;
      r.parent_distance = static_cast<std::uint8_t>(popcount32(genotype[j] ^ x));
      r.displacement_weight = cache_valid ? static_cast<std::uint8_t>(popcount32(displacement)) : kNotApplicable;
      r.applied_mask = mask[j];
    }

    UpdateRecord& u = out.update;
    u.block = block_;
    u.update = static_cast<std::uint16_t>(t);
    u.cell = static_cast<std::uint8_t>(cell_);
    u.m_count = static_cast<std::uint8_t>(m_count);
    u.total_mismatch = static_cast<std::uint16_t>(total_mismatch);
    u.valid_m_cache = static_cast<std::uint8_t>(valid_m_cache);
    u.cache_probe_use = static_cast<std::uint8_t>(true_cache_use + decoy_use);
    u.cache_probe_survivors = static_cast<std::uint8_t>(cache_probe_survivors);
    u.f_to_m = static_cast<std::uint8_t>(f_to_m);
    u.m_to_f = static_cast<std::uint8_t>(m_to_f);
    u.query_count = static_cast<std::uint8_t>(queries);
    u.survival_retries = retry_sum;
    u.recurrence_applied = applied;
    u.true_cache_use = static_cast<std::uint8_t>(true_cache_use);
    u.decoy_use = static_cast<std::uint8_t>(decoy_use);
    u.true_cache_survivors = static_cast<std::uint8_t>(true_cache_survivors);
    u.decoy_survivors = static_cast<std::uint8_t>(decoy_survivors);
    u.valid_m_disp_w0 = static_cast<std::uint8_t>(w0);
    u.valid_m_disp_w32 = static_cast<std::uint8_t>(w32);
    u.decoy_w0 = static_cast<std::uint8_t>(decoy_w0);
    u.decoy_w32 = static_cast<std::uint8_t>(decoy_w32);
    u.decoy_identical_to_cache = static_cast<std::uint8_t>(decoy_identical);
    u.decoy_perm_retry_total = d.perm_retry_total;
    u.decoy_perm_fnv1a = d.perm_fnv;

    if (t >= kLateFirst) {
      totals_.late_m_sum += m_count;
      totals_.late_mismatch_sum += total_mismatch;
    }
    totals_.queries += queries;
    totals_.f_to_m += f_to_m;
    totals_.m_to_f += m_to_f;
    totals_.cache_probe_use += true_cache_use + decoy_use;
    totals_.cache_probe_survivors += cache_probe_survivors;
    totals_.survival_retries += retry_sum;
    if (m_count == kSlots) {
      ++totals_.fixation;
      if (t >= kLateFirst) ++totals_.late_fixation;
    }
    if (m_count == 0) {
      ++totals_.extinction;
      if (t >= kLateFirst) ++totals_.late_extinction;
    }
    totals_.final_m_count = m_count;
    totals_.final_total_mismatch = total_mismatch;
    totals_.true_cache_use += true_cache_use;
    totals_.decoy_use += decoy_use;
    totals_.true_cache_survivors += true_cache_survivors;
    totals_.decoy_survivors += decoy_survivors;
    totals_.valid_m_disp_w0 += w0;
    totals_.valid_m_disp_w32 += w32;
    totals_.decoy_w0 += decoy_w0;
    totals_.decoy_w32 += decoy_w32;
    totals_.decoy_identical += decoy_identical;
    totals_.recurrent_updates += applied;
    totals_.perm_retry_total += d.perm_retry_total;

    // Paired-trajectory hash preimage for this completed update (OUTPUT_FORMATS.md).
    hash_u16(paired_, t);
    hash_u32(paired_, target);
    hash_u8(paired_, recurrence_bit);
    hash_u8(paired_, applied);
    hash_u16(paired_, total_mismatch);
    for (std::uint32_t s = 0; s < kSlots; ++s) hash_u8(paired_, survivor[s]);
    for (std::uint32_t s = 0; s < kSlots; ++s) hash_u32(paired_, post.genotype[s]);
    hash_u32(paired_, start_ == 1 ? (label_mask ^ 0xFFFFFFFFu) : label_mask);

    UpdateSnapshot& sn = snap(cell_, t);
    std::memcpy(sn.candidate, genotype, sizeof sn.candidate);
    std::memcpy(sn.donor, donor_family, sizeof sn.donor);
    for (std::uint32_t s = 0; s < kSlots; ++s) {
      sn.survivor[s] = static_cast<std::uint8_t>(survivor[s]);
      sn.genotype[s] = post.genotype[s];
      sn.cache[s] = post.cache[s];
    }
    sn.label_mask = label_mask;
    sn.valid_mask = valid_mask;
    sn.decoy_differs_mask = decoy_differs_mask;
    sn.total_mismatch = total_mismatch;
    sn.retries = retry_sum;
    sn.valid_m_cache = valid_m_cache;
    sn.w0 = w0;
    sn.w32 = w32;

    ++next_update_;
  }

  PathRecord end_cell() {
    if (next_update_ != kUpdatesPerPath + 1) throw std::logic_error("replay path ended early");
    Sha256 final_hash;
    hash_text(final_hash, "PCONV-FINAL-STATE-V1");
    hash_u32(final_hash, block_);
    hash_u8(final_hash, cell_);
    hash_u32(final_hash, kUpdatesPerPath);
    for (std::uint32_t s = 0; s < kSlots; ++s) {
      hash_u32(final_hash, state_.genotype[s]);
      hash_u8(final_hash, state_.label[s]);
      hash_u8(final_hash, state_.cache_valid[s]);
      hash_u32(final_hash, state_.cache[s]);
    }
    hash_u32(final_hash, draws_.target[256]);
    hash_u32(final_hash, draws_.target[255]);

    PathRecord p;
    p.block = block_;
    p.cell = static_cast<std::uint8_t>(cell_);
    p.arm = static_cast<std::uint8_t>(arm_);
    p.start = static_cast<std::uint8_t>(start_);
    p.late_m_sum = totals_.late_m_sum;
    p.late_mismatch_sum = totals_.late_mismatch_sum;
    p.total_queries = totals_.queries;
    p.f_to_m = totals_.f_to_m;
    p.m_to_f = totals_.m_to_f;
    p.cache_probe_use = totals_.cache_probe_use;
    p.cache_probe_survivors = totals_.cache_probe_survivors;
    p.survival_retries = totals_.survival_retries;
    p.fixation_updates = static_cast<std::uint16_t>(totals_.fixation);
    p.extinction_updates = static_cast<std::uint16_t>(totals_.extinction);
    p.late_fixation_updates = static_cast<std::uint16_t>(totals_.late_fixation);
    p.late_extinction_updates = static_cast<std::uint16_t>(totals_.late_extinction);
    p.final_state_sha256 = final_hash.finish();
    p.final_m_count = totals_.final_m_count;
    p.final_total_mismatch = totals_.final_total_mismatch;
    p.paired_trajectory_sha256 = paired_.finish();
    p.true_cache_use = totals_.true_cache_use;
    p.decoy_use = totals_.decoy_use;
    p.true_cache_survivors = totals_.true_cache_survivors;
    p.decoy_survivors = totals_.decoy_survivors;
    p.valid_m_disp_w0 = totals_.valid_m_disp_w0;
    p.valid_m_disp_w32 = totals_.valid_m_disp_w32;
    p.decoy_w0 = totals_.decoy_w0;
    p.decoy_w32 = totals_.decoy_w32;
    p.decoy_identical_to_cache = totals_.decoy_identical;
    p.recurrent_updates = totals_.recurrent_updates;
    p.decoy_perm_retry_total = totals_.perm_retry_total;

    paired_digest_[cell_] = p.paired_trajectory_sha256;
    late_m_[cell_] = totals_.late_m_sum;
    late_mismatch_[cell_] = totals_.late_mismatch_sum;
    all_queries_ok_ = all_queries_ok_ && totals_.queries == kUpdatesPerPath * kCandidates;
    ++next_cell_;
    return p;
  }

  // The auditor's own N1, N2 and C1 identities and SHAM paired-hash equality, then the block record.
  BlockRecord end_block(BlockIdentities& ids) {
    if (next_cell_ != kCells) throw std::logic_error("replay block ended early");
    ids.n1 = true;
    ids.n2 = true;
    for (std::uint32_t t = 1; t <= kUpdatesPerPath; ++t) {
      const UpdateSnapshot& a = snap(4, t);
      const UpdateSnapshot& b = snap(5, t);
      if (std::memcmp(a.candidate, b.candidate, sizeof a.candidate) != 0 ||
          std::memcmp(a.survivor, b.survivor, sizeof a.survivor) != 0 ||
          std::memcmp(a.genotype, b.genotype, sizeof a.genotype) != 0 || a.total_mismatch != b.total_mismatch ||
          a.retries != b.retries) {
        ids.n1 = false;
      }
      if ((a.label_mask ^ b.label_mask) != 0xFFFFFFFFu) ids.n2 = false;
    }
    ids.sham_paired_hash_equal = paired_digest_[4] == paired_digest_[5];
    std::uint16_t first[2] = {0, 0};
    const bool c1_f = c1_for_start(0, first[0]);
    const bool c1_m = c1_for_start(1, first[1]);
    ids.c1 = c1_f && c1_m;

    std::uint32_t recurrent = 0;
    std::uint64_t perm_total = 0;
    for (std::uint32_t t = 1; t <= kUpdatesPerPath; ++t) {
      recurrent += draws_.recurrence_applied[t];
      perm_total += draws_.per_update[t].perm_retry_total;
    }
    if (perm_total > 0xFFFFFFFFull) {
      throw CoordinateError("DECOY_PERMUTATION block retry total exceeds its unsigned 32-bit record field at block " +
                            std::to_string(block_));
    }

    const std::int64_t l0 = late_m_[0], l1 = late_m_[1], l2 = late_m_[2], l3 = late_m_[3];
    const std::int64_t p0 = late_mismatch_[0], p1 = late_mismatch_[1], p2 = late_mismatch_[2],
                       p3 = late_mismatch_[3], p4 = late_mismatch_[4], p5 = late_mismatch_[5];
    BlockRecord r;
    r.block = block_;
    r.n1_ok = ids.n1 ? 1 : 0;
    r.n2_ok = ids.n2 ? 1 : 0;
    r.query_ok = all_queries_ok_ ? 1 : 0;
    r.audit_block = block_ < 64 ? 1 : 0;
    // Accuracy = 1 - mismatch/65536 per cell, so accuracy differences carry the opposite sign of
    // mismatch differences (OUTPUT_FORMATS.md per-block record).
    r.delta_p_num = static_cast<std::int32_t>((p2 + p3) - (p0 + p1));
    r.d_info_num = static_cast<std::int32_t>(l1 - l0);
    r.d_noninfo_num = static_cast<std::int32_t>(l3 - l2);
    r.e_info_num = static_cast<std::int32_t>((l0 + l1) - (l2 + l3));
    r.e_noninfo_num = static_cast<std::int32_t>((l2 + l3) - 2048);
    r.b_info_num = static_cast<std::int32_t>((p4 + p5) - (p0 + p1));
    r.b_noninfo_num = static_cast<std::int32_t>((p4 + p5) - (p2 + p3));
    for (std::uint32_t c = 0; c < kCells; ++c) {
      r.late_m_sum[c] = late_m_[c];
      r.late_mismatch_sum[c] = late_mismatch_[c];
    }
    r.c1_ok = ids.c1 ? 1 : 0;
    r.first_decoupling_all_f = first[0];
    r.first_decoupling_all_m = first[1];
    r.recurrent_updates = static_cast<std::uint16_t>(recurrent);
    r.decoy_perm_retry_total = static_cast<std::uint32_t>(perm_total);
    return r;
  }

 private:
  UpdateSnapshot& snap(std::uint32_t cell, std::uint32_t t) {
    return snaps_[static_cast<std::size_t>(cell) * (kUpdatesPerPath + 1) + t];
  }
  const UpdateSnapshot& snap(std::uint32_t cell, std::uint32_t t) const {
    return snaps_[static_cast<std::size_t>(cell) * (kUpdatesPerPath + 1) + t];
  }

  // C1 (frozen_config identities.C1) for one start: F = first update at which some NONINFO decoy probe
  // differs from the true cache (0 = never). INFO and NONINFO must be identical in every candidate,
  // donor choice, survivor, post-update genotype, label, cache, mismatch and retry through update F-1;
  // F must be recurrent; at F the pre-update diagnostics agree, parent and scout candidates agree, the
  // policy probe differs exactly at the decoy-differs slots, and local children agree wherever neither
  // arm took the probe as donor.
  bool c1_for_start(std::uint32_t start, std::uint16_t& first) const {
    const std::uint32_t a = kArmInfo * 2 + start;
    const std::uint32_t b = kArmNoninfo * 2 + start;
    first = 0;
    for (std::uint32_t t = 1; t <= kUpdatesPerPath; ++t) {
      if (snap(b, t).decoy_differs_mask != 0) {
        first = static_cast<std::uint16_t>(t);
        break;
      }
    }
    bool ok = true;
    const std::uint32_t last_equal = first == 0 ? kUpdatesPerPath : static_cast<std::uint32_t>(first) - 1u;
    for (std::uint32_t t = 1; t <= last_equal; ++t) {
      if (!same_state(snap(a, t), snap(b, t))) ok = false;
    }
    for (std::uint32_t t = 1; t <= kUpdatesPerPath; ++t) {
      if (snap(a, t).decoy_differs_mask != 0) ok = false;  // INFO never applies a decoy
    }
    if (first != 0) {
      if (draws_.recurrence_applied[first] != 1) ok = false;
      const UpdateSnapshot& x = snap(a, first);
      const UpdateSnapshot& y = snap(b, first);
      if (x.valid_m_cache != y.valid_m_cache || x.w0 != y.w0 || x.w32 != y.w32) ok = false;
      for (std::uint32_t i = 0; i < kSlots; ++i) {
        if (x.candidate[i] != y.candidate[i] || x.candidate[64 + i] != y.candidate[64 + i]) ok = false;
        const bool differs = ((y.decoy_differs_mask >> i) & 1u) != 0;
        if (differs == (x.candidate[32 + i] == y.candidate[32 + i])) ok = false;
        if (x.donor[i] != 1 && y.donor[i] != 1 && x.candidate[96 + i] != y.candidate[96 + i]) ok = false;
      }
    }
    return ok;
  }

  BlockDraws draws_;
  std::uint32_t block_;
  std::vector<UpdateSnapshot> snaps_;
  std::uint32_t cell_ = 0;
  std::uint32_t arm_ = 0;
  std::uint32_t start_ = 0;
  std::uint32_t next_cell_ = 0;
  std::uint32_t next_update_ = 1;
  bool all_queries_ok_ = true;
  CellState state_{};
  CellTotals totals_{};
  Sha256 paired_;
  Sha256::Digest paired_digest_[kCells] = {};
  std::uint32_t late_m_[kCells];
  std::uint32_t late_mismatch_[kCells];
};

}  // namespace pcaudit

#endif  // PCONV_AUDIT_REPLAY_HPP
