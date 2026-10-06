// Policy/cache transition after survival (design section 7 event order):
//  1. survivor inherits the producing parent's pre-update label;
//  2. that label alone flips M<->F iff mutation_word & 31 == 0 (no phenotype mutation);
//  3. post-mutation F: cache invalid;
//  4. post-mutation M: cache = producing parent's pre-update phenotype
//     (also when the winning candidate was that parent's cache probe).
// SHAM writes caches by the same rule; it never reads them (candidates.cpp).
#pragma once

#include "candidates.hpp"
#include "constants.hpp"
#include "state.hpp"
#include "tournament.hpp"

namespace torus {

struct TransitionResult {
  u8 inherited[kPop];   // producing parent's pre-update label
  u8 flip[kPop];        // 1 iff the label flipped
  u8 post_label[kPop];
  u8 f_to_m;
  u8 m_to_f;
  u8 m_count;           // post-transition M count
};

void apply_transition(CellState& st, const CandidateSet& cand, const TournamentResult& tour,
                      const u64 mutation_word[kPop], TransitionResult& out);

}  // namespace torus
