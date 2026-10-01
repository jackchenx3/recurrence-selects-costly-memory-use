// Runs the eight paired cells of one block in lockstep, checks N1/N2 after
// every update and encodes all per-update, per-path, per-block and (for audit
// blocks) candidate-level records. Each path also carries a paired-trajectory
// SHA-256 so that N1/N2 can be re-checked from saved path records in every block.
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include "mm/constants.hpp"
#include "mm/draws.hpp"
#include "mm/keys.hpp"
#include "mm/model.hpp"
#include "mm/records.hpp"

namespace mm {

struct BlockOutput {
  std::vector<std::uint8_t> updates;  // 8 cells x 256 updates, cell-major
  std::vector<std::uint8_t> paths;    // 8 records
  std::vector<std::uint8_t> block;    // 1 record
  std::vector<std::uint8_t> audit;    // 8 x 256 x 128 rows when requested
  BlockSummary summary;
  std::uint64_t queries = 0U;
};

// N1: SHAM genotypes, mismatches, targets and survivor indices are identical
// between the ALL_F and ALL_M starts.
inline bool sham_pair_identical(const UpdateResult& a, const UpdateResult& b, const PathState& sa, const PathState& sb) {
  if (a.target != b.target || a.total_mismatch != b.total_mismatch) return false;
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

inline void run_block(const KeySet& keys, std::uint32_t block, bool with_audit, BlockOutput& out) {
  out.updates.assign(static_cast<std::size_t>(kCells) * kUpdates * kUpdateRecordBytes, 0U);
  out.paths.assign(static_cast<std::size_t>(kCells) * kPathRecordBytes, 0U);
  out.block.assign(kBlockRecordBytes, 0U);
  if (with_audit) {
    out.audit.assign(static_cast<std::size_t>(kCells) * kUpdates * kCandidates * kAuditRowBytes, 0U);
  } else {
    out.audit.clear();
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
  bool n1 = true, n2 = true, query_ok = true;

  for (std::uint32_t t = kFirstUpdate; t <= kLastUpdate; ++t) {
    source.fill_update(t, draws);
    PhiloxSurvivalSource survival(source, t);
    for (std::uint32_t c = 0; c < kCells; ++c) {
      results[c] = step(states[c], cell_spec(c), draws, survival, with_audit ? &trace : nullptr);
      const std::size_t row = static_cast<std::size_t>(c) * kUpdates + (t - 1U);
      encode_update_record(out.updates.data() + row * kUpdateRecordBytes, block, c, results[c]);
      if (with_audit) encode_audit_rows(out.audit.data() + row * kCandidates * kAuditRowBytes, block, c, results[c], trace);
      acc[c].add(results[c]);
      paired[c].add(results[c], states[c]);
      out.queries += results[c].query_count;
      if (results[c].query_count != kCandidates) query_ok = false;
    }
    n1 = n1 && sham_pair_identical(results[4], results[5], states[4], states[5]) &&
         sham_pair_identical(results[6], results[7], states[6], states[7]);
    n2 = n2 && sham_pair_complement(results[4], results[5], states[4], states[5]) &&
         sham_pair_complement(results[6], results[7], states[6], states[7]);
  }

  for (std::uint32_t c = 0; c < kCells; ++c) {
    encode_path_record(out.paths.data() + static_cast<std::size_t>(c) * kPathRecordBytes, block, c, acc[c],
                       final_state_hash(block, c, states[c]), paired[c].finish());
  }
  out.summary = make_block_summary(block, acc, n1, n2, query_ok);
  encode_block_record(out.block.data(), out.summary);
}

}  // namespace mm
