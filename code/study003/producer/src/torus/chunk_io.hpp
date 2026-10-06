// Chunked output: chunk k holds blocks 64k..64k+63 (k = 0..649). Chunk 0 is
// exactly the fixed audit set (blocks 0..63) and also carries the audit rows.
// File contents depend only on block IDs and keys, never on thread count.
#pragma once

#include <string>
#include <vector>

#include "path_runner.hpp"
#include "util.hpp"

namespace torus {

// Appends one block's rows in canonical order (cell 0..7, update 1..256).
void append_block_result(const BlockResult& r, ByteBuf& updates, ByteBuf& paths, ByteBuf& blocks);

std::string chunk_name(u32 chunk, const char* ext);

// Writes updates/, paths/, blocks/ files for one chunk (and audit/ files when
// chunk == 0 and with_audit). Directories must already exist.
void write_chunk(const KeySet& ks, u32 chunk, const std::string& out_dir, bool with_audit);

// Runs `chunks` on `threads` workers via a shared atomic work index.
void run_chunks_threaded(const KeySet& ks, const std::vector<u32>& chunks, const std::string& out_dir,
                         int threads, bool with_audit);

// In-memory variant used by invariance checks: one serialized buffer per block.
void run_blocks_in_memory(const KeySet& ks, const std::vector<u32>& blocks, int threads,
                          std::vector<std::vector<u8>>& out);

}  // namespace torus
