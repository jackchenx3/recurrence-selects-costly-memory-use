// PHASE2-TORUS-MEMORY-003 frozen constants.
// Every scientific count and identity is compiled in and is also bound by
// config/torus_003_frozen_config.json (see config_check.cpp). There are no
// optional scientific modes.
#pragma once

#include <cstdint>

namespace torus {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

// ---- model ---------------------------------------------------------------
constexpr int kPop = 32;            // individuals / parent slots / survivor slots
constexpr int kLoci = 32;           // 16-bit circular coordinates per phenotype
constexpr int kUpdates = 256;       // updates 1..256
constexpr int kLateFirst = 193;     // late window 193..256 inclusive
constexpr int kLateLast = 256;
constexpr int kLateLen = 64;
constexpr int kFamilies = 4;        // parent, policy probe, scout, local child
constexpr int kCandidates = 128;    // 32*family + parent_slot
constexpr int kEntries = 4;         // tournament entries per survivor slot
constexpr int kDonorEntities = 96;  // 3*parent + family, family 0..2

constexpr u64 kLocalReplaceMask = 31;     // replace iff lane & 31 == 0
constexpr u64 kPolicyMutationMask = 31;   // flip iff word0 & 31 == 0
constexpr u64 kTournamentEntryMask = 127; // entry = word0 & 127

constexpr u64 kMaxCoordLoss = 1ULL << 30;
constexpr u64 kMaxIndividualLoss = 1ULL << 35;
constexpr u64 kMaxPopulationLoss = 1ULL << 40;

// ---- study counts --------------------------------------------------------
constexpr u32 kBlocks = 41600;
constexpr int kCells = 8;
constexpr u64 kPaths = 332800ULL;
constexpr u64 kPathUpdates = 85196800ULL;
constexpr u64 kObjectiveQueries = 10905190400ULL;
constexpr u32 kChunkBlocks = 64;
constexpr u32 kChunks = 650;              // 650 * 64 = 41600
constexpr u32 kAuditBlocks = 64;          // blocks 0..63 == chunk 0
constexpr u64 kAuditPaths = 512ULL;
constexpr u64 kAuditPathUpdates = 131072ULL;
constexpr u64 kAuditCandidateRows = 16777216ULL;
constexpr u64 kAuditEntryRows = 16777216ULL;
constexpr int kEstimateRecords = 22;

// Declared Threefry calls per block (see collision.cpp):
// 64 initial + 256 * (2 innovation + 1 copy + 4*64 vector purposes
//                     + 96 donor + 128 entry + 128 tie + 32 mutation)
constexpr u64 kCallsPerUpdate = 2 + 1 + 64 * 4 + 96 + 128 + 128 + 32;  // 643
constexpr u64 kCallsPerBlock = 64 + 256 * kCallsPerUpdate;            // 164672
constexpr u64 kCallsTotal = kCallsPerBlock * kBlocks;                  // 6850355200

static_assert(kChunks * kChunkBlocks == kBlocks, "chunking");
static_assert(kPaths == u64(kBlocks) * kCells, "paths");
static_assert(kPathUpdates == kPaths * kUpdates, "path-updates");
static_assert(kObjectiveQueries == kPathUpdates * kCandidates, "queries");
static_assert(kAuditCandidateRows == u64(kAuditBlocks) * kCells * kUpdates * kCandidates, "audit candidates");
static_assert(kAuditEntryRows == u64(kAuditBlocks) * kCells * kUpdates * kPop * kEntries, "audit entries");
static_assert(kCallsPerUpdate == 643, "calls per update");
static_assert(kCallsTotal == 6850355200ULL, "calls total");

// ---- identities ----------------------------------------------------------
constexpr const char* kStudyTag = "PHASE2-TORUS-MEMORY-003";
constexpr const char* kProductionNamespace = "production-r1";
constexpr const char* kFixtureNamespace = "fixture-r1";
constexpr const char* kTimingNamespace = "timing-r1";

constexpr const char* kDesignSha256 =
    "0ff662dc31807b0b2cdcae371d884473ed398e2583272d94331bac8c8f9ff0b0";
constexpr const char* kDesignGoSha256 =
    "e5dc485dd858f18eb9652cfe6b63f142e2d09fb6b069d640438cd8dbe63a786d";
constexpr const char* kTerminalReviewSha256 =
    "68ccffc3c9a4d013590e7b5c8e5f4200ce955b77a76554bd58bad4bcc7dcc04a";
constexpr const char* kSeedReceiptSha256 =
    "3fe28150db873b0d6ee1e7313de14f271d27d81e7e3ecf1e97f1677aba66dd53";
constexpr const char* kRandom123Commit = "9545ff6413f258be2f04c1d319d99aaef7521150";
constexpr const char* kThreefryHeaderRel = "third_party/random123/include/Random123/threefry.h";
constexpr const char* kThreefryHeaderSha256 =
    "4c210b32b5ba605b059c54d5edd6f01bf04190de49a0abeecec76420cd072a72";
constexpr const char* kKatVectorsRel = "third_party/random123/tests/kat_vectors";
constexpr const char* kKatVectorsSha256 =
    "aab5ebabf40003f63d6d87b24cbd2c8a02652e00cf8bad64226fd50586929183";
constexpr const char* kConfigRel = "config/torus_003_frozen_config.json";

// ---- cells ---------------------------------------------------------------
enum Arm : u8 { ACTIVE = 0, SHAM = 1 };
enum Law : u8 { ZERO = 0, HALF = 1 };
enum Start : u8 { ALL_F = 0, ALL_M = 1 };
enum Label : u8 { LABEL_F = 0, LABEL_M = 1 };
enum Family : u8 { FAM_PARENT = 0, FAM_PROBE = 1, FAM_SCOUT = 2, FAM_LOCAL = 3 };

// Canonical cell index = 4*arm + 2*law + start.
inline constexpr int cell_index(Arm a, Law l, Start s) { return 4 * int(a) + 2 * int(l) + int(s); }
inline constexpr Arm cell_arm(int c) { return Arm((c >> 2) & 1); }
inline constexpr Law cell_law(int c) { return Law((c >> 1) & 1); }
inline constexpr Start cell_start(int c) { return Start(c & 1); }

inline constexpr int candidate_index(int family, int parent) { return 32 * family + parent; }
inline constexpr int candidate_family(int c) { return c >> 5; }
inline constexpr int candidate_parent(int c) { return c & 31; }

}  // namespace torus
