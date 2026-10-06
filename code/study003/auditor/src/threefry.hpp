// Independent Threefry4x64-20 and purpose-keyed draw schema for the
// PHASE2-TORUS-MEMORY-003 auditor.
//
// Written for this package from the frozen specification (section 8): the
// rotation constants, parity constant, 20 rounds, key injection every four
// rounds, the two official known-answer vectors, the SHA-256 purpose-key rule
// and the (block,update,entity,subindex) counter rule.  No Random123 source,
// producer source or prior-auditor source is used.
#ifndef T3A_THREEFRY_HPP
#define T3A_THREEFRY_HPP

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "sha256.hpp"

namespace t3a {

// Fixed model geometry (specification sections 3, 6, 7, 13).
constexpr unsigned kPop = 32;
constexpr unsigned kLoci = 32;
constexpr unsigned kCandidates = 128;
constexpr unsigned kUpdates = 256;
constexpr unsigned kEntries = 4;
constexpr unsigned kLateFirst = 193;
constexpr std::uint32_t kProductionBlocks = 41600;
constexpr std::uint32_t kAuditBlocks = 64;

using Word4 = std::array<std::uint64_t, 4>;

inline constexpr std::uint64_t kThreefryParity = 0x1BD11BDAA9FC1A22ULL;
inline constexpr unsigned kThreefryRot[8][2] = {
    {14, 16}, {52, 57}, {23, 40}, {5, 37}, {25, 33}, {46, 12}, {58, 22}, {32, 32},
};

inline std::uint64_t rotl64(std::uint64_t x, unsigned r) {
    return r == 0 ? x : ((x << r) | (x >> (64u - r)));
}

// Threefry4x64 with 20 rounds.  Key schedule: ks[0..3] = key words,
// ks[4] = parity ^ k0 ^ k1 ^ k2 ^ k3.  Injection s (s = 0..5) adds
// ks[(s+i) mod 5] to word i and s to word 3; injection 0 precedes round 0 and
// injection s follows round 4s-1.  Even rounds mix (0,1),(2,3); odd rounds mix
// (0,3),(2,1); round r uses rotation pair r mod 8.
inline Word4 threefry4x64_20(const Word4& counter, const Word4& key) {
    std::uint64_t ks[5];
    ks[4] = kThreefryParity;
    for (unsigned i = 0; i < 4; ++i) {
        ks[i] = key[i];
        ks[4] ^= key[i];
    }
    std::uint64_t x[4];
    for (unsigned i = 0; i < 4; ++i) x[i] = counter[i] + ks[i];
    for (unsigned r = 0; r < 20; ++r) {
        const unsigned* rot = kThreefryRot[r % 8u];
        if ((r & 1u) == 0) {
            x[0] += x[1]; x[1] = rotl64(x[1], rot[0]); x[1] ^= x[0];
            x[2] += x[3]; x[3] = rotl64(x[3], rot[1]); x[3] ^= x[2];
        } else {
            x[0] += x[3]; x[3] = rotl64(x[3], rot[0]); x[3] ^= x[0];
            x[2] += x[1]; x[1] = rotl64(x[1], rot[1]); x[1] ^= x[2];
        }
        if ((r & 3u) == 3u) {
            const std::uint64_t s = (r + 1u) / 4u;
            for (unsigned i = 0; i < 4; ++i) x[i] += ks[(s + i) % 5u];
            x[3] += s;
        }
    }
    return Word4{x[0], x[1], x[2], x[3]};
}

// Coordinate 16*q + 4*word + lane = (output_word >> (16*lane)) & 0xffff.
inline void extract_lanes(const Word4& out, unsigned q, std::uint16_t* coords32) {
    for (unsigned w = 0; w < 4; ++w) {
        for (unsigned lane = 0; lane < 4; ++lane) {
            coords32[16u * q + 4u * w + lane] =
                static_cast<std::uint16_t>((out[w] >> (16u * lane)) & 0xffffu);
        }
    }
}

class RangeRefusal : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class Purpose : unsigned {
    InitialVector = 0,
    TargetInnovationVector,
    TargetCopy,
    FreshVector,
    ScoutVector,
    LocalReplaceFlag,
    LocalReplaceValue,
    DonorKey,
    TournamentEntry,
    CandidateTieKey,
    PolicyMutation,
};
constexpr unsigned kPurposeCount = 11;

// Declared counter schema per purpose (specification section 8 table).
struct PurposeSchema {
    const char* name;
    std::uint64_t update_min;
    std::uint64_t update_max;
    std::uint64_t entity_count;
    std::uint64_t sub_count;
    bool vector;
};

inline const PurposeSchema& purpose_schema(Purpose p) {
    static const PurposeSchema table[kPurposeCount] = {
        {"INITIAL_VECTOR", 0, 0, 32, 2, true},
        {"TARGET_INNOVATION_VECTOR", 1, 256, 1, 2, true},
        {"TARGET_COPY", 1, 256, 1, 1, false},
        {"FRESH_VECTOR", 1, 256, 32, 2, true},
        {"SCOUT_VECTOR", 1, 256, 32, 2, true},
        {"LOCAL_REPLACE_FLAG", 1, 256, 32, 2, true},
        {"LOCAL_REPLACE_VALUE", 1, 256, 32, 2, true},
        {"DONOR_KEY", 1, 256, 96, 1, false},
        {"TOURNAMENT_ENTRY", 1, 256, 32, 4, false},
        {"CANDIDATE_TIE_KEY", 1, 256, 128, 1, false},
        {"POLICY_MUTATION", 1, 256, 32, 1, false},
    };
    const unsigned i = static_cast<unsigned>(p);
    if (i >= kPurposeCount) throw RangeRefusal("unknown purpose");
    return table[i];
}

// Number of Threefry calls declared per block across all purposes.
inline std::uint64_t declared_draws_per_block() {
    std::uint64_t total = 0;
    for (unsigned i = 0; i < kPurposeCount; ++i) {
        const PurposeSchema& s = purpose_schema(static_cast<Purpose>(i));
        total += (s.update_max - s.update_min + 1) * s.entity_count * s.sub_count;
    }
    return total;
}

inline bool namespace_allowed(const std::string& ns) {
    return ns == "production-r1" || ns == "fixture-r1" || ns == "timing-r1";
}

// Key word j = little-endian unsigned 64-bit value of digest bytes 8j..8j+7.
inline Word4 key_words_from_digest(const Digest& d) {
    Word4 k{};
    for (unsigned j = 0; j < 4; ++j) {
        std::uint64_t v = 0;
        for (unsigned b = 0; b < 8; ++b) v |= static_cast<std::uint64_t>(d[8u * j + b]) << (8u * b);
        k[j] = v;
    }
    return k;
}

inline std::string key_text(const std::string& ns, Purpose p) {
    return std::string("PHASE2-TORUS-MEMORY-003|") + ns + "|" + purpose_schema(p).name;
}

inline Word4 derive_key(const std::string& ns, Purpose p) {
    if (!namespace_allowed(ns)) throw RangeRefusal("namespace not declared");
    const std::string text = key_text(ns, p);
    return key_words_from_digest(sha256_bytes(text.data(), text.size()));
}

// Range-checked access to every declared draw.  Any counter outside the
// declared schema, or a block outside the namespace limit, is refused.
class Keyring {
public:
    Keyring(const std::string& ns, std::uint32_t block_limit) : ns_(ns), block_limit_(block_limit) {
        if (!namespace_allowed(ns)) throw RangeRefusal("namespace not declared");
        for (unsigned i = 0; i < kPurposeCount; ++i) keys_[i] = derive_key(ns, static_cast<Purpose>(i));
    }

    const std::string& name() const { return ns_; }
    std::uint32_t block_limit() const { return block_limit_; }
    const Word4& key(Purpose p) const { return keys_[static_cast<unsigned>(p)]; }

    Word4 call(Purpose p, std::uint64_t block, std::uint64_t update, std::uint64_t entity,
               std::uint64_t sub) const {
        const PurposeSchema& s = purpose_schema(p);
        if (block >= block_limit_ || update < s.update_min || update > s.update_max ||
            entity >= s.entity_count || sub >= s.sub_count) {
            throw RangeRefusal("counter outside declared schema");
        }
        return threefry4x64_20(Word4{block, update, entity, sub}, keys_[static_cast<unsigned>(p)]);
    }

    void vector32(Purpose p, std::uint64_t block, std::uint64_t update, std::uint64_t entity,
                  std::uint16_t* out32) const {
        if (!purpose_schema(p).vector) throw RangeRefusal("scalar purpose used as vector");
        for (unsigned q = 0; q < 2; ++q) extract_lanes(call(p, block, update, entity, q), q, out32);
    }

    std::uint64_t word0(Purpose p, std::uint64_t block, std::uint64_t update, std::uint64_t entity,
                        std::uint64_t sub) const {
        if (purpose_schema(p).vector) throw RangeRefusal("vector purpose used as scalar");
        return call(p, block, update, entity, sub)[0];
    }

private:
    std::string ns_;
    std::uint32_t block_limit_;
    Word4 keys_[kPurposeCount];
};

}  // namespace t3a

#endif  // T3A_THREEFRY_HPP
