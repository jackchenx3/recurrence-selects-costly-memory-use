// Per-(block, update) random tape. Every declared draw for the update is
// generated unconditionally, once, independent of arm, law, start, label,
// cache or branch, and the same tape is applied to all eight cells.
//
// Raw words/lanes are kept; masking (& 31, & 127) is applied by the operator
// modules so that hand-built fixture tapes exercise the same semantics.
#pragma once

#include "constants.hpp"
#include "threefry_rng.hpp"

namespace torus {

struct UpdateTape {
  u16 fresh[kPop][kLoci];        // FRESH_VECTOR (block,t,parent,q)
  u16 scout[kPop][kLoci];        // SCOUT_VECTOR (block,t,parent,q)
  u16 flag_lane[kPop][kLoci];    // LOCAL_REPLACE_FLAG lanes; replace iff lane & 31 == 0
  u16 rep_value[kPop][kLoci];    // LOCAL_REPLACE_VALUE (block,t,parent,q)
  u64 donor_key[kDonorEntities]; // DONOR_KEY word0 at entity 3*parent+family
  u64 entry_word[kPop][kEntries];// TOURNAMENT_ENTRY word0; entry = word & 127
  u64 tie_key[kCandidates];      // CANDIDATE_TIE_KEY word0
  u64 mutation_word[kPop];       // POLICY_MUTATION word0; flip iff word & 31 == 0
};

void generate_update_tape(const KeySet& ks, u32 block, u32 update, UpdateTape& out);

}  // namespace torus
