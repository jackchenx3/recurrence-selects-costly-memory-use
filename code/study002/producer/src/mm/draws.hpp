// Exogenous draws for one (block, update). They are generated unconditionally
// for every coordinate and shared by all six cells of the block. The decoy
// permutation is completed by fill_update_all() in selection.hpp, which reuses
// the accepted Lemire arithmetic; step() refuses draws without it.
#pragma once

#include <cstdint>

#include "mm/constants.hpp"
#include "mm/coords.hpp"
#include "mm/keys.hpp"
#include "mm/philox.hpp"

namespace mm {

// Probability-1/32 rule. Parenthesized explicitly: `word0 & 31 == 0` would
// parse as `word0 & (31 == 0)` and never flip.
inline bool flip_rule(std::uint32_t word0) { return (word0 & 31U) == 0U; }

// word1 is widened to 64 bits before the shift by 32.
inline std::uint64_t u64_from_words(std::uint32_t word0, std::uint32_t word1) {
  const std::uint64_t low = static_cast<std::uint64_t>(word0);
  const std::uint64_t high = static_cast<std::uint64_t>(word1);
  return low | (high << 32U);
}

inline std::uint32_t local_mask_from_words(const std::uint32_t* word0_by_bit) {
  std::uint32_t mask = 0U;
  for (std::uint32_t bit = 0; bit < kGenotypeBits; ++bit)
    if (flip_rule(word0_by_bit[bit])) mask |= (std::uint32_t{1} << bit);
  return mask;
}

// True when perm is a bijection of the 32 bit positions.
inline bool is_bit_permutation(const std::uint8_t* perm) {
  std::uint32_t seen = 0U;
  for (std::uint32_t s = 0; s < kGenotypeBits; ++s) {
    if (perm[s] >= kGenotypeBits) return false;
    seen |= std::uint32_t{1} << perm[s];
  }
  return seen == 0xFFFFFFFFU;
}

// FNV-1a (32-bit) over perm[0..31]; a compact permutation fingerprint stored in
// every per-update record. Offset basis 0x811C9DC5, prime 0x01000193.
inline std::uint32_t perm_fnv1a(const std::uint8_t* perm) {
  std::uint32_t h = 0x811C9DC5U;
  for (std::uint32_t s = 0; s < kGenotypeBits; ++s) {
    h ^= static_cast<std::uint32_t>(perm[s]);
    h *= 0x01000193U;
  }
  return h;
}

struct UpdateDraws {
  std::uint32_t update = 0U;
  std::uint32_t innovation = 0U;
  std::uint32_t copy_bit = 0U;
  std::uint32_t fresh[kPopulation] = {};
  std::uint32_t scout[kPopulation] = {};
  std::uint32_t local_mask[kPopulation] = {};
  std::uint64_t donor[kPopulation * kDonorFamilies] = {};  // index 3*slot + family
  std::uint8_t policy_flip[kSurvivors] = {};                // index = survivor slot
  // DECOY_PERMUTATION (shared by every parent and all six cells; generated for
  // every update whether or not it is used). Source bit s -> destination perm[s].
  std::uint8_t perm[kGenotypeBits] = {};
  std::uint64_t perm_x[kGenotypeBits] = {};      // accepted 64-bit value at step i (index i; 0 unused)
  std::uint32_t perm_retry[kGenotypeBits] = {};  // accepted retry subindex at step i (index i; 0 unused)
  std::uint32_t perm_retry_total = 0U;
  std::uint32_t perm_retry_steps = 0U;           // steps whose accepted retry is nonzero
  std::uint32_t perm_fnv1a = 0U;
  std::uint8_t perm_ready = 0U;
};

class SurvivalSource {
 public:
  virtual ~SurvivalSource() = default;
  virtual std::uint64_t draw_x(std::uint32_t draw, std::uint64_t retry) = 0;
};

// 64-bit values for Fisher-Yates step i (1..31) and Lemire retry r.
class DecoySource {
 public:
  virtual ~DecoySource() = default;
  virtual std::uint64_t draw_x(std::uint32_t step, std::uint64_t retry) = 0;
};

class DrawSource {
 public:
  DrawSource(const KeySet& keys, std::uint32_t block) : keys_(keys), block_(block) {
    require(block < kBlocks, "block outside 0..41599");
  }

  std::uint32_t block() const { return block_; }

  std::uint32_t initial_genotype(std::uint32_t slot) const {
    return gen(Purpose::kInitialGenotype, ctr_initial_genotype(block_, slot))[0];
  }

  // Accepted study-001 purposes. The permutation is added by fill_update_all().
  void fill_update_base(std::uint32_t t, UpdateDraws& d) const {
    d.perm_ready = 0U;
    d.update = t;
    d.innovation = gen(Purpose::kTargetInnovation, ctr_target_innovation(block_, t))[0];
    d.copy_bit = gen(Purpose::kTargetCopy, ctr_target_copy(block_, t))[0] & 1U;
    for (std::uint32_t slot = 0; slot < kPopulation; ++slot) {
      d.fresh[slot] = gen(Purpose::kFreshMask, ctr_fresh_mask(block_, t, slot))[0];
      d.scout[slot] = gen(Purpose::kScoutMask, ctr_scout_mask(block_, t, slot))[0];
      std::uint32_t words[kGenotypeBits];
      for (std::uint32_t bit = 0; bit < kGenotypeBits; ++bit)
        words[bit] = gen(Purpose::kLocalBit, ctr_local_bit(block_, t, slot, bit))[0];
      d.local_mask[slot] = local_mask_from_words(words);
      for (std::uint32_t f = 0; f < kDonorFamilies; ++f) {
        const PhiloxOutput o = gen(Purpose::kDonorKey, ctr_donor_key(block_, t, slot, f));
        d.donor[kDonorFamilies * slot + f] = u64_from_words(o[0], o[1]);
      }
      d.policy_flip[slot] = flip_rule(gen(Purpose::kPolicyMutation, ctr_policy_mutation(block_, t, slot))[0]) ? 1U : 0U;
    }
  }

  std::uint64_t survival_x(std::uint32_t t, std::uint32_t draw, std::uint64_t retry) const {
    const PhiloxOutput o = gen(Purpose::kSurvivalUniform, ctr_survival(block_, t, draw, retry));
    return u64_from_words(o[0], o[1]);
  }

  // word0 | (word1 << 32) with word1 widened first; words 2-3 unused.
  std::uint64_t decoy_x(std::uint32_t t, std::uint32_t step, std::uint64_t retry) const {
    const PhiloxOutput o = gen(Purpose::kDecoyPermutation, ctr_decoy_permutation(block_, t, step, retry));
    return u64_from_words(o[0], o[1]);
  }

 private:
  PhiloxOutput gen(Purpose p, const PhiloxCounter& c) const { return philox4x32_10(c, keys_.key[purpose_index(p)]); }

  const KeySet& keys_;
  std::uint32_t block_;
};

// Survival integers for one update; identical for every cell by construction.
class PhiloxSurvivalSource final : public SurvivalSource {
 public:
  PhiloxSurvivalSource(const DrawSource& source, std::uint32_t t) : source_(source), t_(t) {}
  std::uint64_t draw_x(std::uint32_t draw, std::uint64_t retry) override { return source_.survival_x(t_, draw, retry); }

 private:
  const DrawSource& source_;
  std::uint32_t t_;
};

// Decoy-permutation values for one update; identical for every cell by construction.
class PhiloxDecoySource final : public DecoySource {
 public:
  PhiloxDecoySource(const DrawSource& source, std::uint32_t t) : source_(source), t_(t) {}
  std::uint64_t draw_x(std::uint32_t step, std::uint64_t retry) override { return source_.decoy_x(t_, step, retry); }

 private:
  const DrawSource& source_;
  std::uint32_t t_;
};

}  // namespace mm
