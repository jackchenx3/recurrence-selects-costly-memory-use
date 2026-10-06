#include "candidates.hpp"

#include <cstring>

#include "torus_loss.hpp"
#include "util.hpp"

namespace torus {

void build_candidates(const CellState& st, Arm arm, const u16 target[kLoci], const UpdateTape& tape,
                      CandidateSet& out) {
  out.queries = 0;
  // Families 0..2.
  for (int i = 0; i < kPop; ++i) {
    std::memcpy(out.phen[candidate_index(FAM_PARENT, i)], st.phen[i], sizeof(u16) * kLoci);

    bool read_cache = false;
    if (arm == ACTIVE) {
      // Only ACTIVE consults the label and cache.
      read_cache = st.label[i] == LABEL_M && st.cache_valid[i] == 1;
    }
    out.probe_read_cache[i] = read_cache ? 1 : 0;
    std::memcpy(out.phen[candidate_index(FAM_PROBE, i)], read_cache ? st.cache[i] : tape.fresh[i],
                sizeof(u16) * kLoci);

    std::memcpy(out.phen[candidate_index(FAM_SCOUT, i)], tape.scout[i], sizeof(u16) * kLoci);
  }
  for (int f = 0; f < 3; ++f) {
    for (int i = 0; i < kPop; ++i) {
      const int c = candidate_index(f, i);
      out.loss[c] = individual_loss(out.phen[c], target);
      ++out.queries;
    }
  }
  // Family 3: local coordinate-resampling child.
  for (int i = 0; i < kPop; ++i) {
    int best = 0;
    for (int f = 1; f < 3; ++f) {
      const u64 lf = out.loss[candidate_index(f, i)];
      const u64 lb = out.loss[candidate_index(best, i)];
      const u64 kf = tape.donor_key[3 * i + f];
      const u64 kb = tape.donor_key[3 * i + best];
      // Lower loss, then lower unsigned donor key, then lower family index.
      if (lf < lb || (lf == lb && kf < kb)) best = f;
    }
    out.donor_family[i] = u8(best);
    const u16* donor = out.phen[candidate_index(best, i)];
    u16* child = out.phen[candidate_index(FAM_LOCAL, i)];
    int flagged = 0, changed = 0;
    for (int j = 0; j < kLoci; ++j) {
      if ((u64(tape.flag_lane[i][j]) & kLocalReplaceMask) == 0) {
        child[j] = tape.rep_value[i][j];
        ++flagged;
        if (child[j] != donor[j]) ++changed;
      } else {
        child[j] = donor[j];
      }
    }
    out.flagged[i] = u8(flagged);
    out.changed[i] = u8(changed);
    const int c = candidate_index(FAM_LOCAL, i);
    out.loss[c] = individual_loss(child, target);
    ++out.queries;
  }
  for (int c = 0; c < kCandidates; ++c) TORUS_REQUIRE(out.loss[c] <= kMaxIndividualLoss, "loss domain");
}

}  // namespace torus
