// Exact circular squared loss on Z/2^16 Z (design section 3). Integer only.
#pragma once

#include "constants.hpp"

namespace torus {

// u = (x - t) mod 2^16 in 0..65535; d = min(u, 65536 - u) in 0..32768.
inline u32 circular_distance(u16 x, u16 t) {
  u32 u = u32(u16(x - t));
  u32 v = 65536u - u;
  return u < v ? u : v;
}

inline u64 coordinate_loss(u16 x, u16 t) {
  u64 d = circular_distance(x, t);
  return d * d;  // <= 2^30
}

// Q(x,t) = sum_i d_i^2 over 32 coordinates; <= 2^35.
inline u64 individual_loss(const u16 x[kLoci], const u16 t[kLoci]) {
  u64 q = 0;
  for (int i = 0; i < kLoci; ++i) q += coordinate_loss(x[i], t[i]);
  return q;
}

}  // namespace torus
