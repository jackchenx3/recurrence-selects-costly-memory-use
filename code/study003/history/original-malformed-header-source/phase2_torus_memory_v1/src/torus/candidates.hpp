// Four candidate families in fixed (family, parent_slot) order (design section 6).
#pragma once

#include "constants.hpp"
#include "state.hpp"
#include "tape.hpp"

namespace torus {

struct CandidateSet {
  u16 phen[kCandidates][kLoci];
  u64 loss[kCandidates];
  u8 probe_read_cache[kPop];  // 1 iff family-1 candidate of parent i is its cache (ACTIVE, M, valid)
  u8 donor_family[kPop];      // 0..2, donor of the local child
  u8 flagged[kPop];           // coordinates flagged for replacement (lane & 31 == 0)
  u8 changed[kPop];           // flagged coordinates whose value actually changed
  u32 queries;                // objective evaluations performed (must be 128)
};

// Builds and evaluates all 128 candidates for one cell and update.
//  family 0: parent x_i
//  family 1: ACTIVE & M & valid cache -> cache; every other case -> fresh(t,i)
//  family 2: scout(t,i)
//  family 3: donor = argmin over families 0..2 of (loss, donor key, family);
//            each coordinate replaced by rep_value iff (flag_lane & 31) == 0.
// SHAM never reads a label or a cache here.
void build_candidates(const CellState& st, Arm arm, const u16 target[kLoci], const UpdateTape& tape,
                      CandidateSet& out);

}  // namespace torus
