#include "tournament.hpp"

#include <cstring>

namespace torus {

void run_tournaments(const u64 loss[kCandidates], const u64 tie_key[kCandidates],
                     const u64 entry_word[kPop][kEntries], TournamentResult& out) {
  std::memset(out.win_count, 0, sizeof out.win_count);
  int dup = 0;
  for (int s = 0; s < kPop; ++s) {
    for (int e = 0; e < kEntries; ++e) out.entry[s][e] = u8(entry_word[s][e] & kTournamentEntryMask);
    int w = out.entry[s][0];
    for (int e = 1; e < kEntries; ++e) {
      const int c = out.entry[s][e];
      if (candidate_precedes(c, w, loss, tie_key)) w = c;
    }
    out.winner[s] = u8(w);
    ++out.win_count[w];
    bool has_dup = false;
    for (int a = 0; a < kEntries; ++a)
      for (int b = a + 1; b < kEntries; ++b)
        if (out.entry[s][a] == out.entry[s][b]) has_dup = true;
    if (has_dup) ++dup;
  }
  int distinct = 0;
  for (int c = 0; c < kCandidates; ++c)
    if (out.win_count[c] > 0) ++distinct;
  out.dup_entry_tournaments = u8(dup);
  out.distinct_winners = u8(distinct);
}

}  // namespace torus
