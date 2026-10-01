// Declared counter schemas. Counter words are exactly
// (block, update, entity, subindex). No function here takes an arm, law,
// start, label or cache argument: paired cells share exogenous draws by
// construction. Any out-of-range coordinate aborts.
#pragma once

#include <cstdint>

#include "mm/constants.hpp"
#include "mm/fatal.hpp"
#include "mm/keys.hpp"
#include "mm/philox.hpp"

namespace mm {

inline bool valid_coordinate(Purpose p, std::uint32_t block, std::uint32_t update, std::uint32_t entity,
                             std::uint64_t subindex) {
  if (block >= kBlocks) return false;
  const bool sim_update = update >= kFirstUpdate && update <= kLastUpdate;
  switch (p) {
    case Purpose::kInitialGenotype:
      return update == 0U && entity < kPopulation && subindex == 0U;
    case Purpose::kTargetInnovation:
    case Purpose::kTargetCopy:
      return sim_update && entity == 0U && subindex == 0U;
    case Purpose::kFreshMask:
    case Purpose::kScoutMask:
    case Purpose::kPolicyMutation:
      return sim_update && entity < kPopulation && subindex == 0U;
    case Purpose::kLocalBit:
      return sim_update && entity < kPopulation && subindex < kGenotypeBits;
    case Purpose::kDonorKey:
      return sim_update && entity < kPopulation * kDonorFamilies && subindex == 0U;
    case Purpose::kSurvivalUniform:
      return sim_update && entity < kSurvivors && subindex <= kMaxSurvivalRetry;
  }
  return false;
}

inline PhiloxCounter make_counter(Purpose p, std::uint32_t block, std::uint32_t update, std::uint32_t entity,
                                  std::uint64_t subindex) {
  if (!valid_coordinate(p, block, update, entity, subindex)) fatal("random coordinate outside its declared range");
  return PhiloxCounter{{block, update, entity, static_cast<std::uint32_t>(subindex)}};
}

inline PhiloxCounter ctr_initial_genotype(std::uint32_t block, std::uint32_t slot) {
  return make_counter(Purpose::kInitialGenotype, block, 0U, slot, 0U);
}
inline PhiloxCounter ctr_target_innovation(std::uint32_t block, std::uint32_t t) {
  return make_counter(Purpose::kTargetInnovation, block, t, 0U, 0U);
}
inline PhiloxCounter ctr_target_copy(std::uint32_t block, std::uint32_t t) {
  return make_counter(Purpose::kTargetCopy, block, t, 0U, 0U);
}
inline PhiloxCounter ctr_fresh_mask(std::uint32_t block, std::uint32_t t, std::uint32_t slot) {
  return make_counter(Purpose::kFreshMask, block, t, slot, 0U);
}
inline PhiloxCounter ctr_scout_mask(std::uint32_t block, std::uint32_t t, std::uint32_t slot) {
  return make_counter(Purpose::kScoutMask, block, t, slot, 0U);
}
inline PhiloxCounter ctr_local_bit(std::uint32_t block, std::uint32_t t, std::uint32_t slot, std::uint32_t bit) {
  return make_counter(Purpose::kLocalBit, block, t, slot, bit);
}
inline PhiloxCounter ctr_donor_key(std::uint32_t block, std::uint32_t t, std::uint32_t slot, std::uint32_t family) {
  require(slot < kPopulation && family < kDonorFamilies, "donor coordinate outside its declared range");
  return make_counter(Purpose::kDonorKey, block, t, kDonorFamilies * slot + family, 0U);
}
inline PhiloxCounter ctr_survival(std::uint32_t block, std::uint32_t t, std::uint32_t draw, std::uint64_t retry) {
  return make_counter(Purpose::kSurvivalUniform, block, t, draw, retry);
}
inline PhiloxCounter ctr_policy_mutation(std::uint32_t block, std::uint32_t t, std::uint32_t slot) {
  return make_counter(Purpose::kPolicyMutation, block, t, slot, 0U);
}

}  // namespace mm
