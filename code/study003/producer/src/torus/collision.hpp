// Deterministic declared-address enumeration, collision audit and
// evaluation-order / thread-count invariance check (design sections 8, 14.2).
#pragma once

#include <string>
#include <vector>

#include "threefry_rng.hpp"

namespace torus {

// The declared schema of design section 8, written directly from the table
// (independently of tape.cpp / target_law.cpp), for one block.
void enumerate_declared_addresses(u32 block, std::vector<DrawAddress>& out);

struct AuditOutcome {
  bool ok = false;
  std::string json;  // deterministic receipt
};

// 1. every declared address lies in its declared domain;
// 2. no duplicate (purpose, counter) address within a block;
// 3. for every block 0..41599: counter word 0 == block and the within-block
//    (purpose, update, entity, subindex) list is identical, so no address
//    repeats anywhere in the study;
// 4. the actual generator call set equals the declared set exactly (recorded
//    for sample blocks), with no repeated call;
// 5. vector extraction maps coordinates bijectively onto (q, word, lane);
// 6. all 33 purpose keys of the three namespaces are pairwise distinct.
AuditOutcome run_collision_audit(const KeySet& ks);

// Runs blocks with canonical vs. reversed vs. per-cell-tape evaluation and
// with 1 vs. `threads` worker threads; all serialized outputs must be
// byte-identical. Uses the supplied (non-production) keys only.
AuditOutcome run_invariance_check(const KeySet& non_production_ks, int threads);

}  // namespace torus
