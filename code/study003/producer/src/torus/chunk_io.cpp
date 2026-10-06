#include "chunk_io.hpp"

#include <atomic>
#include <cstdio>
#include <memory>
#include <thread>

namespace torus {

namespace {

class FileAuditSink : public AuditSink {
 public:
  FileAuditSink(const std::string& dir, const std::string& ns) {
    open(cx_, join_path(dir, "audit_context.t3x"), kMagicContext, kContextRowBytes, kAuditPathUpdates, ns);
    open(cd_, join_path(dir, "audit_candidates.t3c"), kMagicCandidate, kCandidateRowBytes, kAuditCandidateRows, ns);
    open(en_, join_path(dir, "audit_entries.t3e"), kMagicEntry, kEntryRowBytes, kAuditEntryRows, ns);
  }
  void context(const ContextAuditRow& r) override {
    serialize_context(r, cx_.buf);
    ++cx_.rows;
    maybe_flush(cx_);
  }
  void candidate(const CandidateAuditRow& r) override {
    serialize_candidate(r, cd_.buf);
    ++cd_.rows;
    maybe_flush(cd_);
  }
  void entry(const EntryAuditRow& r) override {
    serialize_entry(r, en_.buf);
    ++en_.rows;
    maybe_flush(en_);
  }
  void close_all() {
    finish(cx_);
    finish(cd_);
    finish(en_);
  }

 private:
  struct Stream {
    std::FILE* f = nullptr;
    std::string path;
    ByteBuf buf;
    u64 rows = 0;
    u64 expected = 0;
  };
  static void open(Stream& s, const std::string& path, const char* magic, u32 row, u64 expected,
                   const std::string& ns) {
    s.path = path;
    s.expected = expected;
    s.f = create_new_file(path);
    FileHeader h{magic, row, expected, ns, kNoChunk, 0, kAuditBlocks};
    ByteBuf b;
    serialize_header(h, b);
    write_all(s.f, b.data(), b.size(), path);
  }
  static void maybe_flush(Stream& s) {
    if (s.buf.size() >= (8u << 20)) {
      write_all(s.f, s.buf.data(), s.buf.size(), s.path);
      s.buf.clear();
    }
  }
  static void finish(Stream& s) {
    write_all(s.f, s.buf.data(), s.buf.size(), s.path);
    s.buf.clear();
    TORUS_REQUIRE(s.rows == s.expected, "audit row count mismatch: " + s.path);
    close_checked(s.f, s.path);
    s.f = nullptr;
  }
  Stream cx_, cd_, en_;
};

void write_file(const std::string& path, const FileHeader& h, const ByteBuf& rows) {
  TORUS_REQUIRE(rows.size() == h.row_count * h.row_size, "row payload size mismatch: " + path);
  ByteBuf hb;
  serialize_header(h, hb);
  std::FILE* f = create_new_file(path);
  write_all(f, hb.data(), hb.size(), path);
  write_all(f, rows.data(), rows.size(), path);
  close_checked(f, path);
}

}  // namespace

std::string chunk_name(u32 chunk, const char* ext) {
  char b[64];
  std::snprintf(b, sizeof b, "chunk_%05u.%s", chunk, ext);
  return b;
}

void append_block_result(const BlockResult& r, ByteBuf& updates, ByteBuf& paths, ByteBuf& blocks) {
  for (int c = 0; c < kCells; ++c) {
    TORUS_REQUIRE(r.ran[c], "incomplete block result");
    for (int t = 0; t < kUpdates; ++t) serialize_update(r.updates[c][t], updates);
    serialize_path(r.paths[c], paths);
  }
  serialize_block(r.block, blocks);
}

void write_chunk(const KeySet& ks, u32 chunk, const std::string& out_dir, bool with_audit) {
  TORUS_REQUIRE(chunk < kChunks, "chunk out of range");
  const u32 first = chunk * kChunkBlocks;
  std::unique_ptr<FileAuditSink> sink;
  if (with_audit && chunk == 0) sink.reset(new FileAuditSink(join_path(out_dir, "audit"), ks.ns));

  ByteBuf ub, pb, bb;
  std::unique_ptr<BlockResult> res(new BlockResult);
  for (u32 b = first; b < first + kChunkBlocks; ++b) {
    RunOptions opt;
    opt.sink = sink.get();
    run_block(ks, b, opt, *res);
    append_block_result(*res, ub, pb, bb);
  }
  if (sink) sink->close_all();

  const u64 nb = kChunkBlocks;
  write_file(join_path(join_path(out_dir, "updates"), chunk_name(chunk, "t3u")),
             FileHeader{kMagicUpdate, kUpdateRowBytes, nb * kCells * kUpdates, ks.ns, chunk, first, kChunkBlocks}, ub);
  write_file(join_path(join_path(out_dir, "paths"), chunk_name(chunk, "t3p")),
             FileHeader{kMagicPath, kPathRowBytes, nb * kCells, ks.ns, chunk, first, kChunkBlocks}, pb);
  write_file(join_path(join_path(out_dir, "blocks"), chunk_name(chunk, "t3b")),
             FileHeader{kMagicBlock, kBlockRowBytes, nb, ks.ns, chunk, first, kChunkBlocks}, bb);
}

void run_chunks_threaded(const KeySet& ks, const std::vector<u32>& chunks, const std::string& out_dir,
                         int threads, bool with_audit) {
  TORUS_REQUIRE(threads >= 1 && threads <= 32, "threads must be 1..32");
  std::atomic<std::size_t> next(0);
  auto worker = [&]() {
    for (;;) {
      const std::size_t i = next.fetch_add(1);
      if (i >= chunks.size()) return;
      write_chunk(ks, chunks[i], out_dir, with_audit);
    }
  };
  std::vector<std::thread> pool;
  for (int k = 0; k < threads; ++k) pool.emplace_back(worker);
  for (auto& th : pool) th.join();
}

void run_blocks_in_memory(const KeySet& ks, const std::vector<u32>& blocks, int threads,
                          std::vector<std::vector<u8>>& out) {
  TORUS_REQUIRE(threads >= 1 && threads <= 32, "threads must be 1..32");
  out.assign(blocks.size(), std::vector<u8>());
  std::atomic<std::size_t> next(0);
  auto worker = [&]() {
    std::unique_ptr<BlockResult> res(new BlockResult);
    for (;;) {
      const std::size_t i = next.fetch_add(1);
      if (i >= blocks.size()) return;
      RunOptions opt;
      run_block(ks, blocks[i], opt, *res);
      ByteBuf ub, pb, bb;
      append_block_result(*res, ub, pb, bb);
      std::vector<u8>& v = out[i];
      v.insert(v.end(), ub.data(), ub.data() + ub.size());
      v.insert(v.end(), pb.data(), pb.data() + pb.size());
      v.insert(v.end(), bb.data(), bb.data() + bb.size());
    }
  };
  std::vector<std::thread> pool;
  for (int k = 0; k < threads; ++k) pool.emplace_back(worker);
  for (auto& th : pool) th.join();
}

}  // namespace torus
