// Exogenous draws for one (block, update). They are generated unconditionally
// for every coordinate and shared by all eight cells of the block.
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

struct UpdateDraws {
  std::uint32_t update = 0U;
  std::uint32_t innovation = 0U;
  std::uint32_t copy_bit = 0U;
  std::uint32_t fresh[kPopulation] = {};
  std::uint32_t scout[kPopulation] = {};
  std::uint32_t local_mask[kPopulation] = {};
  std::uint64_t donor[kPopulation * kDonorFamilies] = {};  // index 3*slot + family
  std::uint8_t policy_flip[kSurvivors] = {};                // index = survivor slot
};

class SurvivalSource {
 public:
  virtual ~SurvivalSource() = default;
  virtual std::uint64_t draw_x(std::uint32_t draw, std::uint64_t retry) = 0;
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

  void fill_update(std::uint32_t t, UpdateDraws& d) const {
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

}  // namespace mm
