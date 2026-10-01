// Independent block replay for PHASE2-MUTABLE-MEMORY-001 r1, written from specification
// sections 2-8 and the record definitions in OUTPUT_FORMATS.md. Shares no code with the producer.
#ifndef MMEM_AUDIT_REPLAY_HPP
#define MMEM_AUDIT_REPLAY_HPP

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "philox.hpp"
#include "records.hpp"
#include "sha256.hpp"

namespace mmaudit {

constexpr std::uint32_t kSlots = 32;
constexpr std::uint32_t kCandidates = 128;
constexpr std::uint32_t kUpdatesPerPath = 256;
constexpr std::uint32_t kLateFirst = 193;
constexpr std::uint32_t kCells = 8;
constexpr std::uint8_t kNotApplicable = 255;

inline std::uint32_t popcount32(std::uint32_t v) {
  v = v - ((v >> 1) & 0x55555555u);
  v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
  v = (v + (v >> 4)) & 0x0F0F0F0Fu;
  return (v * 0x01010101u) >> 24;
}

// Full 64x64 -> 128-bit unsigned product from 32-bit limbs (no compiler extensions).
inline void mul_64x64(std::uint64_t a, std::uint64_t b, std::uint64_t& hi, std::uint64_t& lo) {
  const std::uint64_t a0 = a & 0xFFFFFFFFull, a1 = a >> 32;
  const std::uint64_t b0 = b & 0xFFFFFFFFull, b1 = b >> 32;
  const std::uint64_t p00 = a0 * b0;
  const std::uint64_t p01 = a0 * b1;
  const std::uint64_t p10 = a1 * b0;
  const std::uint64_t p11 = a1 * b1;
  const std::uint64_t middle = (p00 >> 32) + (p01 & 0xFFFFFFFFull) + (p10 & 0xFFFFFFFFull);
  lo = (p00 & 0xFFFFFFFFull) | (middle << 32);
  hi = p11 + (p01 >> 32) + (p10 >> 32) + (middle >> 32);
}

// threshold = (2^64 - W) mod W, computed in unsigned 64-bit arithmetic.
inline std::uint64_t lemire_threshold(std::uint64_t range) { return (std::uint64_t{0} - range) % range; }

// Specification section 6 step 2: reject when low64(x*W) < threshold, else Z = high64(x*W).
inline bool lemire_accept(std::uint64_t x, std::uint64_t range, std::uint64_t& z) {
  std::uint64_t hi = 0, lo = 0;
  mul_64x64(x, range, hi, lo);
  if (lo < lemire_threshold(range)) return false;
  z = hi;
  return true;
}

inline std::uint64_t join64(const Word4& w) {
  return static_cast<std::uint64_t>(w[0]) | (static_cast<std::uint64_t>(w[1]) << 32);
}

// All per-update draws of one block. Every declared coordinate is generated, whatever the
// cell, arm, law or label; survival retries beyond index 0 are generated on demand.
struct UpdateDraws {
  std::uint32_t innovation = 0;
  std::uint32_t copy_bit = 0;
  std::uint32_t fresh_mask[kSlots] = {};
  std::uint32_t scout_mask[kSlots] = {};
  std::uint32_t local_mask[kSlots] = {};
  std::uint64_t donor_key[3 * kSlots] = {};
  std::uint8_t policy_flip[kSlots] = {};
  std::uint64_t survival_x0[kSlots] = {};
};

struct BlockDraws {
  BlockDraws(const KeyedStream& ks, std::uint32_t block_id)
      : stream(ks), block(block_id), initial_genotype(), per_update(kUpdatesPerPath + 1), target(),
        recurrence_applied() {
    for (std::uint32_t s = 0; s < kSlots; ++s) {
      initial_genotype[s] = stream.draw(Purpose::InitialGenotype, block, 0, s, 0)[0];
    }
    for (std::uint32_t t = 1; t <= kUpdatesPerPath; ++t) {
      UpdateDraws& d = per_update[t];
      d.innovation = stream.draw(Purpose::TargetInnovation, block, t, 0, 0)[0];
      d.copy_bit = stream.draw(Purpose::TargetCopy, block, t, 0, 0)[0] & 1u;
      for (std::uint32_t s = 0; s < kSlots; ++s) {
        d.fresh_mask[s] = stream.draw(Purpose::FreshMask, block, t, s, 0)[0];
        d.scout_mask[s] = stream.draw(Purpose::ScoutMask, block, t, s, 0)[0];
        std::uint32_t mask = 0;
        for (std::uint32_t bit = 0; bit < 32; ++bit) {
          if ((stream.draw(Purpose::LocalBit, block, t, s, bit)[0] & 31u) == 0u) mask |= (1u << bit);
        }
        d.local_mask[s] = mask;
        d.policy_flip[s] = (stream.draw(Purpose::PolicyMutation, block, t, s, 0)[0] & 31u) == 0u ? 1 : 0;
        d.survival_x0[s] = join64(stream.draw(Purpose::SurvivalUniform, block, t, s, 0));
      }
      for (std::uint32_t slot = 0; slot < kSlots; ++slot) {
        for (std::uint32_t family = 0; family < 3; ++family) {
          const std::uint32_t entity = 3 * slot + family;
          d.donor_key[entity] = join64(stream.draw(Purpose::DonorKey, block, t, entity, 0));
        }
      }
    }
    // Target laws (specification section 4). Index 0 = ZERO, 1 = HALF.
    for (std::uint32_t t = 1; t <= kUpdatesPerPath; ++t) {
      const UpdateDraws& d = per_update[t];
      target[0][t] = d.innovation;
      recurrence_applied[0][t] = 0;
      if (t >= 3 && d.copy_bit == 1u) {
        target[1][t] = target[1][t - 2];
        recurrence_applied[1][t] = 1;
      } else {
        target[1][t] = d.innovation;
        recurrence_applied[1][t] = 0;
      }
    }
  }

  std::uint64_t survival_x(std::uint32_t update, std::uint32_t draw_index, std::uint64_t retry) const {
    if (retry == 0) return per_update[update].survival_x0[draw_index];
    return join64(stream.draw(Purpose::SurvivalUniform, block, update, draw_index, retry));
  }

  const KeyedStream& stream;
  std::uint32_t block;
  std::uint32_t initial_genotype[kSlots];
  std::vector<UpdateDraws> per_update;  // index 1..256; index 0 unused
  std::uint32_t target[2][kUpdatesPerPath + 1];
  std::uint8_t recurrence_applied[2][kUpdatesPerPath + 1];
};

struct CellState {
  std::uint32_t genotype[kSlots];
  std::uint8_t label[kSlots];  // 1 = M, 0 = F
  std::uint8_t cache_valid[kSlots];
  std::uint32_t cache[kSlots];  // 0 when invalid
};

struct CellTotals {
  std::uint32_t late_m_sum = 0;
  std::uint32_t late_mismatch_sum = 0;
  std::uint32_t queries = 0;
  std::uint32_t f_to_m = 0;
  std::uint32_t m_to_f = 0;
  std::uint32_t cache_probe_use = 0;
  std::uint32_t cache_probe_survivors = 0;
  std::uint64_t survival_retries = 0;
  std::uint32_t fixation = 0;
  std::uint32_t extinction = 0;
  std::uint32_t late_fixation = 0;
  std::uint32_t late_extinction = 0;
  std::uint32_t final_m_count = 0;
  std::uint32_t final_total_mismatch = 0;
};

struct Trajectory {
  std::uint32_t genotype[kUpdatesPerPath + 1][kSlots];
  std::uint8_t survivor[kUpdatesPerPath + 1][kSlots];
  std::uint16_t total_mismatch[kUpdatesPerPath + 1];
  std::uint32_t label_mask[kUpdatesPerPath + 1];  // raw: bit i = 1 iff slot i is M
};

struct StepOutput {
  AuditRow rows[kCandidates];
  UpdateRecord update;
};

class BlockReplay {
 public:
  BlockReplay(const KeyedStream& ks, std::uint32_t block) : draws_(ks, block), block_(block), traj_(kCells) {
    for (std::uint32_t c = 0; c < kCells; ++c) {
      late_m_[c] = 0;
      late_mismatch_[c] = 0;
    }
  }

  void begin_cell(std::uint32_t cell) {
    if (cell >= kCells || cell != next_cell_) throw std::logic_error("replay cell sequencing violated");
    cell_ = cell;
    arm_ = cell >> 2;
    law_ = (cell >> 1) & 1u;
    start_ = cell & 1u;
    for (std::uint32_t s = 0; s < kSlots; ++s) {
      state_.genotype[s] = draws_.initial_genotype[s];
      state_.label[s] = static_cast<std::uint8_t>(start_);
      state_.cache_valid[s] = 0;
      state_.cache[s] = 0;
    }
    totals_ = CellTotals();
    paired_.reset();
    hash_text(paired_, "MMEM-PAIRED-TRAJECTORY-V1");
    hash_u32(paired_, block_);
    hash_u8(paired_, arm_);
    hash_u8(paired_, law_);
    next_update_ = 1;
  }

  void step(std::uint32_t t, StepOutput& out) {
    if (t != next_update_ || t < 1 || t > kUpdatesPerPath) throw std::logic_error("replay update sequencing violated");
    const UpdateDraws& d = draws_.per_update[t];
    const std::uint32_t target = draws_.target[law_][t];
    const std::uint8_t recurrence_bit = static_cast<std::uint8_t>(d.copy_bit);
    const std::uint8_t applied = draws_.recurrence_applied[law_][t];
    const CellState pre = state_;

    std::uint32_t genotype[kCandidates];
    std::uint32_t mismatch[kCandidates];
    std::uint64_t weight[kCandidates];
    std::uint8_t probe_from_cache[kSlots];
    std::uint8_t donor_family[kSlots];
    std::uint32_t valid_m_cache = 0;
    std::uint32_t cache_probe_use = 0;
    std::uint32_t queries = 0;

    // Candidate construction (specification section 5); index = 32*family + slot.
    for (std::uint32_t i = 0; i < kSlots; ++i) {
      const bool valid_m = pre.label[i] == 1 && pre.cache_valid[i] == 1;
      const bool use_cache = arm_ == 0 && valid_m;
      if (valid_m) ++valid_m_cache;
      if (use_cache) ++cache_probe_use;
      probe_from_cache[i] = use_cache ? 1 : 0;
      genotype[i] = pre.genotype[i];
      genotype[32 + i] = use_cache ? pre.cache[i] : (pre.genotype[i] ^ d.fresh_mask[i]);
      genotype[64 + i] = pre.genotype[i] ^ d.scout_mask[i];
      for (std::uint32_t f = 0; f < 3; ++f) {
        mismatch[32 * f + i] = popcount32(genotype[32 * f + i] ^ target);
        ++queries;
      }
      std::uint32_t best = 0;
      for (std::uint32_t f = 1; f < 3; ++f) {
        const std::uint32_t mf = mismatch[32 * f + i];
        const std::uint32_t mb = mismatch[32 * best + i];
        const std::uint64_t kf = d.donor_key[3 * i + f];
        const std::uint64_t kb = d.donor_key[3 * i + best];
        if (mf < mb || (mf == mb && kf < kb)) best = f;
      }
      donor_family[i] = static_cast<std::uint8_t>(best);
      genotype[96 + i] = genotype[32 * best + i] ^ d.local_mask[i];
      mismatch[96 + i] = popcount32(genotype[96 + i] ^ target);
      ++queries;
    }
    for (std::uint32_t j = 0; j < kCandidates; ++j) weight[j] = std::uint64_t{1} << (32u - mismatch[j]);

    // Sequential integer-weighted survival without replacement (specification section 6).
    bool taken[kCandidates] = {};
    std::uint8_t rank[kCandidates];
    std::uint64_t draw_w[kCandidates] = {};
    std::uint64_t draw_x[kCandidates] = {};
    std::uint64_t draw_z[kCandidates] = {};
    std::uint32_t draw_retry[kCandidates] = {};
    std::memset(rank, kNotApplicable, sizeof rank);
    std::uint32_t survivor[kSlots];
    std::uint64_t retry_sum = 0;
    for (std::uint32_t dr = 0; dr < kSlots; ++dr) {
      std::uint64_t total = 0;
      for (std::uint32_t j = 0; j < kCandidates; ++j) {
        if (!taken[j]) total += weight[j];
      }
      std::uint64_t retry = 0;
      std::uint64_t x = 0;
      std::uint64_t z = 0;
      for (;;) {
        if (retry > kMaxSubindex) {
          throw CoordinateError("survival retry exceeds unsigned 32-bit subindex at block " + std::to_string(block_) +
                                " update " + std::to_string(t) + " draw " + std::to_string(dr));
        }
        x = draws_.survival_x(t, dr, retry);
        if (lemire_accept(x, total, z)) break;
        ++retry;
      }
      std::uint64_t cumulative = 0;
      std::uint32_t pick = kCandidates;
      for (std::uint32_t j = 0; j < kCandidates; ++j) {
        if (taken[j]) continue;
        cumulative += weight[j];
        if (cumulative > z) {
          pick = j;
          break;
        }
      }
      if (pick == kCandidates) throw std::logic_error("survivor selection fell through");
      taken[pick] = true;
      rank[pick] = static_cast<std::uint8_t>(dr);
      draw_w[pick] = total;
      draw_x[pick] = x;
      draw_z[pick] = z;
      draw_retry[pick] = static_cast<std::uint32_t>(retry);
      survivor[dr] = pick;
      retry_sum += retry;
    }

    // Inheritance, symmetric policy mutation and cache transition.
    CellState post{};
    std::uint8_t post_label_of[kCandidates];
    std::uint8_t flip_of[kCandidates];
    std::memset(post_label_of, kNotApplicable, sizeof post_label_of);
    std::memset(flip_of, kNotApplicable, sizeof flip_of);
    std::uint32_t m_count = 0, total_mismatch = 0, f_to_m = 0, m_to_f = 0, cache_probe_survivors = 0;
    std::uint32_t label_mask = 0;
    for (std::uint32_t s = 0; s < kSlots; ++s) {
      const std::uint32_t j = survivor[s];
      const std::uint32_t parent = j & 31u;
      const std::uint8_t inherited = pre.label[parent];
      const std::uint8_t flip = d.policy_flip[s];
      const std::uint8_t label = static_cast<std::uint8_t>(inherited ^ flip);
      post.genotype[s] = genotype[j];
      post.label[s] = label;
      post.cache_valid[s] = label;
      post.cache[s] = label == 1 ? pre.genotype[parent] : 0u;
      if (label == 1) {
        ++m_count;
        label_mask |= (1u << s);
      }
      total_mismatch += mismatch[j];
      if (inherited == 0 && label == 1) ++f_to_m;
      if (inherited == 1 && label == 0) ++m_to_f;
      if ((j >> 5) == 1u && probe_from_cache[parent] == 1) ++cache_probe_survivors;
      post_label_of[j] = label;
      flip_of[j] = flip;
    }
    state_ = post;

    for (std::uint32_t j = 0; j < kCandidates; ++j) {
      const std::uint32_t family = j >> 5;
      const std::uint32_t parent = j & 31u;
      AuditRow& r = out.rows[j];
      r.block = block_;
      r.update = static_cast<std::uint16_t>(t);
      r.cell = static_cast<std::uint8_t>(cell_);
      r.candidate = static_cast<std::uint8_t>(j);
      r.family = static_cast<std::uint8_t>(family);
      r.parent = static_cast<std::uint8_t>(parent);
      r.parent_label_pre = pre.label[parent];
      r.parent_cache_valid = pre.cache_valid[parent];
      r.parent_cache = pre.cache_valid[parent] == 1 ? pre.cache[parent] : 0u;
      r.genotype = genotype[j];
      r.target = target;
      r.mismatch = static_cast<std::uint8_t>(mismatch[j]);
      r.recurrence_bit = recurrence_bit;
      r.recurrence_applied = applied;
      r.probe_source = family == 1 ? probe_from_cache[parent] : kNotApplicable;
      r.weight = weight[j];
      r.selected_rank = rank[j];
      r.post_label = post_label_of[j];
      r.policy_flip = flip_of[j];
      r.donor_family = family == 3 ? donor_family[parent] : kNotApplicable;
      r.W = draw_w[j];
      r.x = draw_x[j];
      r.Z = draw_z[j];
      r.retry = draw_retry[j];
      r.reserved = 0;
    }

    UpdateRecord& u = out.update;
    u.block = block_;
    u.update = static_cast<std::uint16_t>(t);
    u.cell = static_cast<std::uint8_t>(cell_);
    u.m_count = static_cast<std::uint8_t>(m_count);
    u.total_mismatch = static_cast<std::uint16_t>(total_mismatch);
    u.valid_m_cache = static_cast<std::uint8_t>(valid_m_cache);
    u.cache_probe_use = static_cast<std::uint8_t>(cache_probe_use);
    u.cache_probe_survivors = static_cast<std::uint8_t>(cache_probe_survivors);
    u.f_to_m = static_cast<std::uint8_t>(f_to_m);
    u.m_to_f = static_cast<std::uint8_t>(m_to_f);
    u.query_count = static_cast<std::uint8_t>(queries);
    u.survival_retries = retry_sum;

    if (t >= kLateFirst) {
      totals_.late_m_sum += m_count;
      totals_.late_mismatch_sum += total_mismatch;
    }
    totals_.queries += queries;
    totals_.f_to_m += f_to_m;
    totals_.m_to_f += m_to_f;
    totals_.cache_probe_use += cache_probe_use;
    totals_.cache_probe_survivors += cache_probe_survivors;
    totals_.survival_retries += retry_sum;
    if (m_count == kSlots) {
      ++totals_.fixation;
      if (t >= kLateFirst) ++totals_.late_fixation;
    }
    if (m_count == 0) {
      ++totals_.extinction;
      if (t >= kLateFirst) ++totals_.late_extinction;
    }
    totals_.final_m_count = m_count;
    totals_.final_total_mismatch = total_mismatch;

    // Paired-trajectory hash preimage for this completed update.
    hash_u16(paired_, t);
    hash_u32(paired_, target);
    hash_u8(paired_, recurrence_bit);
    hash_u8(paired_, applied);
    hash_u16(paired_, total_mismatch);
    for (std::uint32_t s = 0; s < kSlots; ++s) hash_u8(paired_, survivor[s]);
    for (std::uint32_t s = 0; s < kSlots; ++s) hash_u32(paired_, post.genotype[s]);
    hash_u32(paired_, start_ == 1 ? (label_mask ^ 0xFFFFFFFFu) : label_mask);

    Trajectory& tr = traj_[cell_];
    for (std::uint32_t s = 0; s < kSlots; ++s) {
      tr.genotype[t][s] = post.genotype[s];
      tr.survivor[t][s] = static_cast<std::uint8_t>(survivor[s]);
    }
    tr.total_mismatch[t] = static_cast<std::uint16_t>(total_mismatch);
    tr.label_mask[t] = label_mask;

    ++next_update_;
  }

  PathRecord end_cell() {
    if (next_update_ != kUpdatesPerPath + 1) throw std::logic_error("replay path ended early");
    Sha256 final_hash;
    hash_text(final_hash, "MMEM-FINAL-STATE-V1");
    hash_u32(final_hash, block_);
    hash_u8(final_hash, cell_);
    hash_u32(final_hash, kUpdatesPerPath);
    for (std::uint32_t s = 0; s < kSlots; ++s) {
      hash_u32(final_hash, state_.genotype[s]);
      hash_u8(final_hash, state_.label[s]);
      hash_u8(final_hash, state_.cache_valid[s]);
      hash_u32(final_hash, state_.cache[s]);
    }
    hash_u32(final_hash, draws_.target[law_][256]);
    hash_u32(final_hash, draws_.target[law_][255]);

    PathRecord p;
    p.block = block_;
    p.cell = static_cast<std::uint8_t>(cell_);
    p.arm = static_cast<std::uint8_t>(arm_);
    p.law = static_cast<std::uint8_t>(law_);
    p.start = static_cast<std::uint8_t>(start_);
    p.late_m_sum = totals_.late_m_sum;
    p.late_mismatch_sum = totals_.late_mismatch_sum;
    p.total_queries = totals_.queries;
    p.f_to_m = totals_.f_to_m;
    p.m_to_f = totals_.m_to_f;
    p.cache_probe_use = totals_.cache_probe_use;
    p.cache_probe_survivors = totals_.cache_probe_survivors;
    p.survival_retries = totals_.survival_retries;
    p.fixation_updates = static_cast<std::uint16_t>(totals_.fixation);
    p.extinction_updates = static_cast<std::uint16_t>(totals_.extinction);
    p.late_fixation_updates = static_cast<std::uint16_t>(totals_.late_fixation);
    p.late_extinction_updates = static_cast<std::uint16_t>(totals_.late_extinction);
    p.final_state_sha256 = final_hash.finish();
    p.final_m_count = totals_.final_m_count;
    p.final_total_mismatch = totals_.final_total_mismatch;
    p.reserved0 = 0;
    p.paired_trajectory_sha256 = paired_.finish();

    paired_digest_[cell_] = p.paired_trajectory_sha256;
    late_m_[cell_] = totals_.late_m_sum;
    late_mismatch_[cell_] = totals_.late_mismatch_sum;
    all_queries_ok_ = all_queries_ok_ && totals_.queries == kUpdatesPerPath * kCandidates;
    ++next_cell_;
    return p;
  }

  // n1/n2: the auditor's own SHAM identities; paired_ok: equality of its own SHAM paired hashes.
  BlockRecord end_block(bool& n1_ok, bool& n2_ok, bool& paired_ok) {
    if (next_cell_ != kCells) throw std::logic_error("replay block ended early");
    n1_ok = true;
    n2_ok = true;
    paired_ok = true;
    for (std::uint32_t law = 0; law < 2; ++law) {
      const Trajectory& a = traj_[4 + 2 * law];
      const Trajectory& b = traj_[5 + 2 * law];
      for (std::uint32_t t = 1; t <= kUpdatesPerPath; ++t) {
        if (std::memcmp(a.genotype[t], b.genotype[t], sizeof a.genotype[t]) != 0 ||
            std::memcmp(a.survivor[t], b.survivor[t], sizeof a.survivor[t]) != 0 ||
            a.total_mismatch[t] != b.total_mismatch[t]) {
          n1_ok = false;
        }
        if ((a.label_mask[t] ^ b.label_mask[t]) != 0xFFFFFFFFu) n2_ok = false;
      }
      if (paired_digest_[4 + 2 * law] != paired_digest_[5 + 2 * law]) paired_ok = false;
    }
    const std::int64_t l0 = late_m_[0], l1 = late_m_[1], l2 = late_m_[2], l3 = late_m_[3];
    const std::int64_t p0 = late_mismatch_[0], p1 = late_mismatch_[1], p2 = late_mismatch_[2],
                       p3 = late_mismatch_[3], p4 = late_mismatch_[4], p5 = late_mismatch_[5],
                       p6 = late_mismatch_[6], p7 = late_mismatch_[7];
    BlockRecord r;
    r.block = block_;
    r.n1_ok = n1_ok ? 1 : 0;
    r.n2_ok = n2_ok ? 1 : 0;
    r.query_ok = all_queries_ok_ ? 1 : 0;
    r.audit_block = block_ < 64 ? 1 : 0;
    r.c_abs_num = static_cast<std::int32_t>(l2 + l3 - 2048);
    r.c_rec_num = static_cast<std::int32_t>((l2 + l3) - (l0 + l1));
    r.d_half_num = static_cast<std::int32_t>(l3 - l2);
    r.d_zero_num = static_cast<std::int32_t>(l1 - l0);
    const std::int64_t p_abs = (p6 + p7) - (p2 + p3);
    r.p_abs_num = static_cast<std::int32_t>(p_abs);
    r.p_rec_num = static_cast<std::int32_t>(p_abs - ((p4 + p5) - (p0 + p1)));
    for (std::uint32_t c = 0; c < kCells; ++c) {
      r.late_m_sum[c] = late_m_[c];
      r.late_mismatch_sum[c] = late_mismatch_[c];
    }
    return r;
  }

 private:
  BlockDraws draws_;
  std::uint32_t block_;
  std::vector<Trajectory> traj_;
  std::uint32_t cell_ = 0;
  std::uint32_t arm_ = 0;
  std::uint32_t law_ = 0;
  std::uint32_t start_ = 0;
  std::uint32_t next_cell_ = 0;
  std::uint32_t next_update_ = 1;
  bool all_queries_ok_ = true;
  CellState state_{};
  CellTotals totals_{};
  Sha256 paired_;
  Sha256::Digest paired_digest_[kCells] = {};
  std::uint32_t late_m_[kCells];
  std::uint32_t late_mismatch_[kCells];
};

}  // namespace mmaudit

#endif  // MMEM_AUDIT_REPLAY_HPP
