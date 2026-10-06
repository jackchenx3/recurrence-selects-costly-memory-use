// Deterministic little-endian binary record schemas, version 1.
// Byte layouts are documented in docs/BINARY_SCHEMAS.md and mirrored in
// tools/torus_schema.py. Every file starts with a 64-byte header.
#pragma once

#include <string>

#include "constants.hpp"
#include "sha256.hpp"
#include "util.hpp"

namespace torus {

constexpr u32 kSchemaVersion = 1;
constexpr u32 kEndianMarker = 0x01020304u;  // stored LE: bytes 04 03 02 01
constexpr u32 kHeaderBytes = 64;
constexpr u32 kNoChunk = 0xffffffffu;

constexpr u32 kUpdateRowBytes = 24;
constexpr u32 kPathRowBytes = 224;
constexpr u32 kBlockRowBytes = 136;
constexpr u32 kCandidateRowBytes = 104;
constexpr u32 kEntryRowBytes = 16;
constexpr u32 kContextRowBytes = 4240;
constexpr u32 kEstimateRowBytes = 256;  // written by tools/torus_analyze.py only

constexpr const char* kMagicUpdate = "T3UPDATE";
constexpr const char* kMagicPath = "T3PATH";
constexpr const char* kMagicBlock = "T3BLOCK";
constexpr const char* kMagicCandidate = "T3CANDID";
constexpr const char* kMagicEntry = "T3ENTRY";
constexpr const char* kMagicContext = "T3CONTXT";

struct FileHeader {
  std::string magic;
  u32 row_size;
  u64 row_count;
  std::string ns;
  u32 chunk_index;
  u32 first_block;
  u32 block_count;
};
void serialize_header(const FileHeader& h, ByteBuf& b);

// Per path-update record (24 bytes), one per (block, cell, update).
struct UpdateRecord {
  u64 pop_loss;                  // sum over survivors of exact Q, <= 2^40
  u16 update;                    // 1..256
  u16 query_count;               // objective evaluations, must be 128
  u16 local_flagged;             // coordinates flagged for local replacement, 0..1024
  u16 local_changed;             // flagged coordinates whose value changed, 0..1024
  u8 m_count;                    // post-transition M labels, 0..32
  u8 valid_cache_pre;            // parent slots with a valid cache before the update
  u8 cache_probe_uses;           // policy probes that read a cache (ACTIVE only)
  u8 cache_probe_winner_slots;   // survivor slots won by a cache-reading probe
  u8 f_to_m;                     // label mutations F->M
  u8 m_to_f;                     // label mutations M->F
  u8 dup_entry_tournaments;      // tournaments with a repeated candidate index
  u8 distinct_winners;           // distinct winning candidates, 1..32
};
void serialize_update(const UpdateRecord& r, ByteBuf& b);

// Per-path summary (224 bytes), one per (block, cell).
struct PathRecord {
  u32 block;
  u8 cell, arm, law, start;
  u32 late_m_sum;          // sum of m_count over updates 193..256, 0..2048
  u32 total_queries;       // must be 32768
  u64 late_pop_loss_sum;   // sum of pop_loss over 193..256, <= 2^46
  u64 all_pop_loss_sum;    // sum over 1..256, <= 2^48
  u32 all_m_sum;
  u32 total_cache_probe_uses;
  u32 total_cache_probe_winner_slots;
  u32 total_f_to_m;
  u32 total_m_to_f;
  u32 total_dup_entry_tournaments;
  u32 total_local_flagged;
  u32 total_local_changed;
  Digest final_state_sha256;
  Digest update_records_sha256;
  Digest label_blind_sha256;  // per update: 128 candidate phenotypes, 128 losses, 32 winners, 32 survivor phenotypes
  Digest label_sha256;
  Digest complement_label_sha256;
};
void serialize_path(const PathRecord& r, ByteBuf& b);

// Block flags (all six must be set for a valid block).
constexpr u32 kFlagN1Zero = 1u << 0;
constexpr u32 kFlagN1Half = 1u << 1;
constexpr u32 kFlagN2Zero = 1u << 2;
constexpr u32 kFlagN2Half = 1u << 3;
constexpr u32 kFlagN2Sum = 1u << 4;
constexpr u32 kFlagQueries = 1u << 5;
constexpr u32 kFlagAllValid = 0x3fu;

// Per-block record (136 bytes).
struct BlockRecord {
  u32 block;
  u32 flags;
  u32 late_m_sum[kCells];
  u64 late_pop_loss_sum[kCells];
  i32 c_abs_num;   // / 4096
  i32 c_rec_num;   // / 4096
  i32 d_half_num;  // / 4096
  i32 d_zero_num;  // / 4096
  i64 p_abs_num;   // / 2^47
  i64 p_rec_num;   // / 2^47
};
void serialize_block(const BlockRecord& r, ByteBuf& b);
// Derives the six per-block variables from the 16 late sums.
void derive_block_variables(BlockRecord& r);

// Audit rows (blocks 0..63 only).
struct CandidateAuditRow {
  u32 block;
  u16 update;
  u8 cell, candidate, family, parent, parent_label_pre, flags;
  u8 donor_family;   // family-3 rows: donor family 0..2; else 255
  u8 local_flagged;  // family-3 rows only
  u8 local_changed;  // family-3 rows only
  u8 win_count;
  u64 loss;
  u64 donor_key;     // families 0..2: DONOR_KEY word; family 3: 0
  u64 tie_key;
  u16 phen[kLoci];
};
constexpr u8 kCandFlagParentCacheValidPre = 1u << 0;
constexpr u8 kCandFlagProbeReadCache = 1u << 1;
constexpr u8 kCandFlagSelectedDonor = 1u << 2;
void serialize_candidate(const CandidateAuditRow& r, ByteBuf& b);

struct EntryAuditRow {
  u32 block;
  u16 update;
  u8 cell, slot, entry, candidate, winner, inherited_label, post_label, flip, post_cache_valid;
};
void serialize_entry(const EntryAuditRow& r, ByteBuf& b);

struct ContextAuditRow {
  u32 block;
  u16 update;
  u8 cell;
  u8 copy_bit;        // raw TARGET_COPY bit R_t
  u8 target_is_copy;  // 1 iff this cell's law used T_t = T_(t-2)
  u16 target[kLoci];
  u16 parent_phen[kPop][kLoci];
  u8 parent_label[kPop];
  u8 parent_cache_valid[kPop];
  u16 parent_cache[kPop][kLoci];
};
void serialize_context(const ContextAuditRow& r, ByteBuf& b);

}  // namespace torus
