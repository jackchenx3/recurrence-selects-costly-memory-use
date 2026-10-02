// Deterministic fixtures K0, S0, KEYS, COLLISION and F1-F13 (spec section 13).
// They use hand-constructed or injected values. F6, F9, F10 and F13 run
// complete paths only under the non-scientific fixture key namespace, so no
// production block or scientific outcome is ever sampled. Fixtures report
// PASS/FAIL only.
//
// Map to spec section 13:
//   1 accepted traces unchanged without the new arm .......... F1-F5, F7, F8 (INFO = accepted ACTIVE)
//   2 Fisher-Yates/Lemire, forced retries, known perms ........ F11
//   3 INFO == NONINFO on innovation updates ................... F1b, F3, F12, F13
//   4 first recurrent valid-M coordinate: only the probe differs F12, F13
//   5 33 displacement weights; weights 0 and 32 unchanged ...... F2, F11, F12
//   6 shared permutation preserves weights and overlaps ........ F11
//   7 exhaustive small-bit analogue of the uniform subset law ... F11
//   8 SHAM N1/N2 full paths and mixed-label diagnostic ......... F6, F10
//   9 unconditional generation, collision, branch invariance ... COLLISION, F9
//  10 non-audit path regenerates bit-identically ............... F10
#pragma once

#include <cstdint>
#include <cstring>
#include <map>
#include <set>
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
    std::string s = "{\n  \"receipt\": \"PCONV-FIXTURES-1\",\n  \"all_passed\": ";
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

// Injected Fisher-Yates values: (step, retry) -> x, with a default for retry 0.
class InjectedDecoy final : public DecoySource {
 public:
  explicit InjectedDecoy(std::uint64_t default_first_try) : default_(default_first_try) {}
  void set(std::uint32_t step, std::uint64_t retry, std::uint64_t x) { table_[std::make_pair(step, retry)] = x; }
  std::uint64_t draw_x(std::uint32_t step, std::uint64_t retry) override {
    const auto it = table_.find(std::make_pair(step, retry));
    if (it != table_.end()) return it->second;
    if (retry == 0U) return default_;
    fatal("fixture requested a decoy value that was not injected");
  }

 private:
  std::map<std::pair<std::uint32_t, std::uint64_t>, std::uint64_t> table_;
  std::uint64_t default_;
};

class RecordingDecoy final : public DecoySource {
 public:
  RecordingDecoy(const DrawSource& source, std::uint32_t t) : source_(source), t_(t) {}
  std::uint64_t draw_x(std::uint32_t step, std::uint64_t retry) override {
    log.push_back(std::make_pair(step, retry));
    return source_.decoy_x(t_, step, retry);
  }
  std::vector<std::pair<std::uint32_t, std::uint64_t>> log;

 private:
  const DrawSource& source_;
  std::uint32_t t_;
};

// x = 1 gives Z = 0 (first remaining candidate); x = 2^64-1 gives Z = W-1
// (last remaining candidate). Both are always accepted for any W.
constexpr std::uint64_t kXFirst = 1ULL;
constexpr std::uint64_t kXLast = 0xFFFFFFFFFFFFFFFFULL;
constexpr std::uint64_t kXHalf = 0x8000000000000001ULL;  // Z = floor(W/2) for W < 2^63

inline void identity_perm(std::uint8_t* p) {
  for (std::uint32_t s = 0; s < kGenotypeBits; ++s) p[s] = static_cast<std::uint8_t>(s);
}

// perm[s] = s + 1 mod 32: bit s moves to bit s + 1 (rotate left by one).
inline void rotation_perm(std::uint8_t* p) {
  for (std::uint32_t s = 0; s < kGenotypeBits; ++s) p[s] = static_cast<std::uint8_t>((s + 1U) & 31U);
}

inline std::uint32_t rotl1(std::uint32_t m) { return (m << 1U) | (m >> 31U); }

inline std::uint32_t low_mask(std::uint32_t w) { return w >= 32U ? 0xFFFFFFFFU : ((std::uint32_t{1} << w) - 1U); }

// Installs a hand-chosen permutation (no Fisher-Yates evidence).
inline void set_perm(UpdateDraws& d, const std::uint8_t* perm) {
  for (std::uint32_t s = 0; s < kGenotypeBits; ++s) {
    d.perm[s] = perm[s];
    d.perm_x[s] = 0U;
    d.perm_retry[s] = 0U;
  }
  d.perm_retry_total = 0U;
  d.perm_retry_steps = 0U;
  d.perm_fnv1a = perm_fnv1a(d.perm);
  d.perm_ready = 1U;
}

inline UpdateDraws blank_draws(std::uint32_t t) {
  UpdateDraws d;
  d.update = t;
  for (std::uint32_t i = 0; i < kPopulation; ++i)
    for (std::uint32_t f = 0; f < kDonorFamilies; ++f) d.donor[kDonorFamilies * i + f] = 100U + f;
  std::uint8_t id[kGenotypeBits];
  identity_perm(id);
  set_perm(d, id);
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

class PatternDecoy final : public DecoySource {
 public:
  explicit PatternDecoy(std::uint32_t salt) : salt_(salt) {}
  std::uint64_t draw_x(std::uint32_t step, std::uint64_t retry) override {
    return u64_from_words(pattern32(salt_, 64U * step + static_cast<std::uint32_t>(retry)), pattern32(salt_ + 1U, step));
  }

 private:
  std::uint32_t salt_;
};

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
  PatternDecoy pd(salt + 7U);
  install_decoy_permutation(pd, d);
  return d;
}

inline bool same_draws(const UpdateDraws& a, const UpdateDraws& b) {
  return a.update == b.update && a.innovation == b.innovation && a.copy_bit == b.copy_bit &&
         std::memcmp(a.fresh, b.fresh, sizeof a.fresh) == 0 && std::memcmp(a.scout, b.scout, sizeof a.scout) == 0 &&
         std::memcmp(a.local_mask, b.local_mask, sizeof a.local_mask) == 0 &&
         std::memcmp(a.donor, b.donor, sizeof a.donor) == 0 &&
         std::memcmp(a.policy_flip, b.policy_flip, sizeof a.policy_flip) == 0 &&
         std::memcmp(a.perm, b.perm, sizeof a.perm) == 0 && std::memcmp(a.perm_x, b.perm_x, sizeof a.perm_x) == 0 &&
         std::memcmp(a.perm_retry, b.perm_retry, sizeof a.perm_retry) == 0 && a.perm_retry_total == b.perm_retry_total &&
         a.perm_retry_steps == b.perm_retry_steps && a.perm_fnv1a == b.perm_fnv1a && a.perm_ready == b.perm_ready;
}

inline bool same_state(const PathState& a, const PathState& b) {
  return std::memcmp(a.genotype, b.genotype, sizeof a.genotype) == 0 &&
         std::memcmp(a.label, b.label, sizeof a.label) == 0 &&
         std::memcmp(a.cache_valid, b.cache_valid, sizeof a.cache_valid) == 0 &&
         std::memcmp(a.cache, b.cache, sizeof a.cache) == 0 && a.target_prev1 == b.target_prev1 &&
         a.target_prev2 == b.target_prev2 && a.completed_updates == b.completed_updates;
}

// Candidate genotypes, mismatches, weights, selection and donors (probe-source
// labels are compared separately because INFO and NONINFO label them differently).
inline bool same_trace(const UpdateTrace& a, const UpdateTrace& b) {
  return std::memcmp(a.candidate, b.candidate, sizeof a.candidate) == 0 &&
         std::memcmp(a.mismatch, b.mismatch, sizeof a.mismatch) == 0 &&
         std::memcmp(a.weight, b.weight, sizeof a.weight) == 0 &&
         std::memcmp(a.applied_mask, b.applied_mask, sizeof a.applied_mask) == 0 &&
         std::memcmp(a.selected_rank, b.selected_rank, sizeof a.selected_rank) == 0 &&
         std::memcmp(a.donor_family, b.donor_family, sizeof a.donor_family) == 0;
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
  std::string s = "{\n  \"receipt\": \"PCONV-PHILOX-KAT-1\",\n  \"source\": \"Random123 tests/kat_vectors rows 'philox4x32 10'\",\n  \"vectors\": [\n";
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
  std::string s = "{\n  \"receipt\": \"PCONV-PURPOSE-KEYS-1\",\n  \"namespace\": " + jstr(ks.ns) + ",\n  \"keys\": [\n";
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
  const KeySet old_prod = derive_predecessor_keys(kProductionNamespace);
  const KeySet old_fix = derive_predecessor_keys(kFixtureNamespace);
  R.check(purpose_key_text(kProductionNamespace, Purpose::kInitialGenotype) ==
                  "PHASE2-PERFORMANCE-CONVERSION-002|production-r1|INITIAL_GENOTYPE" &&
              purpose_key_text(kProductionNamespace, Purpose::kDecoyPermutation) ==
                  "PHASE2-PERFORMANCE-CONVERSION-002|production-r1|DECOY_PERMUTATION",
          "KEYS production key text matches the frozen template, including DECOY_PERMUTATION");
  R.check(purpose_key_text_for_study(kPredecessorStudyId, kProductionNamespace, Purpose::kInitialGenotype) ==
              "PHASE2-MUTABLE-MEMORY-001|production-r1|INITIAL_GENOTYPE",
          "KEYS predecessor key text reconstructed for the disjointness check only");
  R.check(keys_pairwise_distinct(prod), "KEYS ten production purpose keys are pairwise distinct");
  R.check(keys_pairwise_distinct(fix), "KEYS ten fixture purpose keys are pairwise distinct");
  R.check(keysets_disjoint(prod, fix), "KEYS fixture namespace shares no key with production");
  R.check(keysets_disjoint(prod, old_prod) && keysets_disjoint(prod, old_fix) && keysets_disjoint(fix, old_prod) &&
              keysets_disjoint(fix, old_fix),
          "KEYS no study-002 key equals any study-001 key (no random tape reused)");
}

// ---------------------------------------------------------------- F1
inline void fixture_f1_hand_trace(Results& R) {
  // The accepted study-001 hand trace, run through INFO (= accepted ACTIVE).
  // Updates 1 and 2 have T_t = I_t under HALF, exactly as under the accepted ZERO law.
  const CellSpec cell{Arm::kInfo, Start::kAllF};
  std::uint32_t g[kPopulation];
  for (std::uint32_t i = 0; i < kPopulation; ++i) g[i] = 0xFFFFFFFFU;
  g[0] = 0x0000000FU;
  g[1] = 0xFFFFFFFEU;
  g[31] = 0x7FFFFFFFU;
  std::uint8_t lab[kPopulation] = {};
  lab[1] = kLabelM;
  PathState s;
  initialize_path(s, g, lab);
  const PathState s0 = s;

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
  const UpdateDraws d_u1 = d;

  InjectedSurvival sv;
  sv.set(0U, 0U, kXHalf);
  sv.set(1U, 0U, kXLast);
  sv.set_default_first_try(kXFirst);
  UpdateTrace tr;
  const UpdateResult r = step(s, cell, d, sv, &tr);

  R.check(r.query_count == 128U, "F1 u1 exactly 128 objective queries");
  R.check(tr.candidate[32] == 0U && tr.candidate[35] == 0x7FFFFFFEU && tr.candidate[33] == 0xFFFFFFFEU &&
              tr.probe_source[1] == 0U,
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
  R.check(tr2.probe_source[1] == 1U && tr2.probe_source[3] == 1U && tr2.probe_source[0] == 0U &&
              tr2.candidate[33] == 0x7FFFFFFFU && tr2.candidate[35] == 0xFFFFFFFEU && tr2.candidate[32] == 0x00000001U,
          "F1 u2 valid INFO-M slots probe their cache");
  R.check(r2.valid_m_cache == 2U && r2.cache_probe_use == 2U && r2.query_count == 128U, "F1 u2 cache use and 128 queries");
  bool order2 = true;
  for (std::uint32_t k = 0; k < kSurvivors; ++k) order2 = order2 && r2.survivor_candidate[k] == k;
  R.check(order2 && r2.m_count == 2U && r2.f_to_m == 0U && r2.m_to_f == 0U && r2.total_mismatch == 963U &&
              r2.cache_probe_survivors == 0U,
          "F1 u2 survivors and labels");
  R.check(s.cache_valid[1] == 1U && s.cache[1] == 0x7FFFFFFFU && s.cache_valid[0] == 0U && s.completed_updates == 2U,
          "F1 u2 cache transition");

  // F1b: the same trace through NONINFO with a non-identity permutation and
  // R_1 = R_2 = 1 (t < 3, so no decoy) equals the accepted INFO trace exactly.
  {
    std::uint8_t rot[kGenotypeBits];
    rotation_perm(rot);
    PathState sn = s0;
    UpdateDraws dn1 = d_u1;
    UpdateDraws dn2 = blank_draws(2U);
    set_perm(dn1, rot);
    set_perm(dn2, rot);
    dn1.copy_bit = 1U;
    dn2.copy_bit = 1U;
    InjectedSurvival svn;
    svn.set(0U, 0U, kXHalf);
    svn.set(1U, 0U, kXLast);
    svn.set_default_first_try(kXFirst);
    InjectedSurvival svn2;
    svn2.set_default_first_try(kXFirst);
    UpdateTrace trn1, trn2;
    const UpdateResult rn1 = step(sn, CellSpec{Arm::kNoninfo, Start::kAllF}, dn1, svn, &trn1);
    const UpdateResult rn2 = step(sn, CellSpec{Arm::kNoninfo, Start::kAllF}, dn2, svn2, &trn2);
    R.check(same_trace(tr, trn1) && same_trace(tr2, trn2) && same_state(s, sn) &&
                std::memcmp(tr2.probe_source, trn2.probe_source, sizeof tr2.probe_source) == 0,
            "F1b NONINFO with R_t = 1 at t = 1, 2 reproduces the accepted INFO hand trace bit for bit");
    R.check(rn1.recurrence_applied == 0U && rn2.recurrence_applied == 0U && rn1.decoy_use == 0U && rn2.decoy_use == 0U &&
                rn2.true_cache_use == 2U,
            "F1b no recurrence event and no decoy at t < 3");
  }
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
  std::uint8_t rot[kGenotypeBits];
  rotation_perm(rot);
  set_perm(d, rot);
  InjectedSurvival sv;
  sv.set_default_first_try(kXFirst);
  PathState sa = single_slot_state(0x5A5A5A5AU, 0x5A5A5A5AU, A, B, A);
  PathState ss = sa;
  PathState sn = sa;
  UpdateTrace ta, ts, tn;
  const UpdateResult ra = step(sa, CellSpec{Arm::kInfo, Start::kAllM}, d, sv, &ta);
  const UpdateResult rs = step(ss, CellSpec{Arm::kSham, Start::kAllM}, d, sv, &ts);
  const UpdateResult rn = step(sn, CellSpec{Arm::kNoninfo, Start::kAllM}, d, sv, &tn);
  R.check(ra.target == A && ra.recurrence_applied == 1U, "F2 lag-two HALF target T_3 = T_1");
  R.check(ta.candidate[32] == A && ta.mismatch[32] == 0U && ta.weight[32] == (1ULL << 32) && ta.probe_source[0] == 1U,
          "F2 INFO cache probe mismatch 0");
  R.check(ts.candidate[32] == 0xA5A55A5AU && ts.mismatch[32] == 16U && ts.weight[32] == (1ULL << 16) &&
              ts.probe_source[0] == 0U && rs.cache_probe_use == 0U,
          "F2 SHAM/fresh probe mismatch 16");
  // Displacement c XOR x = 0xFFFFFFFF has weight 32: no permutation can move it.
  R.check(tn.probe_source[0] == 2U && tn.candidate[32] == A && rn.decoy_use == 1U && rn.decoy_w32 == 1U &&
              rn.decoy_w0 == 0U && rn.decoy_identical == 1U && rn.valid_m_disp_w32 == 1U && same_trace(ta, tn) &&
              same_state(sa, sn),
          "F2 weight-32 displacement: NONINFO decoy equals the true cache and is counted (decoy_w32)");
  R.check(ra.query_count == 128U && rs.query_count == 128U && rn.query_count == 128U, "F2 128 queries");
}

inline void fixture_f3_memory_worse(Results& R) {
  UpdateDraws d = blank_draws(3U);
  d.innovation = 0U;
  d.copy_bit = 0U;
  d.fresh[0] = 0x000000F0U;
  std::uint8_t rot[kGenotypeBits];
  rotation_perm(rot);
  set_perm(d, rot);
  InjectedSurvival sv;
  sv.set_default_first_try(kXFirst);
  PathState base = single_slot_state(0xFFFFFFFFU, 0x000000FFU, 0xFFFFFFF0U, 0x12345678U, 0x9ABCDEF0U);
  PathState sa = base, ss = base, sn = base;
  UpdateTrace ta, ts, tn;
  const UpdateResult ra = step(sa, CellSpec{Arm::kInfo, Start::kAllM}, d, sv, &ta);
  const UpdateResult rs = step(ss, CellSpec{Arm::kSham, Start::kAllM}, d, sv, &ts);
  const UpdateResult rn = step(sn, CellSpec{Arm::kNoninfo, Start::kAllM}, d, sv, &tn);
  R.check(ra.target == 0U && rn.target == 0U && ra.recurrence_applied == 0U && rn.recurrence_applied == 0U,
          "F3 innovation target at t = 3 with R_3 = 0 (no recurrence)");
  R.check(ta.candidate[32] == 0xFFFFFFF0U && ta.mismatch[32] == 28U && ta.weight[32] == 16U, "F3 INFO cache mismatch 28");
  R.check(ts.candidate[32] == 0x0000000FU && ts.mismatch[32] == 4U && ts.weight[32] == (1ULL << 28), "F3 fresh probe mismatch 4");
  R.check(same_trace(ta, tn) && same_state(sa, sn) && tn.probe_source[0] == 1U && rn.decoy_use == 0U &&
              rn.true_cache_use == 1U,
          "F3 NONINFO uses the identical true cache on an innovation update (t >= 3, R_t = 0)");
  R.check(ra.query_count == 128U && rs.query_count == 128U && rn.query_count == 128U, "F3 128 queries");
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
  const UpdateResult r = step(s, CellSpec{Arm::kInfo, Start::kAllM}, d, sv, &tr);
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
  R.check(r.valid_m_disp_w0 == 1U && r.valid_m_disp_w32 == 0U && r.decoy_use == 0U,
          "F4 weight-0 displacement counted diagnostically in INFO");
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
  PathState am, af, sm, sf, nm;
  initialize_path(am, g, mixed);
  initialize_path(af, g, allf);
  initialize_path(sm, g, mixed);
  initialize_path(sf, g, allf);
  initialize_path(nm, g, mixed);
  const UpdateDraws d1 = pattern_draws(1U, 100U);
  InjectedSurvival sv;
  sv.set_default_first_try(kXFirst);
  UpdateTrace t_am, t_af, t_sm, t_sf, t_nm;
  const UpdateResult r_am = step(am, CellSpec{Arm::kInfo, Start::kAllM}, d1, sv, &t_am);
  const UpdateResult r_af = step(af, CellSpec{Arm::kInfo, Start::kAllF}, d1, sv, &t_af);
  const UpdateResult r_sm = step(sm, CellSpec{Arm::kSham, Start::kAllM}, d1, sv, &t_sm);
  const UpdateResult r_sf = step(sf, CellSpec{Arm::kSham, Start::kAllF}, d1, sv, &t_sf);
  const UpdateResult r_nm = step(nm, CellSpec{Arm::kNoninfo, Start::kAllM}, d1, sv, &t_nm);
  const bool same_cands = std::memcmp(t_am.candidate, t_af.candidate, sizeof t_am.candidate) == 0 &&
                          std::memcmp(t_am.candidate, t_sm.candidate, sizeof t_am.candidate) == 0 &&
                          std::memcmp(t_am.candidate, t_sf.candidate, sizeof t_am.candidate) == 0 &&
                          std::memcmp(t_am.candidate, t_nm.candidate, sizeof t_am.candidate) == 0;
  R.check(same_cands && r_am.cache_probe_use == 0U && r_nm.cache_probe_use == 0U,
          "F5 invalid-cache INFO-M and NONINFO-M use the same fresh draw as F and SHAM");
  R.check(std::memcmp(r_am.survivor_candidate, r_sf.survivor_candidate, sizeof r_am.survivor_candidate) == 0 &&
              std::memcmp(r_af.survivor_candidate, r_sm.survivor_candidate, sizeof r_af.survivor_candidate) == 0 &&
              std::memcmp(r_am.survivor_candidate, r_nm.survivor_candidate, sizeof r_am.survivor_candidate) == 0,
          "F5 identical survivors at update 1");
  R.check(same_state(am, sm) && same_state(am, nm), "F5 INFO, NONINFO and SHAM states identical after update 1");

  const PathState before = am;
  const UpdateDraws d2 = pattern_draws(2U, 200U);
  UpdateTrace a2, s2, n2;
  step(am, CellSpec{Arm::kInfo, Start::kAllM}, d2, sv, &a2);
  step(sm, CellSpec{Arm::kSham, Start::kAllM}, d2, sv, &s2);
  step(nm, CellSpec{Arm::kNoninfo, Start::kAllM}, d2, sv, &n2);
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
  R.check(valid == 16U && diverged > 0U && only_valid, "F5 INFO and SHAM first diverge only at valid-M slots");
  R.check(same_trace(a2, n2) && same_state(am, nm), "F5 NONINFO equals INFO at update 2 (t < 3)");
}

// ---------------------------------------------------------------- F6
inline void fixture_f6_sham_paths(Results& R, const KeySet& fixture_keys) {
  const DrawSource src(fixture_keys, 12345U);
  std::uint32_t g[kPopulation];
  for (std::uint32_t i = 0; i < kPopulation; ++i) g[i] = src.initial_genotype(i);
  std::uint8_t lab[4][kPopulation];
  for (std::uint32_t i = 0; i < kPopulation; ++i) {
    lab[0][i] = kLabelF;
    lab[1][i] = kLabelM;
    lab[2][i] = (i % 3U == 0U) ? kLabelM : kLabelF;  // mixed-label diagnostic
    lab[3][i] = static_cast<std::uint8_t>(lab[2][i] ^ 1U);
  }
  PathState st[4];
  for (int k = 0; k < 4; ++k) initialize_path(st[k], g, lab[k]);
  const CellSpec cs{Arm::kSham, Start::kAllF};
  bool n1 = true, n2 = true, q = true;
  std::uint32_t late_sum = 0U;
  UpdateDraws d;
  for (std::uint32_t t = kFirstUpdate; t <= kLastUpdate; ++t) {
    fill_update_all(src, t, d);
    PhiloxSurvivalSource sv(src, t);
    UpdateResult r[4];
    for (int k = 0; k < 4; ++k) {
      r[k] = step(st[k], cs, d, sv, nullptr);
      q = q && r[k].query_count == 128U && r[k].cache_probe_use == 0U && r[k].decoy_use == 0U;
    }
    for (int k = 1; k < 4; ++k) n1 = n1 && sham_pair_identical(r[0], r[k], st[0], st[k]);
    n2 = n2 && sham_pair_complement(r[0], r[1], st[0], st[1]) && sham_pair_complement(r[2], r[3], st[2], st[3]);
    if (t >= kLateFirst) late_sum += r[0].m_count + r[1].m_count;
  }
  R.check(n1, "F6 N1: SHAM ALL_F, ALL_M and mixed starts bit-identical for 256 updates");
  R.check(n2, "F6 N2: SHAM labels exact complements after every update (pure and mixed starts)");
  R.check(late_sum == kLateLength * kPopulation, "F6 start-averaged late M frequency exactly 1/2");
  R.check(q, "F6 128 queries per update, no SHAM cache or decoy use");
}

// ---------------------------------------------------------------- F7
inline void fixture_f7_target_law(Results& R) {
  const std::uint32_t I[9] = {0U, 0x11111111U, 0x22222222U, 0x33333333U, 0x44444444U,
                              0x55555555U, 0x66666666U, 0x77777777U, 0x88888888U};
  const std::uint32_t Rb[9] = {0U, 1U, 1U, 1U, 0U, 1U, 1U, 0U, 1U};
  const std::uint32_t expected_half[9] = {0U, 0x11111111U, 0x22222222U, 0x11111111U, 0x44444444U,
                                          0x11111111U, 0x44444444U, 0x77777777U, 0x44444444U};
  std::uint32_t g[kPopulation];
  for (std::uint32_t i = 0; i < kPopulation; ++i) g[i] = pattern32(9U, i);
  PathState st[kCells];
  for (std::uint32_t c = 0; c < kCells; ++c) initialize_path_for_start(st[c], g, cell_spec(c).start);
  bool half_ok = true, flags = true, shared = true;
  for (std::uint32_t t = 1; t <= 8U; ++t) {
    UpdateDraws d = blank_draws(t);
    d.innovation = I[t];
    d.copy_bit = Rb[t];
    InjectedSurvival sv;
    sv.set_default_first_try(kXFirst);
    UpdateResult r[kCells];
    for (std::uint32_t c = 0; c < kCells; ++c) r[c] = step(st[c], cell_spec(c), d, sv, nullptr);
    half_ok = half_ok && r[0].target == expected_half[t];
    flags = flags && r[0].recurrence_applied == ((t >= 3U && Rb[t] == 1U) ? 1U : 0U);
    for (std::uint32_t c = 1; c < kCells; ++c)
      shared = shared && r[c].target == r[0].target && r[c].recurrence_applied == r[0].recurrence_applied;
  }
  R.check(half_ok && flags, "F7 HALF copies T_(t-2) only when R_t=1 and t>=3 (t=1,2 boundary ignores R)");
  R.check(shared, "F7 all six cells share the target and recurrence-event series");
  R.check(target_for_update(2U, 7U, 1U, 99U) == 7U && target_for_update(3U, 7U, 1U, 99U) == 99U &&
              target_for_update(3U, 7U, 0U, 99U) == 7U && !recurrence_event(2U, 1U) && recurrence_event(3U, 1U) &&
              !recurrence_event(3U, 0U),
          "F7 target_for_update and recurrence_event boundary cases");
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
inline bool perm_evidence_regenerates(const DrawSource& src, std::uint32_t t, const UpdateDraws& d) {
  bool ok = d.perm_ready == 1U && is_bit_permutation(d.perm) && d.perm_fnv1a == perm_fnv1a(d.perm);
  for (std::uint32_t i = 1U; i <= kPermFirstStep; ++i) ok = ok && d.perm_x[i] == src.decoy_x(t, i, d.perm_retry[i]);
  // Replay Fisher-Yates from the stored accepted values only.
  std::uint8_t p[kGenotypeBits];
  identity_perm(p);
  for (std::uint32_t i = kPermFirstStep; i >= 1U; --i) {
    std::uint64_t j = 0U;
    ok = ok && lemire_accept(d.perm_x[i], static_cast<std::uint64_t>(i) + 1U, j);
    const std::uint8_t tmp = p[i];
    p[i] = p[j];
    p[j] = tmp;
  }
  return ok && std::memcmp(p, d.perm, sizeof p) == 0;
}

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
              ctr_policy_mutation(7U, 9U, 5U) == PhiloxCounter{{7U, 9U, 5U, 0U}} &&
              ctr_decoy_permutation(7U, 9U, 31U, 0xFFFFFFFFULL) == PhiloxCounter{{7U, 9U, 31U, 0xFFFFFFFFU}} &&
              ctr_decoy_permutation(7U, 9U, 1U, 0U) == PhiloxCounter{{7U, 9U, 1U, 0U}},
          "F9 literal counter words (block, update, entity, subindex), including DECOY_PERMUTATION (step, retry)");
  R.check(u64_from_words(0x89ABCDEFU, 0x01234567U) == 0x0123456789ABCDEFULL &&
              u64_from_words(0U, 0x80000000U) == 0x8000000000000000ULL && u64_from_words(0xFFFFFFFFU, 0U) == 0xFFFFFFFFULL,
          "F9 word1 widened to 64 bits before << 32");

  const DrawSource src(fixture_keys, 777U);
  const std::uint32_t t = 5U;
  UpdateDraws before, after;
  fill_update_all(src, t, before);
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
  fill_update_all(src, t, after);
  R.check(same_draws(before, after), "F9 update draws (including the permutation) unchanged by labels, arm and start");
  bool consistent = !logs.empty();
  for (const RecordingSurvival::Entry& e : logs) consistent = consistent && e.x == src.survival_x(t, e.draw, e.retry);
  R.check(consistent, "F9 survival value depends only on (block, update, draw, retry)");

  // Unconditional generation: the permutation is generated for recurrent and
  // innovation updates alike, every step 31..1 in descending order.
  bool found_rec = false, found_innov = false, regen = true, order_ok = true;
  for (std::uint32_t u = 3U; u <= kLastUpdate && !(found_rec && found_innov); ++u) {
    UpdateDraws d;
    fill_update_all(src, u, d);
    const bool rec = recurrence_event(u, d.copy_bit);
    if (rec && found_rec) continue;
    if (!rec && found_innov) continue;
    (rec ? found_rec : found_innov) = true;
    regen = regen && perm_evidence_regenerates(src, u, d);
    RecordingDecoy rd(src, u);
    std::uint8_t p[kGenotypeBits];
    fisher_yates_permutation(rd, kGenotypeBits, p, nullptr);
    std::uint32_t expect_step = kPermFirstStep;
    for (const auto& e : rd.log) {
      if (e.second == 0U) {
        order_ok = order_ok && e.first == expect_step;
        --expect_step;
      } else {
        order_ok = order_ok && e.first == expect_step + 1U;
      }
    }
    order_ok = order_ok && expect_step == 0U && std::memcmp(p, d.perm, sizeof p) == 0;
  }
  R.check(found_rec && found_innov && regen,
          "F9 permutation generated and regenerable from (block, update, step, retry) at both recurrent and innovation updates");
  R.check(order_ok, "F9 Fisher-Yates requests steps 31..1 in order, retries only after rejection");
}

// ---------------------------------------------------------------- F10
inline void fixture_f10_regeneration(Results& R, const KeySet& fixture_keys, const std::string& emit_dir) {
  BlockOutput a, other, b;
  run_block(fixture_keys, 41599U, false, a);
  run_block(fixture_keys, 12U, true, other);
  run_block(fixture_keys, 41599U, true, b);
  R.check(a.updates == b.updates && a.paths == b.paths && a.block == b.block,
          "F10 regenerated non-audit block bit-identical (independent of prior blocks and audit capture)");
  R.check(a.summary.n1_ok == 1U && a.summary.n2_ok == 1U && a.summary.query_ok == 1U && a.summary.c1_ok == 1U &&
              a.queries == static_cast<std::uint64_t>(kCells) * kUpdates * kCandidates,
          "F10 N1, N2, C1 and exact 196608 queries per block");
  R.check(b.audit.size() == static_cast<std::size_t>(kCells) * kUpdates * kCandidates * kAuditRowBytes &&
              b.perms.size() == static_cast<std::size_t>(kUpdates) * kAuditPermRecordBytes,
          "F10 audit capture size 6*256*128 rows and 256 permutation records");
  const auto le64 = [](const std::uint8_t* q) {
    return static_cast<std::uint64_t>(le32_from_bytes(q)) | (static_cast<std::uint64_t>(le32_from_bytes(q + 4)) << 32U);
  };
  bool recon = true, retries = true, extras = true, shared_perm = true;
  for (std::uint32_t c = 0; c < kCells; ++c) {
    std::uint32_t lm = 0U, lmm = 0U;
    std::uint64_t ret = 0U, pret = 0U;
    std::uint32_t sums[10] = {};
    for (std::uint32_t t = kFirstUpdate; t <= kLastUpdate; ++t) {
      const std::uint8_t* p = a.updates.data() + (static_cast<std::size_t>(c) * kUpdates + (t - 1U)) * kUpdateRecordBytes;
      const std::uint8_t* p0 = a.updates.data() + (t - 1U) * kUpdateRecordBytes;  // cell 0, same update
      ret += le64(p + 16);
      pret += le32_from_bytes(p + 36);
      for (std::uint32_t k = 0; k < 10U; ++k) sums[k] += p[24 + k];  // offsets 24..33
      shared_perm = shared_perm && le32_from_bytes(p + 40) == le32_from_bytes(p0 + 40) &&
                    le32_from_bytes(p + 36) == le32_from_bytes(p0 + 36) && p[24] == p0[24];
      if (t < kLateFirst) continue;
      lm += p[7];
      lmm += static_cast<std::uint32_t>(p[8]) | (static_cast<std::uint32_t>(p[9]) << 8U);
    }
    const std::uint8_t* q = a.paths.data() + static_cast<std::size_t>(c) * kPathRecordBytes;
    recon = recon && lm == le32_from_bytes(q + 8) && lmm == le32_from_bytes(q + 12);
    retries = retries && ret == le64(q + 36) && le32_from_bytes(q + 92) == 0U && pret == le64(q + 168);
    // Path offsets 164 (recurrent updates) and 128..160 (use/survival/extreme-weight totals).
    extras = extras && sums[0] == le32_from_bytes(q + 164);
    for (std::uint32_t k = 1; k < 10U; ++k) extras = extras && sums[k] == le32_from_bytes(q + 128 + 4 * (k - 1U));
  }
  R.check(recon, "F10 per-path late sums reconstruct from per-update records");
  R.check(retries, "F10 u64 path survival and permutation retry totals equal the sums of the per-update fields");
  R.check(extras, "F10 per-path true-cache/decoy use, survival and weight-0/32 totals reconstruct from per-update records");
  R.check(shared_perm, "F10 permutation fingerprint, retry total and recurrence flag identical across the six cells");
  const auto paired_hash = [&a](std::uint32_t c) {
    const std::uint8_t* q = a.paths.data() + static_cast<std::size_t>(c) * kPathRecordBytes + 96U;
    return std::vector<std::uint8_t>(q, q + 32U);
  };
  R.check(paired_hash(4U) == paired_hash(5U), "F10 SHAM paired-trajectory hashes equal for path records 4/5");
  R.check(paired_hash(0U) != paired_hash(4U) && paired_hash(2U) != paired_hash(4U) && paired_hash(0U) != paired_hash(2U),
          "F10 paired-trajectory hash binds the arm (0, 2 and 4 differ)");
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
    f = open_new_file(emit_dir + "/layout_sample_permutations.bin");
    write_all(f, b.perms.data(), b.perms.size());
    close_file(f);
  }
}

// ---------------------------------------------------------------- F11
// x with high64(x*n) = j and an accepted low word, for 0 <= j < n <= 32.
inline std::uint64_t index_x(std::uint32_t j, std::uint32_t n) {
  const std::uint64_t q = 0xFFFFFFFFFFFFFFFFULL / n;
  return q * j + (1ULL << 32);
}

inline void fixture_f11_permutation(Results& R) {
  R.check(lemire_threshold(2ULL) == 0ULL && lemire_threshold(3ULL) == 1ULL && lemire_threshold(7ULL) == 2ULL &&
              lemire_threshold(30ULL) == 16ULL && lemire_threshold(31ULL) == 16ULL && lemire_threshold(32ULL) == 0ULL,
          "F11 Lemire thresholds 2^64 mod n for n = 2, 3, 7, 30, 31, 32");
  {
    std::uint64_t j = 99U;
    const std::uint64_t rej31 = 0xFFFFFFFFFFFFFFFFULL / 31ULL + 1ULL;  // 31*x = 2^64 + 15: low 15 < 16
    bool ok = !lemire_accept(0ULL, 3ULL, j) && !lemire_accept(0ULL, 31ULL, j) && !lemire_accept(rej31, 31ULL, j);
    ok = ok && lemire_accept(0x5555555555555555ULL, 3ULL, j) && j == 0U;
    ok = ok && lemire_accept(0x5555555555555556ULL, 3ULL, j) && j == 1U;
    ok = ok && lemire_accept(1ULL, 31ULL, j) && j == 0U;
    ok = ok && lemire_accept(0ULL, 32ULL, j) && j == 0U;
    R.check(ok, "F11 exact Lemire accept/reject cases, including a rejected nonzero x for n = 31");
    bool all = true;
    for (std::uint32_t n = 2U; n <= 32U; ++n) {
      all = all && lemire_accept(kXLast, n, j) && j == n - 1U;
      for (std::uint32_t k = 0; k < n; ++k) all = all && lemire_accept(index_x(k, n), n, j) && j == k;
    }
    R.check(all, "F11 every index j in [0, i] is reachable for every bound 2..32");
  }
  {
    InjectedDecoy src(kXLast);
    std::uint8_t p[kGenotypeBits];
    PermutationTrace tr;
    fisher_yates_permutation(src, kGenotypeBits, p, &tr);
    std::uint8_t id[kGenotypeBits];
    identity_perm(id);
    R.check(std::memcmp(p, id, sizeof p) == 0 && tr.retry_total == 0U && tr.retry_steps == 0U,
            "F11 x = 2^64-1 at every step (j = i) gives the identity permutation");
  }
  {
    InjectedDecoy src(1ULL);
    std::uint8_t p[kGenotypeBits];
    fisher_yates_permutation(src, kGenotypeBits, p, nullptr);
    std::uint8_t rot[kGenotypeBits];
    rotation_perm(rot);
    bool ok = std::memcmp(p, rot, sizeof p) == 0 && permute_bits(0x80000001U, p) == 0x00000003U;
    for (std::uint32_t k = 0; k < 16U; ++k) ok = ok && permute_bits(pattern32(31U, k), p) == rotl1(pattern32(31U, k));
    R.check(ok, "F11 x = 1 at every step (j = 0) gives perm[s] = s+1 mod 32; bit s moves to perm[s]");
  }
  {
    // Forced retries: step 30 rejects x = 0 and a nonzero x, accepts retry 2;
    // step 2 rejects x = 0, accepts retry 1; all other steps take j = 0.
    InjectedDecoy src(1ULL);
    src.set(30U, 0U, 0ULL);
    src.set(30U, 1U, 0xFFFFFFFFFFFFFFFFULL / 31ULL + 1ULL);
    src.set(30U, 2U, kXLast);
    src.set(2U, 0U, 0ULL);
    src.set(2U, 1U, kXLast);
    std::uint8_t p[kGenotypeBits];
    PermutationTrace tr;
    fisher_yates_permutation(src, kGenotypeBits, p, &tr);
    std::uint8_t want[kGenotypeBits];
    want[0] = 1U;
    want[1] = 3U;
    want[2] = 2U;
    for (std::uint32_t k = 3; k <= 28U; ++k) want[k] = static_cast<std::uint8_t>(k + 1U);
    want[29] = 31U;
    want[30] = 30U;
    want[31] = 0U;
    bool js = tr.j[30] == 30U && tr.j[2] == 2U;
    for (std::uint32_t i = 1; i <= kPermFirstStep; ++i)
      if (i != 30U && i != 2U) js = js && tr.j[i] == 0U;
    R.check(std::memcmp(p, want, sizeof p) == 0 && js, "F11 forced-retry permutation equals the hand-derived result");
    R.check(tr.retry[30] == 2U && tr.retry[2] == 1U && tr.retry_total == 3U && tr.retry_steps == 2U &&
                tr.x[30] == kXLast && tr.x[2] == kXLast,
            "F11 forced Lemire retries recorded as (step, retry) evidence");
  }
  {
    // Exhaustive small-bit analogues (n = 4 and 5 positions): all n! choice
    // sequences give n! distinct permutations, and every weight-k mask maps
    // onto each of the C(n,k) weight-k subsets exactly n!/C(n,k) times.
    bool ok = true;
    for (std::uint32_t n = 4U; n <= 5U; ++n) {
      std::uint32_t fact = 1U;
      for (std::uint32_t k = 2U; k <= n; ++k) fact *= k;
      std::vector<std::vector<std::uint8_t>> perms;
      std::set<std::vector<std::uint8_t>> distinct;
      for (std::uint32_t code = 0; code < fact; ++code) {
        InjectedDecoy src(kXLast);
        std::uint32_t rest = code;
        std::uint32_t js[kGenotypeBits] = {};
        for (std::uint32_t i = n - 1U; i >= 1U; --i) {
          js[i] = rest % (i + 1U);
          rest /= (i + 1U);
          src.set(i, 0U, index_x(js[i], i + 1U));
        }
        std::uint8_t p[kGenotypeBits];
        identity_perm(p);
        PermutationTrace tr;
        fisher_yates_permutation(src, n, p, &tr);
        for (std::uint32_t i = 1U; i < n; ++i) ok = ok && tr.j[i] == js[i];
        for (std::uint32_t s = n; s < kGenotypeBits; ++s) ok = ok && p[s] == s;
        std::vector<std::uint8_t> v(p, p + n);
        perms.push_back(v);
        distinct.insert(v);
      }
      ok = ok && distinct.size() == fact;
      for (std::uint32_t m = 0; m < (1U << n); ++m) {
        std::map<std::uint32_t, std::uint32_t> images;
        for (const std::vector<std::uint8_t>& v : perms) {
          std::uint8_t full[kGenotypeBits];
          identity_perm(full);
          for (std::uint32_t s = 0; s < n; ++s) full[s] = v[s];
          ++images[permute_bits(m, full)];
        }
        const std::uint32_t k = popcount32(m);
        std::uint32_t binom = 1U;
        for (std::uint32_t r = 0; r < k; ++r) binom = binom * (n - r) / (r + 1U);
        ok = ok && images.size() == binom;
        for (const auto& e : images) ok = ok && popcount32(e.first) == k && e.second == fact / binom;
      }
    }
    R.check(ok, "F11 exhaustive 4- and 5-position analogues: exact uniform permutations and uniform subset law");
  }
  {
    // Fixture 6: one common relabeling preserves weights and pairwise overlaps.
    UpdateDraws d = pattern_draws(9U, 500U);
    std::uint8_t rot[kGenotypeBits];
    rotation_perm(rot);
    bool ok = true;
    for (const std::uint8_t* perm : {static_cast<const std::uint8_t*>(d.perm), static_cast<const std::uint8_t*>(rot)}) {
      for (std::uint32_t s = 0; s < kGenotypeBits; ++s) ok = ok && permute_bits(1U << s, perm) == (1U << perm[s]);
      for (std::uint32_t k = 0; k < 64U; ++k) {
        const std::uint32_t a = pattern32(41U, k), b = pattern32(42U, k);
        const std::uint32_t pa = permute_bits(a, perm), pb = permute_bits(b, perm);
        ok = ok && popcount32(pa) == popcount32(a) && popcount32(pa & pb) == popcount32(a & b) &&
             permute_bits(a ^ b, perm) == (pa ^ pb) && permute_bits(a | b, perm) == (pa | pb);
      }
    }
    R.check(ok, "F11 shared permutation preserves Hamming weights and pairwise overlaps of displacement masks");
  }
  {
    // Fixture 5 at operator level: all 33 displacement weights.
    UpdateDraws d = pattern_draws(9U, 600U);
    bool ok = true;
    std::uint32_t moved = 0U;
    for (std::uint32_t w = 0; w <= 32U; ++w) {
      const std::uint32_t x = pattern32(70U, w);
      const std::uint32_t c = x ^ low_mask(w);
      const std::uint32_t decoy = x ^ permute_bits(c ^ x, d.perm);
      ok = ok && popcount32(decoy ^ x) == w;
      if (w == 0U || w == 32U) ok = ok && decoy == c;
      if (decoy != c) ++moved;
    }
    R.check(ok && moved > 0U, "F11 decoy distance to parent equals cache distance for weights 0..32; weights 0 and 32 unchanged");
  }
}

// ---------------------------------------------------------------- F12
inline PathState f12_state(std::uint32_t completed) {
  std::uint32_t g[kPopulation];
  std::uint8_t lab[kPopulation];
  for (std::uint32_t i = 0; i < kPopulation; ++i) {
    g[i] = pattern32(80U, i);
    lab[i] = (i % 4U == 3U) ? kLabelF : kLabelM;
  }
  PathState s;
  initialize_path(s, g, lab);
  for (std::uint32_t i = 0; i < kPopulation; ++i) {
    if (lab[i] != kLabelM || i == 1U) continue;  // slot 1: M with an invalid cache
    s.cache_valid[i] = 1U;
    s.cache[i] = g[i] ^ (i == 30U ? 0xFFFFFFFFU : low_mask(i));  // weight i; slot 30 weight 32
  }
  s.target_prev1 = pattern32(81U, 1U);
  s.target_prev2 = pattern32(81U, 2U);
  s.completed_updates = completed;
  return s;
}

inline void fixture_f12_noninfo_operator(Results& R) {
  // (a) First recurrent valid-M coordinate.
  UpdateDraws d = pattern_draws(3U, 300U);
  d.copy_bit = 1U;
  InjectedSurvival sv;
  sv.set_default_first_try(kXHalf);
  const PathState s0 = f12_state(2U);
  PathState si = s0, sn = s0, ss = s0;
  UpdateTrace ti, tn, ts;
  const UpdateResult ri = step(si, CellSpec{Arm::kInfo, Start::kAllM}, d, sv, &ti);
  const UpdateResult rn = step(sn, CellSpec{Arm::kNoninfo, Start::kAllM}, d, sv, &tn);
  const UpdateResult rs = step(ss, CellSpec{Arm::kSham, Start::kAllM}, d, sv, &ts);
  std::uint32_t valid = 0U;
  bool fam02 = true, fam1 = true, fam3 = true, weights = true;
  for (std::uint32_t i = 0; i < kPopulation; ++i) {
    const std::uint32_t x = s0.genotype[i];
    const bool v = s0.label[i] == kLabelM && s0.cache_valid[i] != 0U;
    fam02 = fam02 && ti.candidate[i] == tn.candidate[i] && ti.candidate[64 + i] == tn.candidate[64 + i];
    if (v) {
      ++valid;
      const std::uint32_t disp = s0.cache[i] ^ x;
      fam1 = fam1 && ti.candidate[32 + i] == s0.cache[i] && ti.probe_source[i] == 1U && tn.probe_source[i] == 2U &&
             tn.candidate[32 + i] == (x ^ permute_bits(disp, d.perm));
      weights = weights && popcount32(tn.candidate[32 + i] ^ x) == popcount32(disp);
      if (i == 0U || i == 30U) weights = weights && tn.candidate[32 + i] == s0.cache[i];
    } else {
      fam1 = fam1 && ti.candidate[32 + i] == (x ^ d.fresh[i]) && tn.candidate[32 + i] == ti.candidate[32 + i] &&
             ti.probe_source[i] == 0U && tn.probe_source[i] == 0U;
    }
    if (ti.donor_family[i] != 1U && tn.donor_family[i] != 1U)
      fam3 = fam3 && ti.candidate[96 + i] == tn.candidate[96 + i] && ti.donor_family[i] == tn.donor_family[i];
  }
  R.check(fam02, "F12 first recurrent update: parent and scout candidates identical in INFO and NONINFO");
  R.check(fam1 && valid == 23U,
          "F12 only valid-M policy probes differ: NONINFO probe = x XOR pi(c XOR x) with the one shared permutation");
  R.check(fam3, "F12 local children identical wherever neither arm takes the probe as donor (downstream only)");
  R.check(weights, "F12 decoy parent distance equals cache distance for weights 0..29 and 32; weights 0 and 32 equal the cache");
  R.check(rn.decoy_use == valid && rn.true_cache_use == 0U && rn.decoy_w0 == 1U && rn.decoy_w32 == 1U &&
              rn.decoy_identical >= 2U && ri.decoy_use == 0U && ri.true_cache_use == valid && rs.cache_probe_use == 0U,
          "F12 per-update counters: decoy use, extreme weights 0/32, identical-to-cache decoys");
  R.check(ri.valid_m_disp_w0 == 1U && ri.valid_m_disp_w32 == 1U && rn.valid_m_disp_w0 == 1U &&
              rn.valid_m_disp_w32 == 1U && rs.valid_m_disp_w0 == 1U && rs.valid_m_disp_w32 == 1U,
          "F12 diagnostic weight-0/32 counts identical in all three arms");

  // (b) INFO and SHAM ignore the permutation entirely.
  {
    UpdateDraws dr = d;
    std::uint8_t rot[kGenotypeBits];
    rotation_perm(rot);
    set_perm(dr, rot);
    PathState si2 = s0, ss2 = s0;
    UpdateTrace ti2, ts2;
    step(si2, CellSpec{Arm::kInfo, Start::kAllM}, dr, sv, &ti2);
    step(ss2, CellSpec{Arm::kSham, Start::kAllM}, dr, sv, &ts2);
    R.check(same_trace(ti, ti2) && same_state(si, si2) && same_trace(ts, ts2) && same_state(ss, ss2),
            "F12 INFO and SHAM are invariant to the permutation value");
  }
  // (c) Innovation update at t >= 3 and boundary t = 2 with R_t = 1: NONINFO == INFO.
  {
    UpdateDraws di = d;
    di.copy_bit = 0U;
    PathState a = s0, b = s0;
    UpdateTrace ta, tb;
    const UpdateResult ra = step(a, CellSpec{Arm::kInfo, Start::kAllM}, di, sv, &ta);
    const UpdateResult rb = step(b, CellSpec{Arm::kNoninfo, Start::kAllM}, di, sv, &tb);
    UpdateDraws d2 = pattern_draws(2U, 310U);
    d2.copy_bit = 1U;
    PathState a2 = f12_state(1U), b2 = f12_state(1U);
    UpdateTrace ta2, tb2;
    step(a2, CellSpec{Arm::kInfo, Start::kAllM}, d2, sv, &ta2);
    const UpdateResult rb2 = step(b2, CellSpec{Arm::kNoninfo, Start::kAllM}, d2, sv, &tb2);
    R.check(same_trace(ta, tb) && same_state(a, b) && ra.true_cache_use == rb.true_cache_use && rb.decoy_use == 0U &&
                std::memcmp(ta.probe_source, tb.probe_source, sizeof ta.probe_source) == 0,
            "F12 NONINFO equals INFO bit for bit at an innovation update (t >= 3, R_t = 0)");
    R.check(same_trace(ta2, tb2) && same_state(a2, b2) && rb2.decoy_use == 0U && rb2.recurrence_applied == 0U,
            "F12 NONINFO equals INFO at t = 2 even when R_t = 1");
  }
}

// ---------------------------------------------------------------- F13
inline void fixture_f13_coupling(Results& R, const KeySet& fixture_keys) {
  const std::uint32_t block = 2024U;
  BlockOutput out;
  run_block(fixture_keys, block, false, out);
  const DrawSource src(fixture_keys, block);
  std::uint32_t g[kPopulation];
  for (std::uint32_t i = 0; i < kPopulation; ++i) g[i] = src.initial_genotype(i);
  bool ok_all = true, first_ok = true, only_probe = true;
  std::uint32_t first[2] = {0U, 0U};
  for (std::uint32_t st = 0; st < 2U; ++st) {
    const Start start = st == 0U ? Start::kAllF : Start::kAllM;
    PathState si, sn;
    initialize_path_for_start(si, g, start);
    initialize_path_for_start(sn, g, start);
    UpdateDraws d;
    for (std::uint32_t t = kFirstUpdate; t <= kLastUpdate && first[st] == 0U; ++t) {
      fill_update_all(src, t, d);
      PhiloxSurvivalSource sv(src, t);
      const PathState pre = si;
      UpdateTrace ti, tn;
      step(si, CellSpec{Arm::kInfo, start}, d, sv, &ti);
      const UpdateResult rn = step(sn, CellSpec{Arm::kNoninfo, start}, d, sv, &tn);
      if (rn.decoy_use != rn.decoy_identical) {
        first[st] = t;
        first_ok = first_ok && recurrence_event(t, d.copy_bit);
        for (std::uint32_t i = 0; i < kPopulation; ++i) {
          only_probe = only_probe && ti.candidate[i] == tn.candidate[i] && ti.candidate[64 + i] == tn.candidate[64 + i];
          if (tn.probe_source[i] != 2U) only_probe = only_probe && ti.candidate[32 + i] == tn.candidate[32 + i];
          else only_probe = only_probe && tn.candidate[32 + i] == (pre.genotype[i] ^ permute_bits(pre.cache[i] ^ pre.genotype[i], d.perm));
        }
      } else {
        ok_all = ok_all && same_trace(ti, tn) && same_state(si, sn);
      }
    }
  }
  R.check(ok_all, "F13 full fixture paths: INFO and NONINFO bit-identical before the first effective decoy (both starts)");
  R.check(first_ok && first[1] != 0U, "F13 first effective decoy occurs at a recurrent update (ALL_M start decouples)");
  R.check(only_probe, "F13 at the decoupling update only decoy policy probes differ before downstream selection");
  R.check(out.summary.c1_ok == 1U && out.summary.first_decoupling_update[0] == first[0] &&
              out.summary.first_decoupling_update[1] == first[1],
          "F13 run_block C1 flag and recorded decoupling updates agree with the independent stepping");
}

// K0 gates everything: no population path (F6, F9, F10, F13, or production)
// runs unless the literal Philox vectors match.
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
  R.check(rep.pass, "COLLISION declared-schema enumeration and purpose-separated collision audit (six paired cells)");
  fixture_f1_hand_trace(R);
  fixture_f2_memory_better(R);
  fixture_f3_memory_worse(R);
  fixture_f4_duplicate(R);
  fixture_f5_invalid_cache(R);
  fixture_f6_sham_paths(R, fix);
  fixture_f7_target_law(R);
  fixture_f8_selection(R);
  fixture_f9_coordinate_invariance(R, fix);
  fixture_f10_regeneration(R, fix, emit_dir);
  fixture_f11_permutation(R);
  fixture_f12_noninfo_operator(R);
  fixture_f13_coupling(R, fix);
}

}  // namespace fixtures
}  // namespace mm
