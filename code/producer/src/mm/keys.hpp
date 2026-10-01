// Purpose names and purpose-key derivation.
// key text = "PHASE2-MUTABLE-MEMORY-001|<namespace>|<purpose>" (ASCII = UTF-8);
// k0 = digest bytes 0..3 little-endian, k1 = digest bytes 4..7 little-endian.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "mm/constants.hpp"
#include "mm/fatal.hpp"
#include "mm/philox.hpp"
#include "mm/sha256.hpp"

namespace mm {

enum class Purpose : std::uint8_t {
  kInitialGenotype = 0,
  kTargetInnovation = 1,
  kTargetCopy = 2,
  kFreshMask = 3,
  kScoutMask = 4,
  kLocalBit = 5,
  kDonorKey = 6,
  kSurvivalUniform = 7,
  kPolicyMutation = 8,
};
constexpr std::uint32_t kPurposeCount = 9U;

inline std::size_t purpose_index(Purpose p) { return static_cast<std::size_t>(p); }

inline Purpose purpose_from_index(std::uint32_t i) {
  require(i < kPurposeCount, "purpose index out of range");
  return static_cast<Purpose>(i);
}

inline const char* purpose_name(Purpose p) {
  static const char* const kNames[kPurposeCount] = {"INITIAL_GENOTYPE", "TARGET_INNOVATION", "TARGET_COPY",
                                                    "FRESH_MASK",       "SCOUT_MASK",        "LOCAL_BIT",
                                                    "DONOR_KEY",        "SURVIVAL_UNIFORM",  "POLICY_MUTATION"};
  return kNames[purpose_index(p)];
}

inline std::uint32_t le32_from_bytes(const std::uint8_t* b) {
  return static_cast<std::uint32_t>(b[0]) | (static_cast<std::uint32_t>(b[1]) << 8U) |
         (static_cast<std::uint32_t>(b[2]) << 16U) | (static_cast<std::uint32_t>(b[3]) << 24U);
}

inline PhiloxKey key_from_text(const std::string& text) {
  const Sha256Digest d = sha256_bytes(text.data(), text.size());
  return PhiloxKey{le32_from_bytes(d.data()), le32_from_bytes(d.data() + 4)};
}

inline std::string purpose_key_text(const std::string& ns, Purpose p) {
  return std::string(kStudyId) + "|" + ns + "|" + purpose_name(p);
}

struct KeySet {
  std::string ns;
  std::array<PhiloxKey, kPurposeCount> key;
};

inline KeySet derive_keys(const std::string& ns) {
  KeySet ks;
  ks.ns = ns;
  for (std::uint32_t i = 0; i < kPurposeCount; ++i) ks.key[i] = key_from_text(purpose_key_text(ns, purpose_from_index(i)));
  return ks;
}

inline bool same_key(const PhiloxKey& a, const PhiloxKey& b) { return a.k0 == b.k0 && a.k1 == b.k1; }

inline bool keys_pairwise_distinct(const KeySet& ks) {
  for (std::uint32_t i = 0; i < kPurposeCount; ++i)
    for (std::uint32_t j = i + 1U; j < kPurposeCount; ++j)
      if (same_key(ks.key[i], ks.key[j])) return false;
  return true;
}

inline bool keysets_disjoint(const KeySet& a, const KeySet& b) {
  for (std::uint32_t i = 0; i < kPurposeCount; ++i)
    for (std::uint32_t j = 0; j < kPurposeCount; ++j)
      if (same_key(a.key[i], b.key[j])) return false;
  return true;
}

}  // namespace mm
