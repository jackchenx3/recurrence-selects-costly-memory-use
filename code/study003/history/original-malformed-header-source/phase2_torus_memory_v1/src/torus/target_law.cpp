#include "target_law.hpp"

#include <cstring>

namespace torus {

void apply_target_law(const u16 innovation[kUpdates + 1][kLoci], const u8 copy_bit[kUpdates + 1], Law law,
                      u16 out[kUpdates + 1][kLoci], u8 is_copy[kUpdates + 1]) {
  std::memset(out[0], 0, sizeof(u16) * kLoci);
  is_copy[0] = 0;
  for (int t = 1; t <= kUpdates; ++t) {
    const bool copy = law == HALF && t >= 3 && (copy_bit[t] & 1u) == 1u;
    if (copy) {
      std::memcpy(out[t], out[t - 2], sizeof(u16) * kLoci);
    } else {
      std::memcpy(out[t], innovation[t], sizeof(u16) * kLoci);
    }
    is_copy[t] = copy ? 1 : 0;
  }
}

void generate_block_targets(const KeySet& ks, u32 block, BlockTargets& out) {
  std::memset(out.innovation[0], 0, sizeof(u16) * kLoci);
  out.copy_bit[0] = 0;
  for (int t = 1; t <= kUpdates; ++t) {
    draw_vector32(ks, P_TARGET_INNOVATION_VECTOR, block, u64(t), 0, out.innovation[t]);
    Word4 w = draw(ks, P_TARGET_COPY, block, u64(t), 0, 0);
    out.copy_bit[t] = u8(w[0] & 1ULL);
  }
  apply_target_law(out.innovation, out.copy_bit, ZERO, out.target[ZERO], out.target_is_copy[ZERO]);
  apply_target_law(out.innovation, out.copy_bit, HALF, out.target[HALF], out.target_is_copy[HALF]);
}

void generate_initial_phenotypes(const KeySet& ks, u32 block, u16 out[kPop][kLoci]) {
  for (int p = 0; p < kPop; ++p) draw_vector32(ks, P_INITIAL_VECTOR, block, 0, u64(p), out[p]);
}

}  // namespace torus
