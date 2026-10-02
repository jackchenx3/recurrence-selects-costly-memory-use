// Frozen constants for PHASE2-PERFORMANCE-CONVERSION-002 revision 1, adapted in
// place from the accepted PHASE2-MUTABLE-MEMORY-001 producer. Every value is also
// encoded in config/frozen_config.json and cross-checked by config_check.hpp
// before production. No value here is a tunable option.
#pragma once

#include <cstdint>

#include "mm/fatal.hpp"

namespace mm {

constexpr const char* kStudyId = "PHASE2-PERFORMANCE-CONVERSION-002";
constexpr const char* kSpecSha256 = "58ee414d753fab042e6cccee1a4e6b95dd7f167633b4f33c1491687d9afcb895";
constexpr const char* kTerminalReviewSha256 = "76b0c6172e61f60befecef183e83d9e43ea50c1b6bc1e93299668b76f55be60f";
// SHA-256 of the exact DESIGN_002_GO.json bytes, inserted by the supervisor's
// deterministic provenance patch after construction. No package command ran.
// The production driver refuses to start if this is not 64 lowercase hex digits.
constexpr const char* kDesignGoSha256 = "f4a6180109e0d212cbe784a7722a64576e5e7cb0fbfd4b23c757bb5e7ab84d6a";
constexpr const char* kDesignGoRecord = "PHASE2-PERFORMANCE-CONVERSION-002-DESIGN-GO-1";
constexpr const char* kPredecessorStudyId = "PHASE2-MUTABLE-MEMORY-001";
constexpr const char* kProductionNamespace = "production-r1";
// Fixtures and timing benchmarks use a different key namespace, so they can
// never reproduce a production random value or a scientific block.
constexpr const char* kFixtureNamespace = "fixture-r1-nonscientific";
constexpr const char* kProductionAck = "I_ACKNOWLEDGE_SINGLE_FROZEN_PRODUCTION_RUN_PHASE2-PERFORMANCE-CONVERSION-002-R1";
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
constexpr std::uint32_t kCells = 6U;
constexpr std::uint64_t kTotalPaths = 249600ULL;
constexpr std::uint64_t kTotalPathUpdates = 63897600ULL;
constexpr std::uint64_t kTotalQueries = 8178892800ULL;
constexpr std::uint32_t kAuditBlocks = 64U;
constexpr std::uint64_t kAuditPaths = 384ULL;
constexpr std::uint64_t kAuditRows = 12582912ULL;
constexpr std::uint64_t kAuditPermutationRecords = 16384ULL;  // one per (audit block, update)
constexpr std::uint32_t kSavedEstimates = 19U;
constexpr std::uint32_t kShards = 32U;
constexpr std::uint32_t kBlocksPerShard = 1300U;
constexpr std::uint32_t kMaxThreads = 32U;
constexpr std::uint64_t kMaxOutputBytes = 100ULL * 1024ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kMaxWeight = 1ULL << 32U;
constexpr std::uint64_t kMaxWeightSum = 1ULL << 39U;
constexpr std::uint64_t kMaxSurvivalRetry = 0xFFFFFFFFULL;
constexpr std::uint64_t kMaxDecoyRetry = 0xFFFFFFFFULL;
constexpr std::uint32_t kPermFirstStep = 31U;  // Fisher-Yates i = 31, ..., 1
constexpr std::uint32_t kPermSteps = 31U;
constexpr std::uint8_t kLabelF = 0U;
constexpr std::uint8_t kLabelM = 1U;
constexpr std::uint8_t kNotApplicable = 255U;

static_assert(kCandidates == 128U, "128 candidates per update");
static_assert(static_cast<std::uint64_t>(kBlocks) * kCells == kTotalPaths, "path count");
static_assert(kTotalPaths * kUpdates == kTotalPathUpdates, "path-update count");
static_assert(kTotalPathUpdates * kCandidates == kTotalQueries, "query count");
static_assert(static_cast<std::uint64_t>(kAuditBlocks) * kCells == kAuditPaths, "audit paths");
static_assert(kAuditPaths * kUpdates * kCandidates == kAuditRows, "audit rows");
static_assert(static_cast<std::uint64_t>(kAuditBlocks) * kUpdates == kAuditPermutationRecords, "audit permutations");
static_assert(kShards * kBlocksPerShard == kBlocks, "shard partition");
static_assert(kAuditBlocks <= kBlocksPerShard, "audit blocks lie in shard 0");
static_assert(kLateLast - kLateFirst + 1U == kLateLength, "late window");
static_assert(kCandidates * kMaxWeight == kMaxWeightSum, "maximum weight sum 2^39");
static_assert(kPermSteps == kGenotypeBits - 1U && kPermFirstStep == kGenotypeBits - 1U, "Fisher-Yates steps 31..1");
static_assert(3U + 2U + 2U + 2U * kCells == kSavedEstimates, "3 primary + 2 allele + 2 performance + 12 cell means");

enum class Family : std::uint8_t { kParent = 0, kPolicyProbe = 1, kGlobalScout = 2, kLocalChild = 3 };
// INFO is the accepted study-001 ACTIVE operator; NONINFO is the frozen
// recurrence-gated decoy operator; SHAM is the accepted fresh-mask operator.
enum class Arm : std::uint8_t { kInfo = 0, kNoninfo = 1, kSham = 2 };
enum class Start : std::uint8_t { kAllF = 0, kAllM = 1 };
// Policy-probe source recorded per parent: fresh mask, true cache, or decoy.
enum class ProbeSource : std::uint8_t { kFresh = 0, kTrueCache = 1, kDecoy = 2 };

struct CellSpec {
  Arm arm;
  Start start;
};

// cell = 2*arm + start (see config design.cell_index_formula). The single HALF
// law is not a cell coordinate.
inline CellSpec cell_spec(std::uint32_t cell) {
  require(cell < kCells, "cell outside 0..5");
  return CellSpec{static_cast<Arm>(cell >> 1U), (cell & 1U) != 0U ? Start::kAllM : Start::kAllF};
}

inline const char* cell_name(std::uint32_t cell) {
  static const char* const kNames[kCells] = {"INFO|ALL_F",    "INFO|ALL_M", "NONINFO|ALL_F",
                                             "NONINFO|ALL_M", "SHAM|ALL_F", "SHAM|ALL_M"};
  return cell < kCells ? kNames[cell] : "INVALID_CELL";
}

}  // namespace mm
