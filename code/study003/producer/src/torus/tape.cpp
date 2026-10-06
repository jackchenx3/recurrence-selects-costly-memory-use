#include "tape.hpp"

namespace torus {

void generate_update_tape(const KeySet& ks, u32 block, u32 update, UpdateTape& out) {
  const u64 t = update;
  for (int p = 0; p < kPop; ++p) {
    draw_vector32(ks, P_FRESH_VECTOR, block, t, u64(p), out.fresh[p]);
    draw_vector32(ks, P_SCOUT_VECTOR, block, t, u64(p), out.scout[p]);
    draw_vector32(ks, P_LOCAL_REPLACE_FLAG, block, t, u64(p), out.flag_lane[p]);
    draw_vector32(ks, P_LOCAL_REPLACE_VALUE, block, t, u64(p), out.rep_value[p]);
  }
  for (int p = 0; p < kPop; ++p) {
    for (int f = 0; f < 3; ++f) {
      const int e = 3 * p + f;
      out.donor_key[e] = draw(ks, P_DONOR_KEY, block, t, u64(e), 0)[0];
    }
  }
  for (int s = 0; s < kPop; ++s) {
    for (int e = 0; e < kEntries; ++e) out.entry_word[s][e] = draw(ks, P_TOURNAMENT_ENTRY, block, t, u64(s), u64(e))[0];
  }
  for (int c = 0; c < kCandidates; ++c) out.tie_key[c] = draw(ks, P_CANDIDATE_TIE_KEY, block, t, u64(c), 0)[0];
  for (int s = 0; s < kPop; ++s) out.mutation_word[s] = draw(ks, P_POLICY_MUTATION, block, t, u64(s), 0)[0];
}

}  // namespace torus
