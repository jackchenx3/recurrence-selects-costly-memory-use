// Target laws ZERO and HALF (design section 5) and the shared block targets.
#pragma once

#include "constants.hpp"
#include "threefry_rng.hpp"

namespace torus {

// Index 0 is unused; updates are 1..256.
struct BlockTargets {
  u16 innovation[kUpdates + 1][kLoci];
  u8 copy_bit[kUpdates + 1];
  u16 target[2][kUpdates + 1][kLoci];   // [law]
  u8 target_is_copy[2][kUpdates + 1];   // [law]: 1 iff T_t = T_(t-2)
};

// Pure target law. ZERO: T_t = I_t. HALF: T_1 = I_1, T_2 = I_2; for t >= 3,
// T_t = T_(t-2) when R_t = 1 (chained copies allowed), else I_t.
void apply_target_law(const u16 innovation[kUpdates + 1][kLoci], const u8 copy_bit[kUpdates + 1], Law law,
                      u16 out[kUpdates + 1][kLoci], u8 is_copy[kUpdates + 1]);

// Generates every innovation vector and every copy bit (t = 1..256, including
// the unused t = 1, 2 copy bits) and both laws from them.
void generate_block_targets(const KeySet& ks, u32 block, BlockTargets& out);

// INITIAL_VECTOR for parent 0..31 at counter (block, 0, parent, q).
void generate_initial_phenotypes(const KeySet& ks, u32 block, u16 out[kPop][kLoci]);

}  // namespace torus
