// Mechanical enumeration of the declared coordinate schemas and a
// purpose-separated collision audit. Classification:
//   INTENTIONAL_PAIRED_CELL_SHARING  same declared draw in different cells -> same counter (required)
//   WITHIN_PURPOSE_DUPLICATE         distinct declared draws, same purpose, same counter (failure)
//   PURPOSE_SEPARATED_REUSE          same counter under different purpose keys (allowed)
// Counters carry block and update verbatim (checked), so uniqueness within a
// block's (purpose, update) set implies uniqueness across all blocks.
#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "mm/constants.hpp"
#include "mm/coords.hpp"
#include "mm/io.hpp"
#include "mm/keys.hpp"

namespace mm {

struct DrawIdentity {
  std::uint8_t purpose;
  std::uint32_t update;
  std::uint32_t a;  // slot or survivor draw
  std::uint32_t b;  // bit, family or retry
};

inline bool same_identity(const DrawIdentity& x, const DrawIdentity& y) {
  return x.purpose == y.purpose && x.update == y.update && x.a == y.a && x.b == y.b;
}

struct EnumeratedDraw {
  DrawIdentity id;
  PhiloxCounter counter;
};

constexpr std::uint32_t kCollisionRetryDepth = 4U;

template <typename Emit>
void enumerate_block_schema(std::uint32_t block, std::uint32_t retry_depth, Emit&& emit) {
  auto P = [](Purpose p) { return static_cast<std::uint8_t>(p); };
  for (std::uint32_t s = 0; s < kPopulation; ++s)
    emit(DrawIdentity{P(Purpose::kInitialGenotype), 0U, s, 0U}, ctr_initial_genotype(block, s));
  for (std::uint32_t t = kFirstUpdate; t <= kLastUpdate; ++t) {
    emit(DrawIdentity{P(Purpose::kTargetInnovation), t, 0U, 0U}, ctr_target_innovation(block, t));
    emit(DrawIdentity{P(Purpose::kTargetCopy), t, 0U, 0U}, ctr_target_copy(block, t));
    for (std::uint32_t s = 0; s < kPopulation; ++s) {
      emit(DrawIdentity{P(Purpose::kFreshMask), t, s, 0U}, ctr_fresh_mask(block, t, s));
      emit(DrawIdentity{P(Purpose::kScoutMask), t, s, 0U}, ctr_scout_mask(block, t, s));
      for (std::uint32_t bit = 0; bit < kGenotypeBits; ++bit)
        emit(DrawIdentity{P(Purpose::kLocalBit), t, s, bit}, ctr_local_bit(block, t, s, bit));
      for (std::uint32_t f = 0; f < kDonorFamilies; ++f)
        emit(DrawIdentity{P(Purpose::kDonorKey), t, s, f}, ctr_donor_key(block, t, s, f));
      emit(DrawIdentity{P(Purpose::kPolicyMutation), t, s, 0U}, ctr_policy_mutation(block, t, s));
    }
    for (std::uint32_t d = 0; d < kSurvivors; ++d)
      for (std::uint32_t r = 0; r < retry_depth; ++r)
        emit(DrawIdentity{P(Purpose::kSurvivalUniform), t, d, r}, ctr_survival(block, t, d, r));
  }
}

// Independent restatement of spec section 7, used to check the coords.hpp functions.
inline PhiloxCounter declared_counter(std::uint32_t block, const DrawIdentity& id) {
  switch (static_cast<Purpose>(id.purpose)) {
    case Purpose::kInitialGenotype:
      return PhiloxCounter{{block, 0U, id.a, 0U}};
    case Purpose::kTargetInnovation:
    case Purpose::kTargetCopy:
      return PhiloxCounter{{block, id.update, 0U, 0U}};
    case Purpose::kFreshMask:
    case Purpose::kScoutMask:
    case Purpose::kPolicyMutation:
      return PhiloxCounter{{block, id.update, id.a, 0U}};
    case Purpose::kLocalBit:
    case Purpose::kSurvivalUniform:
      return PhiloxCounter{{block, id.update, id.a, id.b}};
    case Purpose::kDonorKey:
      return PhiloxCounter{{block, id.update, 3U * id.a + id.b, 0U}};
  }
  fatal("unknown purpose in declared_counter");
}

inline bool coordinate_boundaries_ok() {
  struct Case {
    Purpose p;
    std::uint32_t block, update, entity;
    std::uint64_t sub;
    bool expected;
  };
  const Case cases[] = {
      {Purpose::kInitialGenotype, 0U, 0U, 31U, 0U, true},     {Purpose::kInitialGenotype, 0U, 1U, 0U, 0U, false},
      {Purpose::kInitialGenotype, 0U, 0U, 32U, 0U, false},    {Purpose::kTargetInnovation, 41599U, 256U, 0U, 0U, true},
      {Purpose::kTargetInnovation, 41600U, 1U, 0U, 0U, false}, {Purpose::kTargetCopy, 0U, 257U, 0U, 0U, false},
      {Purpose::kTargetCopy, 0U, 0U, 0U, 0U, false},          {Purpose::kTargetCopy, 0U, 3U, 1U, 0U, false},
      {Purpose::kFreshMask, 0U, 1U, 31U, 0U, true},           {Purpose::kFreshMask, 0U, 1U, 0U, 1U, false},
      {Purpose::kScoutMask, 0U, 256U, 32U, 0U, false},        {Purpose::kLocalBit, 0U, 1U, 31U, 31U, true},
      {Purpose::kLocalBit, 0U, 1U, 31U, 32U, false},          {Purpose::kDonorKey, 0U, 1U, 95U, 0U, true},
      {Purpose::kDonorKey, 0U, 1U, 96U, 0U, false},           {Purpose::kSurvivalUniform, 0U, 1U, 31U, 0xFFFFFFFFULL, true},
      {Purpose::kSurvivalUniform, 0U, 1U, 31U, 0x100000000ULL, false},
      {Purpose::kSurvivalUniform, 0U, 1U, 32U, 0U, false},    {Purpose::kPolicyMutation, 0U, 1U, 31U, 0U, true},
      {Purpose::kPolicyMutation, 0U, 1U, 32U, 0U, false},
  };
  for (const Case& c : cases)
    if (valid_coordinate(c.p, c.block, c.update, c.entity, c.sub) != c.expected) return false;
  return true;
}

struct CollisionReport {
  bool pass = false;
  bool keys_distinct = false;
  bool boundaries_ok = false;
  std::vector<std::uint32_t> blocks;
  std::uint64_t declared_draws_per_cell_per_block = 0U;
  std::uint64_t enumeration_duplicates = 0U;
  std::uint64_t schema_mismatches = 0U;
  std::uint64_t passthrough_failures = 0U;
  std::uint64_t intentional_paired_cell_sharing = 0U;
  std::uint64_t paired_cell_divergences = 0U;
  std::uint64_t within_purpose_duplicates = 0U;
  std::uint64_t purpose_separated_reuse = 0U;

  std::string to_json() const {
    std::string s = "{\n  \"receipt\": \"MMEM-COLLISION-AUDIT-1\",\n  \"status\": ";
    s += pass ? "\"PASS\"" : "\"FAIL\"";
    s += ",\n  \"keys_pairwise_distinct_and_namespaces_disjoint\": " + std::string(keys_distinct ? "true" : "false");
    s += ",\n  \"coordinate_boundaries_ok\": " + std::string(boundaries_ok ? "true" : "false");
    s += ",\n  \"blocks_enumerated\": [";
    for (std::size_t i = 0; i < blocks.size(); ++i) s += (i ? ", " : "") + std::to_string(blocks[i]);
    s += "],\n  \"survival_retry_depth_enumerated\": " + std::to_string(kCollisionRetryDepth);
    s += ",\n  \"declared_draws_per_cell_per_block\": " + std::to_string(declared_draws_per_cell_per_block);
    s += ",\n  \"enumeration_duplicates\": " + std::to_string(enumeration_duplicates);
    s += ",\n  \"schema_mismatches\": " + std::to_string(schema_mismatches);
    s += ",\n  \"block_update_passthrough_failures\": " + std::to_string(passthrough_failures);
    s += ",\n  \"intentional_paired_cell_sharing\": " + std::to_string(intentional_paired_cell_sharing);
    s += ",\n  \"paired_cell_divergences\": " + std::to_string(paired_cell_divergences);
    s += ",\n  \"within_purpose_duplicates_for_distinct_draws\": " + std::to_string(within_purpose_duplicates);
    s += ",\n  \"purpose_separated_reuse_allowed\": " + std::to_string(purpose_separated_reuse);
    s += "\n}\n";
    return s;
  }
};

inline CollisionReport run_collision_audit(const KeySet& production, const KeySet& fixture) {
  CollisionReport rep;
  rep.keys_distinct = keys_pairwise_distinct(production) && keys_pairwise_distinct(fixture) &&
                      keysets_disjoint(production, fixture);
  rep.boundaries_ok = coordinate_boundaries_ok();
  const std::uint32_t blocks[] = {0U, 1U, 63U, 64U, 20800U, 41598U, 41599U};
  for (std::uint32_t block : blocks) {
    rep.blocks.push_back(block);
    std::vector<EnumeratedDraw> base;
    base.reserve(350000U);
    enumerate_block_schema(block, kCollisionRetryDepth, [&](const DrawIdentity& id, const PhiloxCounter& c) {
      base.push_back(EnumeratedDraw{id, c});
    });
    rep.declared_draws_per_cell_per_block = base.size();
    for (const EnumeratedDraw& e : base) {
      if (e.counter != declared_counter(block, e.id)) ++rep.schema_mismatches;
      if (e.counter[0] != block || e.counter[1] != e.id.update) ++rep.passthrough_failures;
    }
    // The same declared draw in each of the other seven cells (the schema has no
    // cell argument) must resolve to the identical counter.
    for (std::uint32_t cell = 1U; cell < kCells; ++cell) {
      std::size_t i = 0U;
      enumerate_block_schema(block, kCollisionRetryDepth, [&](const DrawIdentity& id, const PhiloxCounter& c) {
        if (i < base.size() && same_identity(id, base[i].id) && c == base[i].counter) {
          ++rep.intentional_paired_cell_sharing;
        } else {
          ++rep.paired_cell_divergences;
        }
        ++i;
      });
      if (i != base.size()) ++rep.paired_cell_divergences;
    }
    std::vector<EnumeratedDraw> v = base;
    auto id_less = [](const EnumeratedDraw& x, const EnumeratedDraw& y) {
      if (x.id.purpose != y.id.purpose) return x.id.purpose < y.id.purpose;
      if (x.id.update != y.id.update) return x.id.update < y.id.update;
      if (x.id.a != y.id.a) return x.id.a < y.id.a;
      return x.id.b < y.id.b;
    };
    std::sort(v.begin(), v.end(), id_less);
    for (std::size_t k = 1U; k < v.size(); ++k)
      if (same_identity(v[k - 1U].id, v[k].id)) ++rep.enumeration_duplicates;
    std::sort(v.begin(), v.end(), [](const EnumeratedDraw& x, const EnumeratedDraw& y) {
      if (x.id.purpose != y.id.purpose) return x.id.purpose < y.id.purpose;
      return x.counter < y.counter;
    });
    for (std::size_t k = 1U; k < v.size(); ++k)
      if (v[k - 1U].id.purpose == v[k].id.purpose && v[k - 1U].counter == v[k].counter &&
          !same_identity(v[k - 1U].id, v[k].id))
        ++rep.within_purpose_duplicates;
    std::sort(v.begin(), v.end(), [](const EnumeratedDraw& x, const EnumeratedDraw& y) {
      if (x.counter != y.counter) return x.counter < y.counter;
      return x.id.purpose < y.id.purpose;
    });
    for (std::size_t k = 1U; k < v.size(); ++k)
      if (v[k - 1U].counter == v[k].counter && v[k - 1U].id.purpose != v[k].id.purpose) ++rep.purpose_separated_reuse;
  }
  rep.pass = rep.keys_distinct && rep.boundaries_ok && rep.enumeration_duplicates == 0U && rep.schema_mismatches == 0U &&
             rep.passthrough_failures == 0U && rep.paired_cell_divergences == 0U &&
             rep.within_purpose_duplicates == 0U &&
             rep.intentional_paired_cell_sharing == rep.declared_draws_per_cell_per_block * (kCells - 1U) * 7U;
  return rep;
}

}  // namespace mm
