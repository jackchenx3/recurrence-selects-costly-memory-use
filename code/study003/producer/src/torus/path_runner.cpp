#include "path_runner.hpp"

#include <cstring>
#include <vector>

#include "util.hpp"

namespace torus {

namespace {

void hash_phen(Sha256& h, const u16 x[kLoci]) {
  u8 b[2 * kLoci];
  for (int j = 0; j < kLoci; ++j) {
    b[2 * j] = u8(x[j]);
    b[2 * j + 1] = u8(x[j] >> 8);
  }
  h.update(b, sizeof b);
}

void hash_u64(Sha256& h, u64 v) {
  u8 b[8];
  for (int i = 0; i < 8; ++i) b[i] = u8(v >> (8 * i));
  h.update(b, 8);
}

struct CellRun {
  CellState st;
  CellState pre;  // pre-update snapshot (audit blocks only)
  StepWork work;
  Sha256 h_updates, h_blind, h_label, h_comp;
  PathRecord path;
};

void emit_audit(AuditSink& sink, u32 block, int t, int cell, const CellRun& cr, const BlockTargets& tg,
                const UpdateTape& tape) {
  const Law law = cell_law(cell);
  ContextAuditRow cx;
  cx.block = block;
  cx.update = u16(t);
  cx.cell = u8(cell);
  cx.copy_bit = tg.copy_bit[t];
  cx.target_is_copy = tg.target_is_copy[law][t];
  std::memcpy(cx.target, tg.target[law][t], sizeof cx.target);
  std::memcpy(cx.parent_phen, cr.pre.phen, sizeof cx.parent_phen);
  std::memcpy(cx.parent_label, cr.pre.label, sizeof cx.parent_label);
  std::memcpy(cx.parent_cache_valid, cr.pre.cache_valid, sizeof cx.parent_cache_valid);
  std::memcpy(cx.parent_cache, cr.pre.cache, sizeof cx.parent_cache);
  sink.context(cx);

  const CandidateSet& cs = cr.work.cand;
  const TournamentResult& tr = cr.work.tour;
  for (int c = 0; c < kCandidates; ++c) {
    const int fam = candidate_family(c), p = candidate_parent(c);
    CandidateAuditRow r;
    r.block = block;
    r.update = u16(t);
    r.cell = u8(cell);
    r.candidate = u8(c);
    r.family = u8(fam);
    r.parent = u8(p);
    r.parent_label_pre = cr.pre.label[p];
    r.flags = 0;
    if (cr.pre.cache_valid[p]) r.flags |= kCandFlagParentCacheValidPre;
    if (fam == FAM_PROBE && cs.probe_read_cache[p]) r.flags |= kCandFlagProbeReadCache;
    if (fam < 3 && cs.donor_family[p] == fam) r.flags |= kCandFlagSelectedDonor;
    r.donor_family = fam == FAM_LOCAL ? cs.donor_family[p] : u8(255);
    r.local_flagged = fam == FAM_LOCAL ? cs.flagged[p] : u8(0);
    r.local_changed = fam == FAM_LOCAL ? cs.changed[p] : u8(0);
    r.win_count = tr.win_count[c];
    r.loss = cs.loss[c];
    r.donor_key = fam < 3 ? tape.donor_key[3 * p + fam] : 0;
    r.tie_key = tape.tie_key[c];
    std::memcpy(r.phen, cs.phen[c], sizeof r.phen);
    sink.candidate(r);
  }
  const TransitionResult& tx = cr.work.trans;
  for (int s = 0; s < kPop; ++s) {
    for (int e = 0; e < kEntries; ++e) {
      EntryAuditRow r;
      r.block = block;
      r.update = u16(t);
      r.cell = u8(cell);
      r.slot = u8(s);
      r.entry = u8(e);
      r.candidate = tr.entry[s][e];
      r.winner = tr.winner[s];
      r.inherited_label = tx.inherited[s];
      r.post_label = tx.post_label[s];
      r.flip = tx.flip[s];
      r.post_cache_valid = cr.st.cache_valid[s];
      sink.entry(r);
    }
  }
}

}  // namespace

Digest final_state_digest(const CellState& st) {
  Sha256 h;
  for (int i = 0; i < kPop; ++i) hash_phen(h, st.phen[i]);
  h.update(st.label, kPop);
  h.update(st.cache_valid, kPop);
  for (int i = 0; i < kPop; ++i) hash_phen(h, st.cache[i]);
  return h.finish();
}

void run_block(const KeySet& ks, u32 block, const RunOptions& opt, BlockResult& out) {
  TORUS_REQUIRE(block < kBlocks, "block out of range");
  {
    bool seen[kCells] = {false};
    for (int k = 0; k < kCells; ++k) {
      const int c = opt.cell_order[k];
      TORUS_REQUIRE(c >= 0 && c < kCells && !seen[c], "cell order is not a permutation");
      seen[c] = true;
    }
  }
  TORUS_REQUIRE(opt.only_cell >= -1 && opt.only_cell < kCells, "only_cell out of range");
  TORUS_REQUIRE(opt.label_override_cell < 0 || cell_arm(opt.label_override_cell) == SHAM,
                "label override is a SHAM-only fixture device");

  std::unique_ptr<BlockTargets> tg(new BlockTargets);
  std::unique_ptr<UpdateTape> tape(new UpdateTape);
  std::unique_ptr<CellRun[]> runs(new CellRun[kCells]);
  u16 initial[kPop][kLoci];

  generate_initial_phenotypes(ks, block, initial);
  generate_block_targets(ks, block, *tg);

  for (int c = 0; c < kCells; ++c) {
    out.ran[c] = opt.only_cell < 0 || opt.only_cell == c;
    if (!out.ran[c]) continue;
    init_cell_state(runs[c].st, initial, cell_start(c));
    if (opt.label_override_cell == c) std::memcpy(runs[c].st.label, opt.label_override, kPop);
    std::memset(&runs[c].path, 0, sizeof(PathRecord));
  }

  for (int t = 1; t <= kUpdates; ++t) {
    if (!opt.tape_per_cell) generate_update_tape(ks, block, u32(t), *tape);
    for (int k = 0; k < kCells; ++k) {
      const int c = opt.cell_order[k];
      if (!out.ran[c]) continue;
      if (opt.tape_per_cell) generate_update_tape(ks, block, u32(t), *tape);
      CellRun& cr = runs[c];
      if (opt.sink) std::memcpy(&cr.pre, &cr.st, sizeof(CellState));
      const UpdateRecord r = step_cell(cr.st, cell_arm(c), t, tg->target[cell_law(c)][t], *tape, cr.work);
      out.updates[c][t - 1] = r;

      ByteBuf b;
      serialize_update(r, b);
      cr.h_updates.update(b.data(), b.size());
      for (int x = 0; x < kCandidates; ++x) hash_phen(cr.h_blind, cr.work.cand.phen[x]);
      for (int x = 0; x < kCandidates; ++x) hash_u64(cr.h_blind, cr.work.cand.loss[x]);
      cr.h_blind.update(cr.work.tour.winner, kPop);
      for (int s = 0; s < kPop; ++s) hash_phen(cr.h_blind, cr.st.phen[s]);
      u8 comp[kPop];
      for (int s = 0; s < kPop; ++s) comp[s] = u8(cr.st.label[s] ^ 1u);
      cr.h_label.update(cr.st.label, kPop);
      cr.h_comp.update(comp, kPop);

      PathRecord& p = cr.path;
      if (t >= kLateFirst) {
        p.late_m_sum += r.m_count;
        p.late_pop_loss_sum += r.pop_loss;
      }
      p.all_m_sum += r.m_count;
      p.all_pop_loss_sum += r.pop_loss;
      p.total_queries += r.query_count;
      p.total_cache_probe_uses += r.cache_probe_uses;
      p.total_cache_probe_winner_slots += r.cache_probe_winner_slots;
      p.total_f_to_m += r.f_to_m;
      p.total_m_to_f += r.m_to_f;
      p.total_dup_entry_tournaments += r.dup_entry_tournaments;
      p.total_local_flagged += r.local_flagged;
      p.total_local_changed += r.local_changed;
    }
    if (opt.sink) {
      // Canonical audit order: update, then cell 0..7, independent of evaluation order.
      TORUS_REQUIRE(!opt.tape_per_cell, "audit rows require the shared tape");
      for (int c = 0; c < kCells; ++c)
        if (out.ran[c]) emit_audit(*opt.sink, block, t, c, runs[c], *tg, *tape);
    }
  }

  for (int c = 0; c < kCells; ++c) {
    if (!out.ran[c]) continue;
    CellRun& cr = runs[c];
    PathRecord& p = cr.path;
    p.block = block;
    p.cell = u8(c);
    p.arm = cell_arm(c);
    p.law = cell_law(c);
    p.start = cell_start(c);
    p.final_state_sha256 = final_state_digest(cr.st);
    p.update_records_sha256 = cr.h_updates.finish();
    p.label_blind_sha256 = cr.h_blind.finish();
    p.label_sha256 = cr.h_label.finish();
    p.complement_label_sha256 = cr.h_comp.finish();
    out.paths[c] = p;
  }

  std::memset(&out.block, 0, sizeof(BlockRecord));
  if (opt.only_cell >= 0) return;

  BlockRecord& br = out.block;
  br.block = block;
  for (int c = 0; c < kCells; ++c) {
    br.late_m_sum[c] = out.paths[c].late_m_sum;
    br.late_pop_loss_sum[c] = out.paths[c].late_pop_loss_sum;
  }
  derive_block_variables(br);

  u32 flags = 0;
  // N1: SHAM ALL_F and ALL_M label-blind trajectories are bit-identical.
  if (out.paths[4].label_blind_sha256 == out.paths[5].label_blind_sha256) flags |= kFlagN1Zero;
  if (out.paths[6].label_blind_sha256 == out.paths[7].label_blind_sha256) flags |= kFlagN1Half;
  // N2: SHAM ALL_F labels equal the complement of SHAM ALL_M labels after every update.
  if (out.paths[4].label_sha256 == out.paths[5].complement_label_sha256) flags |= kFlagN2Zero;
  if (out.paths[6].label_sha256 == out.paths[7].complement_label_sha256) flags |= kFlagN2Half;
  bool sum_ok = out.paths[4].late_m_sum + out.paths[5].late_m_sum == u32(kLateLen * kPop) &&
                out.paths[6].late_m_sum + out.paths[7].late_m_sum == u32(kLateLen * kPop);
  for (int t = 0; t < kUpdates && sum_ok; ++t) {
    if (out.updates[4][t].m_count + out.updates[5][t].m_count != kPop) sum_ok = false;
    if (out.updates[6][t].m_count + out.updates[7][t].m_count != kPop) sum_ok = false;
  }
  if (sum_ok) flags |= kFlagN2Sum;
  bool q_ok = true;
  for (int c = 0; c < kCells; ++c) {
    if (out.paths[c].total_queries != u32(kUpdates * kCandidates)) q_ok = false;
    for (int t = 0; t < kUpdates; ++t)
      if (out.updates[c][t].query_count != kCandidates) q_ok = false;
  }
  if (q_ok) flags |= kFlagQueries;
  br.flags = flags;
}

}  // namespace torus
