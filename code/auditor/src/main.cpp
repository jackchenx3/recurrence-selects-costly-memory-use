// mmem_audit_replay: independent replay verifier for PHASE2-MUTABLE-MEMORY-001 revision 1.
//
// Derived only from the frozen specification, frozen_config.json, OUTPUT_FORMATS.md and
// binary_records.json (see docs/INPUT_CONTRACT.md). It imports, copies and calls no producer code.
//
// Usage:
//   mmem_audit_replay --kat-only --receipt NEW_RECEIPT.json
//   mmem_audit_replay --mode fixture --layout-dir LAYOUT_DIR --receipt NEW_RECEIPT.json
//   mmem_audit_replay --mode production --run-dir PRODUCTION_DIR --receipt NEW_RECEIPT.json
//
// Exit status: 0 PASS; 1 INVALID (receipt written); 2 usage error or unusable receipt path (no
// receipt); 3 INVALID but the receipt could not be written.
//
// Mismatch diagnostics name only coordinates (block/cell/update/candidate) and the record field; they
// never carry an expected or observed value.

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "philox.hpp"
#include "records.hpp"
#include "replay.hpp"
#include "sha256.hpp"

using namespace mmaudit;

namespace {

const char* const kToolName = "mmem_audit_replay";
const char* const kToolVersion = "MMEM-AUDIT-REPLAY-1.0.1";
const char* const kReceiptSchema = "MMEM-AUDITOR-REPLAY-RECEIPT-1";
constexpr std::uint64_t kMismatchCap = 1000000;
constexpr std::size_t kDiagnosticMax = 480;
constexpr std::uint64_t kAnySize = ~std::uint64_t{0};

// Production shard_00 sizes (OUTPUT_FORMATS.md; 1300 blocks per shard).
constexpr std::uint64_t kShardUpdateBytes = 1300ull * 8 * 256 * kUpdateBytes;  // 63,897,600
constexpr std::uint64_t kShardPathBytes = 1300ull * 8 * kPathBytes;           // 1,331,200
constexpr std::uint64_t kShardBlockBytes = 1300ull * kBlockBytes;             // 124,800
constexpr std::uint64_t kAuditBlockRows = 8ull * 256 * 128;                   // 262,144
constexpr std::uint64_t kProductionAuditBlocks = 64;
constexpr std::uint64_t kProductionAuditBytes = kProductionAuditBlocks * kAuditBlockRows * kAuditBytes;

struct AuditFatal : std::runtime_error {
  explicit AuditFatal(const std::string& m) : std::runtime_error(m) {}
};

struct AuditCapped : std::runtime_error {
  AuditCapped() : std::runtime_error("mismatch cap reached") {}
};

std::string bounded(const std::string& s) {
  return s.size() <= kDiagnosticMax ? s : s.substr(0, kDiagnosticMax) + "...";
}

struct Discrepancies {
  std::uint64_t count = 0;
  bool capped = false;
  std::string first;
  void add(const std::string& message) {
    if (count == 0 && first.empty()) first = bounded(message);
    if (count >= kMismatchCap) {
      capped = true;
      return;
    }
    ++count;
  }
};

class InputFile {
 public:
  InputFile(std::string label, std::string path) : label_(std::move(label)), path_(std::move(path)) {}
  ~InputFile() {
    if (file_ != nullptr) std::fclose(file_);
  }
  InputFile(const InputFile&) = delete;
  InputFile& operator=(const InputFile&) = delete;

  bool open() {
    file_ = std::fopen(path_.c_str(), "rb");
    if (file_ == nullptr) return false;
    std::setvbuf(file_, nullptr, _IOFBF, 1u << 22);
    opened_ = true;
    return true;
  }

  bool read_exact(std::uint8_t* buf, std::size_t n) {
    if (file_ == nullptr) return false;
    const std::size_t got = std::fread(buf, 1, n, file_);
    if (got > 0) {
      hash_.update(buf, got);
      bytes_ += got;
    }
    return got == n;
  }

  bool at_eof() {
    if (file_ == nullptr) return true;
    std::uint8_t b = 0;
    if (std::fread(&b, 1, 1, file_) == 1) {
      hash_.update(&b, 1);
      ++bytes_;
      return false;
    }
    return true;
  }

  // Hashes any unread remainder so the receipt always carries whole-file byte counts and digests.
  void finish() {
    if (finished_) return;
    finished_ = true;
    if (file_ == nullptr) return;
    std::vector<std::uint8_t> buf(1u << 20);
    for (;;) {
      const std::size_t got = std::fread(buf.data(), 1, buf.size(), file_);
      if (got == 0) break;
      hash_.update(buf.data(), got);
      bytes_ += got;
    }
    read_error_ = std::ferror(file_) != 0;
    std::fclose(file_);
    file_ = nullptr;
    digest_hex_ = to_hex(hash_.finish());
  }

  const std::string& label() const { return label_; }
  const std::string& path() const { return path_; }
  bool opened() const { return opened_; }
  bool read_error() const { return read_error_; }
  std::uint64_t bytes() const { return bytes_; }
  const std::string& digest_hex() const { return digest_hex_; }

 private:
  std::string label_;
  std::string path_;
  std::FILE* file_ = nullptr;
  bool opened_ = false;
  bool finished_ = false;
  bool read_error_ = false;
  std::uint64_t bytes_ = 0;
  Sha256 hash_;
  std::string digest_hex_;
};

struct KatOutcome {
  std::string name;
  bool pass;
  std::string detail;
};

struct Context {
  std::string mode = "none";
  std::string key_namespace = "none";
  bool production_keys_derived = false;
  bool fixture_keys_derived = false;
  std::string replayed_blocks = "none";
  Discrepancies disc;
  std::vector<std::unique_ptr<InputFile>> inputs;
  std::vector<KatOutcome> kats;
  bool kats_pass = false;
  std::uint64_t blocks = 0, paths = 0, update_records = 0, audit_rows = 0;
  std::uint64_t exp_blocks = 0, exp_paths = 0, exp_update_records = 0, exp_audit_rows = 0;
};

[[noreturn]] void fatal(Context& ctx, const std::string& message) {
  ctx.disc.add(message);
  throw AuditFatal(message);
}

void check_cap(Context& ctx) {
  if (ctx.disc.capped) throw AuditCapped();
}

std::string utc_now() {
  const std::time_t now = std::time(nullptr);
  std::tm parts{};
  gmtime_r(&now, &parts);
  char buf[32];
  std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &parts);
  return buf;
}

std::string json_string(const std::string& s) {
  std::string o = "\"";
  for (const char raw : s) {
    const unsigned char ch = static_cast<unsigned char>(raw);
    switch (ch) {
      case '"':
        o += "\\\"";
        break;
      case '\\':
        o += "\\\\";
        break;
      case '\n':
        o += "\\n";
        break;
      case '\r':
        o += "\\r";
        break;
      case '\t':
        o += "\\t";
        break;
      default:
        if (ch < 0x20 || ch >= 0x7f) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(ch));
          o += buf;
        } else {
          o += static_cast<char>(ch);
        }
    }
  }
  o += "\"";
  return o;
}

std::string hash_file_hex(const std::string& path) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) return "";
  Sha256 h;
  std::vector<std::uint8_t> buf(1u << 20);
  for (;;) {
    const std::size_t got = std::fread(buf.data(), 1, buf.size(), f);
    if (got == 0) break;
    h.update(buf.data(), got);
  }
  const bool error = std::ferror(f) != 0;
  std::fclose(f);
  if (error) return "";
  return to_hex(h.finish());
}

// ---------------------------------------------------------------------------------------------
// Known-answer and self tests. They run before any input is read; any failure is INVALID.

std::vector<KatOutcome> run_known_answer_tests() {
  std::vector<KatOutcome> out;

  struct ShaVector {
    const char* name;
    const char* message;
    const char* digest;
  };
  static const ShaVector kSha[3] = {
      {"sha256_fips180_empty", "", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
      {"sha256_fips180_abc", "abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
      {"sha256_fips180_448bit", "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
       "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
  };
  for (const ShaVector& v : kSha) {
    Sha256 h;
    h.update(v.message, std::strlen(v.message));
    const std::string got = to_hex(h.finish());
    out.push_back({v.name, got == v.digest, "got " + got});
  }

  for (const PhiloxKnownAnswer& k : kPhiloxKnownAnswers) {
    const Word4 counter{{k.counter[0], k.counter[1], k.counter[2], k.counter[3]}};
    const PhiloxKey key{k.key[0], k.key[1]};
    const Word4 got = philox4x32_10(counter, key);
    bool pass = true;
    char buf[64];
    std::snprintf(buf, sizeof buf, "got %08x %08x %08x %08x", got[0], got[1], got[2], got[3]);
    for (int i = 0; i < 4; ++i) pass = pass && got[static_cast<std::size_t>(i)] == k.expected[i];
    out.push_back({k.name, pass, buf});
  }

  {
    std::uint64_t hi = 0, lo = 0;
    bool pass = true;
    mul_64x64(~std::uint64_t{0}, ~std::uint64_t{0}, hi, lo);
    pass = pass && hi == 0xFFFFFFFFFFFFFFFEull && lo == 1ull;
    mul_64x64(1ull << 32, 1ull << 32, hi, lo);
    pass = pass && hi == 1ull && lo == 0ull;
    mul_64x64(~std::uint64_t{0}, 2ull, hi, lo);
    pass = pass && hi == 1ull && lo == 0xFFFFFFFFFFFFFFFEull;
    mul_64x64(0x0000000100000001ull, 0x0000000100000001ull, hi, lo);
    pass = pass && hi == 1ull && lo == 0x0000000200000001ull;
    out.push_back({"u64_by_u64_to_u128_product", pass, ""});
  }

  {
    bool pass = lemire_threshold(3) == 1u && lemire_threshold(5) == 1u && lemire_threshold(6) == 4u &&
                lemire_threshold(1ull << 39) == 0u && lemire_threshold(1) == 0u;
    std::uint64_t z = 99;
    pass = pass && !lemire_accept(0, 3, z);                                // low64 = 0 < 1: forced rejection
    pass = pass && lemire_accept(1, 3, z) && z == 0;                       // low64 = 3, Z = 0
    pass = pass && lemire_accept(1ull << 63, 3, z) && z == 1;              // 3*2^63 = 2^64 + 2^63
    pass = pass && lemire_accept(~std::uint64_t{0}, 1ull << 39, z) && z == (1ull << 39) - 1;
    out.push_back({"lemire_threshold_and_rejection", pass, ""});
  }

  {
    const bool pass = popcount32(0) == 0 && popcount32(0xFFFFFFFFu) == 32 && popcount32(0x80000001u) == 2 &&
                      popcount32(0x0F0F0F0Fu) == 16;
    out.push_back({"popcount32", pass, ""});
  }

  {
    // Declared coordinate schemas: every boundary-valid coordinate is accepted and every
    // boundary-invalid coordinate is rejected; the donor entity map 3*slot+family is injective.
    bool pass = true;
    auto accepts = [](Purpose p, std::uint64_t b, std::uint64_t u, std::uint64_t e, std::uint64_t s) {
      try {
        validate_coordinate(p, b, u, e, s);
        return true;
      } catch (const CoordinateError&) {
        return false;
      }
    };
    pass = pass && accepts(Purpose::InitialGenotype, 41599, 0, 31, 0);
    pass = pass && !accepts(Purpose::InitialGenotype, 0, 1, 0, 0);
    pass = pass && !accepts(Purpose::InitialGenotype, 41600, 0, 0, 0);
    pass = pass && accepts(Purpose::TargetInnovation, 0, 256, 0, 0);
    pass = pass && !accepts(Purpose::TargetInnovation, 0, 257, 0, 0);
    pass = pass && !accepts(Purpose::TargetCopy, 0, 0, 0, 0);
    pass = pass && !accepts(Purpose::TargetCopy, 0, 1, 1, 0);
    pass = pass && !accepts(Purpose::FreshMask, 0, 1, 32, 0);
    pass = pass && !accepts(Purpose::ScoutMask, 0, 1, 0, 1);
    pass = pass && accepts(Purpose::LocalBit, 0, 1, 31, 31);
    pass = pass && !accepts(Purpose::LocalBit, 0, 1, 31, 32);
    pass = pass && accepts(Purpose::DonorKey, 0, 1, 95, 0);
    pass = pass && !accepts(Purpose::DonorKey, 0, 1, 96, 0);
    pass = pass && accepts(Purpose::SurvivalUniform, 0, 256, 31, 0xFFFFFFFFull);
    pass = pass && !accepts(Purpose::SurvivalUniform, 0, 1, 0, 0x100000000ull);
    pass = pass && !accepts(Purpose::SurvivalUniform, 0, 1, 32, 0);
    pass = pass && !accepts(Purpose::PolicyMutation, 0, 1, 32, 0);
    std::set<std::uint32_t> donor_entities;
    for (std::uint32_t slot = 0; slot < 32; ++slot) {
      for (std::uint32_t family = 0; family < 3; ++family) donor_entities.insert(3 * slot + family);
    }
    pass = pass && donor_entities.size() == 96 && *donor_entities.rbegin() == 95;
    out.push_back({"coordinate_schema_ranges_and_donor_injectivity", pass, ""});
  }
  return out;
}

// ---------------------------------------------------------------------------------------------

InputFile& open_input(Context& ctx, const std::string& label, const std::string& path, std::uint64_t expected_size) {
  ctx.inputs.push_back(std::unique_ptr<InputFile>(new InputFile(label, path)));
  InputFile& f = *ctx.inputs.back();
  struct stat sb {};
  if (::stat(path.c_str(), &sb) != 0) fatal(ctx, "missing input " + label + " at " + path);
  if (!S_ISREG(sb.st_mode)) fatal(ctx, "input is not a regular file: " + label);
  if (!f.open()) fatal(ctx, "cannot open input " + label + ": " + std::strerror(errno));
  if (expected_size != kAnySize && static_cast<std::uint64_t>(sb.st_size) != expected_size) {
    fatal(ctx, "input " + label + " has " + std::to_string(static_cast<long long>(sb.st_size)) +
                   " bytes; documented size is " + std::to_string(expected_size));
  }
  return f;
}

void check_purpose_keys(Context& ctx, const KeyedStream& ks) {
  for (unsigned i = 0; i < kPurposeCount; ++i) {
    for (unsigned j = i + 1; j < kPurposeCount; ++j) {
      const PhiloxKey a = ks.key(static_cast<Purpose>(i));
      const PhiloxKey b = ks.key(static_cast<Purpose>(j));
      if (a.k0 == b.k0 && a.k1 == b.k1) {
        fatal(ctx, std::string("purpose keys collide: ") + purpose_name(static_cast<Purpose>(i)) + " and " +
                       purpose_name(static_cast<Purpose>(j)));
      }
    }
  }
}

std::string where(std::uint32_t block, std::uint32_t cell, std::uint32_t update) {
  char buf[96];
  std::snprintf(buf, sizeof buf, "block=%u cell=%u update=%u", block, cell, update);
  return buf;
}

// Replays one block and compares, in file order, every audit row (if audit != nullptr),
// update record, path record and the block record.
void replay_block(Context& ctx, const KeyedStream& ks, std::uint32_t block, InputFile& updates, InputFile& paths,
                  InputFile* blocks_file, const std::uint8_t* preread_block, InputFile* audit) {
  BlockReplay replay(ks, block);
  std::unique_ptr<StepOutput> out(new StepOutput());
  std::vector<std::uint8_t> observed_rows(kCandidates * kAuditBytes);
  std::uint8_t expected_row[kAuditBytes];
  std::uint8_t expected_update[kUpdateBytes], observed_update[kUpdateBytes];
  std::uint8_t expected_path[kPathBytes], observed_path[kPathBytes];
  std::uint8_t expected_block[kBlockBytes], observed_block[kBlockBytes];
  std::string diag;

  for (std::uint32_t cell = 0; cell < kCells; ++cell) {
    replay.begin_cell(cell);
    for (std::uint32_t t = 1; t <= kUpdatesPerPath; ++t) {
      replay.step(t, *out);
      if (audit != nullptr) {
        if (!audit->read_exact(observed_rows.data(), observed_rows.size())) {
          fatal(ctx, where(block, cell, t) + ": audit file ended before the 128 candidate rows of this update");
        }
        for (std::uint32_t j = 0; j < kCandidates; ++j) {
          encode_audit(out->rows[j], expected_row);
          if (!compare_fields("audit_row", kAuditFields, expected_row, observed_rows.data() + j * kAuditBytes,
                              kAuditBytes, diag)) {
            ctx.disc.add(where(block, cell, t) + " candidate=" + std::to_string(j) + " " + diag);
          }
        }
        ctx.audit_rows += kCandidates;
      }
      if (!updates.read_exact(observed_update, kUpdateBytes)) {
        fatal(ctx, where(block, cell, t) + ": update file ended early");
      }
      encode_update(out->update, expected_update);
      if (!compare_fields("update_record", kUpdateFields, expected_update, observed_update, kUpdateBytes, diag)) {
        ctx.disc.add(where(block, cell, t) + " " + diag);
      }
      ++ctx.update_records;
      check_cap(ctx);
    }
    const PathRecord path = replay.end_cell();
    encode_path(path, expected_path);
    if (!paths.read_exact(observed_path, kPathBytes)) fatal(ctx, where(block, cell, 0) + ": path file ended early");
    if (!compare_fields("path_record", kPathFields, expected_path, observed_path, kPathBytes, diag)) {
      ctx.disc.add("block=" + std::to_string(block) + " cell=" + std::to_string(cell) + " " + diag);
    }
    ++ctx.paths;
    check_cap(ctx);
  }

  bool n1 = false, n2 = false, paired = false;
  const BlockRecord expected = replay.end_block(n1, n2, paired);
  if (!n1) ctx.disc.add("block=" + std::to_string(block) + ": auditor replay violates N1 (SHAM start identity)");
  if (!n2) ctx.disc.add("block=" + std::to_string(block) + ": auditor replay violates N2 (SHAM label complement)");
  if (!paired) ctx.disc.add("block=" + std::to_string(block) + ": auditor SHAM paired-trajectory hashes differ");
  encode_block(expected, expected_block);
  if (preread_block != nullptr) {
    std::memcpy(observed_block, preread_block, kBlockBytes);
  } else if (blocks_file == nullptr || !blocks_file->read_exact(observed_block, kBlockBytes)) {
    fatal(ctx, "block=" + std::to_string(block) + ": block file ended early");
  }
  if (!compare_fields("block_record", kBlockFields, expected_block, observed_block, kBlockBytes, diag)) {
    ctx.disc.add("block=" + std::to_string(block) + " " + diag);
  }
  ++ctx.blocks;
  check_cap(ctx);
}

void run_production(Context& ctx, const std::string& run_dir) {
  ctx.mode = "production";
  ctx.key_namespace = namespace_text(KeyNamespace::Production);
  ctx.replayed_blocks = "0-63";
  ctx.exp_blocks = kProductionAuditBlocks;
  ctx.exp_paths = kProductionAuditBlocks * 8;
  ctx.exp_update_records = kProductionAuditBlocks * 8 * 256;
  ctx.exp_audit_rows = kProductionAuditBlocks * kAuditBlockRows;

  const std::string shard = run_dir + "/shards/shard_00/";
  InputFile& updates = open_input(ctx, "shards/shard_00/updates.bin", shard + "updates.bin", kShardUpdateBytes);
  InputFile& paths = open_input(ctx, "shards/shard_00/paths.bin", shard + "paths.bin", kShardPathBytes);
  InputFile& blocks = open_input(ctx, "shards/shard_00/blocks.bin", shard + "blocks.bin", kShardBlockBytes);
  InputFile& audit = open_input(ctx, "shards/shard_00/audit_rows_blocks_0000_0063.bin",
                                shard + "audit_rows_blocks_0000_0063.bin", kProductionAuditBytes);

  const KeyedStream ks(KeyNamespace::Production);
  ctx.production_keys_derived = true;
  check_purpose_keys(ctx, ks);

  for (std::uint32_t block = 0; block < kProductionAuditBlocks; ++block) {
    replay_block(ctx, ks, block, updates, paths, &blocks, nullptr, &audit);
  }
  if (!audit.at_eof()) fatal(ctx, "audit file has bytes after block 63");
  // Records for blocks 64..1299 of shard_00 are outside the replay scope; finish() hashes them.
}

void run_fixture(Context& ctx, const std::string& layout_dir) {
  ctx.mode = "fixture";
  ctx.key_namespace = namespace_text(KeyNamespace::Fixture);
  ctx.exp_blocks = 1;
  ctx.exp_paths = 8;
  ctx.exp_update_records = 8 * 256;
  ctx.exp_audit_rows = kAuditBlockRows;

  static const char* const kNames[5] = {"layout_sample_README.txt", "layout_sample_audit.bin",
                                        "layout_sample_block.bin", "layout_sample_paths.bin",
                                        "layout_sample_updates.bin"};
  DIR* dir = ::opendir(layout_dir.c_str());
  if (dir == nullptr) fatal(ctx, "cannot open fixture layout directory " + layout_dir);
  std::set<std::string> present;
  for (struct dirent* e = ::readdir(dir); e != nullptr; e = ::readdir(dir)) {
    const std::string name = e->d_name;
    if (name != "." && name != "..") present.insert(name);
  }
  ::closedir(dir);
  const std::set<std::string> wanted(kNames, kNames + 5);
  if (present != wanted) {
    std::string listing;
    for (const std::string& n : present) listing += (listing.empty() ? "" : ",") + n;
    fatal(ctx, "fixture layout directory must contain exactly the five documented files; found: " + listing);
  }

  const std::string base = layout_dir + "/";
  InputFile& readme = open_input(ctx, "layout_sample_README.txt", base + "layout_sample_README.txt", kAnySize);
  (void)readme;
  InputFile& block_file = open_input(ctx, "layout_sample_block.bin", base + "layout_sample_block.bin", kBlockBytes);
  std::uint8_t block_record[kBlockBytes];
  if (!block_file.read_exact(block_record, kBlockBytes)) fatal(ctx, "fixture block file is short");
  if (!block_file.at_eof()) fatal(ctx, "fixture block file has trailing bytes");
  const std::uint64_t block = get_le(block_record, 4);
  if (block > kMaxBlock) fatal(ctx, "fixture block id " + std::to_string(block) + " is outside 0..41599");
  ctx.replayed_blocks = std::to_string(block);

  InputFile& updates =
      open_input(ctx, "layout_sample_updates.bin", base + "layout_sample_updates.bin", 8ull * 256 * kUpdateBytes);
  InputFile& paths = open_input(ctx, "layout_sample_paths.bin", base + "layout_sample_paths.bin", 8ull * kPathBytes);
  InputFile& audit =
      open_input(ctx, "layout_sample_audit.bin", base + "layout_sample_audit.bin", kAuditBlockRows * kAuditBytes);

  const KeyedStream ks(KeyNamespace::Fixture);
  ctx.fixture_keys_derived = true;
  check_purpose_keys(ctx, ks);

  replay_block(ctx, ks, static_cast<std::uint32_t>(block), updates, paths, nullptr, block_record, &audit);
  if (!updates.at_eof()) fatal(ctx, "fixture update file has trailing bytes");
  if (!paths.at_eof()) fatal(ctx, "fixture path file has trailing bytes");
  if (!audit.at_eof()) fatal(ctx, "fixture audit file has trailing bytes");
}

// ---------------------------------------------------------------------------------------------

std::string parent_directory(const std::string& path) {
  const std::size_t slash = path.find_last_of('/');
  if (slash == std::string::npos) return ".";
  if (slash == 0) return "/";
  return path.substr(0, slash);
}

std::string real_path(const std::string& path) {
  char buf[PATH_MAX];
  if (::realpath(path.c_str(), buf) == nullptr) return "";
  return buf;
}

// Returns an empty string if the receipt path is usable, otherwise the reason.
std::string receipt_problem(const std::string& receipt, const std::string& input_dir) {
  if (receipt.empty()) return "--receipt is required";
  struct stat sb {};
  if (::lstat(receipt.c_str(), &sb) == 0 || errno != ENOENT) return "receipt path already exists: " + receipt;
  const std::string parent = real_path(parent_directory(receipt));
  if (parent.empty()) return "receipt parent directory does not exist";
  if (!input_dir.empty()) {
    const std::string input = real_path(input_dir);
    if (!input.empty() && (parent == input || parent.compare(0, input.size() + 1, input + "/") == 0)) {
      return "receipt may not be written inside an input directory";
    }
  }
  return "";
}

bool write_exclusive(const std::string& path, const std::string& content, std::string& error) {
  const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0444);
  if (fd < 0) {
    error = std::strerror(errno);
    return false;
  }
  std::size_t done = 0;
  while (done < content.size()) {
    const ssize_t n = ::write(fd, content.data() + done, content.size() - done);
    if (n < 0) {
      if (errno == EINTR) continue;
      error = std::strerror(errno);
      ::close(fd);
      return false;
    }
    done += static_cast<std::size_t>(n);
  }
  if (::fsync(fd) != 0) {
    error = std::strerror(errno);
    ::close(fd);
    return false;
  }
  if (::close(fd) != 0) {
    error = std::strerror(errno);
    return false;
  }
  return true;
}

std::string build_receipt(const Context& ctx, bool pass, const std::string& started, const std::string& finished,
                          const std::string& executable_sha256) {
  std::ostringstream j;
  j << "{\n";
  j << "  \"receipt_schema\": " << json_string(kReceiptSchema) << ",\n";
  j << "  \"status\": " << json_string(pass ? "PASS" : "INVALID") << ",\n";
  j << "  \"tool\": " << json_string(kToolName) << ",\n";
  j << "  \"tool_version\": " << json_string(kToolVersion) << ",\n";
  j << "  \"compiler_version\": " << json_string(__VERSION__) << ",\n";
  j << "  \"cplusplus\": " << __cplusplus << ",\n";
  j << "  \"executable_sha256\": " << (executable_sha256.empty() ? "null" : json_string(executable_sha256)) << ",\n";
  j << "  \"mode\": " << json_string(ctx.mode) << ",\n";
  j << "  \"key_namespace\": " << json_string(ctx.key_namespace) << ",\n";
  j << "  \"production_namespace_keys_derived\": " << (ctx.production_keys_derived ? "true" : "false") << ",\n";
  j << "  \"fixture_namespace_keys_derived\": " << (ctx.fixture_keys_derived ? "true" : "false") << ",\n";
  j << "  \"replayed_blocks\": " << json_string(ctx.replayed_blocks) << ",\n";
  j << "  \"started_at_utc\": " << json_string(started) << ",\n";
  j << "  \"finished_at_utc\": " << json_string(finished) << ",\n";
  j << "  \"inputs\": [";
  for (std::size_t i = 0; i < ctx.inputs.size(); ++i) {
    const InputFile& f = *ctx.inputs[i];
    j << (i ? ",\n" : "\n") << "    {\"label\": " << json_string(f.label()) << ", \"path\": " << json_string(f.path())
      << ", \"opened\": " << (f.opened() ? "true" : "false");
    if (f.opened()) {
      j << ", \"bytes\": " << f.bytes() << ", \"sha256\": " << json_string(f.digest_hex())
        << ", \"read_error\": " << (f.read_error() ? "true" : "false");
    } else {
      j << ", \"bytes\": null, \"sha256\": null";
    }
    j << "}";
  }
  j << (ctx.inputs.empty() ? "],\n" : "\n  ],\n");
  j << "  \"known_answer_tests\": [";
  for (std::size_t i = 0; i < ctx.kats.size(); ++i) {
    const KatOutcome& k = ctx.kats[i];
    j << (i ? ",\n" : "\n") << "    {\"name\": " << json_string(k.name)
      << ", \"result\": " << json_string(k.pass ? "PASS" : "FAIL") << ", \"detail\": " << json_string(k.detail)
      << "}";
  }
  j << (ctx.kats.empty() ? "],\n" : "\n  ],\n");
  j << "  \"known_answer_tests_pass\": " << (ctx.kats_pass ? "true" : "false") << ",\n";
  j << "  \"checked\": {\"blocks\": " << ctx.blocks << ", \"path_records\": " << ctx.paths
    << ", \"update_records\": " << ctx.update_records << ", \"audit_rows\": " << ctx.audit_rows << "},\n";
  j << "  \"expected\": {\"blocks\": " << ctx.exp_blocks << ", \"path_records\": " << ctx.exp_paths
    << ", \"update_records\": " << ctx.exp_update_records << ", \"audit_rows\": " << ctx.exp_audit_rows << "},\n";
  j << "  \"first_discrepancy\": " << (ctx.disc.first.empty() ? "null" : json_string(ctx.disc.first)) << ",\n";
  j << "  \"mismatch_count\": " << ctx.disc.count << ",\n";
  j << "  \"mismatch_count_cap\": " << kMismatchCap << ",\n";
  j << "  \"mismatch_count_capped\": " << (ctx.disc.capped ? "true" : "false") << ",\n";
  j << "  \"is_scientific_result\": false,\n";
  j << "  \"scope\": "
    << json_string(ctx.mode == "kat-only"
                       ? "known-answer tests only; no input was read and no replay was performed"
                       : "replay comparison of the selected blocks only; this receipt reports no estimate")
    << "\n";
  j << "}\n";
  return j.str();
}

}  // namespace

int main(int argc, char** argv) {
  std::string mode, run_dir, layout_dir, receipt, usage_error;
  bool kat_only = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    std::string* target = nullptr;
    if (arg == "--kat-only") {
      kat_only = true;
      continue;
    } else if (arg == "--mode") {
      target = &mode;
    } else if (arg == "--run-dir") {
      target = &run_dir;
    } else if (arg == "--layout-dir") {
      target = &layout_dir;
    } else if (arg == "--receipt") {
      target = &receipt;
    } else {
      if (usage_error.empty()) usage_error = "unknown argument " + arg;
      continue;
    }
    if (i + 1 >= argc) {
      if (usage_error.empty()) usage_error = "missing value for " + arg;
      break;
    }
    *target = argv[++i];
  }
  if (usage_error.empty()) {
    if (kat_only && (!mode.empty() || !run_dir.empty() || !layout_dir.empty())) {
      usage_error = "--kat-only takes no mode or input directory";
    } else if (!kat_only && mode == "production" && (run_dir.empty() || !layout_dir.empty())) {
      usage_error = "production mode requires --run-dir and no --layout-dir";
    } else if (!kat_only && mode == "fixture" && (layout_dir.empty() || !run_dir.empty())) {
      usage_error = "fixture mode requires --layout-dir and no --run-dir";
    } else if (!kat_only && mode != "production" && mode != "fixture") {
      usage_error = "--mode must be production or fixture (or use --kat-only)";
    }
  }

  if (!usage_error.empty()) {
    std::fprintf(stderr, "%s: usage error: %s; no receipt written\n", kToolName, usage_error.c_str());
    return 2;
  }
  const std::string input_dir = !run_dir.empty() ? run_dir : layout_dir;
  const std::string problem = receipt_problem(receipt, input_dir);
  if (!problem.empty()) {
    std::fprintf(stderr, "%s: %s; no receipt written\n", kToolName, problem.c_str());
    return 2;
  }

  Context ctx;
  const std::string started = utc_now();
  ctx.kats = run_known_answer_tests();
  ctx.kats_pass = true;
  for (const KatOutcome& k : ctx.kats) ctx.kats_pass = ctx.kats_pass && k.pass;

  if (!ctx.kats_pass) {
    for (const KatOutcome& k : ctx.kats) {
      if (!k.pass) {
        ctx.disc.add("known-answer test failed: " + k.name + " " + k.detail);
        break;
      }
    }
    ctx.mode = kat_only ? "kat-only" : mode;
  } else if (kat_only) {
    ctx.mode = "kat-only";
  } else {
    try {
      if (mode == "production") {
        run_production(ctx, run_dir);
      } else {
        run_fixture(ctx, layout_dir);
      }
    } catch (const AuditFatal&) {
      // already recorded
    } catch (const AuditCapped&) {
      // mismatch cap reached; count is bounded
    } catch (const CoordinateError& e) {
      ctx.disc.add(std::string("coordinate range violation: ") + e.what());
    } catch (const std::exception& e) {
      ctx.disc.add(std::string("internal error: ") + e.what());
    }
  }

  bool any_read_error = false;
  for (const std::unique_ptr<InputFile>& f : ctx.inputs) {
    f->finish();
    if (f->read_error()) {
      any_read_error = true;
      ctx.disc.add("read error on input " + f->label());
    }
  }

  const bool counts_complete = ctx.blocks == ctx.exp_blocks && ctx.paths == ctx.exp_paths &&
                               ctx.update_records == ctx.exp_update_records && ctx.audit_rows == ctx.exp_audit_rows;
  const bool pass = ctx.kats_pass && ctx.disc.count == 0 && !ctx.disc.capped &&
                    counts_complete && !any_read_error && (ctx.mode == "kat-only" || ctx.mode == "production" ||
                                                           ctx.mode == "fixture");
  if (!pass && ctx.disc.count == 0) ctx.disc.add("incomplete audit: checked counts differ from expected counts");

  const std::string finished = utc_now();
  const std::string body = build_receipt(ctx, pass, started, finished, hash_file_hex("/proc/self/exe"));
  std::string error;
  if (!write_exclusive(receipt, body, error)) {
    std::fprintf(stderr, "%s: could not create receipt %s: %s\n", kToolName, receipt.c_str(), error.c_str());
    return 3;
  }
  std::printf("%s %s mismatches=%llu receipt=%s\n", kToolName, pass ? "PASS" : "INVALID",
              static_cast<unsigned long long>(ctx.disc.count), receipt.c_str());
  return pass ? 0 : 1;
}
