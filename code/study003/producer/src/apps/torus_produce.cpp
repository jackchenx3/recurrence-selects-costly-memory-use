// PHASE2-TORUS-MEMORY-003 producer.
//
// --mode production : the single fixed study (41,600 blocks, 650 chunks) in
//                     namespace production-r1. Requires the exact design,
//                     design-GO record, frozen config + expected SHA-256,
//                     pinned Random123 files and a separate execution
//                     authorization record. Fails closed before any path if
//                     self-tests, KATs, identities, collision audit or
//                     invariance check do not pass.
// --mode timing     : fixture-only timing in namespace timing-r1. Requires a
//                     passing fixture receipt. Writes only a timing receipt;
//                     no scientific record or estimate is written.
//
// Every output path is explicit; existing nonempty output is refused.

#include <chrono>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "../torus/chunk_io.hpp"
#include "../torus/cli.hpp"
#include "../torus/collision.hpp"
#include "../torus/config_check.hpp"
#include "../torus/manifest.hpp"
#include "../torus/threefry_rng.hpp"
#include "../torus/util.hpp"

using namespace torus;

namespace {

int run_production(const Args& a) {
  a.forbid("timing-block-begin", "in production mode");
  a.forbid("timing-block-end", "in production mode");
  a.forbid("fixture-receipt", "in production mode");
  const std::string root = a.req("package-root");
  const std::string out = a.req("out");
  const std::string ns = a.req("namespace");
  TORUS_REQUIRE(ns == kProductionNamespace, "production mode requires --namespace production-r1");
  const std::string expect_cfg = a.req("expect-config-sha256");
  TORUS_REQUIRE(is_sha256_hex(expect_cfg), "--expect-config-sha256 must be 64 lowercase hex characters");
  const u64 threads = parse_u64_strict(a.req("threads"), "--threads");
  TORUS_REQUIRE(threads >= 1 && threads <= 32, "--threads must be 1..32");
  TORUS_REQUIRE(!path_exists(out) || (is_directory(out) && list_directory_sorted(out).empty()),
                "refusing existing nonempty output: " + out);

  // ---- fail-closed identity gate (no path is simulated before this passes) ----
  run_startup_self_tests();
  verify_random123(root);
  const std::string cfg_sha = verify_frozen_config(a.req("config"));
  TORUS_REQUIRE(cfg_sha == expect_cfg, "config SHA-256 does not match --expect-config-sha256");
  verify_design_spec(a.req("design-spec"));
  verify_design_go_record(a.req("design-go-record"));
  const std::string auth_sha = verify_execution_authorization(a.req("execution-authorization"), cfg_sha);

  const KeySet ks = derive_keys(kProductionNamespace);
  const AuditOutcome coll = run_collision_audit(ks);
  TORUS_REQUIRE(coll.ok, "collision audit failed:\n" + coll.json);
  const KeySet fx = derive_keys(kFixtureNamespace);
  const AuditOutcome inv = run_invariance_check(fx, int(threads));
  TORUS_REQUIRE(inv.ok, "invariance check failed:\n" + inv.json);

  // ---- outputs ----
  prepare_output_dir(out);
  for (const char* d : {"preflight", "updates", "paths", "blocks", "audit"}) make_subdir(join_path(out, d));
  write_new_text_file(join_path(out, "preflight/key_derivation.json"), key_derivation_json(ks));
  write_new_text_file(join_path(out, "preflight/collision_receipt.json"), coll.json);
  write_new_text_file(join_path(out, "preflight/invariance_receipt.json"), inv.json);
  {
    std::ostringstream js;
    js << "{\n  \"schema\": \"PHASE2-TORUS-MEMORY-003-IDENTITY-v1\",\n"
       << "  \"namespace\": \"" << kProductionNamespace << "\",\n"
       << "  \"design_sha256\": \"" << kDesignSha256 << "\",\n"
       << "  \"design_go_record_sha256\": \"" << kDesignGoSha256 << "\",\n"
       << "  \"terminal_review_sha256\": \"" << kTerminalReviewSha256 << "\",\n"
       << "  \"seed_receipt_sha256\": \"" << kSeedReceiptSha256 << "\",\n"
       << "  \"config_sha256\": \"" << cfg_sha << "\",\n"
       << "  \"execution_authorization_sha256\": \"" << auth_sha << "\",\n"
       << "  \"random123_commit\": \"" << kRandom123Commit << "\",\n"
       << "  \"random123_threefry_h_sha256\": \"" << kThreefryHeaderSha256 << "\",\n"
       << "  \"random123_kat_vectors_sha256\": \"" << kKatVectorsSha256 << "\",\n"
       << "  \"self_tests\": \"SHA256_AND_THREEFRY_KAT_PASSED\",\n"
       << "  \"threads\": " << threads << "\n}\n";
    write_new_text_file(join_path(out, "preflight/identity.json"), js.str());
  }

  const auto t0 = std::chrono::steady_clock::now();
  std::vector<u32> chunks;
  for (u32 c = 0; c < kChunks; ++c) chunks.push_back(c);
  run_chunks_threaded(ks, chunks, out, int(threads), true);
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

  const auto files = build_manifest(out, {"preflight", "updates", "paths", "blocks", "audit"});
  TORUS_REQUIRE(files.size() == 4 + 3 * kChunks + 3, "unexpected production file count");
  std::ostringstream js;
  js << "{\n  \"schema\": \"PHASE2-TORUS-MEMORY-003-PRODUCTION-MANIFEST-v1\",\n"
     << "  \"namespace\": \"" << kProductionNamespace << "\",\n"
     << "  \"design_sha256\": \"" << kDesignSha256 << "\",\n"
     << "  \"design_go_record_sha256\": \"" << kDesignGoSha256 << "\",\n"
     << "  \"config_sha256\": \"" << cfg_sha << "\",\n"
     << "  \"execution_authorization_sha256\": \"" << auth_sha << "\",\n"
     << "  \"blocks\": " << kBlocks << ",\n  \"chunks\": " << kChunks << ",\n  \"paths\": " << kPaths
     << ",\n  \"path_updates\": " << kPathUpdates << ",\n  \"objective_queries\": " << kObjectiveQueries
     << ",\n  \"audit_candidate_rows\": " << kAuditCandidateRows << ",\n  \"audit_entry_rows\": " << kAuditEntryRows
     << ",\n  \"wall_seconds_simulation\": " << secs << ",\n  \"files\": " << manifest_json_array(files, "  ")
     << "\n}\n";
  const std::string manifest = js.str();
  write_new_text_file(join_path(out, "production_manifest.json"), manifest);
  const std::string msha = digest_hex(sha256_string(manifest));
  write_new_text_file(join_path(out, "PRODUCTION_COMPLETE.json"),
                      "{\n  \"status\": \"PRODUCTION_FILES_COMPLETE_UNANALYZED\",\n  \"production_manifest_sha256\": \"" +
                          msha + "\"\n}\n");
  std::printf("production files complete; manifest sha256 %s\n", msha.c_str());
  return 0;
}

int run_timing(const Args& a) {
  for (const char* k : {"design-spec", "design-go-record", "execution-authorization"})
    a.forbid(k, "in timing mode");
  const std::string root = a.req("package-root");
  const std::string out = a.req("out");
  TORUS_REQUIRE(a.req("namespace") == kTimingNamespace, "timing mode requires --namespace timing-r1");
  const u64 threads = parse_u64_strict(a.req("threads"), "--threads");
  TORUS_REQUIRE(threads >= 1 && threads <= 32, "--threads must be 1..32");
  const u64 b0 = parse_u64_strict(a.req("timing-block-begin"), "--timing-block-begin");
  const u64 b1 = parse_u64_strict(a.req("timing-block-end"), "--timing-block-end");
  TORUS_REQUIRE(b0 < b1 && b1 <= kBlocks, "timing block range must satisfy begin < end <= 41600");
  TORUS_REQUIRE(!path_exists(out) || (is_directory(out) && list_directory_sorted(out).empty()),
                "refusing existing nonempty output: " + out);
  const std::string expect_cfg = a.req("expect-config-sha256");

  run_startup_self_tests();
  verify_random123(root);
  const std::string cfg_sha = verify_frozen_config(a.req("config"));
  TORUS_REQUIRE(cfg_sha == expect_cfg, "config SHA-256 does not match --expect-config-sha256");
  const std::string fixture_receipt = read_text_file(a.req("fixture-receipt"));
  TORUS_REQUIRE(fixture_receipt.find("\"status\": \"ALL_FIXTURES_PASSED\"") != std::string::npos,
                "timing requires a fixture receipt with status ALL_FIXTURES_PASSED");
  TORUS_REQUIRE(fixture_receipt.find("\"config_sha256\": \"" + cfg_sha + "\"") != std::string::npos,
                "fixture receipt does not bind this configuration");

  const KeySet ks = derive_keys(kTimingNamespace);
  prepare_output_dir(out);
  std::vector<u32> blocks;
  for (u64 b = b0; b < b1; ++b) blocks.push_back(u32(b));
  const auto t0 = std::chrono::steady_clock::now();
  {
    // Results are held in memory and discarded: no scientific record is written.
    std::vector<std::vector<u8>> discard;
    run_blocks_in_memory(ks, blocks, int(threads), discard);
    volatile std::size_t sink = 0;
    for (const auto& v : discard) sink = sink + v.size();
    (void)sink;
  }
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  const u64 nb = b1 - b0;
  std::ostringstream js;
  js << "{\n  \"schema\": \"PHASE2-TORUS-MEMORY-003-TIMING-v1\",\n"
     << "  \"namespace\": \"" << kTimingNamespace << "\",\n"
     << "  \"non_production\": true,\n"
     << "  \"config_sha256\": \"" << cfg_sha << "\",\n"
     << "  \"blocks_timed\": " << nb << ",\n  \"threads\": " << threads << ",\n"
     << "  \"path_updates\": " << nb * kCells * kUpdates << ",\n"
     << "  \"objective_queries\": " << nb * kCells * kUpdates * kCandidates << ",\n"
     << "  \"wall_seconds_in_memory_no_io\": " << secs << ",\n"
     << "  \"note\": \"Timing only. Excludes production file I/O, audit-row writing, the full collision audit and manifest hashing. No scientific record or estimate is produced.\"\n}\n";
  write_new_text_file(join_path(out, "timing_receipt.json"), js.str());
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const Args a(argc, argv,
               {"mode", "namespace", "package-root", "config", "expect-config-sha256", "design-spec",
                "design-go-record", "execution-authorization", "threads", "out", "timing-block-begin",
                "timing-block-end", "fixture-receipt"});
  const std::string mode = a.req("mode");
  if (mode == "production") return run_production(a);
  if (mode == "timing") return run_timing(a);
  fatal("--mode must be production or timing");
}
