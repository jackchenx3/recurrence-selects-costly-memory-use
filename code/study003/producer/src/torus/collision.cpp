#include "collision.hpp"

#include <algorithm>
#include <memory>
#include <sstream>

#include "chunk_io.hpp"
#include "tape.hpp"
#include "target_law.hpp"
#include "util.hpp"

namespace torus {

namespace {

void add(std::vector<DrawAddress>& v, int p, u64 b, u64 t, u64 e, u64 s) {
  DrawAddress a;
  a.purpose = u8(p);
  a.ctr[0] = b;
  a.ctr[1] = t;
  a.ctr[2] = e;
  a.ctr[3] = s;
  v.push_back(a);
}

}  // namespace

void enumerate_declared_addresses(u32 block, std::vector<DrawAddress>& out) {
  out.clear();
  const u64 b = block;
  // INITIAL_VECTOR (block,0,parent,q)
  for (u64 p = 0; p < 32; ++p)
    for (u64 q = 0; q < 2; ++q) add(out, P_INITIAL_VECTOR, b, 0, p, q);
  for (u64 t = 1; t <= 256; ++t) {
    for (u64 q = 0; q < 2; ++q) add(out, P_TARGET_INNOVATION_VECTOR, b, t, 0, q);
    add(out, P_TARGET_COPY, b, t, 0, 0);  // generated at every t, including unused t = 1, 2
    const int vec[4] = {P_FRESH_VECTOR, P_SCOUT_VECTOR, P_LOCAL_REPLACE_FLAG, P_LOCAL_REPLACE_VALUE};
    for (int k = 0; k < 4; ++k)
      for (u64 p = 0; p < 32; ++p)
        for (u64 q = 0; q < 2; ++q) add(out, vec[k], b, t, p, q);
    for (u64 p = 0; p < 32; ++p)
      for (u64 f = 0; f < 3; ++f) add(out, P_DONOR_KEY, b, t, 3 * p + f, 0);
    for (u64 s = 0; s < 32; ++s)
      for (u64 e = 0; e < 4; ++e) add(out, P_TOURNAMENT_ENTRY, b, t, s, e);
    for (u64 c = 0; c < 128; ++c) add(out, P_CANDIDATE_TIE_KEY, b, t, c, 0);
    for (u64 s = 0; s < 32; ++s) add(out, P_POLICY_MUTATION, b, t, s, 0);
  }
}

AuditOutcome run_collision_audit(const KeySet& ks) {
  AuditOutcome res;
  std::vector<std::string> failures;
  auto fail = [&](const std::string& m) { failures.push_back(m); };

  // (1)+(2) Declared domain and within-block uniqueness, block 0 reference.
  std::vector<DrawAddress> ref;
  enumerate_declared_addresses(0, ref);
  if (ref.size() != kCallsPerBlock) fail("declared address count per block != 164672");
  u64 per_purpose[kPurposes] = {0};
  for (const DrawAddress& a : ref) {
    check_address_domain(a.purpose, a.ctr[0], a.ctr[1], a.ctr[2], a.ctr[3]);  // aborts if outside
    ++per_purpose[a.purpose];
  }
  const u64 expect_pp[kPurposes] = {64, 512, 256, 16384, 16384, 16384, 16384, 24576, 32768, 32768, 8192};
  for (int p = 0; p < kPurposes; ++p)
    if (per_purpose[p] != expect_pp[p]) fail(std::string("declared count mismatch for ") + purpose_name(p));
  std::vector<DrawAddress> sorted = ref;
  std::sort(sorted.begin(), sorted.end());
  if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) fail("duplicate address within a block");

  // (3) Every block: word 0 == block, words 1..3 and purpose identical to block 0.
  std::vector<DrawAddress> cur;
  u64 total = 0;
  for (u32 b = 0; b < kBlocks; ++b) {
    enumerate_declared_addresses(b, cur);
    if (cur.size() != ref.size()) {
      fail("declared size differs across blocks");
      break;
    }
    bool ok = true;
    for (std::size_t i = 0; i < cur.size(); ++i) {
      const DrawAddress& x = cur[i];
      const DrawAddress& y = ref[i];
      if (x.ctr[0] != b || x.purpose != y.purpose || x.ctr[1] != y.ctr[1] || x.ctr[2] != y.ctr[2] ||
          x.ctr[3] != y.ctr[3]) {
        ok = false;
        break;
      }
    }
    if (!ok) {
      fail("block-structure mismatch at block " + std::to_string(b));
      break;
    }
    total += cur.size();
  }
  if (total != kCallsTotal) fail("total declared addresses != 6850355200");

  // (4) Actual generator call set == declared set, for sample blocks.
  const u32 sample[] = {0, 1, 63, 64, 20800, 41599};
  std::unique_ptr<BlockTargets> tg(new BlockTargets);
  std::unique_ptr<UpdateTape> tape(new UpdateTape);
  for (u32 b : sample) {
    std::vector<DrawAddress> rec;
    rec.reserve(kCallsPerBlock);
    set_thread_draw_recorder(&rec);
    u16 init[kPop][kLoci];
    generate_initial_phenotypes(ks, b, init);
    generate_block_targets(ks, b, *tg);
    for (u32 t = 1; t <= u32(kUpdates); ++t) generate_update_tape(ks, b, t, *tape);
    set_thread_draw_recorder(nullptr);
    std::vector<DrawAddress> decl;
    enumerate_declared_addresses(b, decl);
    std::sort(rec.begin(), rec.end());
    std::sort(decl.begin(), decl.end());
    if (std::adjacent_find(rec.begin(), rec.end()) != rec.end()) fail("generator repeats an address");
    if (!(rec == decl)) fail("generator call set differs from declared schema at block " + std::to_string(b));
  }

  // (5) Vector extraction bijection: coordinate 16q + 4*word + lane.
  {
    bool seen[kLoci] = {false};
    bool ok = true;
    for (int q = 0; q < 2; ++q)
      for (int w = 0; w < 4; ++w)
        for (int l = 0; l < 4; ++l) {
          const int c = 16 * q + 4 * w + l;
          if (c < 0 || c >= kLoci || seen[c]) ok = false;
          else seen[c] = true;
        }
    for (bool s : seen) ok = ok && s;
    if (!ok) fail("vector extraction is not a bijection");
  }

  // (6) Keys pairwise distinct across all namespaces and purposes.
  {
    std::vector<Word4> keys;
    const char* nss[3] = {kProductionNamespace, kFixtureNamespace, kTimingNamespace};
    for (const char* ns : nss) {
      KeySet k = derive_keys(ns);
      for (int p = 0; p < kPurposes; ++p) keys.push_back(k.key[p]);
    }
    std::sort(keys.begin(), keys.end());
    if (std::adjacent_find(keys.begin(), keys.end()) != keys.end()) fail("purpose key collision across namespaces");
  }

  res.ok = failures.empty();
  std::ostringstream js;
  js << "{\n  \"schema\": \"PHASE2-TORUS-MEMORY-003-COLLISION-RECEIPT-v1\",\n";
  js << "  \"namespace\": \"" << json_escape(ks.ns) << "\",\n";
  js << "  \"declared_addresses_per_block\": " << ref.size() << ",\n";
  js << "  \"declared_addresses_total\": " << total << ",\n";
  js << "  \"blocks_checked\": " << kBlocks << ",\n";
  js << "  \"recorded_generator_blocks\": [0, 1, 63, 64, 20800, 41599],\n";
  js << "  \"per_purpose_per_block\": {";
  for (int p = 0; p < kPurposes; ++p) js << (p ? ", " : "") << "\"" << purpose_name(p) << "\": " << per_purpose[p];
  js << "},\n  \"failures\": [";
  for (std::size_t i = 0; i < failures.size(); ++i) js << (i ? ", " : "") << "\"" << json_escape(failures[i]) << "\"";
  js << "],\n  \"status\": \"" << (res.ok ? "NO_COLLISION" : "COLLISION_AUDIT_FAILED") << "\"\n}\n";
  res.json = js.str();
  return res;
}

AuditOutcome run_invariance_check(const KeySet& ks, int threads) {
  TORUS_REQUIRE(ks.ns != kProductionNamespace, "invariance check must not use production keys");
  AuditOutcome res;
  std::vector<std::string> failures;
  const u32 blocks_arr[] = {0, 1, 2, 3, 64, 65, 1000, 41599};
  std::vector<u32> blocks(blocks_arr, blocks_arr + 8);

  // Cell evaluation order and per-cell tape regeneration.
  std::unique_ptr<BlockResult> a(new BlockResult), b(new BlockResult), c(new BlockResult);
  for (u32 blk : {0u, 65u}) {
    RunOptions canon;
    RunOptions rev;
    for (int k = 0; k < kCells; ++k) rev.cell_order[k] = kCells - 1 - k;
    RunOptions per;
    per.tape_per_cell = true;
    const int odd[kCells] = {5, 2, 7, 0, 3, 6, 1, 4};
    for (int k = 0; k < kCells; ++k) per.cell_order[k] = odd[k];
    run_block(ks, blk, canon, *a);
    run_block(ks, blk, rev, *b);
    run_block(ks, blk, per, *c);
    ByteBuf ua, pa, ba, ub, pb, bb, uc, pc, bc;
    append_block_result(*a, ua, pa, ba);
    append_block_result(*b, ub, pb, bb);
    append_block_result(*c, uc, pc, bc);
    auto same = [](const ByteBuf& x, const ByteBuf& y) {
      return x.size() == y.size() && std::equal(x.data(), x.data() + x.size(), y.data());
    };
    if (!same(ua, ub) || !same(pa, pb) || !same(ba, bb)) failures.push_back("cell-order dependence at block " + std::to_string(blk));
    if (!same(ua, uc) || !same(pa, pc) || !same(ba, bc)) failures.push_back("per-cell tape dependence at block " + std::to_string(blk));
  }

  // Thread count.
  std::vector<std::vector<u8>> one, many;
  run_blocks_in_memory(ks, blocks, 1, one);
  run_blocks_in_memory(ks, blocks, threads, many);
  if (one != many) failures.push_back("thread-count dependence");

  res.ok = failures.empty();
  std::ostringstream js;
  js << "{\n  \"schema\": \"PHASE2-TORUS-MEMORY-003-INVARIANCE-RECEIPT-v1\",\n";
  js << "  \"namespace\": \"" << json_escape(ks.ns) << "\",\n";
  js << "  \"blocks\": [0, 1, 2, 3, 64, 65, 1000, 41599],\n";
  js << "  \"threads_compared\": [1, " << threads << "],\n";
  js << "  \"orders_compared\": [\"canonical\", \"reversed\", \"5,2,7,0,3,6,1,4 with per-cell tape\"],\n";
  js << "  \"failures\": [";
  for (std::size_t i = 0; i < failures.size(); ++i) js << (i ? ", " : "") << "\"" << json_escape(failures[i]) << "\"";
  js << "],\n  \"status\": \"" << (res.ok ? "INVARIANT" : "INVARIANCE_FAILED") << "\"\n}\n";
  res.json = js.str();
  return res;
}

}  // namespace torus
