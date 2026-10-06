// Per-cell population state.
#pragma once

#include <cstring>

#include "constants.hpp"

namespace torus {

struct CellState {
  u16 phen[kPop][kLoci];   // phenotypes in deterministic slots 0..31
  u8 label[kPop];          // LABEL_F or LABEL_M
  u8 cache_valid[kPop];    // 0 = invalid, 1 = one cached 32-coordinate phenotype
  u16 cache[kPop][kLoci];  // all zero whenever cache_valid == 0
};

// Initial state: shared phenotypes, every cache invalid, labels by start.
inline void init_cell_state(CellState& st, const u16 initial[kPop][kLoci], Start start) {
  std::memcpy(st.phen, initial, sizeof st.phen);
  const u8 lab = start == ALL_M ? LABEL_M : LABEL_F;
  for (int i = 0; i < kPop; ++i) {
    st.label[i] = lab;
    st.cache_valid[i] = 0;
  }
  std::memset(st.cache, 0, sizeof st.cache);
}

}  // namespace torus
