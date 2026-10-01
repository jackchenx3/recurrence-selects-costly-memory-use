// Little-endian fixed-size output records. Layouts are documented in
// docs/OUTPUT_FORMATS.md and schemas/binary_records.json and mirrored by
// tools/mmem_common.py.
#pragma once

#include <cstddef>
#include <cstdint>

#include "mm/constants.hpp"
#include "mm/fatal.hpp"
#include "mm/model.hpp"
#include "mm/sha256.hpp"

namespace mm {

constexpr std::size_t kUpdateRecordBytes = 24U;
constexpr std::size_t kPathRecordBytes = 128U;
constexpr std::size_t kBlockRecordBytes = 96U;
constexpr std::size_t kAuditRowBytes = 72U;
// Derived from the fixed record counts and sizes (docs/OUTPUT_FORMATS.md).
constexpr std::uint64_t kTotalRecordBytes = kTotalPathUpdates * kUpdateRecordBytes + kTotalPaths * kPathRecordBytes +
                                            static_cast<std::uint64_t>(kBlocks) * kBlockRecordBytes +
                                            kAuditRows * kAuditRowBytes;
static_assert(kTotalRecordBytes == 3299274752ULL, "total record bytes");

inline void put_u8(std::uint8_t* p, std::uint32_t v) {
  require(v <= 0xFFU, "u8 field overflow");
  p[0] = static_cast<std::uint8_t>(v);
}
inline void put_u16(std::uint8_t* p, std::uint32_t v) {
  require(v <= 0xFFFFU, "u16 field overflow");
  p[0] = static_cast<std::uint8_t>(v);
  p[1] = static_cast<std::uint8_t>(v >> 8U);
}
inline void put_u32(std::uint8_t* p, std::uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}
inline void put_i32(std::uint8_t* p, std::int64_t v) {
  require(v >= INT32_MIN && v <= INT32_MAX, "i32 field overflow");
  put_u32(p, static_cast<std::uint32_t>(static_cast<std::int32_t>(v)));
}
inline void put_u64(std::uint8_t* p, std::uint64_t v) {
  for (int i = 0; i < 8; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}

struct PathAccumulator {
  std::uint32_t late_m_sum = 0U;
  std::uint32_t late_mismatch_sum = 0U;
  std::uint32_t total_queries = 0U;
  std::uint32_t f_to_m = 0U;
  std::uint32_t m_to_f = 0U;
  std::uint32_t cache_probe_use = 0U;
  std::uint32_t cache_probe_survivors = 0U;
  std::uint64_t survival_retries = 0U;  // path total < 256*32*2^32 = 2^45
  std::uint32_t fixation_updates = 0U;
  std::uint32_t extinction_updates = 0U;
  std::uint32_t late_fixation_updates = 0U;
  std::uint32_t late_extinction_updates = 0U;
  std::uint32_t final_m_count = 0U;
  std::uint32_t final_total_mismatch = 0U;

  void add(const UpdateResult& r) {
    const bool late = r.update >= kLateFirst && r.update <= kLateLast;
    if (late) {
      late_m_sum += r.m_count;
      late_mismatch_sum += r.total_mismatch;
    }
    total_queries += r.query_count;
    f_to_m += r.f_to_m;
    m_to_f += r.m_to_f;
    cache_probe_use += r.cache_probe_use;
    cache_probe_survivors += r.cache_probe_survivors;
    survival_retries += r.survival_retries;
    if (r.m_count == kPopulation) {
      ++fixation_updates;
      if (late) ++late_fixation_updates;
    }
    if (r.m_count == 0U) {
      ++extinction_updates;
      if (late) ++late_extinction_updates;
    }
    final_m_count = r.m_count;
    final_total_mismatch = r.total_mismatch;
  }
};

// Exact integer numerators of the six block variables (fixed denominators:
// C_abs, C_rec /4096; D_HALF, D_ZERO /2048; P_abs, P_rec /131072).
struct BlockSummary {
  std::uint32_t block = 0U;
  std::uint8_t n1_ok = 0U;
  std::uint8_t n2_ok = 0U;
  std::uint8_t query_ok = 0U;
  std::uint8_t audit_block = 0U;
  std::int64_t c_abs_num = 0;
  std::int64_t c_rec_num = 0;
  std::int64_t d_half_num = 0;
  std::int64_t d_zero_num = 0;
  std::int64_t p_abs_num = 0;
  std::int64_t p_rec_num = 0;
  std::uint32_t late_m_sum[kCells] = {};
  std::uint32_t late_mismatch_sum[kCells] = {};
};

inline BlockSummary make_block_summary(std::uint32_t block, const PathAccumulator* acc, bool n1, bool n2, bool q) {
  BlockSummary b;
  b.block = block;
  b.n1_ok = n1 ? 1U : 0U;
  b.n2_ok = n2 ? 1U : 0U;
  b.query_ok = q ? 1U : 0U;
  b.audit_block = block < kAuditBlocks ? 1U : 0U;
  std::int64_t L[kCells];
  std::int64_t P[kCells];
  for (std::uint32_t c = 0; c < kCells; ++c) {
    b.late_m_sum[c] = acc[c].late_m_sum;
    b.late_mismatch_sum[c] = acc[c].late_mismatch_sum;
    L[c] = acc[c].late_m_sum;
    P[c] = acc[c].late_mismatch_sum;
  }
  // Cells: 0 A|Z|F 1 A|Z|M 2 A|H|F 3 A|H|M 4 S|Z|F 5 S|Z|M 6 S|H|F 7 S|H|M
  b.c_abs_num = (L[2] + L[3]) - 2048;
  b.c_rec_num = (L[2] + L[3]) - (L[0] + L[1]);
  b.d_half_num = L[3] - L[2];
  b.d_zero_num = L[1] - L[0];
  const std::int64_t p_abs_half = (P[6] + P[7]) - (P[2] + P[3]);  // SHAM minus ACTIVE mismatch = ACTIVE minus SHAM accuracy
  const std::int64_t p_abs_zero = (P[4] + P[5]) - (P[0] + P[1]);
  b.p_abs_num = p_abs_half;
  b.p_rec_num = p_abs_half - p_abs_zero;
  return b;
}

inline void encode_update_record(std::uint8_t* p, std::uint32_t block, std::uint32_t cell, const UpdateResult& r) {
  put_u32(p + 0, block);
  put_u16(p + 4, r.update);
  put_u8(p + 6, cell);
  put_u8(p + 7, r.m_count);
  put_u16(p + 8, r.total_mismatch);
  put_u8(p + 10, r.valid_m_cache);
  put_u8(p + 11, r.cache_probe_use);
  put_u8(p + 12, r.cache_probe_survivors);
  put_u8(p + 13, r.f_to_m);
  put_u8(p + 14, r.m_to_f);
  put_u8(p + 15, r.query_count);
  put_u64(p + 16, r.survival_retries);
}

inline void encode_path_record(std::uint8_t* p, std::uint32_t block, std::uint32_t cell, const PathAccumulator& a,
                               const Sha256Digest& final_hash, const Sha256Digest& paired_hash) {
  const CellSpec cs = cell_spec(cell);
  put_u32(p + 0, block);
  put_u8(p + 4, cell);
  put_u8(p + 5, static_cast<std::uint32_t>(cs.arm));
  put_u8(p + 6, static_cast<std::uint32_t>(cs.law));
  put_u8(p + 7, static_cast<std::uint32_t>(cs.start));
  put_u32(p + 8, a.late_m_sum);
  put_u32(p + 12, a.late_mismatch_sum);
  put_u32(p + 16, a.total_queries);
  put_u32(p + 20, a.f_to_m);
  put_u32(p + 24, a.m_to_f);
  put_u32(p + 28, a.cache_probe_use);
  put_u32(p + 32, a.cache_probe_survivors);
  put_u64(p + 36, a.survival_retries);
  put_u16(p + 44, a.fixation_updates);
  put_u16(p + 46, a.extinction_updates);
  put_u16(p + 48, a.late_fixation_updates);
  put_u16(p + 50, a.late_extinction_updates);
  for (std::size_t i = 0; i < 32U; ++i) p[52 + i] = final_hash[i];
  put_u32(p + 84, a.final_m_count);
  put_u32(p + 88, a.final_total_mismatch);
  put_u32(p + 92, 0U);
  for (std::size_t i = 0; i < 32U; ++i) p[96 + i] = paired_hash[i];
}

inline void encode_block_record(std::uint8_t* p, const BlockSummary& b) {
  put_u32(p + 0, b.block);
  put_u8(p + 4, b.n1_ok);
  put_u8(p + 5, b.n2_ok);
  put_u8(p + 6, b.query_ok);
  put_u8(p + 7, b.audit_block);
  put_i32(p + 8, b.c_abs_num);
  put_i32(p + 12, b.c_rec_num);
  put_i32(p + 16, b.d_half_num);
  put_i32(p + 20, b.d_zero_num);
  put_i32(p + 24, b.p_abs_num);
  put_i32(p + 28, b.p_rec_num);
  for (std::uint32_t c = 0; c < kCells; ++c) {
    put_u32(p + 32 + 4 * c, b.late_m_sum[c]);
    put_u32(p + 64 + 4 * c, b.late_mismatch_sum[c]);
  }
}

// 128 rows for one (block, cell, update), in candidate-index order.
inline void encode_audit_rows(std::uint8_t* base, std::uint32_t block, std::uint32_t cell, const UpdateResult& r,
                              const UpdateTrace& tr) {
  for (std::uint32_t j = 0; j < kCandidates; ++j) {
    std::uint8_t* p = base + static_cast<std::size_t>(j) * kAuditRowBytes;
    const std::uint32_t family = j / kPopulation;
    const std::uint32_t parent = j % kPopulation;
    const std::uint8_t rank = tr.selected_rank[j];
    const bool selected = rank != kNotApplicable;
    put_u32(p + 0, block);
    put_u16(p + 4, r.update);
    put_u8(p + 6, cell);
    put_u8(p + 7, j);
    put_u8(p + 8, family);
    put_u8(p + 9, parent);
    put_u8(p + 10, tr.parent_label[parent]);
    put_u8(p + 11, tr.parent_cache_valid[parent]);
    put_u32(p + 12, tr.parent_cache[parent]);
    put_u32(p + 16, tr.candidate[j]);
    put_u32(p + 20, r.target);
    put_u8(p + 24, tr.mismatch[j]);
    put_u8(p + 25, r.copy_bit);
    put_u8(p + 26, r.recurrence_applied);
    put_u8(p + 27, family == 1U ? tr.probe_from_cache[parent] : kNotApplicable);
    put_u64(p + 28, tr.weight[j]);
    put_u8(p + 36, rank);
    put_u8(p + 37, selected ? tr.post_label[rank] : kNotApplicable);
    put_u8(p + 38, selected ? tr.policy_flip[rank] : kNotApplicable);
    put_u8(p + 39, family == 3U ? tr.donor_family[parent] : kNotApplicable);
    put_u64(p + 40, selected ? tr.draw[rank].total_weight : 0U);
    put_u64(p + 48, selected ? tr.draw[rank].x : 0U);
    put_u64(p + 56, selected ? tr.draw[rank].z : 0U);
    put_u32(p + 64, selected ? tr.draw[rank].retry : 0U);
    put_u32(p + 68, 0U);
  }
}

// SHA-256 over "MMEM-FINAL-STATE-V1", block u32, cell u8, completed u32, then
// per slot genotype u32, label u8, cache_valid u8, cache u32, then T_256, T_255.
inline Sha256Digest final_state_hash(std::uint32_t block, std::uint32_t cell, const PathState& s) {
  Sha256 h;
  static const char kTag[] = "MMEM-FINAL-STATE-V1";
  h.update(kTag, sizeof(kTag) - 1U);
  std::uint8_t buf[10];
  put_u32(buf, block);
  h.update(buf, 4U);
  put_u8(buf, cell);
  h.update(buf, 1U);
  put_u32(buf, s.completed_updates);
  h.update(buf, 4U);
  for (std::uint32_t i = 0; i < kPopulation; ++i) {
    put_u32(buf, s.genotype[i]);
    put_u8(buf + 4, s.label[i]);
    put_u8(buf + 5, s.cache_valid[i]);
    put_u32(buf + 6, s.cache[i]);
    h.update(buf, 10U);
  }
  put_u32(buf, s.target_prev1);
  put_u32(buf + 4, s.target_prev2);
  h.update(buf, 8U);
  return h.finish();
}

// Audit evidence only; never read by a scientific operator. SHA-256 over
// "MMEM-PAIRED-TRAJECTORY-V1", block u32, arm u8, law u8 (no cell, no start),
// then after every completed update: update u16, target u32, recurrence bit u8,
// recurrence applied u8, total mismatch u16; 32 survivor candidate indices u8 in
// survivor-slot order; 32 post-update genotypes u32; post-update label mask u32
// (bit i = label of slot i, 1 = M), XORed with 0xffffffff for an ALL_M start.
// Correct SHAM paths 4/5 and 6/7 therefore have identical hashes (N1 and N2).
class PairedTrajectoryHash {
 public:
  void begin(std::uint32_t block, std::uint32_t cell) {
    const CellSpec cs = cell_spec(cell);
    h_.reset();
    all_m_ = cs.start == Start::kAllM;
    static const char kTag[] = "MMEM-PAIRED-TRAJECTORY-V1";
    h_.update(kTag, sizeof(kTag) - 1U);
    std::uint8_t buf[6];
    put_u32(buf, block);
    put_u8(buf + 4, static_cast<std::uint32_t>(cs.arm));
    put_u8(buf + 5, static_cast<std::uint32_t>(cs.law));
    h_.update(buf, 6U);
  }

  void add(const UpdateResult& r, const PathState& s) {
    std::uint8_t buf[10 + kSurvivors + 4U * kPopulation + 4U];
    put_u16(buf + 0, r.update);
    put_u32(buf + 2, r.target);
    put_u8(buf + 6, r.copy_bit);
    put_u8(buf + 7, r.recurrence_applied);
    put_u16(buf + 8, r.total_mismatch);
    std::uint8_t* p = buf + 10;
    for (std::uint32_t k = 0; k < kSurvivors; ++k) put_u8(p + k, r.survivor_candidate[k]);
    p += kSurvivors;
    std::uint32_t mask = 0U;
    for (std::uint32_t i = 0; i < kPopulation; ++i) {
      put_u32(p + 4U * i, s.genotype[i]);
      mask |= static_cast<std::uint32_t>(s.label[i] & 1U) << i;
    }
    p += 4U * kPopulation;
    put_u32(p, all_m_ ? (mask ^ 0xFFFFFFFFU) : mask);
    h_.update(buf, sizeof buf);
  }

  Sha256Digest finish() { return h_.finish(); }

 private:
  Sha256 h_;
  bool all_m_ = false;
};

}  // namespace mm
