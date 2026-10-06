// One update of one cell: candidates -> tournaments -> label/cache transition.
#pragma once

#include "candidates.hpp"
#include "records.hpp"
#include "state.hpp"
#include "tape.hpp"
#include "tournament.hpp"
#include "transition.hpp"

namespace torus {

struct StepWork {
  CandidateSet cand;
  TournamentResult tour;
  TransitionResult trans;
};

// Advances `st` in place. After the call, `w` holds the full detail of the
// update (used by audit rows and trajectory digests).
UpdateRecord step_cell(CellState& st, Arm arm, int update, const u16 target[kLoci], const UpdateTape& tape,
                       StepWork& w);

}  // namespace torus
