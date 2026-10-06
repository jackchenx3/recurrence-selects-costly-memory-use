// Runs one block: shared initial phenotypes, shared targets and one shared
// tape per update applied to all eight paired cells.
#pragma once

#include <memory>

#include "records.hpp"
#include "step.hpp"
#include "target_law.hpp"
#include "threefry_rng.hpp"

namespace torus {

class AuditSink {
 public:
  virtual ~AuditSink() {}
  virtual void context(const ContextAuditRow& r) = 0;
  virtual void candidate(const CandidateAuditRow& r) = 0;
  virtual void entry(const EntryAuditRow& r) = 0;
};

struct RunOptions {
  int cell_order[kCells] = {0, 1, 2, 3, 4, 5, 6, 7};  // evaluation order (must be a permutation)
  int only_cell = -1;          // >= 0: regenerate a single path (no block record / N1 / N2)
  bool tape_per_cell = false;  // fixture only: regenerate the tape separately for each cell
  AuditSink* sink = nullptr;   // audit rows, emitted in canonical (update, cell) order
  // Fixture only: override the initial labels of one cell with an explicit vector.
  int label_override_cell = -1;
  u8 label_override[kPop] = {0};
};

struct BlockResult {
  UpdateRecord updates[kCells][kUpdates];
  PathRecord paths[kCells];
  BlockRecord block;
  bool ran[kCells];
};

void run_block(const KeySet& ks, u32 block, const RunOptions& opt, BlockResult& out);

// SHA-256 over a cell's final state (phenotypes, labels, cache flags, caches; LE).
Digest final_state_digest(const CellState& st);

}  // namespace torus
