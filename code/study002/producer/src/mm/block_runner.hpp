// Runs the six paired cells of one block in lockstep, checks N1/N2 and the
// INFO/NONINFO coupling identity C1 after every update, and encodes all
// per-update, per-path, per-block and (for audit blocks) candidate-level and
// permutation records. Each path also carries a paired-trajectory SHA-256 so
// that N1/N2 can be re-checked from saved path records in every block.
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include "mm/constants.hpp"
#include "mm/draws.hpp"
#include "mm/keys.hpp"
#include "mm/model.hpp"
#include "mm/records.hpp"
#include "mm/selection.hpp"

namespace mm {

struct BlockOutput {
  std::vector<std::uint8_t> updates;  // 6 cells x 256 updates, cell-major
  std::vector<std::uint8_t> paths;    // 6 records
  std::vector<std::uint8_t> block;    // 1 record
  std::vector<std::uint8_t> audit;    // 6 x 256 x 128 rows when requested
  std::vector<std::uint8_t> perms;    // 256 permutation records when requested
  BlockSummary summary;
  std::uint64_t queries = 0U;
};

// N1: SHAM genotypes, mismatches, targets and survivor indices are identical
// between the ALL_F and ALL_M starts.
inline bool sham_pair_identical(const UpdateResult& a, const UpdateResult& b, const PathState& sa, const PathState& sb) {
  if (a.target != b.target || a.total_mismatch != b.total_mismatch) return false;
  if (a.survival_retries != b.survival_retries) return false;
  if (std::memcmp(a.survivor_candidate, b.survivor_candidate, sizeof(a.survivor_candidate)) != 0) return false;
  return std::memcmp(sa.genotype, sb.genotype, sizeof(sa.genotype)) == 0;
}

// N2: SHAM labels in the two starts are exact complements.
inline bool sham_pair_complement(const UpdateResult& a, const UpdateResult& b, const PathState& sa, const PathState& sb) {
  if (a.m_count + b.m_count != kPopulation) return false;
  for (std::uint32_t i = 0; i < kPopulation; ++i)
    if ((sa.label[i] ^ sb.label[i]) != 1U) return false;
  return true;
}

// C1 (spec fixtures 3-4 enforced at run time): while INFO and NONINFO of one
// start have identical states and no decoy probe has differed from the true
// cache, their updates and post-update states are bit-identical.
inline bool coupled_pair_identical(const UpdateResult& a, const UpdateResult& b, const PathState& sa,
                                   const PathState& sb) {
  if (a.target != b.target || a.total_mismatch != b.total_mismatch || a.m_count != b.m_count) return false;
  if (a.f_to_m != b.f_to_m || a.m_to_f != b.m_to_f || a.survival_retries != b.survival_retries) return false;
  if (a.valid_m_cache != b.valid_m_cache || a.cache_probe_use != b.cache_probe_use) return false;
  if (a.cache_probe_survivors != b.cache_probe_survivors) return false;
  if (std::memcmp(a.survivor_candidate, b.survivor_candidate, sizeof(a.survivor_candidate)) != 0) return false;
  return std::memcmp(sa.genotype, sb.genotype, sizeof(sa.genotype)) == 0 &&
         std::memcmp(sa.label, sb.label, sizeof(sa.label)) == 0 &&
         std::memcmp(sa.cache_valid, sb.cache_valid, sizeof(sa.cache_valid)) == 0 &&
         std::memcmp(sa.cache, sb.cache, sizeof(sa.cache)) == 0 && sa.target_prev1 == sb.target_prev1 &&
         sa.target_prev2 == sb.target_prev2 && sa.completed_updates == sb.completed_updates;
}

// Pre-update diagnostics of coupled INFO/NONINFO paths (identical pre-update states).
inline bool coupled_pre_update_identical(const UpdateResult& a, const UpdateResult& b) {
  return a.valid_m_cache == b.valid_m_cache && a.valid_m_disp_w0 == b.valid_m_disp_w0 &&
         a.valid_m_disp_w32 == b.valid_m_disp_w32 && a.cache_probe_use == b.cache_probe_use &&
         a.target == b.target;
}

inline void run_block(const KeySet& keys, std::uint32_t block, bool with_audit, BlockOutput& out) {
  out.updates.assign(static_cast<std::size_t>(kCells) * kUpdates * kUpdateRecordBytes, 0U);
  out.paths.assign(static_cast<std::size_t>(kCells) * kPathRecordBytes, 0U);
  out.block.assign(kBlockRecordBytes, 0U);
  if (with_audit) {
    out.audit.assign(static_cast<std::size_t>(kCells) * kUpdates * kCandidates * kAuditRowBytes, 0U);
    out.perms.assign(static_cast<std::size_t>(kUpdates) * kAuditPermRecordBytes, 0U);
  } else {
    out.audit.clear();
    out.perms.clear();
  }
  out.queries = 0U;

  const DrawSource source(keys, block);
  std::uint32_t initial[kPopulation];
  for (std::uint32_t i = 0; i < kPopulation; ++i) initial[i] = source.initial_genotype(i);

  PathState states[kCells];
  PathAccumulator acc[kCells];
  PairedTrajectoryHash paired[kCells];
  for (std::uint32_t c = 0; c < kCells; ++c) {
    initialize_path_for_start(states[c], initial, cell_spec(c).start);
    paired[c].begin(block, c);
  }

  UpdateDraws draws;
  UpdateTrace trace;
  UpdateResult results[kCells];
  BlockFlags flags;
  flags.n1 = true;
  flags.n2 = true;
  flags.query = true;
  flags.c1 = true;
  bool coupled[2] = {true, true};

  for (std::uint32_t t = kFirstUpdate; t <= kLastUpdate; ++t) {
    // Every exogenous draw of this update, including the decoy permutation, is
    // generated before any cell is stepped and independently of every cell.
    fill_update_all(source, t, draws);
    if (with_audit) encode_audit_permutation(out.perms.data() + static_cast<std::size_t>(t - 1U) * kAuditPermRecordBytes, block, draws);
    if (draws.perm_retry_total > 0xFFFFFFFFU - flags.perm_retry_total) fatal("block permutation retry total overflow");
    flags.perm_retry_total += draws.perm_retry_total;
    if (recurrence_event(t, draws.copy_bit)) ++flags.recurrent_updates;
    PhiloxSurvivalSource survival(source, t);
    for (std::uint32_t c = 0; c < kCells; ++c) {
      results[c] = step(states[c], cell_spec(c), draws, survival, with_audit ? &trace : nullptr);
      const std::size_t row = static_cast<std::size_t>(c) * kUpdates + (t - 1U);
      encode_update_record(out.updates.data() + row * kUpdateRecordBytes, block, c, results[c]);
      if (with_audit) encode_audit_rows(out.audit.data() + row * kCandidates * kAuditRowBytes, block, c, results[c], trace);
      acc[c].add(results[c]);
      paired[c].add(results[c], states[c]);
      out.queries += results[c].query_count;
      if (results[c].query_count != kCandidates) flags.query = false;
    }
    flags.n1 = flags.n1 && sham_pair_identical(results[4], results[5], states[4], states[5]);
    flags.n2 = flags.n2 && sham_pair_complement(results[4], results[5], states[4], states[5]);
    for (std::uint32_t st = 0; st < 2U; ++st) {
      if (!coupled[st]) continue;
      const UpdateResult& ri = results[st];       // INFO, this start
      const UpdateResult& rn = results[2U + st];  // NONINFO, this start
      if (!coupled_pre_update_identical(ri, rn)) flags.c1 = false;
      if (rn.decoy_use != rn.decoy_identical) {
        // First update at which the frozen decoy operator changes a probe;
        // only recurrent updates may decouple.
        if (rn.recurrence_applied != 1U) flags.c1 = false;
        coupled[st] = false;
        flags.first_decoupling_update[st] = t;
        continue;
      }
      if (!coupled_pair_identical(ri, rn, states[st], states[2U + st])) flags.c1 = false;
    }
  }

  for (std::uint32_t c = 0; c < kCells; ++c) {
    encode_path_record(out.paths.data() + static_cast<std::size_t>(c) * kPathRecordBytes, block, c, acc[c],
                       final_state_hash(block, c, states[c]), paired[c].finish());
  }
  out.summary = make_block_summary(block, acc, flags);
  encode_block_record(out.block.data(), out.summary);
}

}  // namespace mm
