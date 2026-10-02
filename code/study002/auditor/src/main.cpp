// pconv_audit_replay: independent replay verifier for PHASE2-PERFORMANCE-CONVERSION-002 revision 1.
//
// Derived only from the frozen specification, frozen_config.json, OUTPUT_FORMATS.md and
// binary_records.json (see docs/INPUT_CONTRACT.md). It imports, copies and calls no producer code.
//
// Usage:
//   pconv_audit_replay --kat-only --receipt NEW_RECEIPT.json
//   pconv_audit_replay --mode fixture --layout-dir LAYOUT_DIR --receipt NEW_RECEIPT.json
//   pconv_audit_replay --mode production --run-dir PRODUCTION_DIR --receipt NEW_RECEIPT.json
//
// Exit status: 0 PASS; 1 INVALID (receipt written); 2 usage error or unusable receipt path (no
// receipt); 3 INVALID but the receipt could not be written.
//
// Mismatch diagnostics name only coordinates (block/cell/update/candidate) and the record field; they
// never carry an expected or observed value. Inputs are opened read-only and streamed.

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
#include <map>
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

using namespace pcaudit;

namespace {

const char* const kToolName = "pconv_audit_replay";
const char* const kToolVersion = "PCONV-AUDIT-REPLAY-1.0.0";
const char* const kReceiptSchema = "PCONV-AUDITOR-REPLAY-RECEIPT-1";
constexpr std::uint64_t kMismatchCap = 1000000;
constexpr std::size_t kDiagnosticMax = 480;
constexpr std::uint64_t kAnySize = ~std::uint64_t{0};

// Production shard_00 sizes (OUTPUT_FORMATS.md; 1300 blocks per shard, six cells per block).
constexpr std::uint64_t kBlocksPerShard = 1300;
constexpr std::uint64_t kShardUpdateBytes = kBlocksPerShard * kCells * kUpdatesPerPath * kUpdateBytes;  // 95,846,400
constexpr std::uint64_t kShardPathBytes = kBlocksPerShard * kCells * kPathBytes;                       // 1,372,800
constexpr std::uint64_t kShardBlockBytes = kBlocksPerShard * kBlockBytes;                              // 124,800
constexpr std::uint64_t kAuditBlockRows = static_cast<std::uint64_t>(kCells) * kUpdatesPerPath * kCandidates;  // 196,608
constexpr std::uint64_t kPermRecordsPerBlock = kUpdatesPerPath;                                        // 256
constexpr std::uint64_t kProductionAuditBlocks = 64;
constexpr std::uint64_t kProductionAuditBytes = kProductionAuditBlocks * kAuditBlockRows * kAuditBytes;  // 1,107,296,256
constexpr std::uint64_t kProductionPermBytes = kProductionAuditBlocks * kPermRecordsPerBlock * kPermBytes;  // 6,815,744

static_assert(kShardUpdateBytes == 95846400ull, "update shard size");
static_assert(kProductionAuditBytes == 1107296256ull, "audit row file size");
static_assert(kProductionPermBytes == 6815744ull, "audit permutation file size");

const char* const kAuditRowsFile = "audit_rows_blocks_0000_0063.bin";
const char* const kAuditPermFile = "audit_permutations_blocks_0000_0063.bin";

// Fixture layout (PROVISIONAL file names; docs/INPUT_CONTRACT.md section 4).
const char* const kFixtureReadme = "layout_sample_README.txt";
const char* const kFixtureAudit = "layout_sample_audit.bin";
const char* const kFixturePerm = "layout_sample_permutations.bin";
const char* const kFixtureBlock = "layout_sample_block.bin";
const char* const kFixturePaths = "layout_sample_paths.bin";
const char* const kFixtureUpdates = "layout_sample_updates.bin";

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
  bool namespace_separation_hashed = false;
  std::string replayed_blocks = "none";
  Discrepancies disc;
  std::vector<std::unique_ptr<InputFile>> inputs;
  std::vector<KatOutcome> kats;
  bool kats_pass = false;
  std::uint64_t blocks = 0, paths = 0, update_records = 0, audit_rows = 0, perm_records = 0;
  std::uint64_t exp_blocks = 0, exp_paths = 0, exp_update_records = 0, exp_audit_rows = 0, exp_perm_records = 0;
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

std::uint64_t reach_x(std::uint64_t bound, std::uint64_t j) {
  // An x whose Lemire result with this bound is j and whose low word is far above the threshold.
  const std::uint64_t q = ~std::uint64_t{0} / bound;
  return j * q + q / 2u;
}

std::uint32_t rotl32(std::uint32_t v, unsigned s) {
  s &= 31u;
  return s == 0 ? v : ((v << s) | (v >> (32u - s)));
}

std::vector<KatOutcome> run_known_answer_tests(bool include_namespace_separation) {
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

  // The three official Random123 Philox4x32-10 rows, unchanged from the accepted seed auditor.
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
    // Key words = SHA-256 digest bytes 0..3 and 4..7 read little-endian; digest("abc") starts
    // ba 78 16 bf 8f 01 cf ea.
    const PhiloxKey k = key_words_from_text("abc");
    out.push_back({"key_words_are_digest_bytes_0_3_and_4_7_little_endian", k.k0 == 0xbf1678bau && k.k1 == 0xeacf018fu,
                   ""});
  }

  {
    // Study-002 key texts (frozen_config rng.key_text_template), literal, for both namespaces.
    static const char* const kProduction[kPurposeCount] = {
        "PHASE2-PERFORMANCE-CONVERSION-002|production-r1|INITIAL_GENOTYPE",
        "PHASE2-PERFORMANCE-CONVERSION-002|production-r1|TARGET_INNOVATION",
        "PHASE2-PERFORMANCE-CONVERSION-002|production-r1|TARGET_COPY",
        "PHASE2-PERFORMANCE-CONVERSION-002|production-r1|FRESH_MASK",
        "PHASE2-PERFORMANCE-CONVERSION-002|production-r1|SCOUT_MASK",
        "PHASE2-PERFORMANCE-CONVERSION-002|production-r1|LOCAL_BIT",
        "PHASE2-PERFORMANCE-CONVERSION-002|production-r1|DONOR_KEY",
        "PHASE2-PERFORMANCE-CONVERSION-002|production-r1|SURVIVAL_UNIFORM",
        "PHASE2-PERFORMANCE-CONVERSION-002|production-r1|POLICY_MUTATION",
        "PHASE2-PERFORMANCE-CONVERSION-002|production-r1|DECOY_PERMUTATION"};
    static const char* const kFixture[kPurposeCount] = {
        "PHASE2-PERFORMANCE-CONVERSION-002|fixture-r1-nonscientific|INITIAL_GENOTYPE",
        "PHASE2-PERFORMANCE-CONVERSION-002|fixture-r1-nonscientific|TARGET_INNOVATION",
        "PHASE2-PERFORMANCE-CONVERSION-002|fixture-r1-nonscientific|TARGET_COPY",
        "PHASE2-PERFORMANCE-CONVERSION-002|fixture-r1-nonscientific|FRESH_MASK",
        "PHASE2-PERFORMANCE-CONVERSION-002|fixture-r1-nonscientific|SCOUT_MASK",
        "PHASE2-PERFORMANCE-CONVERSION-002|fixture-r1-nonscientific|LOCAL_BIT",
        "PHASE2-PERFORMANCE-CONVERSION-002|fixture-r1-nonscientific|DONOR_KEY",
        "PHASE2-PERFORMANCE-CONVERSION-002|fixture-r1-nonscientific|SURVIVAL_UNIFORM",
        "PHASE2-PERFORMANCE-CONVERSION-002|fixture-r1-nonscientific|POLICY_MUTATION",
        "PHASE2-PERFORMANCE-CONVERSION-002|fixture-r1-nonscientific|DECOY_PERMUTATION"};
    bool pass = true;
    for (unsigned i = 0; i < kPurposeCount; ++i) {
      pass = pass && key_text(KeyNamespace::Production, static_cast<Purpose>(i)) == kProduction[i];
      pass = pass && key_text(KeyNamespace::Fixture, static_cast<Purpose>(i)) == kFixture[i];
    }
    out.push_back({"study_002_key_texts_production_and_fixture_including_decoy_permutation", pass, ""});
  }

  if (include_namespace_separation) {
    // Only in --kat-only (no input is read, no stream is constructed, no draw is generated): the ten
    // production and ten fixture study-002 key word pairs and the eighteen reconstructed study-001
    // key word pairs are pairwise distinct (no random tape is shared across purposes, namespaces or
    // studies).
    std::set<std::uint64_t> seen;
    std::size_t total = 0;
    const KeyNamespace spaces[2] = {KeyNamespace::Production, KeyNamespace::Fixture};
    for (const KeyNamespace ns : spaces) {
      for (unsigned i = 0; i < kPurposeCount; ++i) {
        const PhiloxKey k = key_words_from_text(key_text(ns, static_cast<Purpose>(i)));
        seen.insert((static_cast<std::uint64_t>(k.k1) << 32) | k.k0);
        ++total;
      }
      for (unsigned i = 0; i < kPredecessorPurposeCount; ++i) {
        const PhiloxKey k = key_words_from_text(predecessor_key_text(ns, static_cast<Purpose>(i)));
        seen.insert((static_cast<std::uint64_t>(k.k1) << 32) | k.k0);
        ++total;
      }
    }
    out.push_back({"key_words_distinct_across_purposes_namespaces_and_study_001", total == 38 && seen.size() == 38,
                   ""});
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
    out.push_back({"lemire_survival_threshold_and_rejection", pass, ""});
  }

  {
    // Fisher-Yates bounds n = i+1: thresholds 2^64 mod n, and a rejected nonzero x for n = 31
    // (x = (2^64-16)/31 + 1 gives x*31 = 2^64 + 15, low word 15 < 16).
    bool pass = lemire_threshold(2) == 0u && lemire_threshold(7) == 2u && lemire_threshold(30) == 16u &&
                lemire_threshold(31) == 16u && lemire_threshold(32) == 0u;
    std::uint64_t z = 0;
    const std::uint64_t rejected = 0xFFFFFFFFFFFFFFF0ull / 31u + 1u;
    pass = pass && !lemire_accept(rejected, 31, z);
    pass = pass && !lemire_accept(0, 31, z) && lemire_accept(0, 32, z) && z == 0;
    // Every j in [0, i] is reachable for every bound 2..32.
    for (std::uint64_t n = 2; n <= 32; ++n) {
      for (std::uint64_t j = 0; j < n; ++j) {
        z = 1000;
        pass = pass && lemire_accept(reach_x(n, j), n, z) && z == j;
      }
      pass = pass && lemire_accept(~std::uint64_t{0}, n, z) && z == n - 1;
    }
    out.push_back({"lemire_fisher_yates_thresholds_rejection_and_reachability", pass, ""});
  }

  {
    const bool pass = popcount32(0) == 0 && popcount32(0xFFFFFFFFu) == 32 && popcount32(0x80000001u) == 2 &&
                      popcount32(0x0F0F0F0Fu) == 16;
    out.push_back({"popcount32", pass, ""});
  }

  {
    // Published FNV-1a 32 vectors.
    const bool pass = fnv1a32(reinterpret_cast<const std::uint8_t*>(""), 0) == 0x811c9dc5u &&
                      fnv1a32(reinterpret_cast<const std::uint8_t*>("a"), 1) == 0xe40c292cu &&
                      fnv1a32(reinterpret_cast<const std::uint8_t*>("foobar"), 6) == 0xbf9cf968u;
    out.push_back({"fnv1a32_published_vectors", pass, ""});
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
    // DECOY_PERMUTATION: update 1..256, entity = step 1..31, subindex = retry 0..2^32-1.
    pass = pass && accepts(Purpose::DecoyPermutation, 0, 1, 1, 0);
    pass = pass && accepts(Purpose::DecoyPermutation, 41599, 256, 31, 0xFFFFFFFFull);
    pass = pass && !accepts(Purpose::DecoyPermutation, 0, 1, 0, 0);
    pass = pass && !accepts(Purpose::DecoyPermutation, 0, 1, 32, 0);
    pass = pass && !accepts(Purpose::DecoyPermutation, 0, 0, 1, 0);
    pass = pass && !accepts(Purpose::DecoyPermutation, 0, 257, 1, 0);
    pass = pass && !accepts(Purpose::DecoyPermutation, 0, 1, 1, 0x100000000ull);
    pass = pass && !accepts(Purpose::DecoyPermutation, 41600, 1, 1, 0);
    std::set<std::uint32_t> donor_entities;
    for (std::uint32_t slot = 0; slot < 32; ++slot) {
      for (std::uint32_t family = 0; family < 3; ++family) donor_entities.insert(3 * slot + family);
    }
    pass = pass && donor_entities.size() == 96 && *donor_entities.rbegin() == 95;
    out.push_back({"coordinate_schema_ranges_including_decoy_permutation_and_donor_injectivity", pass, ""});
  }

  // Fisher-Yates permutation fixtures driven by constructed draws.
  std::uint8_t identity_perm[32];
  std::uint8_t rotation_perm[32];
  std::uint8_t forced_perm[32];
  {
    std::vector<std::pair<unsigned, std::uint64_t>> requests;
    auto all_ones = [&requests](unsigned step, std::uint64_t retry) -> std::uint64_t {
      requests.push_back(std::make_pair(step, retry));
      return ~std::uint64_t{0};
    };
    std::uint64_t ax[31];
    std::uint32_t ar[31];
    fisher_yates(all_ones, identity_perm, ax, ar);
    bool pass = requests.size() == 31;
    for (unsigned k = 0; k < 31 && pass; ++k) {
      pass = requests[k].first == 31 - k && requests[k].second == 0 && ar[k] == 0 && ax[k] == ~std::uint64_t{0};
    }
    for (unsigned s = 0; s < 32; ++s) pass = pass && identity_perm[s] == s;
    out.push_back({"fisher_yates_x_all_ones_gives_identity_steps_31_to_1", pass, ""});
  }
  {
    auto ones = [](unsigned, std::uint64_t) -> std::uint64_t { return 1u; };
    std::uint64_t ax[31];
    std::uint32_t ar[31];
    fisher_yates(ones, rotation_perm, ax, ar);
    bool pass = true;
    for (unsigned s = 0; s < 32; ++s) pass = pass && rotation_perm[s] == ((s + 1u) & 31u);
    // Orientation: source bit s moves to destination perm[s] (here s+1 mod 32).
    pass = pass && apply_permutation(0x00000001u, rotation_perm) == 0x00000002u;
    pass = pass && apply_permutation(0x80000000u, rotation_perm) == 0x00000001u;
    pass = pass && apply_permutation(0x80000001u, rotation_perm) == 0x00000003u;
    out.push_back({"fisher_yates_x_one_gives_rotation_and_source_bit_to_destination_orientation", pass, ""});
  }
  {
    // Forced Lemire retry at step 30 (bound 31): retry 0 has x = 0 (rejected), retry 1 has
    // x = 2^63 (31*2^63 = 15*2^64 + 2^63, accepted, j = 15). Every other step has j = i.
    std::vector<std::pair<unsigned, std::uint64_t>> requests;
    auto forced = [&requests](unsigned step, std::uint64_t retry) -> std::uint64_t {
      requests.push_back(std::make_pair(step, retry));
      if (step == 30 && retry == 0) return 0u;
      if (step == 30 && retry == 1) return 1ull << 63;
      return ~std::uint64_t{0};
    };
    std::uint64_t ax[31];
    std::uint32_t ar[31];
    fisher_yates(forced, forced_perm, ax, ar);
    bool pass = requests.size() == 32 && requests[0] == std::make_pair(31u, std::uint64_t{0}) &&
                requests[1] == std::make_pair(30u, std::uint64_t{0}) &&
                requests[2] == std::make_pair(30u, std::uint64_t{1});
    for (unsigned k = 3; k < requests.size() && pass; ++k) {
      pass = requests[k].first == 32 - k && requests[k].second == 0;
    }
    std::uint64_t retry_total = 0;
    unsigned retry_steps = 0;
    for (unsigned k = 0; k < 31; ++k) {
      retry_total += ar[k];
      if (ar[k] != 0) ++retry_steps;
    }
    pass = pass && ar[1] == 1 && ax[1] == (1ull << 63) && retry_total == 1 && retry_steps == 1;
    for (unsigned s = 0; s < 32; ++s) {
      const unsigned want = s == 15 ? 30u : (s == 30 ? 15u : s);
      pass = pass && forced_perm[s] == want;
    }
    pass = pass && apply_permutation(1u << 30, forced_perm) == (1u << 15) &&
           apply_permutation(1u << 15, forced_perm) == (1u << 30);
    out.push_back({"fisher_yates_forced_lemire_retry_exact_requests_and_permutation", pass, ""});
  }
  {
    // Small exhaustive analogues: every choice sequence of a 4- and 5-position Fisher-Yates gives a
    // distinct permutation (exactly 4! and 5!), and the 5-position image of each weight-2 mask is
    // uniform over the ten weight-2 subsets (uniform subset law).
    bool pass = true;
    std::set<std::uint32_t> perms4;
    for (unsigned j3 = 0; j3 < 4; ++j3) {
      for (unsigned j2 = 0; j2 < 3; ++j2) {
        for (unsigned j1 = 0; j1 < 2; ++j1) {
          const unsigned choice[4] = {0, j1, j2, j3};
          auto src = [&choice](unsigned step, std::uint64_t) -> std::uint64_t {
            return reach_x(step + 1u, choice[step]);
          };
          std::uint8_t p[4];
          std::uint64_t ax[3];
          std::uint32_t ar[3];
          fisher_yates(src, p, ax, ar);
          perms4.insert(static_cast<std::uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24)));
        }
      }
    }
    pass = pass && perms4.size() == 24;
    std::set<std::uint64_t> perms5;
    std::map<std::uint32_t, std::map<std::uint32_t, unsigned>> images;
    for (unsigned j4 = 0; j4 < 5; ++j4) {
      for (unsigned j3 = 0; j3 < 4; ++j3) {
        for (unsigned j2 = 0; j2 < 3; ++j2) {
          for (unsigned j1 = 0; j1 < 2; ++j1) {
            const unsigned choice[5] = {0, j1, j2, j3, j4};
            auto src = [&choice](unsigned step, std::uint64_t) -> std::uint64_t {
              return reach_x(step + 1u, choice[step]);
            };
            std::uint8_t p[5];
            std::uint64_t ax[4];
            std::uint32_t ar[4];
            fisher_yates(src, p, ax, ar);
            std::uint64_t code = 0;
            for (unsigned s = 0; s < 5; ++s) code |= static_cast<std::uint64_t>(p[s]) << (8u * s);
            perms5.insert(code);
            for (std::uint32_t m = 0; m < 32; ++m) {
              if (popcount32(m) == 2) ++images[m][apply_permutation(m, p)];
            }
          }
        }
      }
    }
    pass = pass && perms5.size() == 120 && images.size() == 10;
    for (const auto& entry : images) {
      pass = pass && entry.second.size() == 10;
      for (const auto& count : entry.second) pass = pass && popcount32(count.first) == 2 && count.second == 12;
    }
    out.push_back({"fisher_yates_exhaustive_4_and_5_position_analogues_and_uniform_subset_law", pass, ""});
  }
  {
    // Decoy operator: parent distance equals cache distance for every displacement weight 0..32;
    // weights 0 and 32 are unchanged by any permutation (direction retained); the shared permutation
    // preserves Hamming weights, pairwise overlaps and XOR structure of displacement masks.
    std::uint8_t mixed_perm[32];
    auto mixed = [](unsigned step, std::uint64_t) -> std::uint64_t {
      return reach_x(step + 1u, (7u * step + 3u) % (step + 1u));
    };
    std::uint64_t ax[31];
    std::uint32_t ar[31];
    fisher_yates(mixed, mixed_perm, ax, ar);
    std::uint8_t (*perms[3])[32] = {&rotation_perm, &forced_perm, &mixed_perm};
    bool pass = true;
    const std::uint32_t parent = 0x5A5A5A5Au;
    for (unsigned which = 0; which < 3; ++which) {
      const std::uint8_t (&p)[32] = *perms[which];
      for (unsigned w = 0; w <= 32; ++w) {
        const std::uint32_t low = w == 32 ? 0xFFFFFFFFu : ((1u << w) - 1u);
        const std::uint32_t d = rotl32(low, (5u * w) & 31u);
        const std::uint32_t cache = parent ^ d;
        const Probe probe = policy_probe(kArmNoninfo, 1, 1, parent, cache, 0x0F0F0000u, true, p);
        pass = pass && probe.source == kSourceDecoy && probe.mask == apply_permutation(d, p) &&
               (probe.genotype ^ parent) == probe.mask && popcount32(probe.mask) == w &&
               popcount32(probe.genotype ^ parent) == popcount32(cache ^ parent);
        if (w == 0 || w == 32) pass = pass && probe.genotype == cache;
      }
      static const std::uint32_t kMasks[8] = {0x0000FFFFu, 0x00FF00FFu, 0x0F0F0F0Fu, 0x33333333u,
                                              0x55555555u, 0x80000001u, 0xFFFFFFFFu, 0x00000000u};
      for (unsigned a = 0; a < 8; ++a) {
        for (unsigned b = 0; b < 8; ++b) {
          const std::uint32_t pa = apply_permutation(kMasks[a], p), pb = apply_permutation(kMasks[b], p);
          pass = pass && popcount32(pa & pb) == popcount32(kMasks[a] & kMasks[b]) &&
                 apply_permutation(kMasks[a] ^ kMasks[b], p) == (pa ^ pb);
        }
      }
    }
    // A nontrivial permutation does move some weight-1 displacement.
    pass = pass && apply_permutation(1u << 30, forced_perm) != (1u << 30);
    out.push_back({"decoy_weights_0_to_32_distance_matched_weights_0_and_32_retained_overlaps_preserved", pass, ""});
  }
  {
    // Probe rule table (section 5 item 2) with the rotation permutation, parent x and cache c = x ^ 1.
    const std::uint32_t x = 0x12345678u, c = x ^ 0x1u, fresh = 0x0F0F0000u;
    bool pass = true;
    const bool recurrent_cases[2] = {false, true};
    for (const bool rec : recurrent_cases) {
      const Probe info = policy_probe(kArmInfo, 1, 1, x, c, fresh, rec, rotation_perm);
      pass = pass && info.genotype == c && info.source == kSourceTrueCache && info.mask == (c ^ x);
      const Probe sham = policy_probe(kArmSham, 1, 1, x, c, fresh, rec, rotation_perm);
      pass = pass && sham.genotype == (x ^ fresh) && sham.source == kSourceFresh && sham.mask == fresh;
      for (std::uint32_t arm = 0; arm < 3; ++arm) {
        // Invalid-cache M, and F (whatever the cache flag), use the fresh proposal in every arm.
        const Probe invalid_m = policy_probe(arm, 1, 0, x, 0u, fresh, rec, rotation_perm);
        const Probe label_f = policy_probe(arm, 0, 1, x, c, fresh, rec, rotation_perm);
        pass = pass && invalid_m.genotype == (x ^ fresh) && invalid_m.source == kSourceFresh;
        pass = pass && label_f.genotype == (x ^ fresh) && label_f.source == kSourceFresh;
      }
    }
    // NONINFO equals INFO off recurrence (t < 3, or R_t = 0) and differs at a recurrent update.
    const Probe off = policy_probe(kArmNoninfo, 1, 1, x, c, fresh, false, rotation_perm);
    pass = pass && off.genotype == c && off.source == kSourceTrueCache;
    const Probe on = policy_probe(kArmNoninfo, 1, 1, x, c, fresh, true, rotation_perm);
    pass = pass && on.genotype == (x ^ 0x2u) && on.genotype != c && on.source == kSourceDecoy && on.mask == 0x2u;
    // Recurrence gate: t >= 3 and R_t = 1.
    pass = pass && !recurrence_applied_at(1, 1) && !recurrence_applied_at(2, 1) && !recurrence_applied_at(3, 0) &&
           recurrence_applied_at(3, 1) && recurrence_applied_at(256, 1) && !recurrence_applied_at(256, 0);
    out.push_back({"probe_rule_info_noninfo_equal_off_recurrence_decoy_on_recurrence_sham_fresh", pass, ""});
  }
  {
    // HALF law: T1 = I1, T2 = I2 (R ignored), T3 = T1 (R3 = 1), T4 = I4 (R4 = 0), T5 = T3, T6 = T4.
    std::uint32_t innovation[7] = {0, 0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u, 0x55555555u, 0x66666666u};
    std::uint32_t copy[7] = {0, 1, 1, 1, 0, 1, 1};
    std::uint32_t target[7] = {};
    std::uint8_t applied[7] = {};
    half_targets(innovation, copy, 6, target, applied);
    const bool pass = target[1] == innovation[1] && target[2] == innovation[2] && target[3] == innovation[1] &&
                      target[4] == innovation[4] && target[5] == innovation[1] && target[6] == innovation[4] &&
                      applied[1] == 0 && applied[2] == 0 && applied[3] == 1 && applied[4] == 0 && applied[5] == 1 &&
                      applied[6] == 1;
    out.push_back({"half_target_law_and_recurrence_indicator", pass, ""});
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
    fatal(ctx, "input " + label + " size differs from its documented record size");
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

// Replays one block and compares, in file order, the 256 shared permutation records (if perms !=
// nullptr), every audit row (if audit != nullptr), update record, path record and the block record.
void replay_block(Context& ctx, const KeyedStream& ks, std::uint32_t block, InputFile& updates, InputFile& paths,
                  InputFile* blocks_file, const std::uint8_t* preread_block, InputFile* audit, InputFile* perms) {
  std::unique_ptr<BlockReplay> replay(new BlockReplay(ks, block));
  std::unique_ptr<StepOutput> out(new StepOutput());
  std::vector<std::uint8_t> observed_rows(kCandidates * kAuditBytes);
  std::uint8_t expected_row[kAuditBytes];
  std::uint8_t expected_perm[kPermBytes], observed_perm[kPermBytes];
  std::uint8_t expected_update[kUpdateBytes], observed_update[kUpdateBytes];
  std::uint8_t expected_path[kPathBytes], observed_path[kPathBytes];
  std::uint8_t expected_block[kBlockBytes], observed_block[kBlockBytes];
  std::string diag;

  if (perms != nullptr) {
    for (std::uint32_t t = 1; t <= kUpdatesPerPath; ++t) {
      encode_perm(replay->permutation_record(t), expected_perm);
      if (!perms->read_exact(observed_perm, kPermBytes)) {
        fatal(ctx, "block=" + std::to_string(block) + " update=" + std::to_string(t) +
                       ": permutation file ended before this record");
      }
      if (!compare_fields("audit_permutation", kPermFields, expected_perm, observed_perm, kPermBytes, diag)) {
        ctx.disc.add("block=" + std::to_string(block) + " update=" + std::to_string(t) + " " + diag);
      }
      ++ctx.perm_records;
      check_cap(ctx);
    }
  }

  for (std::uint32_t cell = 0; cell < kCells; ++cell) {
    replay->begin_cell(cell);
    for (std::uint32_t t = 1; t <= kUpdatesPerPath; ++t) {
      replay->step(t, *out);
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
    const PathRecord path = replay->end_cell();
    encode_path(path, expected_path);
    if (!paths.read_exact(observed_path, kPathBytes)) fatal(ctx, where(block, cell, 0) + ": path file ended early");
    if (!compare_fields("path_record", kPathFields, expected_path, observed_path, kPathBytes, diag)) {
      ctx.disc.add("block=" + std::to_string(block) + " cell=" + std::to_string(cell) + " " + diag);
    }
    ++ctx.paths;
    check_cap(ctx);
  }

  BlockIdentities ids;
  const BlockRecord expected = replay->end_block(ids);
  const std::string b = "block=" + std::to_string(block);
  if (!ids.n1) ctx.disc.add(b + ": auditor replay violates N1 (SHAM start identity)");
  if (!ids.n2) ctx.disc.add(b + ": auditor replay violates N2 (SHAM label complement)");
  if (!ids.sham_paired_hash_equal) ctx.disc.add(b + ": auditor SHAM paired-trajectory hashes differ");
  if (!ids.c1) ctx.disc.add(b + ": auditor replay violates C1 (INFO/NONINFO coupling identity)");
  encode_block(expected, expected_block);
  if (preread_block != nullptr) {
    std::memcpy(observed_block, preread_block, kBlockBytes);
  } else if (blocks_file == nullptr || !blocks_file->read_exact(observed_block, kBlockBytes)) {
    fatal(ctx, b + ": block file ended early");
  }
  if (!compare_fields("block_record", kBlockFields, expected_block, observed_block, kBlockBytes, diag)) {
    ctx.disc.add(b + " " + diag);
  }
  ++ctx.blocks;
  check_cap(ctx);
}

void run_production(Context& ctx, const std::string& run_dir) {
  ctx.mode = "production";
  ctx.key_namespace = namespace_text(KeyNamespace::Production);
  ctx.replayed_blocks = "0-63";
  ctx.exp_blocks = kProductionAuditBlocks;                                       // 64
  ctx.exp_paths = kProductionAuditBlocks * kCells;                               // 384
  ctx.exp_update_records = kProductionAuditBlocks * kCells * kUpdatesPerPath;    // 98,304
  ctx.exp_audit_rows = kProductionAuditBlocks * kAuditBlockRows;                 // 12,582,912
  ctx.exp_perm_records = kProductionAuditBlocks * kPermRecordsPerBlock;          // 16,384

  const std::string shard = run_dir + "/shards/shard_00/";
  InputFile& updates = open_input(ctx, "shards/shard_00/updates.bin", shard + "updates.bin", kShardUpdateBytes);
  InputFile& paths = open_input(ctx, "shards/shard_00/paths.bin", shard + "paths.bin", kShardPathBytes);
  InputFile& blocks = open_input(ctx, "shards/shard_00/blocks.bin", shard + "blocks.bin", kShardBlockBytes);
  InputFile& audit = open_input(ctx, std::string("shards/shard_00/") + kAuditRowsFile, shard + kAuditRowsFile,
                                kProductionAuditBytes);
  InputFile& perms = open_input(ctx, std::string("shards/shard_00/") + kAuditPermFile, shard + kAuditPermFile,
                                kProductionPermBytes);

  const KeyedStream ks(KeyNamespace::Production);
  ctx.production_keys_derived = true;
  check_purpose_keys(ctx, ks);

  for (std::uint32_t block = 0; block < kProductionAuditBlocks; ++block) {
    replay_block(ctx, ks, block, updates, paths, &blocks, nullptr, &audit, &perms);
  }
  if (!audit.at_eof()) fatal(ctx, "audit row file has bytes after block 63");
  if (!perms.at_eof()) fatal(ctx, "audit permutation file has bytes after block 63");
  // Records for blocks 64..1299 of shard_00 are outside the replay scope; finish() hashes them.
}

void run_fixture(Context& ctx, const std::string& layout_dir) {
  ctx.mode = "fixture";
  ctx.key_namespace = namespace_text(KeyNamespace::Fixture);
  ctx.exp_blocks = 1;
  ctx.exp_paths = kCells;
  ctx.exp_update_records = kCells * kUpdatesPerPath;
  ctx.exp_audit_rows = kAuditBlockRows;
  ctx.exp_perm_records = kPermRecordsPerBlock;

  const char* const names[6] = {kFixtureReadme, kFixtureAudit, kFixturePerm, kFixtureBlock, kFixturePaths,
                                kFixtureUpdates};
  DIR* dir = ::opendir(layout_dir.c_str());
  if (dir == nullptr) fatal(ctx, "cannot open fixture layout directory " + layout_dir);
  std::set<std::string> present;
  for (struct dirent* e = ::readdir(dir); e != nullptr; e = ::readdir(dir)) {
    const std::string name = e->d_name;
    if (name != "." && name != "..") present.insert(name);
  }
  ::closedir(dir);
  const std::set<std::string> wanted(names, names + 6);
  if (present != wanted) {
    std::string listing;
    for (const std::string& n : present) listing += (listing.empty() ? "" : ",") + n;
    fatal(ctx, "fixture layout directory must contain exactly the six documented files; found: " + listing);
  }

  const std::string base = layout_dir + "/";
  InputFile& readme = open_input(ctx, kFixtureReadme, base + kFixtureReadme, kAnySize);
  (void)readme;
  InputFile& block_file = open_input(ctx, kFixtureBlock, base + kFixtureBlock, kBlockBytes);
  std::uint8_t block_record[kBlockBytes];
  if (!block_file.read_exact(block_record, kBlockBytes)) fatal(ctx, "fixture block file is short");
  if (!block_file.at_eof()) fatal(ctx, "fixture block file has trailing bytes");
  const std::uint64_t block = get_le(block_record, 4);
  if (block > kMaxBlock) fatal(ctx, "fixture block id is outside 0..41599");
  ctx.replayed_blocks = std::to_string(block);

  InputFile& updates =
      open_input(ctx, kFixtureUpdates, base + kFixtureUpdates, static_cast<std::uint64_t>(kCells) * kUpdatesPerPath * kUpdateBytes);
  InputFile& paths = open_input(ctx, kFixturePaths, base + kFixturePaths, static_cast<std::uint64_t>(kCells) * kPathBytes);
  InputFile& audit = open_input(ctx, kFixtureAudit, base + kFixtureAudit, kAuditBlockRows * kAuditBytes);
  InputFile& perms = open_input(ctx, kFixturePerm, base + kFixturePerm, kPermRecordsPerBlock * kPermBytes);

  const KeyedStream ks(KeyNamespace::Fixture);
  ctx.fixture_keys_derived = true;
  check_purpose_keys(ctx, ks);

  replay_block(ctx, ks, static_cast<std::uint32_t>(block), updates, paths, nullptr, block_record, &audit, &perms);
  if (!updates.at_eof()) fatal(ctx, "fixture update file has trailing bytes");
  if (!paths.at_eof()) fatal(ctx, "fixture path file has trailing bytes");
  if (!audit.at_eof()) fatal(ctx, "fixture audit row file has trailing bytes");
  if (!perms.at_eof()) fatal(ctx, "fixture audit permutation file has trailing bytes");
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
  j << "  \"study_id\": \"PHASE2-PERFORMANCE-CONVERSION-002\",\n";
  j << "  \"tool\": " << json_string(kToolName) << ",\n";
  j << "  \"tool_version\": " << json_string(kToolVersion) << ",\n";
  j << "  \"compiler_version\": " << json_string(__VERSION__) << ",\n";
  j << "  \"cplusplus\": " << __cplusplus << ",\n";
  j << "  \"executable_sha256\": " << (executable_sha256.empty() ? "null" : json_string(executable_sha256)) << ",\n";
  j << "  \"mode\": " << json_string(ctx.mode) << ",\n";
  j << "  \"key_namespace\": " << json_string(ctx.key_namespace) << ",\n";
  j << "  \"production_namespace_stream_constructed\": " << (ctx.production_keys_derived ? "true" : "false") << ",\n";
  j << "  \"fixture_namespace_stream_constructed\": " << (ctx.fixture_keys_derived ? "true" : "false") << ",\n";
  j << "  \"namespace_separation_key_words_hashed\": " << (ctx.namespace_separation_hashed ? "true" : "false")
    << ",\n";
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
    << ", \"update_records\": " << ctx.update_records << ", \"audit_rows\": " << ctx.audit_rows
    << ", \"permutation_records\": " << ctx.perm_records << "},\n";
  j << "  \"expected\": {\"blocks\": " << ctx.exp_blocks << ", \"path_records\": " << ctx.exp_paths
    << ", \"update_records\": " << ctx.exp_update_records << ", \"audit_rows\": " << ctx.exp_audit_rows
    << ", \"permutation_records\": " << ctx.exp_perm_records << "},\n";
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
  ctx.kats = run_known_answer_tests(kat_only);
  ctx.namespace_separation_hashed = kat_only;
  ctx.kats_pass = true;
  for (const KatOutcome& k : ctx.kats) ctx.kats_pass = ctx.kats_pass && k.pass;

  if (!ctx.kats_pass) {
    for (const KatOutcome& k : ctx.kats) {
      if (!k.pass) {
        ctx.disc.add("known-answer test failed: " + k.name);
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
                               ctx.update_records == ctx.exp_update_records &&
                               ctx.audit_rows == ctx.exp_audit_rows && ctx.perm_records == ctx.exp_perm_records;
  const bool pass = ctx.kats_pass && ctx.disc.count == 0 && !ctx.disc.capped && counts_complete && !any_read_error &&
                    (ctx.mode == "kat-only" || ctx.mode == "production" || ctx.mode == "fixture");
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
