#include "step.hpp"

#include "util.hpp"

namespace torus {

UpdateRecord step_cell(CellState& st, Arm arm, int update, const u16 target[kLoci], const UpdateTape& tape,
                       StepWork& w) {
  TORUS_REQUIRE(update >= 1 && update <= kUpdates, "update out of range");
  UpdateRecord r;
  int valid_pre = 0;
  for (int i = 0; i < kPop; ++i) valid_pre += st.cache_valid[i];

  build_candidates(st, arm, target, tape, w.cand);
  TORUS_REQUIRE(w.cand.queries == u32(kCandidates), "objective query count is not 128");
  run_tournaments(w.cand.loss, tape.tie_key, tape.entry_word, w.tour);

  u64 pop_loss = 0;
  int probe_uses = 0, probe_wins = 0, flagged = 0, changed = 0;
  for (int i = 0; i < kPop; ++i) {
    probe_uses += w.cand.probe_read_cache[i];
    flagged += w.cand.flagged[i];
    changed += w.cand.changed[i];
  }
  for (int s = 0; s < kPop; ++s) {
    const int c = w.tour.winner[s];
    pop_loss += w.cand.loss[c];
    if (candidate_family(c) == FAM_PROBE && w.cand.probe_read_cache[candidate_parent(c)]) ++probe_wins;
  }
  TORUS_REQUIRE(pop_loss <= kMaxPopulationLoss, "population loss domain");

  apply_transition(st, w.cand, w.tour, tape.mutation_word, w.trans);

  r.pop_loss = pop_loss;
  r.update = u16(update);
  r.query_count = u16(w.cand.queries);
  r.local_flagged = u16(flagged);
  r.local_changed = u16(changed);
  r.m_count = w.trans.m_count;
  r.valid_cache_pre = u8(valid_pre);
  r.cache_probe_uses = u8(probe_uses);
  r.cache_probe_winner_slots = u8(probe_wins);
  r.f_to_m = w.trans.f_to_m;
  r.m_to_f = w.trans.m_to_f;
  r.dup_entry_tournaments = w.tour.dup_entry_tournaments;
  r.distinct_winners = w.tour.distinct_winners;
  return r;
}

}  // namespace torus
