// Canonical Philox4x32-10, purpose-key derivation and coordinate validation for the
// PHASE2-PERFORMANCE-CONVERSION-002 r1 replay auditor. Derived from specification section 7 and
// frozen_config.json "rng"; shares no code with the producer.
#ifndef PCONV_AUDIT_PHILOX_HPP
#define PCONV_AUDIT_PHILOX_HPP

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "sha256.hpp"

namespace pcaudit {

using Word4 = std::array<std::uint32_t, 4>;

struct PhiloxKey {
  std::uint32_t k0;
  std::uint32_t k1;
};

constexpr std::uint32_t kPhiloxMultiplier0 = 0xD2511F53u;
constexpr std::uint32_t kPhiloxMultiplier1 = 0xCD9E8D57u;
constexpr std::uint32_t kPhiloxWeyl0 = 0x9E3779B9u;
constexpr std::uint32_t kPhiloxWeyl1 = 0xBB67AE85u;
constexpr int kPhiloxRounds = 10;

// Ten rounds; the key is bumped by the Weyl constants before every round after the first.
inline Word4 philox4x32_10(const Word4& counter, const PhiloxKey& key) {
  std::uint32_t c0 = counter[0], c1 = counter[1], c2 = counter[2], c3 = counter[3];
  std::uint32_t k0 = key.k0, k1 = key.k1;
  for (int round = 0; round < kPhiloxRounds; ++round) {
    if (round > 0) {
      k0 += kPhiloxWeyl0;
      k1 += kPhiloxWeyl1;
    }
    const std::uint64_t product0 = static_cast<std::uint64_t>(kPhiloxMultiplier0) * c0;
    const std::uint64_t product1 = static_cast<std::uint64_t>(kPhiloxMultiplier1) * c2;
    const std::uint32_t hi0 = static_cast<std::uint32_t>(product0 >> 32);
    const std::uint32_t lo0 = static_cast<std::uint32_t>(product0);
    const std::uint32_t hi1 = static_cast<std::uint32_t>(product1 >> 32);
    const std::uint32_t lo1 = static_cast<std::uint32_t>(product1);
    const std::uint32_t next0 = hi1 ^ c1 ^ k0;
    const std::uint32_t next2 = hi0 ^ c3 ^ k1;
    c0 = next0;
    c1 = lo1;
    c2 = next2;
    c3 = lo0;
  }
  return Word4{{c0, c1, c2, c3}};
}

// Official Random123 known-answer rows, (counter[4], key[2]) -> expected[4].
struct PhiloxKnownAnswer {
  const char* name;
  std::uint32_t counter[4];
  std::uint32_t key[2];
  std::uint32_t expected[4];
};

static const PhiloxKnownAnswer kPhiloxKnownAnswers[3] = {
    {"philox4x32_10_random123_zero",
     {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
     {0x00000000u, 0x00000000u},
     {0x6627e8d5u, 0xe169c58du, 0xbc57ac4cu, 0x9b00dbd8u}},
    {"philox4x32_10_random123_all_ones",
     {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu},
     {0xffffffffu, 0xffffffffu},
     {0x408f276du, 0x41c83b0eu, 0xa20bc7c6u, 0x6d5451fdu}},
    {"philox4x32_10_random123_pi_digits",
     {0x243f6a88u, 0x85a308d3u, 0x13198a2eu, 0x03707344u},
     {0xa4093822u, 0x299f31d0u},
     {0xd16cfe09u, 0x94fdccebu, 0x5001e420u, 0x24126ea1u}},
};

// frozen_config.json rng.purposes_in_order.
enum class Purpose : unsigned {
  InitialGenotype = 0,
  TargetInnovation = 1,
  TargetCopy = 2,
  FreshMask = 3,
  ScoutMask = 4,
  LocalBit = 5,
  DonorKey = 6,
  SurvivalUniform = 7,
  PolicyMutation = 8,
  DecoyPermutation = 9,
};

constexpr unsigned kPurposeCount = 10;
// The first nine purposes are the study-001 set; used only to reconstruct predecessor key texts for
// the key-disjointness check (no study-001 draw is ever generated).
constexpr unsigned kPredecessorPurposeCount = 9;

inline const char* purpose_name(Purpose p) {
  static const char* const kNames[kPurposeCount] = {
      "INITIAL_GENOTYPE", "TARGET_INNOVATION", "TARGET_COPY",      "FRESH_MASK",      "SCOUT_MASK",
      "LOCAL_BIT",        "DONOR_KEY",         "SURVIVAL_UNIFORM", "POLICY_MUTATION", "DECOY_PERMUTATION"};
  return kNames[static_cast<unsigned>(p)];
}

constexpr std::uint64_t kMaxBlock = 41599u;
constexpr std::uint64_t kLastUpdate = 256u;
constexpr std::uint64_t kMaxSubindex = 0xFFFFFFFFull;

class CoordinateError : public std::runtime_error {
 public:
  explicit CoordinateError(const std::string& what) : std::runtime_error(what) {}
};

inline std::string describe_coordinate(Purpose p, std::uint64_t block, std::uint64_t update, std::uint64_t entity,
                                       std::uint64_t subindex) {
  return std::string("purpose=") + purpose_name(p) + " block=" + std::to_string(block) +
         " update=" + std::to_string(update) + " entity=" + std::to_string(entity) +
         " subindex=" + std::to_string(subindex);
}

// Declared coordinate schemas (specification section 7; frozen_config rng.coordinate_schemas).
// Throws CoordinateError for any coordinate outside its declared range.
inline void validate_coordinate(Purpose p, std::uint64_t block, std::uint64_t update, std::uint64_t entity,
                                std::uint64_t subindex) {
  if (block > kMaxBlock) {
    throw CoordinateError("block out of range: " + describe_coordinate(p, block, update, entity, subindex));
  }
  if (p == Purpose::InitialGenotype) {
    if (update != 0) {
      throw CoordinateError("update out of range: " + describe_coordinate(p, block, update, entity, subindex));
    }
  } else if (update < 1 || update > kLastUpdate) {
    throw CoordinateError("update out of range: " + describe_coordinate(p, block, update, entity, subindex));
  }
  std::uint64_t min_entity = 0;
  std::uint64_t max_entity = 0;
  std::uint64_t max_subindex = 0;
  switch (p) {
    case Purpose::InitialGenotype:
    case Purpose::FreshMask:
    case Purpose::ScoutMask:
    case Purpose::PolicyMutation:
      max_entity = 31;
      max_subindex = 0;
      break;
    case Purpose::TargetInnovation:
    case Purpose::TargetCopy:
      max_entity = 0;
      max_subindex = 0;
      break;
    case Purpose::LocalBit:
      max_entity = 31;
      max_subindex = 31;
      break;
    case Purpose::DonorKey:
      max_entity = 95;
      max_subindex = 0;
      break;
    case Purpose::SurvivalUniform:
      max_entity = 31;
      max_subindex = kMaxSubindex;
      break;
    case Purpose::DecoyPermutation:
      // entity = Fisher-Yates step i in 1..31; subindex = Lemire retry.
      min_entity = 1;
      max_entity = 31;
      max_subindex = kMaxSubindex;
      break;
    default:
      throw CoordinateError("undeclared purpose: " + std::to_string(static_cast<unsigned>(p)));
  }
  if (entity < min_entity || entity > max_entity) {
    throw CoordinateError("entity out of range: " + describe_coordinate(p, block, update, entity, subindex));
  }
  if (subindex > max_subindex) {
    throw CoordinateError("subindex out of range: " + describe_coordinate(p, block, update, entity, subindex));
  }
}

enum class KeyNamespace { Production, Fixture };

inline const char* namespace_text(KeyNamespace ns) {
  return ns == KeyNamespace::Production ? "production-r1" : "fixture-r1-nonscientific";
}

constexpr const char* kStudyKeyPrefix = "PHASE2-PERFORMANCE-CONVERSION-002|";
constexpr const char* kPredecessorKeyPrefix = "PHASE2-MUTABLE-MEMORY-001|";

inline std::string key_text(KeyNamespace ns, Purpose p) {
  return std::string(kStudyKeyPrefix) + namespace_text(ns) + "|" + purpose_name(p);
}

// Study-001 key text, reconstructed only for the disjointness known-answer test.
inline std::string predecessor_key_text(KeyNamespace ns, Purpose p) {
  return std::string(kPredecessorKeyPrefix) + namespace_text(ns) + "|" + purpose_name(p);
}

inline std::uint32_t le32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

// Key word 0 = SHA-256 digest bytes 0..3 little-endian; key word 1 = bytes 4..7 little-endian.
inline PhiloxKey key_words_from_text(const std::string& text) {
  Sha256 h;
  h.update(text.data(), text.size());
  const Sha256::Digest d = h.finish();
  return PhiloxKey{le32(d.data()), le32(d.data() + 4)};
}

// A process may derive stream keys for exactly one namespace. A fixture-mode process can therefore
// never construct a production-namespace stream, and vice versa.
inline void claim_namespace(KeyNamespace ns) {
  static bool claimed = false;
  static KeyNamespace owner = KeyNamespace::Fixture;
  if (!claimed) {
    claimed = true;
    owner = ns;
    return;
  }
  if (owner != ns) throw std::logic_error("process is already bound to a different key namespace");
}

class KeyedStream {
 public:
  explicit KeyedStream(KeyNamespace ns) : ns_(ns) {
    claim_namespace(ns);
    for (unsigned i = 0; i < kPurposeCount; ++i) keys_[i] = key_words_from_text(key_text(ns, static_cast<Purpose>(i)));
  }

  KeyNamespace key_namespace() const { return ns_; }

  PhiloxKey key(Purpose p) const { return keys_[static_cast<unsigned>(p)]; }

  // Counter words are exactly (block, update, entity, subindex).
  Word4 draw(Purpose p, std::uint64_t block, std::uint64_t update, std::uint64_t entity,
             std::uint64_t subindex) const {
    validate_coordinate(p, block, update, entity, subindex);
    const Word4 counter{{static_cast<std::uint32_t>(block), static_cast<std::uint32_t>(update),
                         static_cast<std::uint32_t>(entity), static_cast<std::uint32_t>(subindex)}};
    return philox4x32_10(counter, keys_[static_cast<unsigned>(p)]);
  }

 private:
  KeyNamespace ns_;
  PhiloxKey keys_[kPurposeCount];
};

}  // namespace pcaudit

#endif  // PCONV_AUDIT_PHILOX_HPP
