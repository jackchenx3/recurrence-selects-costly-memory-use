// Thirty-two independent four-entry tournaments with replacement (design section 7).
#pragma once

#include "constants.hpp"

namespace torus {

struct TournamentResult {
  u8 entry[kPop][kEntries];  // candidate indices (entry_word & 127)
  u8 winner[kPop];           // winning candidate index per survivor slot
  u8 win_count[kCandidates]; // survivor slots won by each candidate
  u8 dup_entry_tournaments;  // tournaments in which some candidate index appears >1 time
  u8 distinct_winners;       // number of distinct winning candidates
};

// Strict label-blind order: lower exact loss, then lower tie key, then lower
// candidate index. Entry position never matters.
inline bool candidate_precedes(int a, int b, const u64 loss[kCandidates], const u64 tie_key[kCandidates]) {
  if (loss[a] != loss[b]) return loss[a] < loss[b];
  if (tie_key[a] != tie_key[b]) return tie_key[a] < tie_key[b];
  return a < b;
}

void run_tournaments(const u64 loss[kCandidates], const u64 tie_key[kCandidates],
                     const u64 entry_word[kPop][kEntries], TournamentResult& out);

}  // namespace torus
