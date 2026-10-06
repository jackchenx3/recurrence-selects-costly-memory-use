// PHASE2-TORUS-MEMORY-003 deterministic fixtures (design section 14, groups 1-16).
//
// Hand-built tapes are used wherever an expected value must be derivable by
// hand; Threefry-driven checks use namespace fixture-r1 only. No production
// key, production block outcome or scientific estimate is produced.

#include <cmath>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "../torus/chunk_io.hpp"
#include "../torus/cli.hpp"
#include "../torus/collision.hpp"
#include "../torus/config_check.hpp"
#include "../torus/path_runner.hpp"
#include "../torus/step.hpp"
#include "../torus/target_law.hpp"
#include "../torus/torus_loss.hpp"
#include "../torus/util.hpp"

using namespace torus;

namespace {

struct Group {
  int id;
  std::string name;
  bool pass = true;
  std::vector<std::string> notes;
};

#define CHECK(g, cond, msg)                  \
  do {                                       \
    if (!(cond)) {                           \
      (g).pass = false;                      \
      (g).notes.push_back(std::string(msg)); \
    }                                        \
  } while (0)

void fill(u16 x[kLoci], u16 v) {
  for (int j = 0; j < kLoci; ++j) x[j] = v;
}

bool all_eq(const u16 x[kLoci], u16 v) {
  for (int j = 0; j < kLoci; ++j)
    if (x[j] != v) return false;
  return true;
}

bool same_vec(const u16 a[kLoci], const u16 b[kLoci]) { return std::memcmp(a, b, sizeof(u16) * kLoci) == 0; }

// Default hand tape: fresh = 1000, scout = 2000, no local flags (lane 1),
// replacement value 7, donor key = entity, tie key = candidate index,
// tournament s = four entries of candidate 64+s (scout of slot s), no flips.
std::unique_ptr<UpdateTape> default_tape() {
  std::unique_ptr<UpdateTape> t(new UpdateTape);
  for (int i = 0; i < kPop; ++i) {
    fill(t->fresh[i], 1000);
    fill(t->scout[i], 2000);
    fill(t->flag_lane[i], 1);
    fill(t->rep_value[i], 7);
    t->mutation_word[i] = 1;
    for (int e = 0; e < kEntries; ++e) t->entry_word[i][e] = u64(64 + i);
  }
  for (int k = 0; k < kDonorEntities; ++k) t->donor_key[k] = u64(k);
  for (int c = 0; c < kCandidates; ++c) t->tie_key[c] = u64(c);
  return t;
}

std::unique_ptr<CellState> uniform_state(u16 v, u8 label) {
  std::unique_ptr<CellState> s(new CellState);
  for (int i = 0; i < kPop; ++i) {
    fill(s->phen[i], v);
    s->label[i] = label;
    s->cache_valid[i] = 0;
    fill(s->cache[i], 0);
  }
  return s;
}

void set_entries(UpdateTape& t, int s, int a, int b, int c, int d) {
  t.entry_word[s][0] = u64(a);
  t.entry_word[s][1] = u64(b);
  t.entry_word[s][2] = u64(c);
  t.entry_word[s][3] = u64(d);
}

const u16 kZeroTarget[kLoci] = {0};

// ---------------------------------------------------------------- group 1
Group g01_kat(const std::string& root) {
  Group g{1, "official Threefry4x64-20 known-answer vectors and SHA-256 self-test"};
  CHECK(g, sha256_self_test(), "SHA-256 self-test");
  CHECK(g, threefry_design_kats(), "design all-zero / all-one KATs");
  // Every threefry4x64 20-round line of the pinned official kat_vectors file.
  const std::string text = read_text_file(join_path(root, kKatVectorsRel));
  std::istringstream in(text);
  std::string line;
  int n = 0;
  while (std::getline(in, line)) {
    std::istringstream ls(line);
    std::vector<std::string> tok;
    std::string w;
    while (ls >> w) tok.push_back(w);
    if (tok.size() != 14 || tok[0] != "threefry4x64" || tok[1] != "20") continue;
    Word4 ctr, key, want;
    for (int i = 0; i < 4; ++i) {
      ctr[i] = std::stoull(tok[2 + i], nullptr, 16);
      key[i] = std::stoull(tok[6 + i], nullptr, 16);
      want[i] = std::stoull(tok[10 + i], nullptr, 16);
    }
    CHECK(g, threefry4x64_20(ctr, key) == want, "official KAT line mismatch: " + line);
    ++n;
  }
  CHECK(g, n >= 3, "fewer than three threefry4x64 20-round official KAT lines found");
  g.notes.push_back("official threefry4x64-20 KAT lines checked: " + std::to_string(n));
  return g;
}

// ---------------------------------------------------------------- group 2
Group g02_collision(const KeySet& fx, int threads) {
  Group g{2, "declared-address enumeration, collision audit, thread/cell-order invariance"};
  const AuditOutcome c = run_collision_audit(fx);
  CHECK(g, c.ok, "collision audit: " + c.json);
  const AuditOutcome v = run_invariance_check(fx, threads);
  CHECK(g, v.ok, "invariance: " + v.json);
  return g;
}

// ---------------------------------------------------------------- group 3
Group g03_wrap() {
  Group g{3, "circular-distance wrap, equality and antipode"};
  CHECK(g, circular_distance(0, 65535) == 1, "(0,65535)");
  CHECK(g, circular_distance(65535, 0) == 1, "(65535,0)");
  CHECK(g, circular_distance(12345, 12345) == 0, "equality");
  CHECK(g, circular_distance(0, 32768) == 32768, "antipode (0,32768)");
  CHECK(g, circular_distance(32768, 0) == 32768, "antipode (32768,0)");
  CHECK(g, circular_distance(40000, 7232) == 32768, "antipode (40000,7232)");
  CHECK(g, circular_distance(1, 65534) == 3, "wrap (1,65534)");
  CHECK(g, circular_distance(32767, 0) == 32767 && circular_distance(32769, 0) == 32767, "near-antipode symmetry");
  CHECK(g, coordinate_loss(0, 32768) == (1ULL << 30), "max coordinate loss 2^30");
  CHECK(g, coordinate_loss(0, 65535) == 1, "wrap loss 1");
  return g;
}

// ---------------------------------------------------------------- group 4
Group g04_max_loss() {
  Group g{4, "maximum coordinate, individual and population loss; exact accuracy numerator"};
  u16 x[kLoci];
  fill(x, 32768);
  CHECK(g, individual_loss(x, kZeroTarget) == 34359738368ULL, "max individual loss 2^35");
  CHECK(g, individual_loss(kZeroTarget, kZeroTarget) == 0, "zero loss");
  // Whole population at the antipode with antipodal fresh/scout proposals.
  auto st = uniform_state(32768, LABEL_F);
  auto t = default_tape();
  for (int i = 0; i < kPop; ++i) {
    fill(t->fresh[i], 32768);
    fill(t->scout[i], 32768);
  }
  StepWork w;
  const UpdateRecord r = step_cell(*st, ACTIVE, 1, kZeroTarget, *t, w);
  CHECK(g, r.pop_loss == 1099511627776ULL, "max population loss 2^40");
  CHECK(g, kMaxPopulationLoss - r.pop_loss == 0, "accuracy numerator 0 at maximum loss");
  auto st2 = uniform_state(0, LABEL_F);
  auto t2 = default_tape();
  for (int i = 0; i < kPop; ++i) {
    fill(t2->fresh[i], 0);
    fill(t2->scout[i], 0);
  }
  const UpdateRecord r2 = step_cell(*st2, ACTIVE, 1, kZeroTarget, *t2, w);
  CHECK(g, r2.pop_loss == 0 && kMaxPopulationLoss - r2.pop_loss == 1099511627776ULL,
        "accuracy numerator 2^40 at exact agreement");
  ByteBuf b;
  serialize_update(r, b);
  CHECK(g, b.size() == kUpdateRowBytes, "max-loss record serializes");
  return g;
}

// ---------------------------------------------------------------- group 5
Group g05_hand_trace() {
  Group g{5, "one- and two-update hand trace: all families, tournaments, label mutation, cache transitions"};
  // Target 0. All parents 3000 (loss 32*3000^2 = 288,000,000).
  // Slot 0: M with valid cache of 5s (loss 800). Slot 1: M, invalid cache.
  auto st = uniform_state(3000, LABEL_F);
  st->label[0] = LABEL_M;
  st->label[1] = LABEL_M;
  st->cache_valid[0] = 1;
  fill(st->cache[0], 5);
  auto t1 = default_tape();
  t1->flag_lane[2][0] = 32;  // 32 & 31 == 0 -> replace
  t1->rep_value[2][0] = 0;   // child of slot 2: (0, 1000 x 31), loss 31,000,000
  set_entries(*t1, 0, 32, 32, 32, 32);  // cache probe of slot 0
  set_entries(*t1, 1, 33, 1, 65, 97);   // 33 vs 97 tie at 32e6 -> tie key 33 < 97
  set_entries(*t1, 2, 98, 34, 2, 66);   // 98 has loss 31e6 -> wins
  set_entries(*t1, 3, 3, 3, 3, 3);      // parent 3
  t1->mutation_word[0] = 32;  // flip slot 0: M -> F
  t1->mutation_word[3] = 0;   // flip slot 3: F -> M
  StepWork w;
  const UpdateRecord r1 = step_cell(*st, ACTIVE, 1, kZeroTarget, *t1, w);
  CHECK(g, w.cand.loss[32] == 800 && w.cand.probe_read_cache[0] == 1, "u1 valid ACTIVE-M reads cache");
  CHECK(g, w.cand.loss[33] == 32000000ULL && w.cand.probe_read_cache[1] == 0, "u1 invalid ACTIVE-M uses fresh");
  CHECK(g, w.cand.loss[0] == 288000000ULL && w.cand.loss[64] == 128000000ULL, "u1 parent / scout losses");
  CHECK(g, w.cand.donor_family[0] == FAM_PROBE && w.cand.loss[96] == 800, "u1 slot-0 donor is the cache probe");
  CHECK(g, w.cand.loss[98] == 31000000ULL && w.cand.flagged[2] == 1 && w.cand.changed[2] == 1, "u1 local child slot 2");
  CHECK(g, w.tour.winner[0] == 32 && w.tour.winner[1] == 33 && w.tour.winner[2] == 98 && w.tour.winner[3] == 3,
        "u1 tournament winners 0..3");
  for (int s = 4; s < kPop; ++s) CHECK(g, w.tour.winner[s] == 64 + s, "u1 default scout winners");
  CHECK(g, r1.pop_loss == 3935000800ULL, "u1 population loss 3,935,000,800");
  CHECK(g, r1.m_count == 2 && r1.valid_cache_pre == 1 && r1.cache_probe_uses == 1 && r1.cache_probe_winner_slots == 1,
        "u1 M count / cache counters");
  CHECK(g, r1.f_to_m == 1 && r1.m_to_f == 1, "u1 mutation counts");
  CHECK(g, r1.dup_entry_tournaments == 30 && r1.distinct_winners == 32, "u1 duplicate / distinct counts");
  CHECK(g, r1.query_count == 128 && r1.local_flagged == 1 && r1.local_changed == 1, "u1 query / local counts");
  CHECK(g, st->label[0] == LABEL_F && st->cache_valid[0] == 0 && all_eq(st->cache[0], 0) && all_eq(st->phen[0], 5),
        "u1 slot 0: cache-probe phenotype, M->F, cache invalidated");
  CHECK(g, st->label[1] == LABEL_M && st->cache_valid[1] == 1 && all_eq(st->cache[1], 3000) && all_eq(st->phen[1], 1000),
        "u1 slot 1: fresh phenotype, M keeps M, caches parent's pre-update phenotype");
  CHECK(g, st->label[2] == LABEL_F && st->cache_valid[2] == 0 && st->phen[2][0] == 0 && st->phen[2][1] == 1000,
        "u1 slot 2: local child, F");
  CHECK(g, st->label[3] == LABEL_M && st->cache_valid[3] == 1 && all_eq(st->cache[3], 3000) && all_eq(st->phen[3], 3000),
        "u1 slot 3: F->M caches parent's pre-update phenotype");

  // Update 2: every tournament is four copies of the policy probe 32+s.
  CellState after1;
  std::memcpy(&after1, st.get(), sizeof after1);
  auto t2 = default_tape();
  for (int s = 0; s < kPop; ++s) set_entries(*t2, s, 32 + s, 32 + s, 32 + s, 32 + s);
  const UpdateRecord r2 = step_cell(*st, ACTIVE, 2, kZeroTarget, *t2, w);
  CHECK(g, r2.pop_loss == 1536000000ULL, "u2 ACTIVE population loss 1,536,000,000 (memory-worse caches)");
  CHECK(g, r2.m_count == 2 && r2.valid_cache_pre == 2 && r2.cache_probe_uses == 2 && r2.cache_probe_winner_slots == 2,
        "u2 ACTIVE cache counters");
  CHECK(g, r2.f_to_m == 0 && r2.m_to_f == 0 && r2.dup_entry_tournaments == 32 && r2.distinct_winners == 32,
        "u2 ACTIVE mutation / tournament counters");
  CHECK(g, all_eq(st->phen[1], 3000) && st->cache_valid[1] == 1 && all_eq(st->cache[1], 1000),
        "u2 slot 1 won with its cache probe but caches the parent's pre-update phenotype (1000s)");
  CHECK(g, all_eq(st->phen[3], 3000) && all_eq(st->cache[3], 3000), "u2 slot 3 cache = parent's pre-update phenotype");
  // SHAM from the same post-update-1 state: never reads caches, still writes them.
  CellState sh;
  std::memcpy(&sh, &after1, sizeof sh);
  const UpdateRecord r2s = step_cell(sh, SHAM, 2, kZeroTarget, *t2, w);
  CHECK(g, r2s.pop_loss == 1024000000ULL && r2s.cache_probe_uses == 0 && r2s.cache_probe_winner_slots == 0,
        "u2 SHAM uses fresh proposals only");
  CHECK(g, sh.cache_valid[1] == 1 && all_eq(sh.cache[1], 1000) && all_eq(sh.phen[1], 1000),
        "u2 SHAM writes the M cache by the same rule");
  return g;
}

// ---------------------------------------------------------------- group 6
Group g06_target(const KeySet& fx) {
  Group g{6, "target-law pairing at t=1/2, innovations, direct and chained copies"};
  std::unique_ptr<BlockTargets> b(new BlockTargets);
  std::memset(b.get(), 0, sizeof(BlockTargets));
  for (int t = 1; t <= kUpdates; ++t)
    for (int j = 0; j < kLoci; ++j) b->innovation[t][j] = u16(100 * t + j);
  const u8 bits[9] = {0, 1, 1, 1, 0, 1, 1, 1, 0};
  for (int t = 1; t <= 8; ++t) b->copy_bit[t] = bits[t];
  apply_target_law(b->innovation, b->copy_bit, ZERO, b->target[ZERO], b->target_is_copy[ZERO]);
  apply_target_law(b->innovation, b->copy_bit, HALF, b->target[HALF], b->target_is_copy[HALF]);
  for (int t = 1; t <= kUpdates; ++t) CHECK(g, same_vec(b->target[ZERO][t], b->innovation[t]), "ZERO: T_t = I_t");
  CHECK(g, same_vec(b->target[HALF][1], b->innovation[1]) && same_vec(b->target[HALF][2], b->innovation[2]),
        "HALF t=1,2 use innovations although R_1 = R_2 = 1");
  CHECK(g, same_vec(b->target[HALF][3], b->innovation[1]), "HALF t=3 direct copy of T_1");
  CHECK(g, same_vec(b->target[HALF][4], b->innovation[4]), "HALF t=4 new innovation (R_4 = 0)");
  CHECK(g, same_vec(b->target[HALF][5], b->innovation[1]), "HALF t=5 chained copy T_5 = T_3 = I_1");
  CHECK(g, same_vec(b->target[HALF][6], b->innovation[4]), "HALF t=6 copy T_6 = T_4 = I_4");
  CHECK(g, same_vec(b->target[HALF][7], b->innovation[1]), "HALF t=7 doubly chained copy = I_1");
  CHECK(g, same_vec(b->target[HALF][8], b->innovation[8]), "HALF t=8 innovation");
  const u8 expect_copy[9] = {0, 0, 0, 1, 0, 1, 1, 1, 0};
  for (int t = 1; t <= 8; ++t) CHECK(g, b->target_is_copy[HALF][t] == expect_copy[t], "HALF copy indicator");
  // Threefry-generated targets share every innovation between laws.
  std::unique_ptr<BlockTargets> r(new BlockTargets);
  generate_block_targets(fx, 7, *r);
  for (int t = 1; t <= kUpdates; ++t) {
    CHECK(g, same_vec(r->target[ZERO][t], r->innovation[t]), "generated ZERO uses shared innovation");
    if (t <= 2) {
      CHECK(g, same_vec(r->target[HALF][t], r->innovation[t]), "generated HALF t<=2");
    } else if (r->copy_bit[t] & 1) {
      CHECK(g, same_vec(r->target[HALF][t], r->target[HALF][t - 2]), "generated HALF copy");
    } else {
      CHECK(g, same_vec(r->target[HALF][t], r->innovation[t]), "generated HALF innovation");
    }
  }
  return g;
}

// ---------------------------------------------------------------- group 7
Group g07_memory_better_worse() {
  Group g{7, "memory-better and memory-worse torus cases with exact integer losses"};
  auto t = default_tape();  // fresh 1000s: loss 32,000,000
  StepWork w;
  auto better = uniform_state(3000, LABEL_F);
  better->label[0] = LABEL_M;
  better->cache_valid[0] = 1;
  fill(better->cache[0], 65535);  // distance 1 to 0 across the wrap
  step_cell(*better, ACTIVE, 1, kZeroTarget, *t, w);
  CHECK(g, w.cand.loss[32] == 32 && w.cand.loss[33] == 32000000ULL, "memory-better: cache loss 32 < fresh 32e6");
  auto worse = uniform_state(3000, LABEL_F);
  worse->label[0] = LABEL_M;
  worse->cache_valid[0] = 1;
  fill(worse->cache[0], 32768);  // antipode
  step_cell(*worse, ACTIVE, 1, kZeroTarget, *t, w);
  CHECK(g, w.cand.loss[32] == 34359738368ULL && w.cand.loss[33] == 32000000ULL,
        "memory-worse: cache loss 2^35 > fresh 32e6");
  return g;
}

// ---------------------------------------------------------------- group 8
Group g08_duplicate() {
  Group g{8, "duplicate candidates: cache equals parent, both evaluated, 128 queries"};
  auto st = uniform_state(3000, LABEL_M);
  for (int i = 0; i < kPop; ++i) {
    st->cache_valid[i] = 1;
    fill(st->cache[i], 3000);
  }
  auto t = default_tape();
  StepWork w;
  const UpdateRecord r = step_cell(*st, ACTIVE, 1, kZeroTarget, *t, w);
  for (int i = 0; i < kPop; ++i) {
    CHECK(g, same_vec(w.cand.phen[i], w.cand.phen[32 + i]) && w.cand.loss[i] == w.cand.loss[32 + i],
          "parent and cache probe identical");
  }
  CHECK(g, w.cand.queries == 128 && r.query_count == 128, "query count stays 128 with duplicates");
  CHECK(g, r.cache_probe_uses == 32, "all 32 duplicate cache probes counted as cache uses");
  return g;
}

// ---------------------------------------------------------------- group 9
Group g09_invalid_cache() {
  Group g{9, "invalid cache: ACTIVE-M uses exactly the same fresh vector as F and SHAM"};
  auto t = default_tape();
  for (int i = 0; i < kPop; ++i)
    for (int j = 0; j < kLoci; ++j) t->fresh[i][j] = u16(1000 + 37 * i + j);
  auto st = uniform_state(3000, LABEL_F);
  for (int i = 0; i < kPop; ++i) {
    st->label[i] = (i % 2 == 0) ? LABEL_M : LABEL_F;
    if (i % 4 == 0) {
      st->cache_valid[i] = 1;
      fill(st->cache[i], 5);
    }
  }
  CellState a, s;
  std::memcpy(&a, st.get(), sizeof a);
  std::memcpy(&s, st.get(), sizeof s);
  StepWork wa, ws;
  step_cell(a, ACTIVE, 1, kZeroTarget, *t, wa);
  step_cell(s, SHAM, 1, kZeroTarget, *t, ws);
  int reads = 0;
  for (int i = 0; i < kPop; ++i) {
    const bool reads_cache = (i % 4 == 0);
    reads += wa.cand.probe_read_cache[i];
    if (reads_cache) {
      CHECK(g, all_eq(wa.cand.phen[32 + i], 5), "ACTIVE valid M reads cache");
    } else {
      CHECK(g, same_vec(wa.cand.phen[32 + i], t->fresh[i]), "ACTIVE invalid-M or F uses fresh(t,i)");
    }
    CHECK(g, same_vec(ws.cand.phen[32 + i], t->fresh[i]) && ws.cand.probe_read_cache[i] == 0,
          "SHAM uses fresh(t,i) for every label and cache");
  }
  CHECK(g, reads == 8, "exactly the 8 valid ACTIVE-M slots read caches");
  return g;
}

// ---------------------------------------------------------------- group 10
Group g10_donor_tie() {
  Group g{10, "donor tie: equal losses, lower donor key, lower family index"};
  // Target 0; parent 100s, fresh 65436s and scout alternating 100/65436: all
  // have distance 100 per coordinate (loss 320,000) but distinct phenotypes.
  auto st = uniform_state(100, LABEL_F);
  auto t = default_tape();
  for (int i = 0; i < kPop; ++i) {
    fill(t->fresh[i], 65436);
    for (int j = 0; j < kLoci; ++j) t->scout[i][j] = (j % 2 == 0) ? 100 : 65436;
  }
  auto keys = [&](int i, u64 k0, u64 k1, u64 k2) {
    t->donor_key[3 * i + 0] = k0;
    t->donor_key[3 * i + 1] = k1;
    t->donor_key[3 * i + 2] = k2;
  };
  keys(0, 50, 10, 10);  // tie on key between 1 and 2 -> family 1
  keys(1, 5, 10, 10);   // lowest key -> family 0
  keys(2, 50, 20, 10);  // lowest key -> family 2
  keys(3, 7, 7, 7);     // all tied -> family 0
  keys(4, 0, 0, ~0ULL); // scout strictly lower loss wins despite largest key
  fill(t->scout[4], 0);
  StepWork w;
  step_cell(*st, ACTIVE, 1, kZeroTarget, *t, w);
  CHECK(g, w.cand.loss[0] == 320000 && w.cand.loss[32] == 320000 && w.cand.loss[64] == 320000, "equal donor losses");
  const int expect[5] = {1, 0, 2, 0, 2};
  for (int i = 0; i < 5; ++i) {
    CHECK(g, w.cand.donor_family[i] == expect[i], "donor family slot " + std::to_string(i));
    CHECK(g, same_vec(w.cand.phen[96 + i], w.cand.phen[32 * expect[i] + i]), "child copies chosen donor");
  }
  return g;
}

// ---------------------------------------------------------------- group 11
Group g11_local_child() {
  Group g{11, "local child: flagged replacement equal to donor value; flagged vs actual-change counts"};
  // Target 0; parent 1000s (best donor), fresh 2000s, scout 3000s.
  auto st = uniform_state(1000, LABEL_F);
  auto t = default_tape();
  for (int i = 0; i < kPop; ++i) {
    fill(t->fresh[i], 2000);
    fill(t->scout[i], 3000);
  }
  t->flag_lane[0][0] = 0;      t->rep_value[0][0] = 1000;  // flagged, equals donor -> unchanged
  t->flag_lane[0][1] = 32;     t->rep_value[0][1] = 5;     // flagged, changes
  t->flag_lane[0][2] = 65504;  t->rep_value[0][2] = 1000;  // flagged (65504 & 31 == 0), unchanged
  t->flag_lane[0][3] = 31;     t->rep_value[0][3] = 9;     // not flagged
  t->flag_lane[0][4] = 33;     t->rep_value[0][4] = 9;     // not flagged
  StepWork w;
  const UpdateRecord r = step_cell(*st, ACTIVE, 1, kZeroTarget, *t, w);
  CHECK(g, w.cand.donor_family[0] == FAM_PARENT, "donor is parent");
  CHECK(g, w.cand.flagged[0] == 3 && w.cand.changed[0] == 1, "flagged 3, actually changed 1");
  CHECK(g, w.cand.phen[96][0] == 1000 && w.cand.phen[96][1] == 5 && w.cand.phen[96][2] == 1000 &&
               w.cand.phen[96][3] == 1000 && w.cand.phen[96][4] == 1000,
        "child coordinates");
  CHECK(g, r.local_flagged == 3 && r.local_changed == 1, "update record local counts");
  // Exact operator probabilities over the full lane / value domains.
  u64 flagged_lanes = 0;
  for (u32 v = 0; v < 65536; ++v)
    if ((u64(v) & kLocalReplaceMask) == 0) ++flagged_lanes;
  CHECK(g, flagged_lanes == 2048, "flag probability exactly 2048/65536 = 1/32");
  CHECK(g, flagged_lanes * 65535ULL * 32ULL * 65536ULL == 65535ULL * 65536ULL * 65536ULL,
        "actual-change probability (1/32)(1 - 2^-16)");
  return g;
}

// ---------------------------------------------------------------- group 12
Group g12_tournament() {
  Group g{12, "tournament: duplicate entries, cross-label ties, forced key tie, index fallback, multiple wins"};
  // Target 0; parents 0s (loss 0), fresh 1000s, scout 2000s; children copy
  // the parent (loss 0). Labels alternate F/M; caches invalid.
  auto st = uniform_state(0, LABEL_F);
  for (int i = 0; i < kPop; ++i) st->label[i] = (i % 2) ? LABEL_M : LABEL_F;
  auto t = default_tape();
  t->tie_key[0] = 7;
  t->tie_key[1] = 7;    // forced tie-key equality between candidates 0 and 1
  t->tie_key[2] = 100;
  t->tie_key[3] = 50;   // F-parent 2 vs M-parent 3, equal loss: key decides
  set_entries(*t, 0, 1, 0, 1, 1);  // -> 0 by candidate-index fallback
  set_entries(*t, 1, 1, 1, 1, 1);  // -> 1
  for (int e = 0; e < 4; ++e) t->entry_word[2][e] = (u64(12345) << 7) | u64(e == 0 || e == 3 ? 2 : 3);  // mask & 127
  set_entries(*t, 3, 0, 0, 0, 0);
  set_entries(*t, 4, 0, 0, 0, 0);
  set_entries(*t, 5, 0, 0, 0, 0);
  set_entries(*t, 6, 0, 33, 64, 0);
  CellState a, b;
  std::memcpy(&a, st.get(), sizeof a);
  std::memcpy(&b, st.get(), sizeof b);
  for (int i = 0; i < kPop; ++i) b.label[i] ^= 1u;  // complemented labels
  StepWork wa, wb;
  const UpdateRecord ra = step_cell(a, ACTIVE, 1, kZeroTarget, *t, wa);
  step_cell(b, ACTIVE, 1, kZeroTarget, *t, wb);
  CHECK(g, wa.tour.entry[2][0] == 2 && wa.tour.entry[2][1] == 3 && wa.tour.entry[2][2] == 3 && wa.tour.entry[2][3] == 2,
        "entry index = word & 127");
  CHECK(g, wa.tour.winner[0] == 0, "forced key tie resolved by lower candidate index");
  CHECK(g, wa.tour.winner[1] == 1, "all-duplicate tournament");
  CHECK(g, wa.tour.winner[2] == 3, "cross-label equal-loss decided by tie key only");
  CHECK(g, wa.tour.winner[3] == 0 && wa.tour.winner[4] == 0 && wa.tour.winner[5] == 0 && wa.tour.winner[6] == 0,
        "candidate 0 wins several slots");
  CHECK(g, wa.tour.win_count[0] == 5, "candidate 0 has five descendants");
  CHECK(g, ra.dup_entry_tournaments == 32, "every tournament here has a duplicate entry");
  CHECK(g, std::memcmp(wa.tour.winner, wb.tour.winner, kPop) == 0, "winners invariant to label complement");
  // Strict order primitive.
  u64 loss[kCandidates] = {0}, key[kCandidates] = {0};
  loss[5] = 1;
  loss[6] = 1;
  key[5] = 9;
  key[6] = 3;
  CHECK(g, candidate_precedes(0, 5, loss, key) && candidate_precedes(6, 5, loss, key) &&
               candidate_precedes(1, 2, loss, key) && !candidate_precedes(2, 1, loss, key),
        "loss, then key, then index");
  return g;
}

// ---------------------------------------------------------------- group 13
const u64 kRankN[128] = {
    8290815, 8097265, 7906751, 7719249, 7534735, 7353185, 7174575, 6998881, 6826079, 6656145,
    6489055, 6324785, 6163311, 6004609, 5848655, 5695425, 5544895, 5397041, 5251839, 5109265,
    4969295, 4831905, 4697071, 4564769, 4434975, 4307665, 4182815, 4060401, 3940399, 3822785,
    3707535, 3594625, 3484031, 3375729, 3269695, 3165905, 3064335, 2964961, 2867759, 2772705,
    2679775, 2588945, 2500191, 2413489, 2328815, 2246145, 2165455, 2086721, 2009919, 1935025,
    1862015, 1790865, 1721551, 1654049, 1588335, 1524385, 1462175, 1401681, 1342879, 1285745,
    1230255, 1176385, 1124111, 1073409, 1024255, 976625,  930495,  885841,  842639,  800865,
    760495,  721505,  683871,  647569,  612575,  578865,  546415,  515201,  485199,  456385,
    428735,  402225,  376831,  352529,  329295,  307105,  285935,  265761,  246559,  228305,
    210975,  194545,  178991,  164289,  150415,  137345,  125055,  113521,  102719,  92625,
    83215,   74465,   66351,   58849,   51935,   45585,   39775,   34481,   29679,   25345,
    21455,   17985,   14911,   12209,   9855,    7825,    6095,    4641,    3439,    2465,
    1695,    1105,    671,     369,     175,     65,      15,      1};

Group g13_rank_table() {
  Group g{13, "exact four-entry tournament rank table, all 128 ranks, sum p_r = 1"};
  u64 sum = 0;
  for (int r = 1; r <= 128; ++r) {
    const u64 a = u64(129 - r), b = u64(128 - r);
    const u64 n = a * a * a * a - b * b * b * b;
    CHECK(g, n == kRankN[r - 1], "formula vs Appendix A at rank " + std::to_string(r));
    sum += n;
  }
  CHECK(g, sum == (1ULL << 28), "sum N_r = 2^28 (sum p_r = 1)");
  // Exhaustive enumeration of all 128^4 entry tuples through the production
  // order primitive, with candidate c at rank c+1.
  u64 loss[kCandidates], key[kCandidates];
  for (int c = 0; c < kCandidates; ++c) {
    loss[c] = u64(c);
    key[c] = 0;
  }
  std::vector<u64> wins(kCandidates, 0);
  for (int e0 = 0; e0 < 128; ++e0)
    for (int e1 = 0; e1 < 128; ++e1) {
      const int w01 = candidate_precedes(e1, e0, loss, key) ? e1 : e0;
      for (int e2 = 0; e2 < 128; ++e2) {
        const int w012 = candidate_precedes(e2, w01, loss, key) ? e2 : w01;
        for (int e3 = 0; e3 < 128; ++e3) ++wins[candidate_precedes(e3, w012, loss, key) ? e3 : w012];
      }
    }
  for (int c = 0; c < kCandidates; ++c) CHECK(g, wins[c] == kRankN[c], "enumerated wins at rank " + std::to_string(c + 1));
  const long double p1 = (long double)kRankN[0] / (long double)(1ULL << 28);
  CHECK(g, std::fabs((double)(32.0L * p1) - 0.988342165946960) < 1e-12, "best-candidate expected offspring");
  CHECK(g, std::fabs((double)std::pow(1.0L - p1, 32.0L) - 0.366437715922037) < 1e-12,
        "best-candidate zero-offspring probability");
  return g;
}

// ---------------------------------------------------------------- group 14
Group g14_sham_paths(const KeySet& fx) {
  Group g{14, "full 256-update SHAM paths from ALL_F, ALL_M and mixed labels: N1 and N2"};
  std::unique_ptr<BlockResult> r(new BlockResult);
  RunOptions opt;
  run_block(fx, 11, opt, *r);
  CHECK(g, r->block.flags == kFlagAllValid, "block flags N1/N2/sum/query all set");
  CHECK(g, r->paths[4].final_state_sha256 != r->paths[5].final_state_sha256 ||
               r->paths[4].label_sha256 != r->paths[5].label_sha256,
        "SHAM starts differ in labels");
  CHECK(g, r->paths[6].late_m_sum + r->paths[7].late_m_sum == 2048, "HALF start-averaged late frequency exactly 1/2");
  CHECK(g, r->paths[4].late_m_sum + r->paths[5].late_m_sum == 2048, "ZERO start-averaged late frequency exactly 1/2");
  for (int t = 0; t < kUpdates; ++t) {
    CHECK(g, r->updates[6][t].pop_loss == r->updates[7][t].pop_loss, "N1 per-update loss identity");
    CHECK(g, r->updates[6][t].m_count + r->updates[7][t].m_count == 32, "N2 per-update complement");
  }
  // Mixed label assignment in cell 6 and its complement.
  std::unique_ptr<BlockResult> m(new BlockResult), mc(new BlockResult);
  RunOptions om, omc;
  om.only_cell = 6;
  omc.only_cell = 6;
  om.label_override_cell = 6;
  omc.label_override_cell = 6;
  for (int i = 0; i < kPop; ++i) {
    om.label_override[i] = u8((i * 7 + 3) % 5 < 2 ? LABEL_M : LABEL_F);
    omc.label_override[i] = u8(om.label_override[i] ^ 1u);
  }
  run_block(fx, 11, om, *m);
  run_block(fx, 11, omc, *mc);
  CHECK(g, m->paths[6].label_blind_sha256 == r->paths[6].label_blind_sha256, "N1 holds for a mixed assignment");
  CHECK(g, mc->paths[6].label_blind_sha256 == r->paths[6].label_blind_sha256, "N1 holds for the complement");
  CHECK(g, m->paths[6].label_sha256 == mc->paths[6].complement_label_sha256, "N2 complement for mixed assignment");
  CHECK(g, m->paths[6].late_m_sum + mc->paths[6].late_m_sum == 2048, "mixed + complement late sum 2048");
  return g;
}

// ---------------------------------------------------------------- group 15
Group g15_random_invariance(const KeySet& fx, int threads) {
  Group g{15, "random-coordinate invariance to labels, arm, law, start, cell order and thread count"};
  std::unique_ptr<UpdateTape> a(new UpdateTape), b(new UpdateTape);
  generate_update_tape(fx, 9, 17, *a);
  {
    std::unique_ptr<BlockResult> tmp(new BlockResult);
    RunOptions opt;
    run_block(fx, 9, opt, *tmp);  // no hidden generator state
  }
  generate_update_tape(fx, 9, 17, *b);
  CHECK(g, std::memcmp(a.get(), b.get(), sizeof(UpdateTape)) == 0, "tape is a pure function of (block, update)");
  // Same tape, different labels / arms: every random-coordinate family agrees.
  auto f = uniform_state(3000, LABEL_F);
  auto mm = uniform_state(3000, LABEL_M);
  StepWork wf, wm, wa;
  step_cell(*f, SHAM, 17, kZeroTarget, *a, wf);
  step_cell(*mm, SHAM, 17, kZeroTarget, *a, wm);
  CHECK(g, std::memcmp(wf.cand.phen, wm.cand.phen, sizeof wf.cand.phen) == 0, "SHAM candidates label-invariant");
  auto act = uniform_state(3000, LABEL_M);
  step_cell(*act, ACTIVE, 17, kZeroTarget, *a, wa);
  CHECK(g, std::memcmp(wf.cand.phen[64], wa.cand.phen[64], sizeof(u16) * kLoci * kPop) == 0, "scouts arm-invariant");
  CHECK(g, std::memcmp(wf.tour.entry, wa.tour.entry, sizeof wf.tour.entry) == 0, "tournament entries arm-invariant");
  const AuditOutcome v = run_invariance_check(fx, threads > 2 ? threads - 1 : 3);
  CHECK(g, v.ok, "order / thread invariance: " + v.json);
  return g;
}

// ---------------------------------------------------------------- group 16
Group g16_regeneration(const KeySet& fx) {
  Group g{16, "regeneration of a non-audit path from block ID to bit-identical saved summaries"};
  const u32 block = 70;  // chunk 1, outside the audit set
  std::unique_ptr<BlockResult> full(new BlockResult), one(new BlockResult);
  RunOptions opt;
  run_block(fx, block, opt, *full);
  for (int c = 0; c < kCells; ++c) {
    RunOptions oc;
    oc.only_cell = c;
    run_block(fx, block, oc, *one);
    ByteBuf a, b, pa, pb;
    for (int t = 0; t < kUpdates; ++t) {
      serialize_update(full->updates[c][t], a);
      serialize_update(one->updates[c][t], b);
    }
    serialize_path(full->paths[c], pa);
    serialize_path(one->paths[c], pb);
    CHECK(g, a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size()) == 0,
          "update rows regenerate identically, cell " + std::to_string(c));
    CHECK(g, pa.size() == pb.size() && std::memcmp(pa.data(), pb.data(), pa.size()) == 0,
          "path summary regenerates identically, cell " + std::to_string(c));
  }
  return g;
}

}  // namespace

int main(int argc, char** argv) {
  const Args a(argc, argv, {"package-root", "config", "expect-config-sha256", "threads", "out"});
  const std::string root = a.req("package-root");
  const std::string out = a.req("out");
  const u64 threads = parse_u64_strict(a.req("threads"), "--threads");
  TORUS_REQUIRE(threads >= 2 && threads <= 32, "--threads must be 2..32 (thread-count comparison)");
  TORUS_REQUIRE(!path_exists(out) || (is_directory(out) && list_directory_sorted(out).empty()),
                "refusing existing nonempty output: " + out);
  run_startup_self_tests();
  verify_random123(root);
  const std::string cfg_sha = verify_frozen_config(a.req("config"));
  TORUS_REQUIRE(cfg_sha == a.req("expect-config-sha256"), "config SHA-256 does not match --expect-config-sha256");

  const KeySet fx = derive_keys(kFixtureNamespace);
  std::vector<Group> gs;
  gs.push_back(g01_kat(root));
  gs.push_back(g02_collision(fx, int(threads)));
  gs.push_back(g03_wrap());
  gs.push_back(g04_max_loss());
  gs.push_back(g05_hand_trace());
  gs.push_back(g06_target(fx));
  gs.push_back(g07_memory_better_worse());
  gs.push_back(g08_duplicate());
  gs.push_back(g09_invalid_cache());
  gs.push_back(g10_donor_tie());
  gs.push_back(g11_local_child());
  gs.push_back(g12_tournament());
  gs.push_back(g13_rank_table());
  gs.push_back(g14_sham_paths(fx));
  gs.push_back(g15_random_invariance(fx, int(threads)));
  gs.push_back(g16_regeneration(fx));

  bool all = true;
  std::ostringstream js;
  js << "{\n  \"schema\": \"PHASE2-TORUS-MEMORY-003-FIXTURE-RECEIPT-v1\",\n"
     << "  \"namespace\": \"" << kFixtureNamespace << "\",\n"
     << "  \"non_production\": true,\n"
     << "  \"config_sha256\": \"" << cfg_sha << "\",\n"
     << "  \"threads\": " << threads << ",\n  \"groups\": [";
  for (std::size_t i = 0; i < gs.size(); ++i) {
    all = all && gs[i].pass;
    js << (i ? "," : "") << "\n    {\"group\": " << gs[i].id << ", \"name\": \"" << json_escape(gs[i].name)
       << "\", \"result\": \"" << (gs[i].pass ? "PASS" : "FAIL") << "\", \"notes\": [";
    for (std::size_t k = 0; k < gs[i].notes.size(); ++k)
      js << (k ? ", " : "") << "\"" << json_escape(gs[i].notes[k]) << "\"";
    js << "]}";
  }
  js << "\n  ],\n  \"status\": \"" << (all && gs.size() == 16 ? "ALL_FIXTURES_PASSED" : "FIXTURES_FAILED") << "\"\n}\n";
  prepare_output_dir(out);
  write_new_text_file(join_path(out, "fixture_receipt.json"), js.str());
  write_new_text_file(join_path(out, "fixture_key_derivation.json"), key_derivation_json(fx));
  std::printf("%s\n", all ? "ALL_FIXTURES_PASSED" : "FIXTURES_FAILED");
  return all ? 0 : 1;
}
