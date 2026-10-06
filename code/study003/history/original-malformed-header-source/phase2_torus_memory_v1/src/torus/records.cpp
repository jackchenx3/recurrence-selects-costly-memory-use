#include "records.hpp"

namespace torus {

namespace {

void check_size(const ByteBuf& b, std::size_t start, u32 expected, const char* what) {
  TORUS_REQUIRE(b.size() - start == expected, std::string("serialized size mismatch: ") + what);
}

void put_phen(ByteBuf& b, const u16 x[kLoci]) {
  for (int j = 0; j < kLoci; ++j) b.u16v(x[j]);
}

}  // namespace

void serialize_header(const FileHeader& h, ByteBuf& b) {
  const std::size_t s = b.size();
  b.fixed_ascii(h.magic, 8);
  b.u32v(kSchemaVersion);
  b.u32v(kEndianMarker);
  b.u32v(h.row_size);
  b.u32v(kHeaderBytes);
  b.u64v(h.row_count);
  b.fixed_ascii(h.ns, 16);
  b.u32v(h.chunk_index);
  b.u32v(h.first_block);
  b.u32v(h.block_count);
  b.u32v(0);
  check_size(b, s, kHeaderBytes, "header");
}

void serialize_update(const UpdateRecord& r, ByteBuf& b) {
  const std::size_t s = b.size();
  TORUS_REQUIRE(r.pop_loss <= kMaxPopulationLoss, "pop loss domain");
  TORUS_REQUIRE(r.update >= 1 && r.update <= kUpdates, "update domain");
  TORUS_REQUIRE(r.m_count <= kPop && r.valid_cache_pre <= kPop && r.cache_probe_uses <= kPop &&
                    r.cache_probe_winner_slots <= kPop && r.f_to_m <= kPop && r.m_to_f <= kPop &&
                    r.dup_entry_tournaments <= kPop && r.distinct_winners >= 1 && r.distinct_winners <= kPop,
                "update record domain");
  TORUS_REQUIRE(r.local_flagged <= kPop * kLoci && r.local_changed <= r.local_flagged, "local count domain");
  b.u64v(r.pop_loss);
  b.u16v(r.update);
  b.u16v(r.query_count);
  b.u16v(r.local_flagged);
  b.u16v(r.local_changed);
  b.u8v(r.m_count);
  b.u8v(r.valid_cache_pre);
  b.u8v(r.cache_probe_uses);
  b.u8v(r.cache_probe_winner_slots);
  b.u8v(r.f_to_m);
  b.u8v(r.m_to_f);
  b.u8v(r.dup_entry_tournaments);
  b.u8v(r.distinct_winners);
  check_size(b, s, kUpdateRowBytes, "update");
}

void serialize_path(const PathRecord& r, ByteBuf& b) {
  const std::size_t s = b.size();
  b.u32v(r.block);
  b.u8v(r.cell);
  b.u8v(r.arm);
  b.u8v(r.law);
  b.u8v(r.start);
  b.u32v(r.late_m_sum);
  b.u32v(r.total_queries);
  b.u64v(r.late_pop_loss_sum);
  b.u64v(r.all_pop_loss_sum);
  b.u32v(r.all_m_sum);
  b.u32v(r.total_cache_probe_uses);
  b.u32v(r.total_cache_probe_winner_slots);
  b.u32v(r.total_f_to_m);
  b.u32v(r.total_m_to_f);
  b.u32v(r.total_dup_entry_tournaments);
  b.u32v(r.total_local_flagged);
  b.u32v(r.total_local_changed);
  b.bytes(r.final_state_sha256.data(), 32);
  b.bytes(r.update_records_sha256.data(), 32);
  b.bytes(r.label_blind_sha256.data(), 32);
  b.bytes(r.label_sha256.data(), 32);
  b.bytes(r.complement_label_sha256.data(), 32);
  check_size(b, s, kPathRowBytes, "path");
}

void derive_block_variables(BlockRecord& r) {
  const i64 s0 = r.late_m_sum[0], s1 = r.late_m_sum[1], s2 = r.late_m_sum[2], s3 = r.late_m_sum[3];
  for (int c = 0; c < kCells; ++c) {
    TORUS_REQUIRE(r.late_m_sum[c] <= u32(kLateLen * kPop), "late M sum domain");
    TORUS_REQUIRE(r.late_pop_loss_sum[c] <= u64(kLateLen) * kMaxPopulationLoss, "late loss sum domain");
  }
  // Cells: 0 AZF, 1 AZM, 2 AHF, 3 AHM, 4 SZF, 5 SZM, 6 SHF, 7 SHM.
  r.c_abs_num = i32((s2 + s3) - 2048);
  r.c_rec_num = i32((s2 + s3) - (s0 + s1));
  r.d_half_num = i32(2 * (s3 - s2));
  r.d_zero_num = i32(2 * (s1 - s0));
  const i64 L[8] = {i64(r.late_pop_loss_sum[0]), i64(r.late_pop_loss_sum[1]), i64(r.late_pop_loss_sum[2]),
                    i64(r.late_pop_loss_sum[3]), i64(r.late_pop_loss_sum[4]), i64(r.late_pop_loss_sum[5]),
                    i64(r.late_pop_loss_sum[6]), i64(r.late_pop_loss_sum[7])};
  const i64 half_gap = (L[6] + L[7]) - (L[2] + L[3]);  // SHAM minus ACTIVE loss, HALF
  const i64 zero_gap = (L[4] + L[5]) - (L[0] + L[1]);  // SHAM minus ACTIVE loss, ZERO
  r.p_abs_num = half_gap;
  r.p_rec_num = half_gap - zero_gap;
}

void serialize_block(const BlockRecord& r, ByteBuf& b) {
  const std::size_t s = b.size();
  b.u32v(r.block);
  b.u32v(r.flags);
  for (int c = 0; c < kCells; ++c) b.u32v(r.late_m_sum[c]);
  for (int c = 0; c < kCells; ++c) b.u64v(r.late_pop_loss_sum[c]);
  b.i32v(r.c_abs_num);
  b.i32v(r.c_rec_num);
  b.i32v(r.d_half_num);
  b.i32v(r.d_zero_num);
  b.i64v(r.p_abs_num);
  b.i64v(r.p_rec_num);
  check_size(b, s, kBlockRowBytes, "block");
}

void serialize_candidate(const CandidateAuditRow& r, ByteBuf& b) {
  const std::size_t s = b.size();
  b.u32v(r.block);
  b.u16v(r.update);
  b.u8v(r.cell);
  b.u8v(r.candidate);
  b.u8v(r.family);
  b.u8v(r.parent);
  b.u8v(r.parent_label_pre);
  b.u8v(r.flags);
  b.u8v(r.donor_family);
  b.u8v(r.local_flagged);
  b.u8v(r.local_changed);
  b.u8v(r.win_count);
  b.u64v(r.loss);
  b.u64v(r.donor_key);
  b.u64v(r.tie_key);
  put_phen(b, r.phen);
  check_size(b, s, kCandidateRowBytes, "candidate");
}

void serialize_entry(const EntryAuditRow& r, ByteBuf& b) {
  const std::size_t s = b.size();
  b.u32v(r.block);
  b.u16v(r.update);
  b.u8v(r.cell);
  b.u8v(r.slot);
  b.u8v(r.entry);
  b.u8v(r.candidate);
  b.u8v(r.winner);
  b.u8v(r.inherited_label);
  b.u8v(r.post_label);
  b.u8v(r.flip);
  b.u8v(r.post_cache_valid);
  b.u8v(0);
  check_size(b, s, kEntryRowBytes, "entry");
}

void serialize_context(const ContextAuditRow& r, ByteBuf& b) {
  const std::size_t s = b.size();
  b.u32v(r.block);
  b.u16v(r.update);
  b.u8v(r.cell);
  b.u8v(r.copy_bit);
  b.u8v(r.target_is_copy);
  b.zeros(7);
  put_phen(b, r.target);
  for (int i = 0; i < kPop; ++i) put_phen(b, r.parent_phen[i]);
  b.bytes(r.parent_label, kPop);
  b.bytes(r.parent_cache_valid, kPop);
  for (int i = 0; i < kPop; ++i) put_phen(b, r.parent_cache[i]);
  check_size(b, s, kContextRowBytes, "context");
}

}  // namespace torus
