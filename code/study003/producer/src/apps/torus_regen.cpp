// Regenerates one path (block, cell) or one full block from its block ID and
// the frozen source/config, writes the regenerated records and, optionally,
// compares them byte-for-byte with a saved production/fixture output set.
//
// namespace production-r1 requires the same identity gate and execution
// authorization as production; fixture-r1 requires only the frozen config.

#include <sys/types.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "../torus/chunk_io.hpp"
#include "../torus/cli.hpp"
#include "../torus/config_check.hpp"
#include "../torus/path_runner.hpp"
#include "../torus/util.hpp"

using namespace torus;

namespace {

class VectorSink : public AuditSink {
 public:
  ByteBuf cx, cd, en;
  u64 ncx = 0, ncd = 0, nen = 0;
  void context(const ContextAuditRow& r) override {
    serialize_context(r, cx);
    ++ncx;
  }
  void candidate(const CandidateAuditRow& r) override {
    serialize_candidate(r, cd);
    ++ncd;
  }
  void entry(const EntryAuditRow& r) override {
    serialize_entry(r, en);
    ++nen;
  }
};

void write_rows(const std::string& path, const char* magic, u32 row, u64 count, const std::string& ns, u32 block,
                const ByteBuf& rows) {
  TORUS_REQUIRE(rows.size() == count * row, "row payload size mismatch: " + path);
  ByteBuf h;
  serialize_header(FileHeader{magic, row, count, ns, kNoChunk, block, 1}, h);
  std::FILE* f = create_new_file(path);
  write_all(f, h.data(), h.size(), path);
  write_all(f, rows.data(), rows.size(), path);
  close_checked(f, path);
}

bool compare_slice(const std::string& file, u64 offset, const ByteBuf& expect) {
  std::FILE* f = std::fopen(file.c_str(), "rb");
  TORUS_REQUIRE(f != nullptr, "cannot open comparison file: " + file);
  TORUS_REQUIRE(fseeko(f, off_t(offset), SEEK_SET) == 0, "seek failed: " + file);
  std::vector<u8> got(expect.size());
  const std::size_t n = std::fread(got.data(), 1, got.size(), f);
  std::fclose(f);
  return n == got.size() && std::equal(got.begin(), got.end(), expect.data());
}

}  // namespace

int main(int argc, char** argv) {
  const Args a(argc, argv,
               {"namespace", "package-root", "config", "expect-config-sha256", "design-spec", "design-go-record",
                "execution-authorization", "block", "cell", "with-audit-rows", "compare-dir", "out"});
  const std::string ns = a.req("namespace");
  const std::string out = a.req("out");
  const u64 block = parse_u64_strict(a.req("block"), "--block");
  TORUS_REQUIRE(block < kBlocks, "--block must be 0..41599");
  const std::string cell_arg = a.req("cell");
  const bool whole_block = cell_arg == "all";
  const u64 cell = whole_block ? 0 : parse_u64_strict(cell_arg, "--cell");
  TORUS_REQUIRE(cell < u64(kCells), "--cell must be 0..7 or all");
  const std::string audit_arg = a.req("with-audit-rows");
  TORUS_REQUIRE(audit_arg == "yes" || audit_arg == "no", "--with-audit-rows must be yes or no");
  TORUS_REQUIRE(!path_exists(out) || (is_directory(out) && list_directory_sorted(out).empty()),
                "refusing existing nonempty output: " + out);

  run_startup_self_tests();
  verify_random123(a.req("package-root"));
  const std::string cfg_sha = verify_frozen_config(a.req("config"));
  TORUS_REQUIRE(cfg_sha == a.req("expect-config-sha256"), "config SHA-256 does not match --expect-config-sha256");
  if (ns == kProductionNamespace) {
    verify_design_spec(a.req("design-spec"));
    verify_design_go_record(a.req("design-go-record"));
    verify_execution_authorization(a.req("execution-authorization"), cfg_sha);
  } else {
    TORUS_REQUIRE(ns == kFixtureNamespace, "--namespace must be production-r1 or fixture-r1");
    a.forbid("execution-authorization", "outside production-r1");
  }
  const KeySet ks = derive_keys(ns);

  std::unique_ptr<BlockResult> res(new BlockResult);
  VectorSink sink;
  RunOptions opt;
  opt.only_cell = whole_block ? -1 : int(cell);
  if (audit_arg == "yes") opt.sink = &sink;
  run_block(ks, u32(block), opt, *res);

  prepare_output_dir(out);
  const int c0 = whole_block ? 0 : int(cell), c1 = whole_block ? kCells : int(cell) + 1;
  const u64 npaths = u64(c1 - c0);
  ByteBuf ub, pb;
  for (int c = c0; c < c1; ++c) {
    for (int t = 0; t < kUpdates; ++t) serialize_update(res->updates[c][t], ub);
    serialize_path(res->paths[c], pb);
  }
  write_rows(join_path(out, "regen_updates.t3u"), kMagicUpdate, kUpdateRowBytes, npaths * kUpdates, ns, u32(block), ub);
  write_rows(join_path(out, "regen_paths.t3p"), kMagicPath, kPathRowBytes, npaths, ns, u32(block), pb);
  ByteBuf bb;
  if (whole_block) {
    serialize_block(res->block, bb);
    write_rows(join_path(out, "regen_block.t3b"), kMagicBlock, kBlockRowBytes, 1, ns, u32(block), bb);
  }
  if (opt.sink) {
    TORUS_REQUIRE(sink.ncx == npaths * kUpdates && sink.ncd == npaths * kUpdates * kCandidates &&
                      sink.nen == npaths * kUpdates * kPop * kEntries,
                  "regenerated audit row count mismatch");
    write_rows(join_path(out, "regen_audit_context.t3x"), kMagicContext, kContextRowBytes, sink.ncx, ns, u32(block), sink.cx);
    write_rows(join_path(out, "regen_audit_candidates.t3c"), kMagicCandidate, kCandidateRowBytes, sink.ncd, ns, u32(block), sink.cd);
    write_rows(join_path(out, "regen_audit_entries.t3e"), kMagicEntry, kEntryRowBytes, sink.nen, ns, u32(block), sink.en);
  }

  std::ostringstream js;
  js << "{\n  \"schema\": \"PHASE2-TORUS-MEMORY-003-REGEN-RECEIPT-v1\",\n"
     << "  \"namespace\": \"" << ns << "\",\n  \"config_sha256\": \"" << cfg_sha << "\",\n"
     << "  \"block\": " << block << ",\n  \"cell\": \"" << cell_arg << "\"";
  if (a.has("compare-dir")) {
    // Saved layout: chunk k = block / 64; rows ordered (block, cell, update).
    const std::string dir = a.req("compare-dir");
    const u32 chunk = u32(block / kChunkBlocks);
    const u64 local = block - u64(chunk) * kChunkBlocks;
    const u64 first_path = local * kCells + u64(c0);
    const bool upd_ok = compare_slice(join_path(dir, "updates/" + chunk_name(chunk, "t3u")),
                                      kHeaderBytes + first_path * kUpdates * kUpdateRowBytes, ub);
    const bool path_ok =
        compare_slice(join_path(dir, "paths/" + chunk_name(chunk, "t3p")), kHeaderBytes + first_path * kPathRowBytes, pb);
    bool block_ok = true;
    if (whole_block)
      block_ok = compare_slice(join_path(dir, "blocks/" + chunk_name(chunk, "t3b")), kHeaderBytes + local * kBlockRowBytes, bb);
    js << ",\n  \"compared_update_rows_identical\": " << (upd_ok ? "true" : "false")
       << ",\n  \"compared_path_rows_identical\": " << (path_ok ? "true" : "false")
       << ",\n  \"compared_block_row_identical\": " << (block_ok ? "true" : "false");
    js << ",\n  \"status\": \"" << ((upd_ok && path_ok && block_ok) ? "REGENERATION_IDENTICAL" : "REGENERATION_MISMATCH")
       << "\"";
  } else {
    js << ",\n  \"status\": \"REGENERATED_NOT_COMPARED\"";
  }
  js << "\n}\n";
  write_new_text_file(join_path(out, "regen_receipt.json"), js.str());
  return 0;
}
