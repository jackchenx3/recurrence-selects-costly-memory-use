// Fixed little-endian record layouts from OUTPUT_FORMATS.md / binary_records.json,
// with field tables used to name the first differing field of a mismatching record.
#ifndef MMEM_AUDIT_RECORDS_HPP
#define MMEM_AUDIT_RECORDS_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "sha256.hpp"

namespace mmaudit {

constexpr std::size_t kUpdateBytes = 24;
constexpr std::size_t kPathBytes = 128;
constexpr std::size_t kBlockBytes = 96;
constexpr std::size_t kAuditBytes = 72;

struct FieldSpec {
  const char* name;
  unsigned offset;
  unsigned width;
  bool is_signed;
};

static const FieldSpec kUpdateFields[] = {
    {"block", 0, 4, false},           {"update", 4, 2, false},
    {"cell", 6, 1, false},            {"m_count", 7, 1, false},
    {"total_mismatch", 8, 2, false},  {"valid_m_cache", 10, 1, false},
    {"cache_probe_use", 11, 1, false}, {"cache_probe_survivors", 12, 1, false},
    {"f_to_m", 13, 1, false},         {"m_to_f", 14, 1, false},
    {"query_count", 15, 1, false},    {"survival_retries_u64", 16, 8, false},
};

static const FieldSpec kPathFields[] = {
    {"block", 0, 4, false},
    {"cell", 4, 1, false},
    {"arm", 5, 1, false},
    {"law", 6, 1, false},
    {"start", 7, 1, false},
    {"late_m_sum", 8, 4, false},
    {"late_mismatch_sum", 12, 4, false},
    {"total_queries", 16, 4, false},
    {"f_to_m", 20, 4, false},
    {"m_to_f", 24, 4, false},
    {"cache_probe_use", 28, 4, false},
    {"cache_probe_survivors", 32, 4, false},
    {"survival_retries_u64", 36, 8, false},
    {"fixation_updates", 44, 2, false},
    {"extinction_updates", 46, 2, false},
    {"late_fixation_updates", 48, 2, false},
    {"late_extinction_updates", 50, 2, false},
    {"final_state_sha256", 52, 32, false},
    {"final_m_count", 84, 4, false},
    {"final_total_mismatch", 88, 4, false},
    {"reserved0", 92, 4, false},
    {"paired_trajectory_sha256", 96, 32, false},
};

static const FieldSpec kBlockFields[] = {
    {"block", 0, 4, false},
    {"n1_ok", 4, 1, false},
    {"n2_ok", 5, 1, false},
    {"query_ok", 6, 1, false},
    {"audit_block", 7, 1, false},
    {"c_abs_num_over_4096", 8, 4, true},
    {"c_rec_num_over_4096", 12, 4, true},
    {"d_half_num_over_2048", 16, 4, true},
    {"d_zero_num_over_2048", 20, 4, true},
    {"p_abs_num_over_131072", 24, 4, true},
    {"p_rec_num_over_131072", 28, 4, true},
    {"late_m_sum[0]", 32, 4, false},
    {"late_m_sum[1]", 36, 4, false},
    {"late_m_sum[2]", 40, 4, false},
    {"late_m_sum[3]", 44, 4, false},
    {"late_m_sum[4]", 48, 4, false},
    {"late_m_sum[5]", 52, 4, false},
    {"late_m_sum[6]", 56, 4, false},
    {"late_m_sum[7]", 60, 4, false},
    {"late_mismatch_sum[0]", 64, 4, false},
    {"late_mismatch_sum[1]", 68, 4, false},
    {"late_mismatch_sum[2]", 72, 4, false},
    {"late_mismatch_sum[3]", 76, 4, false},
    {"late_mismatch_sum[4]", 80, 4, false},
    {"late_mismatch_sum[5]", 84, 4, false},
    {"late_mismatch_sum[6]", 88, 4, false},
    {"late_mismatch_sum[7]", 92, 4, false},
};

static const FieldSpec kAuditFields[] = {
    {"block", 0, 4, false},
    {"update", 4, 2, false},
    {"cell", 6, 1, false},
    {"candidate", 7, 1, false},
    {"family", 8, 1, false},
    {"parent", 9, 1, false},
    {"parent_label_pre", 10, 1, false},
    {"parent_cache_valid", 11, 1, false},
    {"parent_cache", 12, 4, false},
    {"genotype", 16, 4, false},
    {"target", 20, 4, false},
    {"mismatch", 24, 1, false},
    {"recurrence_bit", 25, 1, false},
    {"recurrence_applied", 26, 1, false},
    {"probe_source", 27, 1, false},
    {"weight", 28, 8, false},
    {"selected_rank", 36, 1, false},
    {"post_label", 37, 1, false},
    {"policy_flip", 38, 1, false},
    {"donor_family", 39, 1, false},
    {"W", 40, 8, false},
    {"x", 48, 8, false},
    {"Z", 56, 8, false},
    {"retry", 64, 4, false},
    {"reserved", 68, 4, false},
};

inline void put_le(std::uint8_t* p, std::uint64_t v, unsigned width) {
  for (unsigned i = 0; i < width; ++i) p[i] = static_cast<std::uint8_t>(v >> (8u * i));
}

inline std::uint64_t get_le(const std::uint8_t* p, unsigned width) {
  std::uint64_t v = 0;
  for (unsigned i = 0; i < width; ++i) v |= static_cast<std::uint64_t>(p[i]) << (8u * i);
  return v;
}

struct UpdateRecord {
  std::uint32_t block = 0;
  std::uint16_t update = 0;
  std::uint8_t cell = 0;
  std::uint8_t m_count = 0;
  std::uint16_t total_mismatch = 0;
  std::uint8_t valid_m_cache = 0;
  std::uint8_t cache_probe_use = 0;
  std::uint8_t cache_probe_survivors = 0;
  std::uint8_t f_to_m = 0;
  std::uint8_t m_to_f = 0;
  std::uint8_t query_count = 0;
  std::uint64_t survival_retries = 0;
};

struct PathRecord {
  std::uint32_t block = 0;
  std::uint8_t cell = 0;
  std::uint8_t arm = 0;
  std::uint8_t law = 0;
  std::uint8_t start = 0;
  std::uint32_t late_m_sum = 0;
  std::uint32_t late_mismatch_sum = 0;
  std::uint32_t total_queries = 0;
  std::uint32_t f_to_m = 0;
  std::uint32_t m_to_f = 0;
  std::uint32_t cache_probe_use = 0;
  std::uint32_t cache_probe_survivors = 0;
  std::uint64_t survival_retries = 0;
  std::uint16_t fixation_updates = 0;
  std::uint16_t extinction_updates = 0;
  std::uint16_t late_fixation_updates = 0;
  std::uint16_t late_extinction_updates = 0;
  Sha256::Digest final_state_sha256{};
  std::uint32_t final_m_count = 0;
  std::uint32_t final_total_mismatch = 0;
  std::uint32_t reserved0 = 0;
  Sha256::Digest paired_trajectory_sha256{};
};

struct BlockRecord {
  std::uint32_t block = 0;
  std::uint8_t n1_ok = 0;
  std::uint8_t n2_ok = 0;
  std::uint8_t query_ok = 0;
  std::uint8_t audit_block = 0;
  std::int32_t c_abs_num = 0;
  std::int32_t c_rec_num = 0;
  std::int32_t d_half_num = 0;
  std::int32_t d_zero_num = 0;
  std::int32_t p_abs_num = 0;
  std::int32_t p_rec_num = 0;
  std::uint32_t late_m_sum[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  std::uint32_t late_mismatch_sum[8] = {0, 0, 0, 0, 0, 0, 0, 0};
};

struct AuditRow {
  std::uint32_t block = 0;
  std::uint16_t update = 0;
  std::uint8_t cell = 0;
  std::uint8_t candidate = 0;
  std::uint8_t family = 0;
  std::uint8_t parent = 0;
  std::uint8_t parent_label_pre = 0;
  std::uint8_t parent_cache_valid = 0;
  std::uint32_t parent_cache = 0;
  std::uint32_t genotype = 0;
  std::uint32_t target = 0;
  std::uint8_t mismatch = 0;
  std::uint8_t recurrence_bit = 0;
  std::uint8_t recurrence_applied = 0;
  std::uint8_t probe_source = 0;
  std::uint64_t weight = 0;
  std::uint8_t selected_rank = 0;
  std::uint8_t post_label = 0;
  std::uint8_t policy_flip = 0;
  std::uint8_t donor_family = 0;
  std::uint64_t W = 0;
  std::uint64_t x = 0;
  std::uint64_t Z = 0;
  std::uint32_t retry = 0;
  std::uint32_t reserved = 0;
};

inline void encode_update(const UpdateRecord& r, std::uint8_t* o) {
  std::memset(o, 0, kUpdateBytes);
  put_le(o + 0, r.block, 4);
  put_le(o + 4, r.update, 2);
  put_le(o + 6, r.cell, 1);
  put_le(o + 7, r.m_count, 1);
  put_le(o + 8, r.total_mismatch, 2);
  put_le(o + 10, r.valid_m_cache, 1);
  put_le(o + 11, r.cache_probe_use, 1);
  put_le(o + 12, r.cache_probe_survivors, 1);
  put_le(o + 13, r.f_to_m, 1);
  put_le(o + 14, r.m_to_f, 1);
  put_le(o + 15, r.query_count, 1);
  put_le(o + 16, r.survival_retries, 8);
}

inline void encode_path(const PathRecord& r, std::uint8_t* o) {
  std::memset(o, 0, kPathBytes);
  put_le(o + 0, r.block, 4);
  put_le(o + 4, r.cell, 1);
  put_le(o + 5, r.arm, 1);
  put_le(o + 6, r.law, 1);
  put_le(o + 7, r.start, 1);
  put_le(o + 8, r.late_m_sum, 4);
  put_le(o + 12, r.late_mismatch_sum, 4);
  put_le(o + 16, r.total_queries, 4);
  put_le(o + 20, r.f_to_m, 4);
  put_le(o + 24, r.m_to_f, 4);
  put_le(o + 28, r.cache_probe_use, 4);
  put_le(o + 32, r.cache_probe_survivors, 4);
  put_le(o + 36, r.survival_retries, 8);
  put_le(o + 44, r.fixation_updates, 2);
  put_le(o + 46, r.extinction_updates, 2);
  put_le(o + 48, r.late_fixation_updates, 2);
  put_le(o + 50, r.late_extinction_updates, 2);
  std::memcpy(o + 52, r.final_state_sha256.data(), 32);
  put_le(o + 84, r.final_m_count, 4);
  put_le(o + 88, r.final_total_mismatch, 4);
  put_le(o + 92, r.reserved0, 4);
  std::memcpy(o + 96, r.paired_trajectory_sha256.data(), 32);
}

inline void encode_block(const BlockRecord& r, std::uint8_t* o) {
  std::memset(o, 0, kBlockBytes);
  put_le(o + 0, r.block, 4);
  put_le(o + 4, r.n1_ok, 1);
  put_le(o + 5, r.n2_ok, 1);
  put_le(o + 6, r.query_ok, 1);
  put_le(o + 7, r.audit_block, 1);
  put_le(o + 8, static_cast<std::uint32_t>(r.c_abs_num), 4);
  put_le(o + 12, static_cast<std::uint32_t>(r.c_rec_num), 4);
  put_le(o + 16, static_cast<std::uint32_t>(r.d_half_num), 4);
  put_le(o + 20, static_cast<std::uint32_t>(r.d_zero_num), 4);
  put_le(o + 24, static_cast<std::uint32_t>(r.p_abs_num), 4);
  put_le(o + 28, static_cast<std::uint32_t>(r.p_rec_num), 4);
  for (unsigned c = 0; c < 8; ++c) {
    put_le(o + 32 + 4 * c, r.late_m_sum[c], 4);
    put_le(o + 64 + 4 * c, r.late_mismatch_sum[c], 4);
  }
}

inline void encode_audit(const AuditRow& r, std::uint8_t* o) {
  std::memset(o, 0, kAuditBytes);
  put_le(o + 0, r.block, 4);
  put_le(o + 4, r.update, 2);
  put_le(o + 6, r.cell, 1);
  put_le(o + 7, r.candidate, 1);
  put_le(o + 8, r.family, 1);
  put_le(o + 9, r.parent, 1);
  put_le(o + 10, r.parent_label_pre, 1);
  put_le(o + 11, r.parent_cache_valid, 1);
  put_le(o + 12, r.parent_cache, 4);
  put_le(o + 16, r.genotype, 4);
  put_le(o + 20, r.target, 4);
  put_le(o + 24, r.mismatch, 1);
  put_le(o + 25, r.recurrence_bit, 1);
  put_le(o + 26, r.recurrence_applied, 1);
  put_le(o + 27, r.probe_source, 1);
  put_le(o + 28, r.weight, 8);
  put_le(o + 36, r.selected_rank, 1);
  put_le(o + 37, r.post_label, 1);
  put_le(o + 38, r.policy_flip, 1);
  put_le(o + 39, r.donor_family, 1);
  put_le(o + 40, r.W, 8);
  put_le(o + 48, r.x, 8);
  put_le(o + 56, r.Z, 8);
  put_le(o + 64, r.retry, 4);
  put_le(o + 68, r.reserved, 4);
}

// Byte-exact comparison; on mismatch names the first differing documented field. The diagnostic
// carries the record kind and field name only, never the expected or observed value, so a failure
// receipt cannot reveal per-block sums, numerators, genotypes or hashes.
template <std::size_t N>
bool compare_fields(const char* kind, const FieldSpec (&fields)[N], const std::uint8_t* expected,
                    const std::uint8_t* observed, std::size_t size, std::string& diagnostic) {
  if (std::memcmp(expected, observed, size) == 0) return true;
  for (std::size_t i = 0; i < N; ++i) {
    const FieldSpec& f = fields[i];
    if (std::memcmp(expected + f.offset, observed + f.offset, f.width) == 0) continue;
    diagnostic = std::string(kind) + "." + f.name + " mismatch";
    return false;
  }
  diagnostic = std::string(kind) + ": bytes differ outside the documented fields";
  return false;
}

}  // namespace mmaudit

#endif  // MMEM_AUDIT_RECORDS_HPP
