// Frozen constants for PHASE2-MUTABLE-MEMORY-001 revision 1.
// Every value is also encoded in config/frozen_config.json and cross-checked
// by config_check.hpp before production. No value here is a tunable option.
#pragma once

#include <cstdint>

namespace mm {

constexpr const char* kStudyId = "PHASE2-MUTABLE-MEMORY-001";
constexpr const char* kSpecSha256 = "c17a3a1e9ac9cf2260f6743eaf08e4c2808080a767af16d2f9e9d2b9ad3e821d";
constexpr const char* kTerminalReviewSha256 = "787cf32a20312f2682c761243f3dec04c5eb8036d14d6efc444e9830db37960a";
constexpr const char* kProductionNamespace = "production-r1";
// Fixtures and timing benchmarks use a different key namespace, so they can
// never reproduce a production random value or a scientific block.
constexpr const char* kFixtureNamespace = "fixture-r1-nonscientific";
constexpr const char* kProductionAck = "I_ACKNOWLEDGE_SINGLE_FROZEN_PRODUCTION_RUN_PHASE2-MUTABLE-MEMORY-001-R1";
constexpr const char* kPostProductionAuditAck = "I_ACKNOWLEDGE_POST_PRODUCTION_REGENERATION_AUDIT";

constexpr std::uint32_t kPopulation = 32U;
constexpr std::uint32_t kGenotypeBits = 32U;
constexpr std::uint32_t kFamilies = 4U;
constexpr std::uint32_t kCandidates = kFamilies * kPopulation;
constexpr std::uint32_t kDonorFamilies = 3U;
constexpr std::uint32_t kSurvivors = kPopulation;
constexpr std::uint32_t kFirstUpdate = 1U;
constexpr std::uint32_t kLastUpdate = 256U;
constexpr std::uint32_t kUpdates = 256U;
constexpr std::uint32_t kLateFirst = 193U;
constexpr std::uint32_t kLateLast = 256U;
constexpr std::uint32_t kLateLength = 64U;
constexpr std::uint32_t kBlocks = 41600U;
constexpr std::uint32_t kCells = 8U;
constexpr std::uint64_t kTotalPaths = 332800ULL;
constexpr std::uint64_t kTotalPathUpdates = 85196800ULL;
constexpr std::uint64_t kTotalQueries = 10905190400ULL;
constexpr std::uint32_t kAuditBlocks = 64U;
constexpr std::uint64_t kAuditPaths = 512ULL;
constexpr std::uint64_t kAuditRows = 16777216ULL;
constexpr std::uint32_t kShards = 32U;
constexpr std::uint32_t kBlocksPerShard = 1300U;
constexpr std::uint32_t kMaxThreads = 32U;
constexpr std::uint64_t kMaxOutputBytes = 100ULL * 1024ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kMaxWeight = 1ULL << 32U;
constexpr std::uint64_t kMaxWeightSum = 1ULL << 39U;
constexpr std::uint64_t kMaxSurvivalRetry = 0xFFFFFFFFULL;
constexpr std::uint8_t kLabelF = 0U;
constexpr std::uint8_t kLabelM = 1U;
constexpr std::uint8_t kNotApplicable = 255U;

static_assert(kCandidates == 128U, "128 candidates per update");
static_assert(static_cast<std::uint64_t>(kBlocks) * kCells == kTotalPaths, "path count");
static_assert(kTotalPaths * kUpdates == kTotalPathUpdates, "path-update count");
static_assert(kTotalPathUpdates * kCandidates == kTotalQueries, "query count");
static_assert(static_cast<std::uint64_t>(kAuditBlocks) * kCells == kAuditPaths, "audit paths");
static_assert(kAuditPaths * kUpdates * kCandidates == kAuditRows, "audit rows");
static_assert(kShards * kBlocksPerShard == kBlocks, "shard partition");
static_assert(kAuditBlocks <= kBlocksPerShard, "audit blocks lie in shard 0");
static_assert(kLateLast - kLateFirst + 1U == kLateLength, "late window");
static_assert(kCandidates * kMaxWeight == kMaxWeightSum, "maximum weight sum 2^39");

enum class Family : std::uint8_t { kParent = 0, kPolicyProbe = 1, kGlobalScout = 2, kLocalChild = 3 };
enum class Arm : std::uint8_t { kActive = 0, kSham = 1 };
enum class Law : std::uint8_t { kZero = 0, kHalf = 1 };
enum class Start : std::uint8_t { kAllF = 0, kAllM = 1 };

struct CellSpec {
  Arm arm;
  Law law;
  Start start;
};

// cell = 4*arm + 2*law + start (see config design.cell_index_formula).
inline CellSpec cell_spec(std::uint32_t cell) {
  return CellSpec{(cell & 4U) != 0U ? Arm::kSham : Arm::kActive, (cell & 2U) != 0U ? Law::kHalf : Law::kZero,
                  (cell & 1U) != 0U ? Start::kAllM : Start::kAllF};
}

inline const char* cell_name(std::uint32_t cell) {
  static const char* const kNames[kCells] = {"ACTIVE|ZERO|ALL_F", "ACTIVE|ZERO|ALL_M", "ACTIVE|HALF|ALL_F",
                                             "ACTIVE|HALF|ALL_M", "SHAM|ZERO|ALL_F",   "SHAM|ZERO|ALL_M",
                                             "SHAM|HALF|ALL_F",   "SHAM|HALF|ALL_M"};
  return cell < kCells ? kNames[cell] : "INVALID_CELL";
}

}  // namespace mm
