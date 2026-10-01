// mmem_fixtures: runs K0 (Philox KAT, first), S0, KEYS, COLLISION and F1-F10.
//   mmem_fixtures [--receipt NEW_FILE] [--emit-layout-sample EXISTING_EMPTY_DIR]
//   mmem_fixtures --timing-benchmark K     (K fixture-namespace blocks; prints timing only)
// Exit 0 only if every check passes.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "mm/block_runner.hpp"
#include "mm/constants.hpp"
#include "mm/fixtures.hpp"
#include "mm/io.hpp"
#include "mm/keys.hpp"

int main(int argc, char** argv) {
  std::string receipt, emit_dir;
  long bench = -1;
  for (int i = 1; i < argc; ++i) {
    const std::string k = argv[i];
    if (i + 1 >= argc) {
      std::fprintf(stderr, "missing value for %s\n", k.c_str());
      return 64;
    }
    const std::string v = argv[++i];
    if (k == "--receipt") receipt = v;
    else if (k == "--emit-layout-sample") emit_dir = v;
    else if (k == "--timing-benchmark") bench = std::strtol(v.c_str(), nullptr, 10);
    else {
      std::fprintf(stderr, "unknown argument %s\n", k.c_str());
      return 64;
    }
  }

  if (bench >= 0) {
    // Fixture namespace only: never a production draw. Prints no outcome.
    if (bench < 1 || bench > 1000) {
      std::fprintf(stderr, "--timing-benchmark K requires 1 <= K <= 1000\n");
      return 64;
    }
    const mm::KeySet fix = mm::derive_keys(mm::kFixtureNamespace);
    mm::BlockOutput out;
    const auto t0 = std::chrono::steady_clock::now();
    for (long b = 0; b < bench; ++b) mm::run_block(fix, static_cast<std::uint32_t>(b), b < 2, out);
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double per_block = sec / static_cast<double>(bench);
    std::printf("fixture-namespace blocks: %ld (first two with audit capture)\nseconds: %.3f\nseconds per block: %.6f\n"
                "projected single-thread CPU-seconds for 41600 blocks: %.0f\n",
                bench, sec, per_block, per_block * 41600.0);
    return 0;
  }

  mm::fixtures::Results results;
  mm::fixtures::run_all(results, emit_dir);
  for (const std::string& line : results.lines()) std::printf("%s\n", line.c_str());
  std::printf("passed=%d failed=%d\n", results.passed(), results.failed());
  if (!receipt.empty()) mm::write_new_text_file(receipt, results.to_json());
  return (results.failed() == 0 && results.passed() > 0) ? 0 : 1;
}
