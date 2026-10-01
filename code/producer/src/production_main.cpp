// mmem_production: the single frozen production run. NOT AUTHORIZED until a
// separate review grants execution authority.
//
// mmem_production --production <ACK> --config config/frozen_config.json
//                 --spec SPEC.md --review REVIEW.md --out NEW_DIR --threads N
//
// Refuses: missing/incorrect acknowledgement, any config mismatch, spec/review
// hash mismatch, threads outside 1..32, an existing output directory, or an
// output directory inside the source package. Block count is compiled in and
// cannot be changed; nothing depends on timing. Blocks are partitioned into 32
// fixed shards (shard = block / 1300) so output bytes do not depend on N.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "mm/block_runner.hpp"
#include "mm/collision_audit.hpp"
#include "mm/config_check.hpp"
#include "mm/constants.hpp"
#include "mm/fixtures.hpp"
#include "mm/io.hpp"
#include "mm/keys.hpp"
#include "mm/records.hpp"
#include "mm/sha256.hpp"

namespace fs = std::filesystem;

namespace {

struct Args {
  std::string production, config, spec, review, out, threads;
};

[[noreturn]] void refuse(const std::string& why, int code) {
  std::fprintf(stderr, "mmem_production REFUSED: %s\n", why.c_str());
  std::fprintf(stderr,
               "usage: mmem_production --production %s --config FROZEN_CONFIG.json --spec SPEC.md --review REVIEW.md "
               "--out NEW_OUTPUT_DIR --threads N(1..32)\n",
               mm::kProductionAck);
  std::exit(code);
}

Args parse_args(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    const std::string k = argv[i];
    if (i + 1 >= argc) refuse("missing value for " + k, 64);
    const std::string v = argv[++i];
    std::string* slot = nullptr;
    if (k == "--production") slot = &a.production;
    else if (k == "--config") slot = &a.config;
    else if (k == "--spec") slot = &a.spec;
    else if (k == "--review") slot = &a.review;
    else if (k == "--out") slot = &a.out;
    else if (k == "--threads") slot = &a.threads;
    else refuse("unknown argument " + k, 64);
    if (!slot->empty()) refuse("duplicate argument " + k, 64);
    *slot = v;
  }
  if (a.production != mm::kProductionAck) refuse("explicit --production acknowledgement missing or incorrect", 64);
  if (a.config.empty() || a.spec.empty() || a.review.empty() || a.out.empty() || a.threads.empty())
    refuse("all arguments are required", 64);
  return a;
}

std::string utc_now() {
  const std::time_t now = std::time(nullptr);
  char buf[32];
  std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
  return buf;
}

bool is_within(const fs::path& p, const fs::path& root) {
  auto pi = p.begin();
  for (auto ri = root.begin(); ri != root.end(); ++ri, ++pi) {
    if (ri->empty()) continue;
    if (pi == p.end() || *pi != *ri) return false;
  }
  return true;
}

struct ShardResult {
  std::uint64_t queries = 0U;
  std::uint64_t n1_failed_blocks = 0U;
  std::uint64_t n2_failed_blocks = 0U;
  std::uint64_t query_failed_blocks = 0U;
};

std::string shard_name(std::uint32_t shard) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "shard_%02u", static_cast<unsigned>(shard));
  return buf;
}

void run_shard(const mm::KeySet& keys, std::uint32_t shard, const fs::path& shards_dir, ShardResult& res) {
  const fs::path dir = shards_dir / shard_name(shard);
  std::error_code ec;
  if (!fs::create_directory(dir, ec) || ec) mm::fatal("cannot create shard directory");
  const std::string u = (dir / "updates.bin").string(), p = (dir / "paths.bin").string(), b = (dir / "blocks.bin").string();
  const std::string au = (dir / "audit_rows_blocks_0000_0063.bin").string();
  std::FILE* fu = mm::open_new_file(u + ".partial");
  std::FILE* fp = mm::open_new_file(p + ".partial");
  std::FILE* fb = mm::open_new_file(b + ".partial");
  std::FILE* fa = shard == 0U ? mm::open_new_file(au + ".partial") : nullptr;
  mm::BlockOutput out;
  const std::uint32_t first = shard * mm::kBlocksPerShard;
  for (std::uint32_t block = first; block < first + mm::kBlocksPerShard; ++block) {
    const bool audit = block < mm::kAuditBlocks;
    mm::run_block(keys, block, audit, out);
    mm::write_all(fu, out.updates.data(), out.updates.size());
    mm::write_all(fp, out.paths.data(), out.paths.size());
    mm::write_all(fb, out.block.data(), out.block.size());
    if (audit) mm::write_all(fa, out.audit.data(), out.audit.size());
    res.queries += out.queries;
    if (out.summary.n1_ok == 0U) ++res.n1_failed_blocks;
    if (out.summary.n2_ok == 0U) ++res.n2_failed_blocks;
    if (out.summary.query_ok == 0U) ++res.query_failed_blocks;
  }
  mm::close_file(fu);
  mm::close_file(fp);
  mm::close_file(fb);
  if (fa != nullptr) mm::close_file(fa);
  fs::rename(u + ".partial", u, ec);
  if (ec) mm::fatal("rename failed");
  fs::rename(p + ".partial", p, ec);
  if (ec) mm::fatal("rename failed");
  fs::rename(b + ".partial", b, ec);
  if (ec) mm::fatal("rename failed");
  if (fa != nullptr) {
    fs::rename(au + ".partial", au, ec);
    if (ec) mm::fatal("rename failed");
  }
}

}  // namespace

int main(int argc, char** argv) {
  const Args args = parse_args(argc, argv);
  const auto wall0 = std::chrono::steady_clock::now();
  const std::string started = utc_now();

  unsigned long threads = 0UL;
  if (args.threads.find_first_not_of("0123456789") != std::string::npos || args.threads.size() > 2U)
    refuse("--threads must be an integer 1..32", 64);
  threads = std::stoul(args.threads);
  if (threads < 1UL || threads > mm::kMaxThreads) refuse("--threads must be an integer 1..32", 64);

  std::vector<std::string> errors;
  if (!mm::load_and_check_config(args.config, errors)) {
    for (const std::string& e : errors) std::fprintf(stderr, "%s\n", e.c_str());
    refuse("frozen configuration mismatch", 65);
  }
  std::string config_sha, spec_sha, review_sha;
  if (!mm::sha256_file_hex(args.config, config_sha)) refuse("cannot hash config", 65);
  if (!mm::sha256_file_hex(args.spec, spec_sha) || spec_sha != mm::kSpecSha256) refuse("specification SHA-256 mismatch", 65);
  if (!mm::sha256_file_hex(args.review, review_sha) || review_sha != mm::kTerminalReviewSha256)
    refuse("terminal review SHA-256 mismatch", 65);

  std::error_code ec;
  const fs::path package_root = fs::canonical(fs::path(args.config), ec).parent_path().parent_path();
  if (ec) refuse("cannot resolve package root", 65);
  fs::path out = fs::weakly_canonical(fs::path(args.out), ec);
  if (ec) refuse("cannot resolve output directory", 65);
  if (out.filename().empty()) out = out.parent_path();
  if (is_within(out, package_root)) refuse("output directory lies inside the source package", 65);
  if (fs::exists(out, ec)) refuse("output directory already exists; production writes only to a new directory", 65);

  const std::uint64_t projected = mm::kTotalRecordBytes;
  if (projected > mm::kMaxOutputBytes / 2U) refuse("projected output exceeds budget", 65);

  if (!fs::create_directory(out, ec) || ec) refuse("cannot create output directory", 66);
  const fs::path pre = out / "preflight";
  const fs::path shards_dir = out / "shards";
  if (!fs::create_directory(pre, ec) || ec || !fs::create_directory(shards_dir, ec) || ec)
    refuse("cannot create output subdirectories", 66);

  // Preflight: Philox KAT first, then every fixture, the collision audit and key receipts.
  mm::write_new_text_file((pre / "philox_kat.json").string(), mm::fixtures::philox_kat_json());
  mm::fixtures::Results fx;
  mm::fixtures::run_all(fx, "");
  mm::write_new_text_file((pre / "fixtures.json").string(), fx.to_json());
  const mm::KeySet keys = mm::derive_keys(mm::kProductionNamespace);
  const mm::KeySet fixture_keys = mm::derive_keys(mm::kFixtureNamespace);
  mm::write_new_text_file((pre / "purpose_keys.json").string(), mm::fixtures::purpose_keys_json(keys));
  const mm::CollisionReport collision = mm::run_collision_audit(keys, fixture_keys);
  mm::write_new_text_file((pre / "collision_audit.json").string(), collision.to_json());
  if (fx.failed() != 0 || fx.passed() == 0 || !collision.pass) {
    mm::write_new_text_file((out / "PREFLIGHT_FAILED.json").string(),
                            "{\"status\": \"PREFLIGHT_FAILED\", \"population_paths_run\": false}\n");
    std::fprintf(stderr, "preflight failed; no production block was run\n");
    return 67;
  }

  std::printf("preflight PASS; starting %u shards with %lu threads\n", mm::kShards, threads);
  std::fflush(stdout);
  std::vector<ShardResult> results(mm::kShards);
  std::atomic<std::uint32_t> next{0U};
  std::mutex print_mutex;
  std::vector<std::thread> pool;
  for (unsigned long w = 0; w < threads; ++w) {
    pool.emplace_back([&]() {
      for (;;) {
        const std::uint32_t s = next.fetch_add(1U);
        if (s >= mm::kShards) return;
        run_shard(keys, s, shards_dir, results[s]);
        std::lock_guard<std::mutex> lock(print_mutex);
        std::printf("%s complete\n", shard_name(s).c_str());
        std::fflush(stdout);
      }
    });
  }
  for (std::thread& t : pool) t.join();

  ShardResult total;
  for (const ShardResult& r : results) {
    total.queries += r.queries;
    total.n1_failed_blocks += r.n1_failed_blocks;
    total.n2_failed_blocks += r.n2_failed_blocks;
    total.query_failed_blocks += r.query_failed_blocks;
  }

  std::string outputs;
  std::uint64_t total_bytes = 0U;
  for (std::uint32_t s = 0; s < mm::kShards; ++s) {
    std::vector<std::string> names = {"updates.bin", "paths.bin", "blocks.bin"};
    if (s == 0U) names.push_back("audit_rows_blocks_0000_0063.bin");
    for (const std::string& n : names) {
      const std::string rel = "shards/" + shard_name(s) + "/" + n;
      std::string h;
      if (!mm::sha256_file_hex((out / rel).string(), h)) mm::fatal("cannot hash output");
      const std::uint64_t bytes = static_cast<std::uint64_t>(fs::file_size(out / rel));
      total_bytes += bytes;
      outputs += std::string(outputs.empty() ? "" : ",\n") + "    {\"path\": " + mm::jstr(rel) +
                 ", \"bytes\": " + std::to_string(bytes) + ", \"sha256\": " + mm::jstr(h) + "}";
    }
  }
  const bool identities_ok = total.n1_failed_blocks == 0U && total.n2_failed_blocks == 0U &&
                             total.query_failed_blocks == 0U && total.queries == mm::kTotalQueries;
  const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall0).count();
  std::string m = "{\n  \"manifest\": \"MMEM-RUN-MANIFEST-1\",\n  \"study_id\": " + mm::jstr(mm::kStudyId);
  m += ",\n  \"status\": " + mm::jstr(identities_ok ? "COMPLETE" : "COMPLETE_WITH_IDENTITY_OR_COUNT_FAILURE");
  m += ",\n  \"config_path\": " + mm::jstr(fs::canonical(fs::path(args.config)).string());
  m += ",\n  \"config_sha256\": " + mm::jstr(config_sha);
  m += ",\n  \"specification_sha256\": " + mm::jstr(spec_sha);
  m += ",\n  \"terminal_review_sha256\": " + mm::jstr(review_sha);
  m += ",\n  \"key_namespace\": " + mm::jstr(mm::kProductionNamespace);
  m += ",\n  \"compiler_version\": " + mm::jstr(__VERSION__);
  m += ",\n  \"cplusplus\": " + std::to_string(__cplusplus);
  m += ",\n  \"threads\": " + std::to_string(threads);
  m += ",\n  \"shards\": " + std::to_string(mm::kShards) + ",\n  \"blocks\": " + std::to_string(mm::kBlocks);
  m += ",\n  \"started_utc\": " + mm::jstr(started) + ",\n  \"finished_utc\": " + mm::jstr(utc_now());
  m += ",\n  \"wall_seconds\": " + std::to_string(wall);
  m += ",\n  \"total_objective_queries\": " + std::to_string(total.queries);
  m += ",\n  \"expected_total_objective_queries\": " + std::to_string(mm::kTotalQueries);
  m += ",\n  \"n1_failed_blocks\": " + std::to_string(total.n1_failed_blocks);
  m += ",\n  \"n2_failed_blocks\": " + std::to_string(total.n2_failed_blocks);
  m += ",\n  \"query_failed_blocks\": " + std::to_string(total.query_failed_blocks);
  m += ",\n  \"total_output_bytes\": " + std::to_string(total_bytes);
  m += ",\n  \"regeneration_command\": " +
       mm::jstr(std::string("mmem_regenerate --post-production-audit ") + mm::kPostProductionAuditAck +
                " --config <frozen_config.json> --production-dir <this directory> --block <B> --receipt <new file>");
  m += ",\n  \"outputs\": [\n" + outputs + "\n  ]\n}\n";
  mm::write_new_text_file((out / "run_manifest.json").string(), m);
  std::printf("production finished; status %s\n", identities_ok ? "COMPLETE" : "COMPLETE_WITH_IDENTITY_OR_COUNT_FAILURE");
  return identities_ok ? 0 : 2;
}
