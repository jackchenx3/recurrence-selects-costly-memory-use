// Coordinate-keyed Threefry4x64-20 addressing (design section 8).
//
// Key for purpose P in namespace NS = SHA-256("PHASE2-TORUS-MEMORY-003|NS|P"),
// digest bytes 8j..8j+7 read little-endian as key word j. Counter words are
// (block, update, entity, subindex). The only Threefry implementation is the
// pinned public Random123 header.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "constants.hpp"

namespace torus {

enum Purpose : u8 {
  P_INITIAL_VECTOR = 0,
  P_TARGET_INNOVATION_VECTOR = 1,
  P_TARGET_COPY = 2,
  P_FRESH_VECTOR = 3,
  P_SCOUT_VECTOR = 4,
  P_LOCAL_REPLACE_FLAG = 5,
  P_LOCAL_REPLACE_VALUE = 6,
  P_DONOR_KEY = 7,
  P_TOURNAMENT_ENTRY = 8,
  P_CANDIDATE_TIE_KEY = 9,
  P_POLICY_MUTATION = 10,
};
constexpr int kPurposes = 11;
const char* purpose_name(int p);

using Word4 = std::array<u64, 4>;

struct KeySet {
  std::string ns;
  std::array<Word4, kPurposes> key;
};

// Namespace must be exactly one of production-r1, fixture-r1, timing-r1.
KeySet derive_keys(const std::string& ns);
std::string key_text(const std::string& ns, int purpose);
// Deterministic receipt: key text, digest and key words for every purpose.
std::string key_derivation_json(const KeySet& ks);

// Raw canonical Random123 Threefry4x64-20.
Word4 threefry4x64_20(const Word4& ctr, const Word4& key);

// Official known-answer vectors from the frozen design; false on mismatch.
bool threefry_design_kats();

// Declared-domain check for one draw address. Aborts on any out-of-range
// block, update, entity or subindex (design: "abort on out-of-range").
void check_address_domain(int purpose, u64 block, u64 update, u64 entity, u64 sub);

// One addressed draw. Every Threefry call in the scientific code goes
// through here, so the collision checker can record the complete call set.
Word4 draw(const KeySet& ks, int purpose, u64 block, u64 update, u64 entity, u64 sub);

// 32-coordinate vector: two calls (q=0,1); coordinate 16q+4*word+lane gets
// (output_word >> 16*lane) & 0xffff.
void draw_vector32(const KeySet& ks, int purpose, u64 block, u64 update, u64 entity, u16 out[32]);

// Optional per-thread recorder of every draw address (collision audit only).
struct DrawAddress {
  u8 purpose;
  u64 ctr[4];
  bool operator<(const DrawAddress& o) const;
  bool operator==(const DrawAddress& o) const;
};
void set_thread_draw_recorder(std::vector<DrawAddress>* rec);

}  // namespace torus
