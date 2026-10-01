// Path state, target laws, the four candidate families, survival, policy
// mutation and cache transition for one update (spec sections 2-6).
#pragma once

#include <cstdint>

#include "mm/constants.hpp"
#include "mm/draws.hpp"
#include "mm/fatal.hpp"
#include "mm/selection.hpp"

namespace mm {

inline std::uint32_t popcount32(std::uint32_t v) {
  v = v - ((v >> 1U) & 0x55555555U);
  v = (v & 0x33333333U) + ((v >> 2U) & 0x33333333U);
  v = (v + (v >> 4U)) & 0x0F0F0F0FU;
  return (v * 0x01010101U) >> 24U;
}

// w = 2^(32-h), proportional to 2^(-h).
inline std::uint64_t weight_for_mismatch(std::uint32_t h) {
  require(h <= kGenotypeBits, "mismatch outside 0..32");
  return std::uint64_t{1} << (kGenotypeBits - h);
}

inline std::uint32_t candidate_index(Family f, std::uint32_t slot) {
  return static_cast<std::uint32_t>(f) * kPopulation + slot;
}

struct PathState {
  std::uint32_t genotype[kPopulation] = {};
  std::uint8_t label[kPopulation] = {};
  std::uint8_t cache_valid[kPopulation] = {};
  std::uint32_t cache[kPopulation] = {};
  std::uint32_t target_prev1 = 0U;  // T_(t-1) of this path's law
  std::uint32_t target_prev2 = 0U;  // T_(t-2) of this path's law
  std::uint32_t completed_updates = 0U;
};

inline void initialize_path(PathState& s, const std::uint32_t* genotypes, const std::uint8_t* labels) {
  for (std::uint32_t i = 0; i < kPopulation; ++i) {
    require(labels[i] == kLabelF || labels[i] == kLabelM, "label must be F or M");
    s.genotype[i] = genotypes[i];
    s.label[i] = labels[i];
    s.cache_valid[i] = 0U;
    s.cache[i] = 0U;
  }
  s.target_prev1 = 0U;
  s.target_prev2 = 0U;
  s.completed_updates = 0U;
}

inline void initialize_path_for_start(PathState& s, const std::uint32_t* genotypes, Start start) {
  std::uint8_t labels[kPopulation];
  for (std::uint32_t i = 0; i < kPopulation; ++i) labels[i] = (start == Start::kAllM) ? kLabelM : kLabelF;
  initialize_path(s, genotypes, labels);
}

inline std::uint32_t target_for_update(Law law, std::uint32_t t, std::uint32_t innovation, std::uint32_t copy_bit,
                                       std::uint32_t prev2) {
  if (law == Law::kZero) return innovation;
  if (t <= 2U) return innovation;
  return (copy_bit == 1U) ? prev2 : innovation;
}

struct UpdateResult {
  std::uint32_t update = 0U;
  std::uint32_t target = 0U;
  std::uint32_t copy_bit = 0U;
  std::uint32_t recurrence_applied = 0U;
  std::uint32_t m_count = 0U;
  std::uint32_t total_mismatch = 0U;
  std::uint32_t valid_m_cache = 0U;          // diagnostic: parents labelled M with a valid cache (any arm)
  std::uint32_t cache_probe_use = 0U;        // policy probes taken from cache (always 0 in SHAM)
  std::uint32_t cache_probe_survivors = 0U;  // survivors that are cache-sourced policy probes
  std::uint32_t f_to_m = 0U;
  std::uint32_t m_to_f = 0U;
  std::uint32_t query_count = 0U;
  std::uint64_t survival_retries = 0U;       // sum of 32 accepted u32 retry indices (< 2^37)
  std::uint32_t survivor_candidate[kSurvivors] = {};
};

struct UpdateTrace {
  std::uint32_t candidate[kCandidates] = {};
  std::uint32_t mismatch[kCandidates] = {};
  std::uint64_t weight[kCandidates] = {};
  std::uint8_t selected_rank[kCandidates] = {};
  std::uint8_t probe_from_cache[kPopulation] = {};
  std::uint8_t donor_family[kPopulation] = {};
  std::uint8_t parent_label[kPopulation] = {};
  std::uint8_t parent_cache_valid[kPopulation] = {};
  std::uint32_t parent_cache[kPopulation] = {};
  DrawTrace draw[kSurvivors];
  std::uint8_t inherited_label[kSurvivors] = {};
  std::uint8_t policy_flip[kSurvivors] = {};
  std::uint8_t post_label[kSurvivors] = {};
};

inline UpdateResult step(PathState& s, const CellSpec& cell, const UpdateDraws& d, SurvivalSource& survival,
                         UpdateTrace* trace) {
  const std::uint32_t t = d.update;
  require(t >= kFirstUpdate && t <= kLastUpdate, "update outside 1..256");
  require(t == s.completed_updates + 1U, "updates must be applied in order");
  require(d.copy_bit <= 1U, "copy bit must be 0 or 1");

  UpdateResult r;
  r.update = t;
  r.copy_bit = d.copy_bit;
  const std::uint32_t target = target_for_update(cell.law, t, d.innovation, d.copy_bit, s.target_prev2);
  r.target = target;
  r.recurrence_applied = (cell.law == Law::kHalf && t >= 3U && d.copy_bit == 1U) ? 1U : 0U;

  std::uint32_t queries = 0U;
  std::uint32_t cand[kCandidates];
  std::uint32_t mism[kCandidates];
  std::uint8_t from_cache[kPopulation];
  std::uint8_t donor_family[kPopulation];

  // Families 0-2. SHAM never reads label or cache when constructing genotypes.
  for (std::uint32_t i = 0; i < kPopulation; ++i) {
    const std::uint32_t x = s.genotype[i];
    bool use_cache = false;
    if (cell.arm == Arm::kActive) use_cache = (s.label[i] == kLabelM) && (s.cache_valid[i] != 0U);
    cand[candidate_index(Family::kParent, i)] = x;
    cand[candidate_index(Family::kPolicyProbe, i)] = use_cache ? s.cache[i] : (x ^ d.fresh[i]);
    cand[candidate_index(Family::kGlobalScout, i)] = x ^ d.scout[i];
    from_cache[i] = use_cache ? 1U : 0U;
    r.cache_probe_use += from_cache[i];
  }
  // Diagnostic only; never enters an operator.
  for (std::uint32_t i = 0; i < kPopulation; ++i)
    if (s.label[i] == kLabelM && s.cache_valid[i] != 0U) ++r.valid_m_cache;

  for (std::uint32_t j = 0; j < kDonorFamilies * kPopulation; ++j) {
    mism[j] = popcount32(cand[j] ^ target);
    ++queries;
  }
  // Family 3: lowest mismatch among families 0-2, then lowest donor key, then
  // lowest family index; XOR with the local mask.
  for (std::uint32_t i = 0; i < kPopulation; ++i) {
    std::uint32_t best = 0U;
    for (std::uint32_t f = 1U; f < kDonorFamilies; ++f) {
      const std::uint32_t hf = mism[f * kPopulation + i];
      const std::uint32_t hb = mism[best * kPopulation + i];
      const std::uint64_t kf = d.donor[kDonorFamilies * i + f];
      const std::uint64_t kb = d.donor[kDonorFamilies * i + best];
      if (hf < hb || (hf == hb && kf < kb)) best = f;
    }
    donor_family[i] = static_cast<std::uint8_t>(best);
    const std::uint32_t j = candidate_index(Family::kLocalChild, i);
    cand[j] = cand[best * kPopulation + i] ^ d.local_mask[i];
    mism[j] = popcount32(cand[j] ^ target);
    ++queries;
  }
  require(queries == kCandidates, "objective query count differs from 128");
  r.query_count = queries;

  std::uint64_t weight[kCandidates];
  for (std::uint32_t j = 0; j < kCandidates; ++j) weight[j] = weight_for_mismatch(mism[j]);

  std::uint32_t sel[kSurvivors];
  DrawTrace dt[kSurvivors];
  plackett_luce_select(weight, kCandidates, kSurvivors, survival, sel, dt);

  if (trace != nullptr) {
    for (std::uint32_t j = 0; j < kCandidates; ++j) {
      trace->candidate[j] = cand[j];
      trace->mismatch[j] = mism[j];
      trace->weight[j] = weight[j];
      trace->selected_rank[j] = kNotApplicable;
    }
    for (std::uint32_t i = 0; i < kPopulation; ++i) {
      trace->probe_from_cache[i] = from_cache[i];
      trace->donor_family[i] = donor_family[i];
      trace->parent_label[i] = s.label[i];
      trace->parent_cache_valid[i] = s.cache_valid[i];
      trace->parent_cache[i] = s.cache_valid[i] != 0U ? s.cache[i] : 0U;
    }
  }

  // Inheritance, symmetric policy mutation, then cache transition.
  std::uint32_t new_genotype[kPopulation];
  std::uint8_t new_label[kPopulation];
  std::uint8_t new_valid[kPopulation];
  std::uint32_t new_cache[kPopulation];
  for (std::uint32_t slot = 0; slot < kSurvivors; ++slot) {
    const std::uint32_t j = sel[slot];
    const std::uint32_t parent = j % kPopulation;
    const std::uint32_t family = j / kPopulation;
    const std::uint8_t inherited = s.label[parent];
    const std::uint8_t flip = d.policy_flip[slot];
    require(flip <= 1U, "policy flip must be 0 or 1");
    const std::uint8_t post = static_cast<std::uint8_t>(inherited ^ flip);
    new_genotype[slot] = cand[j];
    new_label[slot] = post;
    new_valid[slot] = (post == kLabelM) ? 1U : 0U;
    new_cache[slot] = (post == kLabelM) ? s.genotype[parent] : 0U;  // producing parent's pre-update genotype
    if (inherited == kLabelF && post == kLabelM) ++r.f_to_m;
    if (inherited == kLabelM && post == kLabelF) ++r.m_to_f;
    if (family == static_cast<std::uint32_t>(Family::kPolicyProbe) && from_cache[parent] != 0U) ++r.cache_probe_survivors;
    r.total_mismatch += mism[j];
    r.survival_retries += static_cast<std::uint64_t>(dt[slot].retry);
    r.survivor_candidate[slot] = j;
    if (trace != nullptr) {
      trace->selected_rank[j] = static_cast<std::uint8_t>(slot);
      trace->draw[slot] = dt[slot];
      trace->inherited_label[slot] = inherited;
      trace->policy_flip[slot] = flip;
      trace->post_label[slot] = post;
    }
  }

  for (std::uint32_t i = 0; i < kPopulation; ++i) {
    s.genotype[i] = new_genotype[i];
    s.label[i] = new_label[i];
    s.cache_valid[i] = new_valid[i];
    s.cache[i] = new_cache[i];
    r.m_count += new_label[i];
  }
  s.target_prev2 = s.target_prev1;
  s.target_prev1 = target;
  s.completed_updates = t;
  return r;
}

}  // namespace mm
