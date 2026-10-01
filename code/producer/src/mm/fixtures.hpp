// Deterministic fixtures K0, S0, KEYS, COLLISION and F1-F10 (spec section 13).
// They use hand-constructed or injected values. F6 and F10 run complete paths
// only under the non-scientific fixture key namespace, so no production block
// or scientific outcome is ever sampled. Fixtures report PASS/FAIL only.
#pragma once

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "mm/block_runner.hpp"
#include "mm/collision_audit.hpp"
#include "mm/constants.hpp"
#include "mm/draws.hpp"
#include "mm/io.hpp"
#include "mm/kat.hpp"
#include "mm/keys.hpp"
#include "mm/model.hpp"
#include "mm/records.hpp"
#include "mm/selection.hpp"

namespace mm {
namespace fixtures {

class Results {
 public:
  void check(bool ok, const std::string& id) {
    lines_.push_back(std::string(ok ? "PASS " : "FAIL ") + id);
    if (ok) ++passed_; else ++failed_;
  }
  int passed() const { return passed_; }
  int failed() const { return failed_; }
  const std::vector<std::string>& lines() const { return lines_; }
  std::string to_json() const {
    std::string s = "{\n  \"receipt\": \"MMEM-FIXTURES-1\",\n  \"all_passed\": ";
    s += (failed_ == 0 && passed_ > 0) ? "true" : "false";
    s += ",\n  \"passed\": " + std::to_string(passed_) + ",\n  \"failed\": " + std::to_string(failed_);
    s += ",\n  \"checks\": [\n";
    for (std::size_t i = 0; i < lines_.size(); ++i) s += "    " + jstr(lines_[i]) + (i + 1 < lines_.size() ? ",\n" : "\n");
    s += "  ]\n}\n";
    return s;
  }

 private:
  int passed_ = 0;
  int failed_ = 0;
  std::vector<std::string> lines_;
};

class InjectedSurvival final : public SurvivalSource {
 public:
  void set(std::uint32_t draw, std::uint64_t retry, std::uint64_t x) { table_[std::make_pair(draw, retry)] = x; }
  void set_default_first_try(std::uint64_t x) {
    has_default_ = true;
    default_ = x;
  }
  std::uint64_t draw_x(std::uint32_t draw, std::uint64_t retry) override {
    const auto it = table_.find(std::make_pair(draw, retry));
    if (it != table_.end()) return it->second;
    if (has_default_ && retry == 0U) return default_;
    fatal("fixture requested a survival value that was not injected");
  }

 private:
  std::map<std::pair<std::uint32_t, std::uint64_t>, std::uint64_t> table_;
  bool has_default_ = false;
  std::uint64_t default_ = 0U;
};

class RecordingSurvival final : public SurvivalSource {
 public:
  RecordingSurvival(const DrawSource& source, std::uint32_t t) : source_(source), t_(t) {}
  std::uint64_t draw_x(std::uint32_t draw, std::uint64_t retry) override {
    const std::uint64_t x = source_.survival_x(t_, draw, retry);
    log.push_back(Entry{draw, retry, x});
    return x;
  }
  struct Entry {
    std::uint32_t draw;
    std::uint64_t retry;
    std::uint64_t x;
  };
  std::vector<Entry> log;

 private:
  const DrawSource& source_;
  std::uint32_t t_;
};

// x = 1 gives Z = 0 (first remaining candidate); x = 2^64-1 gives Z = W-1
// (last remaining candidate). Both are always accepted for any W.
constexpr std::uint64_t kXFirst = 1ULL;
constexpr std::uint64_t kXLast = 0xFFFFFFFFFFFFFFFFULL;
constexpr std::uint64_t kXHalf = 0x8000000000000001ULL;  // Z = floor(W/2) for W < 2^63

inline UpdateDraws blank_draws(std::uint32_t t) {
  UpdateDraws d;
  d.update = t;
  for (std::uint32_t i = 0; i < kPopulation; ++i)
    for (std::uint32_t f = 0; f < kDonorFamilies; ++f) d.donor[kDonorFamilies * i + f] = 100U + f;
  return d;
}

// Hand-defined non-Philox integer pattern for fixture inputs.
inline std::uint32_t pattern32(std::uint32_t a, std::uint32_t b) {
  std::uint32_t z = a * 0x9E3779B1U + b * 0x85EBCA77U + 0x165667B1U;
  z ^= z >> 15U;
  z *= 0x2C1B3C6DU;
  z ^= z >> 12U;
  z *= 0x297A2D39U;
  z ^= z >> 15U;
  return z;
}

inline UpdateDraws pattern_draws(std::uint32_t t, std::uint32_t salt) {
  UpdateDraws d;
  d.update = t;
  d.innovation = pattern32(salt, t);
  d.copy_bit = pattern32(salt + 1U, t) & 1U;
  for (std::uint32_t i = 0; i < kPopulation; ++i) {
    d.fresh[i] = pattern32(salt + 2U, 64U * t + i);
    d.scout[i] = pattern32(salt + 3U, 64U * t + i);
    std::uint32_t words[kGenotypeBits];
    for (std::uint32_t b = 0; b < kGenotypeBits; ++b) words[b] = pattern32(salt + 4U, 4096U * t + 32U * i + b);
    d.local_mask[i] = local_mask_from_words(words);
    for (std::uint32_t f = 0; f < kDonorFamilies; ++f)
      d.donor[kDonorFamilies * i + f] = u64_from_words(pattern32(salt + 5U, 256U * t + 3U * i + f), pattern32(salt + 6U, t + i + f));
    d.policy_flip[i] = 0U;
  }
  return d;
}

inline bool same_draws(const UpdateDraws& a, const UpdateDraws& b) {
  return a.update == b.update && a.innovation == b.innovation && a.copy_bit == b.copy_bit &&
         std::memcmp(a.fresh, b.fresh, sizeof a.fresh) == 0 && std::memcmp(a.scout, b.scout, sizeof a.scout) == 0 &&
         std::memcmp(a.local_mask, b.local_mask, sizeof a.local_mask) == 0 &&
         std::memcmp(a.donor, b.donor, sizeof a.donor) == 0 &&
         std::memcmp(a.policy_flip, b.policy_flip, sizeof a.policy_flip) == 0;
}

inline bool same_state(const PathState& a, const PathState& b) {
  return std::memcmp(a.genotype, b.genotype, sizeof a.genotype) == 0 &&
         std::memcmp(a.label, b.label, sizeof a.label) == 0 &&
         std::memcmp(a.cache_valid, b.cache_valid, sizeof a.cache_valid) == 0 &&
         std::memcmp(a.cache, b.cache, sizeof a.cache) == 0 && a.target_prev1 == b.target_prev1 &&
         a.target_prev2 == b.target_prev2 && a.completed_updates == b.completed_updates;
}

// ---------------------------------------------------------------- K0, S0, KEYS
inline bool fixture_k0_philox(Results& R) {
  std::size_t n = 0U;
  const PhiloxKat* kats = philox_kats(n);
  bool all = true;
  for (std::size_t i = 0; i < n; ++i) {
    const bool ok = philox4x32_10(kats[i].counter, kats[i].key) == kats[i].expected;
    all = all && ok;
    R.check(ok, std::string("K0 Philox4x32-10 known-answer vector: ") + kats[i].label);
  }
  return all;
}

inline std::string philox_kat_json() {
  std::size_t n = 0U;
  const PhiloxKat* kats = philox_kats(n);
  std::string s = "{\n  \"receipt\": \"MMEM-PHILOX-KAT-1\",\n  \"source\": \"Random123 tests/kat_vectors rows 'philox4x32 10'\",\n  \"vectors\": [\n";
  bool all = true;
  for (std::size_t i = 0; i < n; ++i) {
    const PhiloxOutput o = philox4x32_10(kats[i].counter, kats[i].key);
    const bool ok = o == kats[i].expected;
    all = all && ok;
    s += "    {\"label\": " + jstr(kats[i].label) + ", \"counter\": [";
    for (int k = 0; k < 4; ++k) s += (k ? ", " : "") + jstr(hex32(kats[i].counter[k]));
    s += "], \"key\": [" + jstr(hex32(kats[i].key.k0)) + ", " + jstr(hex32(kats[i].key.k1)) + "], \"expected\": [";
    for (int k = 0; k < 4; ++k) s += (k ? ", " : "") + jstr(hex32(kats[i].expected[k]));
    s += "], \"actual\": [";
    for (int k = 0; k < 4; ++k) s += (k ? ", " : "") + jstr(hex32(o[k]));
    s += std::string("], \"match\": ") + (ok ? "true" : "false") + "}" + (i + 1 < n ? ",\n" : "\n");
  }
  s += std::string("  ],\n  \"status\": ") + (all ? "\"PASS\"" : "\"FAIL\"") + "\n}\n";
  return s;
}

inline void fixture_s0_sha256(Results& R) {
  std::size_t n = 0U;
  const Sha256Kat* kats = sha256_kats(n);
  for (std::size_t i = 0; i < n; ++i) {
    Sha256 h;
    const std::size_t len = std::strlen(kats[i].message);
    for (std::size_t r = 0; r < kats[i].repeat; ++r) h.update(kats[i].message, len);
    R.check(to_hex(h.finish()) == kats[i].expected_hex, "S0 SHA-256 known-answer vector " + std::to_string(i));
  }
  // Little-endian key-word extraction from SHA-256("abc") = ba7816bf 8f01cfea ...
  const PhiloxKey k = key_from_text("abc");
  R.check(k.k0 == 0xbf1678baU && k.k1 == 0xeacf018fU, "S0 key words are digest bytes 0-3 and 4-7 little-endian");
}

inline std::string purpose_keys_json(const KeySet& ks) {
  std::string s = "{\n  \"receipt\": \"MMEM-PURPOSE-KEYS-1\",\n  \"namespace\": " + jstr(ks.ns) + ",\n  \"keys\": [\n";
  for (std::uint32_t i = 0; i < kPurposeCount; ++i) {
    const Purpose p = purpose_from_index(i);
    s += "    {\"purpose\": " + jstr(purpose_name(p)) + ", \"text\": " + jstr(purpose_key_text(ks.ns, p)) +
         ", \"k0\": " + jstr(hex32(ks.key[i].k0)) + ", \"k1\": " + jstr(hex32(ks.key[i].k1)) + "}" +
         (i + 1U < kPurposeCount ? ",\n" : "\n");
  }
  return s + "  ]\n}\n";
}

inline void fixture_key_derivation(Results& R) {
  const KeySet prod = derive_keys(kProductionNamespace);
  const KeySet fix = derive_keys(kFixtureNamespace);
  R.check(purpose_key_text(kProductionNamespace, Purpose::kInitialGenotype) ==
              "PHASE2-MUTABLE-MEMORY-001|production-r1|INITIAL_GENOTYPE",
          "KEYS production key text matches the frozen template");
  R.check(keys_pairwise_distinct(prod), "KEYS nine production purpose keys are pairwise distinct");
  R.check(keys_pairwise_distinct(fix), "KEYS nine fixture purpose keys are pairwise distinct");
  R.check(keysets_disjoint(prod, fix), "KEYS fixture namespace shares no key with production");
}

// ---------------------------------------------------------------- F1
inline void fixture_f1_hand_trace(Results& R) {
  const CellSpec cell{Arm::kActive, Law::kZero, Start::kAllF};
  std::uint32_t g[kPopulation];
  for (std::uint32_t i = 0; i < kPopulation; ++i) g[i] = 0xFFFFFFFFU;
  g[0] = 0x0000000FU;
  g[1] = 0xFFFFFFFEU;
  g[31] = 0x7FFFFFFFU;
  std::uint8_t lab[kPopulation] = {};
  lab[1] = kLabelM;
  PathState s;
  initialize_path(s, g, lab);

  UpdateDraws d = blank_draws(1U);
  d.innovation = 0U;
  d.fresh[0] = 0x0000000FU;
  d.fresh[3] = 0x80000001U;
  d.fresh[4] = 0x80000001U;
  d.scout[1] = 0xFFFFFFFFU;
  d.scout[3] = 0x00000003U;
  d.scout[4] = 0x00000003U;
  std::uint32_t words[kGenotypeBits];
  for (std::uint32_t b = 0; b < kGenotypeBits; ++b) words[b] = 1U;
  words[5] = 0x20U;  // (0x20 & 31) == 0: flip
  words[6] = 0x21U;  // no flip
  d.local_mask[0] = local_mask_from_words(words);
  R.check(d.local_mask[0] == 0x00000020U, "F1 exercised local-mutation flip: only bit 5 flips");
  R.check(flip_rule(0x20U) && flip_rule(0U) && flip_rule(0xFFFFFFE0U) && !flip_rule(0x21U) && !flip_rule(0x1FU),
          "F1 flip rule is (word0 & 31U) == 0U");
  d.donor[3 * 2 + 0] = 5U;
  d.donor[3 * 2 + 1] = 3U;
  d.donor[3 * 2 + 2] = 1U;
  d.donor[3 * 3 + 1] = 9U;
  d.donor[3 * 3 + 2] = 7U;
  d.donor[3 * 4 + 1] = 7U;
  d.donor[3 * 4 + 2] = 7U;
  std::uint32_t pw[kSurvivors];
  for (std::uint32_t i = 0; i < kSurvivors; ++i) pw[i] = 1U;
  pw[0] = 0x40U;
  pw[1] = 0x00U;
  pw[2] = 0x3FU;
  for (std::uint32_t i = 0; i < kSurvivors; ++i) d.policy_flip[i] = flip_rule(pw[i]) ? 1U : 0U;
  R.check(d.policy_flip[0] == 1U && d.policy_flip[1] == 1U && d.policy_flip[2] == 0U && d.policy_flip[3] == 0U,
          "F1 exercised policy flips at survivor slots 0 and 1 only");

  InjectedSurvival sv;
  sv.set(0U, 0U, kXHalf);
  sv.set(1U, 0U, kXLast);
  sv.set_default_first_try(kXFirst);
  UpdateTrace tr;
  const UpdateResult r = step(s, cell, d, sv, &tr);

  R.check(r.query_count == 128U, "F1 u1 exactly 128 objective queries");
  R.check(tr.candidate[32] == 0U && tr.candidate[35] == 0x7FFFFFFEU && tr.candidate[33] == 0xFFFFFFFEU &&
              tr.probe_from_cache[1] == 0U,
          "F1 u1 policy probes: fresh XOR; invalid-cache M uses fresh probe");
  R.check(tr.candidate[65] == 0x00000001U && tr.candidate[67] == 0xFFFFFFFCU, "F1 u1 global scouts");
  R.check(tr.donor_family[0] == 1U && tr.donor_family[1] == 2U && tr.donor_family[2] == 2U && tr.donor_family[3] == 2U &&
              tr.donor_family[4] == 1U && tr.donor_family[31] == 0U,
          "F1 u1 donor choice: mismatch, then donor key, then family index");
  R.check(tr.candidate[96] == 0x00000020U && tr.candidate[97] == 0x00000001U && tr.candidate[99] == 0xFFFFFFFCU &&
              tr.candidate[100] == 0x7FFFFFFEU && tr.candidate[127] == 0x7FFFFFFFU,
          "F1 u1 local children");
  R.check(tr.weight[32] == (1ULL << 32) && tr.weight[0] == (1ULL << 28) && tr.weight[65] == (1ULL << 31) &&
              tr.weight[96] == (1ULL << 31) && tr.weight[35] == 4U && tr.weight[2] == 1U,
          "F1 u1 exact integer weights 2^(32-h)");
  const std::uint64_t w0 = (1ULL << 33) + (1ULL << 31) + (1ULL << 29) + 146ULL;
  R.check(tr.draw[0].total_weight == w0 && tr.draw[0].z == (1ULL << 32) + (1ULL << 30) + (1ULL << 28) + 73ULL &&
              tr.draw[0].selected == 65U && tr.draw[0].retry == 0U,
          "F1 u1 draw 0: W=2^33+2^31+2^29+146, Z=2^32+2^30+2^28+73 selects candidate 65");
  R.check(tr.draw[1].total_weight == w0 - (1ULL << 31) && tr.draw[1].z == w0 - (1ULL << 31) - 1ULL &&
              tr.draw[1].selected == 127U,
          "F1 u1 draw 1 selects last remaining candidate 127");
  bool order = r.survivor_candidate[0] == 65U && r.survivor_candidate[1] == 127U;
  for (std::uint32_t k = 2; k < kSurvivors; ++k) order = order && r.survivor_candidate[k] == k - 2U;
  R.check(order, "F1 u1 exact survivor order [65,127,0,1,...,29]");
  bool geno = s.genotype[0] == 1U && s.genotype[1] == 0x7FFFFFFFU && s.genotype[2] == 0x0FU && s.genotype[3] == 0xFFFFFFFEU;
  for (std::uint32_t k = 4; k < kPopulation; ++k) geno = geno && s.genotype[k] == 0xFFFFFFFFU;
  R.check(geno, "F1 u1 survivor genotypes");
  bool labels = true;
  for (std::uint32_t k = 0; k < kPopulation; ++k) labels = labels && s.label[k] == ((k == 1U || k == 3U) ? kLabelM : kLabelF);
  R.check(labels && r.m_count == 2U && r.f_to_m == 1U && r.m_to_f == 1U, "F1 u1 inheritance and policy mutation");
  bool caches = s.cache_valid[1] == 1U && s.cache[1] == 0x7FFFFFFFU && s.cache_valid[3] == 1U && s.cache[3] == 0xFFFFFFFEU;
  for (std::uint32_t k = 0; k < kPopulation; ++k)
    if (k != 1U && k != 3U) caches = caches && s.cache_valid[k] == 0U && s.cache[k] == 0U;
  R.check(caches, "F1 u1 cache = producing parent's pre-update genotype for post-mutation M only");
  R.check(r.total_mismatch == 963U && r.cache_probe_use == 0U && r.valid_m_cache == 0U && r.cache_probe_survivors == 0U &&
              r.survival_retries == 0U,
          "F1 u1 per-update record fields");

  // Update 2: the new F->M mutant (slot 1) uses its cache; the M->F slot 0 does not.
  UpdateDraws d2 = blank_draws(2U);
  InjectedSurvival sv2;
  sv2.set_default_first_try(kXFirst);
  UpdateTrace tr2;
  const UpdateResult r2 = step(s, cell, d2, sv2, &tr2);
  R.check(tr2.probe_from_cache[1] == 1U && tr2.probe_from_cache[3] == 1U && tr2.probe_from_cache[0] == 0U &&
              tr2.candidate[33] == 0x7FFFFFFFU && tr2.candidate[35] == 0xFFFFFFFEU && tr2.candidate[32] == 0x00000001U,
          "F1 u2 valid ACTIVE-M slots probe their cache");
  R.check(r2.valid_m_cache == 2U && r2.cache_probe_use == 2U && r2.query_count == 128U, "F1 u2 cache use and 128 queries");
  bool order2 = true;
  for (std::uint32_t k = 0; k < kSurvivors; ++k) order2 = order2 && r2.survivor_candidate[k] == k;
  R.check(order2 && r2.m_count == 2U && r2.f_to_m == 0U && r2.m_to_f == 0U && r2.total_mismatch == 963U &&
              r2.cache_probe_survivors == 0U,
          "F1 u2 survivors and labels");
  R.check(s.cache_valid[1] == 1U && s.cache[1] == 0x7FFFFFFFU && s.cache_valid[0] == 0U && s.completed_updates == 2U,
          "F1 u2 cache transition");
}

// ---------------------------------------------------------------- F2, F3
inline PathState single_slot_state(std::uint32_t g_other, std::uint32_t g0, std::uint32_t cache0, std::uint32_t prev1,
                                   std::uint32_t prev2) {
  std::uint32_t g[kPopulation];
  std::uint8_t lab[kPopulation] = {};
  for (std::uint32_t i = 0; i < kPopulation; ++i) g[i] = g_other;
  g[0] = g0;
  lab[0] = kLabelM;
  PathState s;
  initialize_path(s, g, lab);
  s.cache_valid[0] = 1U;
  s.cache[0] = cache0;
  s.target_prev1 = prev1;
  s.target_prev2 = prev2;
  s.completed_updates = 2U;
  return s;
}

inline void fixture_f2_memory_better(Results& R) {
  const std::uint32_t A = 0xA5A5A5A5U, B = 0x0F0F0F0FU;
  UpdateDraws d = blank_draws(3U);
  d.innovation = 0x3C3C3C3CU;
  d.copy_bit = 1U;
  d.fresh[0] = 0xFFFF0000U;
  InjectedSurvival sv;
  sv.set_default_first_try(kXFirst);
  PathState sa = single_slot_state(0x5A5A5A5AU, 0x5A5A5A5AU, A, B, A);
  PathState ss = sa;
  UpdateTrace ta, ts;
  const UpdateResult ra = step(sa, CellSpec{Arm::kActive, Law::kHalf, Start::kAllM}, d, sv, &ta);
  const UpdateResult rs = step(ss, CellSpec{Arm::kSham, Law::kHalf, Start::kAllM}, d, sv, &ts);
  R.check(ra.target == A && ra.recurrence_applied == 1U, "F2 lag-two HALF target T_3 = T_1");
  R.check(ta.candidate[32] == A && ta.mismatch[32] == 0U && ta.weight[32] == (1ULL << 32) && ta.probe_from_cache[0] == 1U,
          "F2 ACTIVE cache probe mismatch 0");
  R.check(ts.candidate[32] == 0xA5A55A5AU && ts.mismatch[32] == 16U && ts.weight[32] == (1ULL << 16) &&
              ts.probe_from_cache[0] == 0U && rs.cache_probe_use == 0U,
          "F2 SHAM/fresh probe mismatch 16");
  R.check(ra.query_count == 128U && rs.query_count == 128U, "F2 128 queries");
}

inline void fixture_f3_memory_worse(Results& R) {
  UpdateDraws d = blank_draws(3U);
  d.innovation = 0U;
  d.copy_bit = 0U;
  d.fresh[0] = 0x000000F0U;
  InjectedSurvival sv;
  sv.set_default_first_try(kXFirst);
  PathState base = single_slot_state(0xFFFFFFFFU, 0x000000FFU, 0xFFFFFFF0U, 0x12345678U, 0x9ABCDEF0U);
  PathState sa = base, ss = base, sh = base;
  UpdateTrace ta, ts, th;
  const UpdateResult ra = step(sa, CellSpec{Arm::kActive, Law::kZero, Start::kAllM}, d, sv, &ta);
  const UpdateResult rs = step(ss, CellSpec{Arm::kSham, Law::kZero, Start::kAllM}, d, sv, &ts);
  const UpdateResult rh = step(sh, CellSpec{Arm::kActive, Law::kHalf, Start::kAllM}, d, sv, &th);
  R.check(ra.target == 0U && rh.target == 0U && rh.recurrence_applied == 0U, "F3 fresh target (no recurrence)");
  R.check(ta.candidate[32] == 0xFFFFFFF0U && ta.mismatch[32] == 28U && ta.weight[32] == 16U, "F3 ACTIVE cache mismatch 28");
  R.check(ts.candidate[32] == 0x0000000FU && ts.mismatch[32] == 4U && ts.weight[32] == (1ULL << 28), "F3 fresh probe mismatch 4");
  R.check(std::memcmp(ta.candidate, th.candidate, sizeof ta.candidate) == 0, "F3 HALF without recurrence equals ZERO");
  R.check(ra.query_count == 128U && rs.query_count == 128U && rh.query_count == 128U, "F3 128 queries");
}

// ---------------------------------------------------------------- F4
inline void fixture_f4_duplicate(Results& R) {
  const std::uint32_t T = 0x12345678U;
  std::uint32_t g[kPopulation];
  std::uint8_t lab[kPopulation] = {};
  for (std::uint32_t i = 0; i < kPopulation; ++i) g[i] = ~T;
  g[0] = T;
  lab[0] = kLabelM;
  PathState s;
  initialize_path(s, g, lab);
  s.cache_valid[0] = 1U;
  s.cache[0] = T;  // cache equals the unchanged parent
  s.completed_updates = 1U;
  UpdateDraws d = blank_draws(2U);
  d.innovation = T;
  d.scout[0] = 0xFFFFFFFFU;
  d.local_mask[0] = 0xFFFFFFFFU;
  InjectedSurvival sv;
  sv.set(0U, 0U, kXFirst);
  sv.set(1U, 0U, kXHalf);
  sv.set_default_first_try(kXFirst);
  UpdateTrace tr;
  const UpdateResult r = step(s, CellSpec{Arm::kActive, Law::kZero, Start::kAllM}, d, sv, &tr);
  R.check(tr.candidate[0] == T && tr.candidate[32] == T && tr.mismatch[0] == 0U && tr.mismatch[32] == 0U,
          "F4 cache equals parent; both copies evaluated");
  R.check(r.query_count == 128U, "F4 query count remains 128 with duplicates");
  R.check(tr.draw[0].total_weight == (1ULL << 33) + 126ULL && tr.draw[1].total_weight == (1ULL << 32) + 126ULL &&
              tr.draw[1].z == (1ULL << 31) + 63ULL,
          "F4 weights of both duplicates enter W");
  bool order = r.survivor_candidate[0] == 0U && r.survivor_candidate[1] == 32U;
  for (std::uint32_t k = 2; k < kSurvivors; ++k) order = order && r.survivor_candidate[k] == k - 1U;
  R.check(order, "F4 both duplicate candidates selected in exact order [0,32,1,...,30]");
  R.check(r.cache_probe_use == 1U && r.cache_probe_survivors == 1U && s.genotype[0] == T && s.genotype[1] == T &&
              s.label[1] == kLabelM && s.cache[1] == T,
          "F4 cache-probe survival counted; cache transition");
}

// ---------------------------------------------------------------- F5
inline void fixture_f5_invalid_cache(Results& R) {
  std::uint32_t g[kPopulation];
  std::uint8_t mixed[kPopulation];
  std::uint8_t allf[kPopulation] = {};
  for (std::uint32_t i = 0; i < kPopulation; ++i) {
    g[i] = pattern32(7U, i);
    mixed[i] = (i % 2U == 0U) ? kLabelM : kLabelF;
  }
  PathState am, af, sm, sf;
  initialize_path(am, g, mixed);
  initialize_path(af, g, allf);
  initialize_path(sm, g, mixed);
  initialize_path(sf, g, allf);
  const UpdateDraws d1 = pattern_draws(1U, 100U);
  InjectedSurvival sv;
  sv.set_default_first_try(kXFirst);
  UpdateTrace t_am, t_af, t_sm, t_sf;
  const UpdateResult r_am = step(am, CellSpec{Arm::kActive, Law::kZero, Start::kAllM}, d1, sv, &t_am);
  const UpdateResult r_af = step(af, CellSpec{Arm::kActive, Law::kZero, Start::kAllF}, d1, sv, &t_af);
  const UpdateResult r_sm = step(sm, CellSpec{Arm::kSham, Law::kZero, Start::kAllM}, d1, sv, &t_sm);
  const UpdateResult r_sf = step(sf, CellSpec{Arm::kSham, Law::kZero, Start::kAllF}, d1, sv, &t_sf);
  const bool same_cands = std::memcmp(t_am.candidate, t_af.candidate, sizeof t_am.candidate) == 0 &&
                          std::memcmp(t_am.candidate, t_sm.candidate, sizeof t_am.candidate) == 0 &&
                          std::memcmp(t_am.candidate, t_sf.candidate, sizeof t_am.candidate) == 0;
  R.check(same_cands && r_am.cache_probe_use == 0U, "F5 invalid-cache ACTIVE-M uses the same fresh draw as F and SHAM");
  R.check(std::memcmp(r_am.survivor_candidate, r_sf.survivor_candidate, sizeof r_am.survivor_candidate) == 0 &&
              std::memcmp(r_af.survivor_candidate, r_sm.survivor_candidate, sizeof r_af.survivor_candidate) == 0,
          "F5 identical survivors at update 1");
  R.check(same_state(am, sm), "F5 ACTIVE and SHAM states identical after update 1");

  const PathState before = am;
  const UpdateDraws d2 = pattern_draws(2U, 200U);
  UpdateTrace a2, s2;
  step(am, CellSpec{Arm::kActive, Law::kZero, Start::kAllM}, d2, sv, &a2);
  step(sm, CellSpec{Arm::kSham, Law::kZero, Start::kAllM}, d2, sv, &s2);
  std::uint32_t valid = 0U, diverged = 0U;
  bool only_valid = true;
  for (std::uint32_t i = 0; i < kPopulation; ++i) {
    const bool v = before.label[i] == kLabelM && before.cache_valid[i] != 0U;
    valid += v ? 1U : 0U;
    only_valid = only_valid && a2.candidate[i] == s2.candidate[i] && a2.candidate[64 + i] == s2.candidate[64 + i];
    if (v) {
      only_valid = only_valid && a2.candidate[32 + i] == before.cache[i] &&
                   s2.candidate[32 + i] == (before.genotype[i] ^ d2.fresh[i]);
      if (a2.candidate[32 + i] != s2.candidate[32 + i]) ++diverged;
    } else {
      only_valid = only_valid && a2.candidate[32 + i] == s2.candidate[32 + i] && a2.candidate[96 + i] == s2.candidate[96 + i];
    }
  }
  R.check(valid == 16U && diverged > 0U && only_valid, "F5 ACTIVE and SHAM first diverge only at valid-M slots");
}

// ---------------------------------------------------------------- F6
inline void fixture_f6_sham_paths(Results& R, const KeySet& fixture_keys) {
  for (std::uint32_t lawi = 0; lawi < 2U; ++lawi) {
    const Law law = lawi == 0U ? Law::kZero : Law::kHalf;
    const DrawSource src(fixture_keys, 12345U);
    std::uint32_t g[kPopulation];
    for (std::uint32_t i = 0; i < kPopulation; ++i) g[i] = src.initial_genotype(i);
    std::uint8_t lab[4][kPopulation];
    for (std::uint32_t i = 0; i < kPopulation; ++i) {
      lab[0][i] = kLabelF;
      lab[1][i] = kLabelM;
      lab[2][i] = (i % 3U == 0U) ? kLabelM : kLabelF;
      lab[3][i] = static_cast<std::uint8_t>(lab[2][i] ^ 1U);
    }
    PathState st[4];
    for (int k = 0; k < 4; ++k) initialize_path(st[k], g, lab[k]);
    const CellSpec cs{Arm::kSham, law, Start::kAllF};
    bool n1 = true, n2 = true, q = true;
    std::uint32_t late_sum = 0U;
    UpdateDraws d;
    for (std::uint32_t t = kFirstUpdate; t <= kLastUpdate; ++t) {
      src.fill_update(t, d);
      PhiloxSurvivalSource sv(src, t);
      UpdateResult r[4];
      for (int k = 0; k < 4; ++k) {
        r[k] = step(st[k], cs, d, sv, nullptr);
        q = q && r[k].query_count == 128U && r[k].cache_probe_use == 0U;
      }
      for (int k = 1; k < 4; ++k) n1 = n1 && sham_pair_identical(r[0], r[k], st[0], st[k]);
      n2 = n2 && sham_pair_complement(r[0], r[1], st[0], st[1]) && sham_pair_complement(r[2], r[3], st[2], st[3]);
      if (t >= kLateFirst) late_sum += r[0].m_count + r[1].m_count;
    }
    const std::string tag = lawi == 0U ? "ZERO" : "HALF";
    R.check(n1, "F6 " + tag + " N1: SHAM ALL_F, ALL_M and mixed starts bit-identical for 256 updates");
    R.check(n2, "F6 " + tag + " N2: SHAM labels exact complements after every update");
    R.check(late_sum == kLateLength * kPopulation, "F6 " + tag + " start-averaged late M frequency exactly 1/2");
    R.check(q, "F6 " + tag + " 128 queries per update, no SHAM cache use");
  }
}

// ---------------------------------------------------------------- F7
inline void fixture_f7_target_pairing(Results& R) {
  const std::uint32_t I[9] = {0U, 0x11111111U, 0x22222222U, 0x33333333U, 0x44444444U,
                              0x55555555U, 0x66666666U, 0x77777777U, 0x88888888U};
  const std::uint32_t Rb[9] = {0U, 1U, 1U, 1U, 0U, 1U, 1U, 0U, 1U};
  const std::uint32_t expected_half[9] = {0U, 0x11111111U, 0x22222222U, 0x11111111U, 0x44444444U,
                                          0x11111111U, 0x44444444U, 0x77777777U, 0x44444444U};
  std::uint32_t g[kPopulation];
  std::uint8_t lab[kPopulation] = {};
  for (std::uint32_t i = 0; i < kPopulation; ++i) g[i] = pattern32(9U, i);
  PathState sz, sh;
  initialize_path(sz, g, lab);
  initialize_path(sh, g, lab);
  bool zero_ok = true, half_ok = true, paired = true, flags = true;
  for (std::uint32_t t = 1; t <= 8U; ++t) {
    UpdateDraws d = blank_draws(t);
    d.innovation = I[t];
    d.copy_bit = Rb[t];
    InjectedSurvival sv;
    sv.set_default_first_try(kXFirst);
    const UpdateResult rz = step(sz, CellSpec{Arm::kActive, Law::kZero, Start::kAllF}, d, sv, nullptr);
    const UpdateResult rh = step(sh, CellSpec{Arm::kActive, Law::kHalf, Start::kAllF}, d, sv, nullptr);
    zero_ok = zero_ok && rz.target == I[t] && rz.recurrence_applied == 0U;
    half_ok = half_ok && rh.target == expected_half[t];
    flags = flags && rh.recurrence_applied == ((t >= 3U && Rb[t] == 1U) ? 1U : 0U);
    if (rh.recurrence_applied == 0U) paired = paired && rh.target == rz.target;
  }
  R.check(zero_ok, "F7 ZERO target equals the innovation at every update");
  R.check(half_ok && flags, "F7 HALF copies T_(t-2) only when R_t=1 and t>=3 (t=1,2 boundary ignores R)");
  R.check(paired, "F7 ZERO and HALF share the innovation at every non-recurrent update");
  R.check(target_for_update(Law::kHalf, 2U, 7U, 1U, 99U) == 7U && target_for_update(Law::kHalf, 3U, 7U, 1U, 99U) == 99U &&
              target_for_update(Law::kHalf, 3U, 7U, 0U, 99U) == 7U,
          "F7 target_for_update boundary cases");
}

// ---------------------------------------------------------------- F8
inline void fixture_f8_selection(Results& R) {
  R.check(lemire_threshold(1ULL) == 0ULL, "F8 threshold for W=1 is 0");
  R.check(lemire_threshold(1ULL << 39) == 0ULL, "F8 threshold for W=2^39 is 0");
  R.check(lemire_threshold(3ULL) == 1ULL, "F8 threshold for W=3 is 1");
  R.check(lemire_threshold((1ULL << 32) + 4ULL) == 16ULL, "F8 threshold for W=2^32+4 is 16");
  R.check(lemire_threshold(127ULL << 32) == (16ULL << 32), "F8 threshold for W=127*2^32 is 16*2^32");
  const U128Parts m1 = mul_u64_u64(kXLast, kXLast);
  const U128Parts m2 = mul_u64_u64(1ULL << 63, 4ULL);
  R.check(m1.high == 0xFFFFFFFFFFFFFFFEULL && m1.low == 1ULL && m2.high == 2ULL && m2.low == 0ULL,
          "F8 portable 64x64->128 product");
#if defined(__SIZEOF_INT128__)
  {
    __extension__ typedef unsigned __int128 u128;
    bool ok = true;
    for (std::uint32_t i = 0; i < 64U; ++i) {
      const std::uint64_t a = u64_from_words(pattern32(1U, i), pattern32(2U, i));
      const std::uint64_t b = u64_from_words(pattern32(3U, i), pattern32(4U, i)) >> (i % 40U);
      const u128 p = static_cast<u128>(a) * static_cast<u128>(b);
      const U128Parts q = mul_u64_u64(a, b);
      ok = ok && q.high == static_cast<std::uint64_t>(p >> 64U) && q.low == static_cast<std::uint64_t>(p);
    }
    R.check(ok, "F8 portable product agrees with compiler 128-bit arithmetic");
  }
#endif
  {
    InjectedSurvival sv;
    sv.set(0U, 0U, 0x123456789ABCDEF0ULL);
    DrawTrace tr;
    R.check(lemire_uniform(sv, 0U, 1ULL, tr) == 0ULL && tr.retry == 0U, "F8 W=1 always yields Z=0");
    InjectedSurvival s2;
    s2.set(0U, 0U, kXLast);
    s2.set(1U, 0U, 1ULL << 25);
    DrawTrace t2a, t2b;
    R.check(lemire_uniform(s2, 0U, 1ULL << 39, t2a) == (1ULL << 39) - 1ULL &&
                lemire_uniform(s2, 1U, 1ULL << 39, t2b) == 1ULL,
            "F8 W=2^39 exact high64 results");
    InjectedSurvival s3;
    s3.set(0U, 0U, 0ULL);        // low64(0*3) = 0 < threshold 1: rejected
    s3.set(0U, 1U, 1ULL << 63);  // 3*2^63 = 2^64 + 2^63: Z = 1
    DrawTrace t3;
    R.check(lemire_uniform(s3, 0U, 3ULL, t3) == 1ULL && t3.retry == 1U && t3.x == (1ULL << 63),
            "F8 forced Lemire rejection then retry 1 accepted");
  }
  {
    // Four candidates, mismatches 32, 0, 31, 32; candidates 0 and 3 share a genotype.
    const std::uint32_t target = 0U;
    const std::uint32_t geno[4] = {0xFFFFFFFFU, 0x00000000U, 0xFFFFFFFEU, 0xFFFFFFFFU};
    std::uint64_t w[4];
    for (int j = 0; j < 4; ++j) w[j] = weight_for_mismatch(popcount32(geno[j] ^ target));
    R.check(w[0] == 1U && w[1] == (1ULL << 32) && w[2] == 2U && w[3] == 1U, "F8 weights for mismatch 32, 0, 31, 32");
    InjectedSurvival sv;
    sv.set(0U, 0U, 0ULL);
    sv.set(0U, 1U, kXHalf);
    sv.set(1U, 0U, 1ULL << 63);
    sv.set(2U, 0U, 1ULL << 63);
    sv.set(3U, 0U, 0x123456789ABCDEF0ULL);
    std::uint32_t sel[4];
    DrawTrace dt[4];
    plackett_luce_select(w, 4U, 4U, sv, sel, dt);
    R.check(sel[0] == 1U && sel[1] == 2U && sel[2] == 3U && sel[3] == 0U, "F8 exact survivor order [1,2,3,0]");
    R.check(dt[0].total_weight == (1ULL << 32) + 4ULL && dt[0].threshold == 16ULL && dt[0].retry == 1U &&
                dt[0].z == (1ULL << 31) + 2ULL,
            "F8 draw 0 forced retry and Z");
    R.check(dt[1].total_weight == 4U && dt[1].z == 2U && dt[2].total_weight == 2U && dt[2].z == 1U &&
                dt[3].total_weight == 1U && dt[3].z == 0U,
            "F8 draws 1-3 W and Z");
  }
  {
    // 128 candidates of mismatch 0: W_0 = 2^39; x = 2^64-1 takes the last remaining.
    std::uint64_t w[kCandidates];
    for (std::uint32_t j = 0; j < kCandidates; ++j) w[j] = weight_for_mismatch(0U);
    InjectedSurvival sv;
    sv.set_default_first_try(kXLast);
    std::uint32_t sel[kSurvivors];
    DrawTrace dt[kSurvivors];
    plackett_luce_select(w, kCandidates, kSurvivors, sv, sel, dt);
    bool ok = dt[0].total_weight == (1ULL << 39);
    for (std::uint32_t d = 0; d < kSurvivors; ++d)
      ok = ok && sel[d] == 127U - d && dt[d].total_weight == (static_cast<std::uint64_t>(128U - d) << 32);
    R.check(ok, "F8 full 128-candidate selection with W=2^39");
  }
}

// ---------------------------------------------------------------- F9
inline void fixture_f9_coordinate_invariance(Results& R, const KeySet& fixture_keys) {
  R.check(ctr_initial_genotype(7U, 31U) == PhiloxCounter{{7U, 0U, 31U, 0U}} &&
              ctr_target_innovation(7U, 9U) == PhiloxCounter{{7U, 9U, 0U, 0U}} &&
              ctr_target_copy(7U, 9U) == PhiloxCounter{{7U, 9U, 0U, 0U}} &&
              ctr_fresh_mask(7U, 9U, 5U) == PhiloxCounter{{7U, 9U, 5U, 0U}} &&
              ctr_scout_mask(7U, 9U, 5U) == PhiloxCounter{{7U, 9U, 5U, 0U}} &&
              ctr_local_bit(7U, 9U, 5U, 31U) == PhiloxCounter{{7U, 9U, 5U, 31U}} &&
              ctr_donor_key(7U, 9U, 5U, 2U) == PhiloxCounter{{7U, 9U, 17U, 0U}} &&
              ctr_donor_key(7U, 9U, 31U, 2U) == PhiloxCounter{{7U, 9U, 95U, 0U}} &&
              ctr_survival(7U, 9U, 31U, 0xFFFFFFFFULL) == PhiloxCounter{{7U, 9U, 31U, 0xFFFFFFFFU}} &&
              ctr_policy_mutation(7U, 9U, 5U) == PhiloxCounter{{7U, 9U, 5U, 0U}},
          "F9 literal counter words (block, update, entity, subindex)");
  R.check(u64_from_words(0x89ABCDEFU, 0x01234567U) == 0x0123456789ABCDEFULL &&
              u64_from_words(0U, 0x80000000U) == 0x8000000000000000ULL && u64_from_words(0xFFFFFFFFU, 0U) == 0xFFFFFFFFULL,
          "F9 word1 widened to 64 bits before << 32");

  const DrawSource src(fixture_keys, 777U);
  const std::uint32_t t = 5U;
  UpdateDraws before, after;
  src.fill_update(t, before);
  std::vector<RecordingSurvival::Entry> logs;
  for (std::uint32_t c = 0; c < kCells; ++c) {
    std::uint32_t g[kPopulation];
    std::uint8_t lab[kPopulation];
    for (std::uint32_t i = 0; i < kPopulation; ++i) {
      g[i] = pattern32(11U + c, i);
      lab[i] = static_cast<std::uint8_t>((i + c) % 2U);
    }
    PathState s;
    initialize_path(s, g, lab);
    for (std::uint32_t i = 0; i < kPopulation; i += 3U) {
      s.cache_valid[i] = 1U;
      s.cache[i] = pattern32(99U, i + c);
    }
    s.completed_updates = t - 1U;
    RecordingSurvival sv(src, t);
    step(s, cell_spec(c), before, sv, nullptr);
    logs.insert(logs.end(), sv.log.begin(), sv.log.end());
  }
  src.fill_update(t, after);
  R.check(same_draws(before, after), "F9 update draws unchanged by labels, arm, law and start");
  bool consistent = !logs.empty();
  for (const RecordingSurvival::Entry& e : logs) consistent = consistent && e.x == src.survival_x(t, e.draw, e.retry);
  R.check(consistent, "F9 survival value depends only on (block, update, draw, retry)");
}

// ---------------------------------------------------------------- F10
inline void fixture_f10_regeneration(Results& R, const KeySet& fixture_keys, const std::string& emit_dir) {
  BlockOutput a, other, b;
  run_block(fixture_keys, 41599U, false, a);
  run_block(fixture_keys, 12U, true, other);
  run_block(fixture_keys, 41599U, true, b);
  R.check(a.updates == b.updates && a.paths == b.paths && a.block == b.block,
          "F10 regenerated non-audit block bit-identical (independent of prior blocks and audit capture)");
  R.check(a.summary.n1_ok == 1U && a.summary.n2_ok == 1U && a.summary.query_ok == 1U &&
              a.queries == static_cast<std::uint64_t>(kCells) * kUpdates * kCandidates,
          "F10 N1, N2 and exact 262144 queries per block");
  R.check(b.audit.size() == static_cast<std::size_t>(kCells) * kUpdates * kCandidates * kAuditRowBytes,
          "F10 audit capture size 8*256*128 rows");
  // Path late sums and u64 retry totals re-derived from the encoded per-update bytes.
  const auto le64 = [](const std::uint8_t* b) {
    return static_cast<std::uint64_t>(le32_from_bytes(b)) | (static_cast<std::uint64_t>(le32_from_bytes(b + 4)) << 32U);
  };
  bool recon = true, retries = true;
  for (std::uint32_t c = 0; c < kCells; ++c) {
    std::uint32_t lm = 0U, lmm = 0U;
    std::uint64_t ret = 0U;
    for (std::uint32_t t = kFirstUpdate; t <= kLastUpdate; ++t) {
      const std::uint8_t* p = a.updates.data() + (static_cast<std::size_t>(c) * kUpdates + (t - 1U)) * kUpdateRecordBytes;
      ret += le64(p + 16);
      if (t < kLateFirst) continue;
      lm += p[7];
      lmm += static_cast<std::uint32_t>(p[8]) | (static_cast<std::uint32_t>(p[9]) << 8U);
    }
    const std::uint8_t* q = a.paths.data() + static_cast<std::size_t>(c) * kPathRecordBytes;
    recon = recon && lm == le32_from_bytes(q + 8) && lmm == le32_from_bytes(q + 12);
    retries = retries && ret == le64(q + 36) && le32_from_bytes(q + 92) == 0U;
  }
  R.check(recon, "F10 per-path late sums reconstruct from per-update records");
  R.check(retries, "F10 u64 path retry total (offset 36) equals the sum of u64 update retry totals (offset 16)");
  // Paired-trajectory SHA-256 at path offsets 96-127.
  const auto paired_hash = [&a](std::uint32_t c) {
    const std::uint8_t* q = a.paths.data() + static_cast<std::size_t>(c) * kPathRecordBytes + 96U;
    return std::vector<std::uint8_t>(q, q + 32U);
  };
  R.check(paired_hash(4U) == paired_hash(5U) && paired_hash(6U) == paired_hash(7U),
          "F10 SHAM paired-trajectory hashes equal for path records 4/5 and 6/7");
  R.check(paired_hash(4U) != paired_hash(6U) && paired_hash(0U) != paired_hash(4U),
          "F10 paired-trajectory hash binds law and arm (4 vs 6, 0 vs 4 differ)");
  if (!emit_dir.empty()) {
    write_new_text_file(emit_dir + "/layout_sample_README.txt",
                        "Fixture-namespace block 41599 (non-scientific). Read by tests/test_merge_analyze.py.\n");
    std::FILE* f = open_new_file(emit_dir + "/layout_sample_updates.bin");
    write_all(f, a.updates.data(), a.updates.size());
    close_file(f);
    f = open_new_file(emit_dir + "/layout_sample_paths.bin");
    write_all(f, a.paths.data(), a.paths.size());
    close_file(f);
    f = open_new_file(emit_dir + "/layout_sample_block.bin");
    write_all(f, a.block.data(), a.block.size());
    close_file(f);
    f = open_new_file(emit_dir + "/layout_sample_audit.bin");
    write_all(f, b.audit.data(), b.audit.size());
    close_file(f);
  }
}

// K0 gates everything: no population path (F6, F10, or production) runs unless
// the literal Philox vectors match.
inline void run_all(Results& R, const std::string& emit_dir) {
  if (!fixture_k0_philox(R)) {
    R.check(false, "K0 gate: Philox mismatch; no population path was run");
    return;
  }
  fixture_s0_sha256(R);
  fixture_key_derivation(R);
  const KeySet prod = derive_keys(kProductionNamespace);
  const KeySet fix = derive_keys(kFixtureNamespace);
  const CollisionReport rep = run_collision_audit(prod, fix);
  R.check(rep.pass, "COLLISION declared-schema enumeration and purpose-separated collision audit");
  fixture_f1_hand_trace(R);
  fixture_f2_memory_better(R);
  fixture_f3_memory_worse(R);
  fixture_f4_duplicate(R);
  fixture_f5_invalid_cache(R);
  fixture_f6_sham_paths(R, fix);
  fixture_f7_target_pairing(R);
  fixture_f8_selection(R);
  fixture_f9_coordinate_invariance(R, fix);
  fixture_f10_regeneration(R, fix, emit_dir);
}

}  // namespace fixtures
}  // namespace mm
