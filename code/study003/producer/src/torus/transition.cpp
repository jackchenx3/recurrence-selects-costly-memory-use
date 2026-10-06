#include "transition.hpp"

#include <cstring>

namespace torus {

void apply_transition(CellState& st, const CandidateSet& cand, const TournamentResult& tour,
                      const u64 mutation_word[kPop], TransitionResult& out) {
  CellState next;
  int f2m = 0, m2f = 0, m = 0;
  for (int s = 0; s < kPop; ++s) {
    const int w = tour.winner[s];
    const int producer = candidate_parent(w);
    std::memcpy(next.phen[s], cand.phen[w], sizeof(u16) * kLoci);
    const u8 inherited = st.label[producer];
    const u8 flip = (mutation_word[s] & kPolicyMutationMask) == 0 ? 1 : 0;
    const u8 post = flip ? u8(inherited ^ 1u) : inherited;
    if (flip && inherited == LABEL_F) ++f2m;
    if (flip && inherited == LABEL_M) ++m2f;
    next.label[s] = post;
    if (post == LABEL_M) {
      next.cache_valid[s] = 1;
      std::memcpy(next.cache[s], st.phen[producer], sizeof(u16) * kLoci);  // pre-update phenotype
      ++m;
    } else {
      next.cache_valid[s] = 0;
      std::memset(next.cache[s], 0, sizeof(u16) * kLoci);
    }
    out.inherited[s] = inherited;
    out.flip[s] = flip;
    out.post_label[s] = post;
  }
  out.f_to_m = u8(f2m);
  out.m_to_f = u8(m2f);
  out.m_count = u8(m);
  std::memcpy(&st, &next, sizeof st);
}

}  // namespace torus
