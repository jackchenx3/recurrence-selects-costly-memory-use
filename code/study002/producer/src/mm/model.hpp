// Path state, the single HALF target law, the four candidate families,
// survival, policy mutation and cache transition for one update (spec
// sections 2-6). Relative to the accepted study-001 operator the only rule
// change is the policy-probe source of a valid-M parent in the NONINFO arm at
// recurrent updates (section 5); INFO is the accepted ACTIVE operator and SHAM
// the accepted SHAM operator.
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

// Common coordinate relabeling: source bit s of mask moves to destination
// perm[s]. Hamming weight and pairwise overlaps of masks are preserved.
inline std::uint32_t permute_bits(std::uint32_t mask, const std::uint8_t* perm) {
  std::uint32_t out = 0U;
  for (std::uint32_t s = 0; s < kGenotypeBits; ++s)
    if (((mask >> s) & 1U) != 0U) out |= std::uint32_t{1} << perm[s];
  return out;
}

// Recurrence event read by the experimental operator (never by the allele).
inline bool recurrence_event(std::uint32_t t, std::uint32_t copy_bit) { return t >= 3U && copy_bit == 1U; }

struct PathState {
  std::uint32_t genotype[kPopulation] = {};
  std::uint8_t label[kPopulation] = {};
  std::uint8_t cache_valid[kPopulation] = {};
  std::uint32_t cache[kPopulation] = {};
  std::uint32_t target_prev1 = 0U;  // T_(t-1)
  std::uint32_t target_prev2 = 0U;  // T_(t-2)
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

// Single HALF law: T_1 = I_1, T_2 = I_2; for t >= 3, T_t = T_(t-2) if R_t = 1, else I_t.
inline std::uint32_t target_for_update(std::uint32_t t, std::uint32_t innovation, std::uint32_t copy_bit,
                                       std::uint32_t prev2) {
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
  std::uint32_t cache_probe_use = 0U;        // valid-probe use: probes from the cache (true or decoy); 0 in SHAM
  std::uint32_t cache_probe_survivors = 0U;  // survivors that are cache-derived policy probes (true or decoy)
  std::uint32_t true_cache_use = 0U;         // probes equal to the true cache c
  std::uint32_t decoy_use = 0U;              // probes x XOR pi(c XOR x) (NONINFO, recurrent updates only)
  std::uint32_t true_cache_survivors = 0U;
  std::uint32_t decoy_survivors = 0U;
  std::uint32_t valid_m_disp_w0 = 0U;        // diagnostic (any arm): valid-M parents with popcount(c^x) = 0
  std::uint32_t valid_m_disp_w32 = 0U;       // diagnostic (any arm): valid-M parents with popcount(c^x) = 32
  std::uint32_t decoy_w0 = 0U;               // decoy applications at displacement weight 0 (cannot scramble)
  std::uint32_t decoy_w32 = 0U;              // decoy applications at displacement weight 32 (cannot scramble)
  std::uint32_t decoy_identical = 0U;        // decoy applications whose probe equals the true cache
  std::uint32_t f_to_m = 0U;
  std::uint32_t m_to_f = 0U;
  std::uint32_t query_count = 0U;
  std::uint64_t survival_retries = 0U;       // sum of 32 accepted u32 retry indices (< 2^37)
  std::uint32_t perm_retry_total = 0U;       // shared draw evidence copied from UpdateDraws
  std::uint32_t perm_fnv1a = 0U;             // shared draw evidence copied from UpdateDraws
  std::uint32_t survivor_candidate[kSurvivors] = {};
};

struct UpdateTrace {
  std::uint32_t candidate[kCandidates] = {};
  std::uint32_t mismatch[kCandidates] = {};
  std::uint64_t weight[kCandidates] = {};
  std::uint32_t applied_mask[kCandidates] = {};  // genotype = base XOR applied_mask (base: parent, or donor for family 3)
  std::uint8_t selected_rank[kCandidates] = {};
  std::uint8_t probe_source[kPopulation] = {};   // ProbeSource of each parent's policy probe
  std::uint8_t donor_family[kPopulation] = {};
  std::uint8_t parent_label[kPopulation] = {};
  std::uint8_t parent_cache_valid[kPopulation] = {};
  std::uint32_t parent_cache[kPopulation] = {};
  std::uint32_t parent_genotype[kPopulation] = {};
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
  require(d.perm_ready == 1U, "decoy permutation was not generated for this update");
  require(is_bit_permutation(d.perm), "decoy permutation is not a bijection of the 32 bit positions");
  require(cell.arm == Arm::kInfo || cell.arm == Arm::kNoninfo || cell.arm == Arm::kSham, "unknown arm");

  UpdateResult r;
  r.update = t;
  r.copy_bit = d.copy_bit;
  const std::uint32_t target = target_for_update(t, d.innovation, d.copy_bit, s.target_prev2);
  r.target = target;
  const bool recurrent = recurrence_event(t, d.copy_bit);
  r.recurrence_applied = recurrent ? 1U : 0U;
  r.perm_retry_total = d.perm_retry_total;
  r.perm_fnv1a = d.perm_fnv1a;

  std::uint32_t queries = 0U;
  std::uint32_t cand[kCandidates];
  std::uint32_t mism[kCandidates];
  std::uint32_t applied[kCandidates];
  std::uint8_t source[kPopulation];
  std::uint8_t donor_family[kPopulation];

  // Families 0-2. SHAM never reads label or cache when constructing genotypes.
  // INFO and NONINFO read them only to decide whether a valid-M parent probes
  // its cache; NONINFO additionally reads the recurrence event (operator only).
  for (std::uint32_t i = 0; i < kPopulation; ++i) {
    const std::uint32_t x = s.genotype[i];
    ProbeSource src = ProbeSource::kFresh;
    if (cell.arm == Arm::kInfo || cell.arm == Arm::kNoninfo) {
      if (s.label[i] == kLabelM && s.cache_valid[i] != 0U)
        src = (cell.arm == Arm::kNoninfo && recurrent) ? ProbeSource::kDecoy : ProbeSource::kTrueCache;
    }
    std::uint32_t probe_mask = 0U;
    if (src == ProbeSource::kFresh) {
      probe_mask = d.fresh[i];
    } else if (src == ProbeSource::kTrueCache) {
      probe_mask = s.cache[i] ^ x;  // probe = c exactly
    } else {
      const std::uint32_t displacement = s.cache[i] ^ x;
      probe_mask = permute_bits(displacement, d.perm);  // probe = x XOR pi(c XOR x)
      const std::uint32_t w = popcount32(displacement);
      if (w == 0U) ++r.decoy_w0;
      if (w == kGenotypeBits) ++r.decoy_w32;
      if ((x ^ probe_mask) == s.cache[i]) ++r.decoy_identical;
    }
    cand[candidate_index(Family::kParent, i)] = x;
    cand[candidate_index(Family::kPolicyProbe, i)] = x ^ probe_mask;
    cand[candidate_index(Family::kGlobalScout, i)] = x ^ d.scout[i];
    applied[candidate_index(Family::kParent, i)] = 0U;
    applied[candidate_index(Family::kPolicyProbe, i)] = probe_mask;
    applied[candidate_index(Family::kGlobalScout, i)] = d.scout[i];
    source[i] = static_cast<std::uint8_t>(src);
    if (src == ProbeSource::kTrueCache) ++r.true_cache_use;
    if (src == ProbeSource::kDecoy) ++r.decoy_use;
  }
  r.cache_probe_use = r.true_cache_use + r.decoy_use;
  // Diagnostics only; never enter an operator.
  for (std::uint32_t i = 0; i < kPopulation; ++i) {
    if (s.label[i] == kLabelM && s.cache_valid[i] != 0U) {
      ++r.valid_m_cache;
      const std::uint32_t w = popcount32(s.cache[i] ^ s.genotype[i]);
      if (w == 0U) ++r.valid_m_disp_w0;
      if (w == kGenotypeBits) ++r.valid_m_disp_w32;
    }
  }

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
    applied[j] = d.local_mask[i];
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
      trace->applied_mask[j] = applied[j];
      trace->selected_rank[j] = kNotApplicable;
    }
    for (std::uint32_t i = 0; i < kPopulation; ++i) {
      trace->probe_source[i] = source[i];
      trace->donor_family[i] = donor_family[i];
      trace->parent_label[i] = s.label[i];
      trace->parent_cache_valid[i] = s.cache_valid[i];
      trace->parent_cache[i] = s.cache_valid[i] != 0U ? s.cache[i] : 0U;
      trace->parent_genotype[i] = s.genotype[i];
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
    if (family == static_cast<std::uint32_t>(Family::kPolicyProbe)) {
      if (source[parent] == static_cast<std::uint8_t>(ProbeSource::kTrueCache)) ++r.true_cache_survivors;
      if (source[parent] == static_cast<std::uint8_t>(ProbeSource::kDecoy)) ++r.decoy_survivors;
    }
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
  r.cache_probe_survivors = r.true_cache_survivors + r.decoy_survivors;

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
