// mmem_regenerate: post-production regeneration of one block from its block ID
// and the frozen config, compared byte-for-byte with the stored shard records.
// Writes a PASS/FAIL receipt only; prints no outcome values.
//
// mmem_regenerate --post-production-audit <ACK> --config C --production-dir D --block B --receipt NEW_FILE
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "mm/block_runner.hpp"
#include "mm/config_check.hpp"
#include "mm/constants.hpp"
#include "mm/io.hpp"
#include "mm/keys.hpp"
#include "mm/records.hpp"
#include "mm/sha256.hpp"

namespace {

bool read_range(const std::string& path, std::uint64_t offset, std::size_t n, std::vector<std::uint8_t>& out) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) return false;
  out.assign(n, 0U);
  bool ok = std::fseek(f, static_cast<long>(offset), SEEK_SET) == 0 && std::fread(out.data(), 1U, n, f) == n;
  std::fclose(f);
  return ok;
}

}  // namespace

int main(int argc, char** argv) {
  std::string ack, config, dir, block_text, receipt;
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string k = argv[i], v = argv[i + 1];
    if (k == "--post-production-audit") ack = v;
    else if (k == "--config") config = v;
    else if (k == "--production-dir") dir = v;
    else if (k == "--block") block_text = v;
    else if (k == "--receipt") receipt = v;
    else {
      std::fprintf(stderr, "unknown argument %s\n", k.c_str());
      return 64;
    }
  }
  if (ack != mm::kPostProductionAuditAck || config.empty() || dir.empty() || block_text.empty() || receipt.empty() ||
      block_text.find_first_not_of("0123456789") != std::string::npos || block_text.size() > 5U) {
    std::fprintf(stderr, "usage: mmem_regenerate --post-production-audit %s --config C --production-dir D --block B --receipt NEW_FILE\n",
                 mm::kPostProductionAuditAck);
    return 64;
  }
  const unsigned long block_ul = std::strtoul(block_text.c_str(), nullptr, 10);
  if (block_ul >= mm::kBlocks) return 64;
  const std::uint32_t block = static_cast<std::uint32_t>(block_ul);
  std::vector<std::string> errors;
  if (!mm::load_and_check_config(config, errors)) {
    for (const std::string& e : errors) std::fprintf(stderr, "%s\n", e.c_str());
    return 65;
  }

  const mm::KeySet keys = mm::derive_keys(mm::kProductionNamespace);
  mm::BlockOutput out;
  const bool audit = block < mm::kAuditBlocks;
  mm::run_block(keys, block, audit, out);

  char shard[16];
  std::snprintf(shard, sizeof shard, "shard_%02u", static_cast<unsigned>(block / mm::kBlocksPerShard));
  const std::string base = dir + "/shards/" + shard + "/";
  const std::uint64_t local = block % mm::kBlocksPerShard;
  std::vector<std::uint8_t> su, sp, sb, sa;
  const bool read_ok =
      read_range(base + "updates.bin", local * out.updates.size(), out.updates.size(), su) &&
      read_range(base + "paths.bin", local * out.paths.size(), out.paths.size(), sp) &&
      read_range(base + "blocks.bin", local * out.block.size(), out.block.size(), sb) &&
      (!audit || read_range(base + "audit_rows_blocks_0000_0063.bin", block * out.audit.size(), out.audit.size(), sa));
  const bool match = read_ok && su == out.updates && sp == out.paths && sb == out.block && (!audit || sa == out.audit);

  std::string r = "{\n  \"receipt\": \"MMEM-REGENERATION-1\",\n  \"block\": " + std::to_string(block);
  r += ",\n  \"audit_block\": " + std::string(audit ? "true" : "false");
  r += ",\n  \"stored_records_readable\": " + std::string(read_ok ? "true" : "false");
  r += ",\n  \"regenerated_updates_sha256\": " + mm::jstr(mm::to_hex(mm::sha256_bytes(out.updates.data(), out.updates.size())));
  r += ",\n  \"regenerated_paths_sha256\": " + mm::jstr(mm::to_hex(mm::sha256_bytes(out.paths.data(), out.paths.size())));
  r += ",\n  \"status\": " + mm::jstr(match ? "PASS" : "FAIL") + "\n}\n";
  mm::write_new_text_file(receipt, r);
  return match ? 0 : 1;
}
