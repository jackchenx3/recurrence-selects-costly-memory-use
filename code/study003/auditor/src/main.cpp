// torus_audit: independent PHASE2-TORUS-MEMORY-003 replay auditor.
//
// C++17, standard library only, pinned for GCC 8.5 (see Makefile).
// Commands (future, separately authorized; see README.md):
//   torus_audit selftest     --source-dir DIR --receipt FILE
//   torus_audit replay       --root DIR --manifest FILE --source-dir DIR --receipt FILE
//                            [--namespace production-r1|fixture-r1] [--blocks N]
//   torus_audit emit-fixture --out DIR --blocks N        (fixture-r1 only, N in 1..8)
// Exit codes: 0 PASS, 1 INVALID (receipt written), 2 usage/refusal (no
// receipt), 3 receipt could not be written.
//
// This program never imports, invokes or reads producer code.  It opens every
// input read-only, verifies input hashes before and after, and stops at the
// first discrepancy with a bounded, value-redacted diagnostic.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "records.hpp"
#include "replay.hpp"
#include "sha256.hpp"
#include "threefry.hpp"

namespace t3a {
namespace {

constexpr int kExitPass = 0;
constexpr int kExitInvalid = 1;
constexpr int kExitUsage = 2;
constexpr int kExitReceipt = 3;

const char* const kToolName = "torus_audit";
const char* const kToolVersion = "1";
const char* const kSourceFiles[] = {
    "Makefile", "src/main.cpp", "src/records.hpp", "src/replay.hpp", "src/sha256.hpp", "src/threefry.hpp",
};

class UsageError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void expect(bool ok, const char* name) {
    if (!ok) throw AuditFailure("SELFTEST_FAILED", name);
}

std::uint64_t hex_u64(const char* s) {
    std::uint64_t v = 0;
    for (unsigned i = 0; i < 16; ++i) {
        const char c = s[i];
        unsigned d = 0;
        if (c >= '0' && c <= '9') {
            d = static_cast<unsigned>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = static_cast<unsigned>(c - 'a' + 10);
        } else {
            throw std::logic_error("bad hex constant");
        }
        v = (v << 4) | d;
    }
    if (s[16] != '\0') throw std::logic_error("bad hex constant length");
    return v;
}

std::string utc_now() {
    const std::time_t t = std::time(nullptr);
    const std::tm* g = std::gmtime(&t);
    char buf[32] = {0};
    if (g == nullptr || std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", g) == 0) return "unavailable";
    return buf;
}

// ---------------------------------------------------------------------------
// Self-tests run before any replay.

void selftest_sha256(std::vector<std::string>& passed) {
    struct Vec {
        const char* msg;
        const char* hex;
    };
    const Vec vecs[] = {
        {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
         "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
    };
    for (const Vec& v : vecs) {
        expect(to_hex(sha256_bytes(v.msg, std::strlen(v.msg))) == v.hex, "sha256 FIPS vector");
    }
    Sha256 h;
    const std::string piece(997, 'a');
    std::size_t left = 1000000;
    while (left > 0) {
        const std::size_t n = std::min(left, piece.size());
        h.update(piece.data(), n);
        left -= n;
    }
    expect(to_hex(h.finish()) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
           "sha256 one-million-a vector with uneven buffering");
    passed.push_back("sha256_fips_vectors");
}

// Purpose-key derivation cross-check against the saved fixture key-derivation
// record (fixture-r1 namespace).  These are saved-record values, not code.
void selftest_key_derivation(std::vector<std::string>& passed) {
    struct Vec {
        Purpose purpose;
        const char* digest;
        const char* words[4];
    };
    const Vec vecs[] = {
        {Purpose::InitialVector, "7da8f6f586e04d37c539933f68e71ad292cdfe9e8354662b7274840ed14e2b3b",
         {"374de086f5f6a87d", "d21ae7683f9339c5", "2b6654839efecd92", "3b2b4ed10e847472"}},
        {Purpose::TargetInnovationVector, "f4f07f245da3ef50e659b93997eb9f8347a2cb88a6a0484e094ce3ef0ea92914",
         {"50efa35d247ff0f4", "839feb9739b959e6", "4e48a0a688cba247", "1429a90eefe34c09"}},
        {Purpose::TargetCopy, "04d50d37e702564153c1a135c9d3271c4d583d0978636ea3456d76ba2e3c85ec",
         {"415602e7370dd504", "1c27d3c935a1c153", "a36e6378093d584d", "ec853c2eba766d45"}},
        {Purpose::FreshVector, "aa0e9495ba34cae7e5debe7e11143392ad489eb0cf17376eae89900796e2b1d0",
         {"e7ca34ba95940eaa", "923314117ebedee5", "6e3717cfb09e48ad", "d0b1e296079089ae"}},
        {Purpose::ScoutVector, "8f7a352737117a651dbf11a148ff9ec77a1826bba34b91131ab952ec1d993bca",
         {"657a113727357a8f", "c79eff48a111bf1d", "13914ba3bb26187a", "ca3b991dec52b91a"}},
        {Purpose::LocalReplaceFlag, "bd891da61cb887a0b089c0e2fae5081582ecee75fc36d5e45ca47cc89a1c6090",
         {"a087b81ca61d89bd", "1508e5fae2c089b0", "e4d536fc75eeec82", "90601c9ac87ca45c"}},
        {Purpose::LocalReplaceValue, "8593c60994d1170128ca54a49b3874abe9ba0b5968dea93093afb019c8cd2152",
         {"0117d19409c69385", "ab74389ba454ca28", "30a9de68590bbae9", "5221cdc819b0af93"}},
        {Purpose::DonorKey, "f254658b9d858b7481580b3844715bd5270519202125326096c83ef034631142",
         {"748b859d8b6554f2", "d55b7144380b5881", "6032252120190527", "42116334f03ec896"}},
        {Purpose::TournamentEntry, "6c216f0d0bd25edad2c1626014f8f5afbdb3b7507ec4fc40a2396eae8a1b285f",
         {"da5ed20b0d6f216c", "aff5f8146062c1d2", "40fcc47e50b7b3bd", "5f281b8aae6e39a2"}},
        {Purpose::CandidateTieKey, "d3ec6fcb352ca52b0caef5914e48833822b8d683c904a6f47fe699219d685a66",
         {"2ba52c35cb6fecd3", "3883484e91f5ae0c", "f4a604c983d6b822", "665a689d2199e67f"}},
        {Purpose::PolicyMutation, "af608423033944a7242d574f09130acce1e3a5c8a2420cb8633908685f66251b",
         {"a7443903238460af", "cc0a13094f572d24", "b80c42a2c8a5e3e1", "1b25665f68083963"}},
    };
    for (const Vec& v : vecs) {
        const std::string text = key_text("fixture-r1", v.purpose);
        expect(to_hex(sha256_bytes(text.data(), text.size())) == v.digest, "fixture-r1 purpose-key digest");
        const Word4 k = derive_key("fixture-r1", v.purpose);
        for (unsigned j = 0; j < 4; ++j) expect(k[j] == hex_u64(v.words[j]), "fixture-r1 purpose-key words");
    }
    // Byte order: digest bytes 0..31 map little-endian into words.
    Digest d{};
    for (unsigned i = 0; i < 32; ++i) d[i] = static_cast<std::uint8_t>(i);
    const Word4 k = key_words_from_digest(d);
    expect(k[0] == 0x0706050403020100ULL && k[1] == 0x0f0e0d0c0b0a0908ULL && k[2] == 0x1716151413121110ULL &&
               k[3] == 0x1f1e1d1c1b1a1918ULL,
           "key word little-endian byte order");
    // Purpose and namespace separation.
    for (unsigned a = 0; a < kPurposeCount; ++a) {
        const Word4 ka = derive_key("production-r1", static_cast<Purpose>(a));
        expect(ka != derive_key("fixture-r1", static_cast<Purpose>(a)), "namespace separation");
        for (unsigned b = a + 1; b < kPurposeCount; ++b) {
            expect(ka != derive_key("production-r1", static_cast<Purpose>(b)), "purpose key separation");
            const Word4 ctr{0, 1, 0, 0};
            expect(threefry4x64_20(ctr, ka) != threefry4x64_20(ctr, derive_key("production-r1", static_cast<Purpose>(b))),
                   "purpose output separation at identical counter");
        }
    }
    passed.push_back("purpose_key_derivation_and_separation");
}

void selftest_threefry(std::vector<std::string>& passed) {
    const Word4 zero{0, 0, 0, 0};
    const Word4 out0 = threefry4x64_20(zero, zero);
    expect(out0[0] == 0x09218ebde6c85537ULL && out0[1] == 0x55941f5266d86105ULL && out0[2] == 0x4bd25e16282434dcULL &&
               out0[3] == 0xee29ec846bd2e40bULL,
           "official Threefry4x64-20 all-zero vector");
    const std::uint64_t ones = ~static_cast<std::uint64_t>(0);
    const Word4 all1{ones, ones, ones, ones};
    const Word4 out1 = threefry4x64_20(all1, all1);
    expect(out1[0] == 0x29c24097942bba1bULL && out1[1] == 0x0371bbfb0f6f4e11ULL && out1[2] == 0x3c231ffa33f83a1cULL &&
               out1[3] == 0xcd29113fde32d168ULL,
           "official Threefry4x64-20 all-one vector");
    passed.push_back("threefry_official_kat");

    // Counter order: the keyed call uses (block, update, entity, subindex).
    const Keyring kr("fixture-r1", kAuditBlocks);
    const Word4 via_ring = kr.call(Purpose::FreshVector, 1, 2, 3, 1);
    expect(via_ring == threefry4x64_20(Word4{1, 2, 3, 1}, kr.key(Purpose::FreshVector)), "counter word order");
    expect(via_ring != threefry4x64_20(Word4{1, 3, 2, 1}, kr.key(Purpose::FreshVector)), "counter order sensitivity");
    passed.push_back("counter_order");

    // Lane extraction: coordinate 16q + 4w + lane, least significant lane first.
    const Word4 w{0x4444333322221111ULL, 0x8888777766665555ULL, 0xccccbbbbaaaa9999ULL, 0x0000ffffeeeeddddULL};
    std::uint16_t coords[32] = {0};
    extract_lanes(w, 1, coords);
    expect(coords[16] == 0x1111 && coords[17] == 0x2222 && coords[19] == 0x4444 && coords[20] == 0x5555 &&
               coords[24] == 0x9999 && coords[30] == 0xffff && coords[31] == 0x0000 && coords[0] == 0,
           "lane extraction order");
    std::uint16_t v[32] = {0};
    kr.vector32(Purpose::ScoutVector, 0, 1, 0, v);
    const Word4 q0 = kr.call(Purpose::ScoutVector, 0, 1, 0, 0);
    const Word4 q1 = kr.call(Purpose::ScoutVector, 0, 1, 0, 1);
    expect(v[0] == static_cast<std::uint16_t>(q0[0] & 0xffffu) && v[5] == static_cast<std::uint16_t>((q0[1] >> 16) & 0xffffu) &&
               v[31] == static_cast<std::uint16_t>(q1[3] >> 48),
           "vector draw uses subindex 0 then 1");
    passed.push_back("lane_extraction");

    // Range refusal for every declared schema boundary.
    struct Bad {
        Purpose p;
        std::uint64_t b, u, e, s;
    };
    const Bad bad[] = {
        {Purpose::InitialVector, 0, 1, 0, 0},          {Purpose::InitialVector, 0, 0, 32, 0},
        {Purpose::InitialVector, 0, 0, 0, 2},          {Purpose::InitialVector, kAuditBlocks, 0, 0, 0},
        {Purpose::TargetInnovationVector, 0, 0, 0, 0}, {Purpose::TargetInnovationVector, 0, 257, 0, 0},
        {Purpose::TargetInnovationVector, 0, 1, 1, 0}, {Purpose::TargetCopy, 0, 1, 0, 1},
        {Purpose::TargetCopy, 0, 1, 1, 0},             {Purpose::FreshVector, 0, 1, 32, 0},
        {Purpose::ScoutVector, 0, 1, 0, 2},            {Purpose::LocalReplaceFlag, 0, 0, 0, 0},
        {Purpose::LocalReplaceValue, 0, 257, 0, 0},    {Purpose::DonorKey, 0, 1, 96, 0},
        {Purpose::DonorKey, 0, 1, 0, 1},               {Purpose::TournamentEntry, 0, 1, 32, 0},
        {Purpose::TournamentEntry, 0, 1, 0, 4},        {Purpose::CandidateTieKey, 0, 1, 128, 0},
        {Purpose::PolicyMutation, 0, 1, 0, 1},         {Purpose::PolicyMutation, 0, 1, 32, 0},
    };
    for (const Bad& x : bad) {
        bool refused = false;
        try {
            (void)kr.call(x.p, x.b, x.u, x.e, x.s);
        } catch (const RangeRefusal&) {
            refused = true;
        }
        expect(refused, "out-of-range counter refused");
    }
    bool refused_vec = false;
    try {
        (void)kr.word0(Purpose::FreshVector, 0, 1, 0, 0);
    } catch (const RangeRefusal&) {
        refused_vec = true;
    }
    expect(refused_vec, "vector purpose refused as scalar");
    bool refused_ns = false;
    try {
        Keyring other("production-r2", 1);
        (void)other;
    } catch (const RangeRefusal&) {
        refused_ns = true;
    }
    expect(refused_ns, "undeclared namespace refused");
    (void)kr.call(Purpose::InitialVector, kAuditBlocks - 1, 0, 31, 1);
    (void)kr.call(Purpose::DonorKey, 0, 256, 95, 0);
    (void)kr.call(Purpose::TournamentEntry, 0, 256, 31, 3);
    (void)kr.call(Purpose::CandidateTieKey, 0, 256, 127, 0);
    expect(declared_draws_per_block() == 164672u, "declared draws per block");
    expect(declared_draws_per_block() * kProductionBlocks == 6850355200ULL, "declared draws total");
    passed.push_back("range_refusal_and_schema_count");
}

void selftest_loss_and_rank(std::vector<std::string>& passed) {
    expect(circular_distance(0, 65535) == 1 && circular_distance(65535, 0) == 1, "circular wrap");
    expect(circular_distance(1234, 1234) == 0, "circular equality");
    expect(circular_distance(0, 32768) == 32768 && circular_distance(32768, 0) == 32768, "circular antipode");
    expect(circular_distance(100, 40000) == 25636 && circular_distance(40000, 100) == 25636, "circular symmetry");
    Pheno zero;
    zero.fill(0);
    Pheno anti;
    anti.fill(32768);
    expect(torus_loss(anti, zero) == (static_cast<std::uint64_t>(1) << 35), "maximum individual loss");
    expect(static_cast<std::uint64_t>(32768) * 32768 == (static_cast<std::uint64_t>(1) << 30), "maximum coordinate loss");
    expect(32 * torus_loss(anti, zero) == (static_cast<std::uint64_t>(1) << 40), "maximum population loss");
    std::uint64_t sum = 0;
    for (std::uint64_t r = 1; r <= 128; ++r) {
        const std::uint64_t a = 129 - r;
        const std::uint64_t b = 128 - r;
        const std::uint64_t n = a * a * a * a - b * b * b * b;
        if (r == 1) expect(n == 8290815u, "rank 1 numerator");
        if (r == 2) expect(n == 8097265u, "rank 2 numerator");
        if (r == 64) expect(n == 1073409u, "rank 64 numerator");
        if (r == 128) expect(n == 1u, "rank 128 numerator");
        sum += n;
    }
    expect(sum == (static_cast<std::uint64_t>(1) << 28), "rank probabilities sum to one");
    passed.push_back("circular_loss_and_rank_table");
}

void selftest_target_law(std::vector<std::string>& passed) {
    auto bt = std::make_unique<BlockTargets>();
    for (unsigned t = 0; t <= kUpdates; ++t) {
        bt->innovation[t].fill(static_cast<std::uint16_t>(t));
        bt->copy_bit[t] = 0;
    }
    bt->copy_bit[1] = 1;
    bt->copy_bit[2] = 1;
    bt->copy_bit[3] = 1;
    bt->copy_bit[5] = 1;
    apply_target_laws(*bt);
    expect(bt->target[1][1] == bt->innovation[1] && bt->target[1][2] == bt->innovation[2], "HALF t=1,2 innovations");
    expect(bt->copied[1][1] == 0 && bt->copied[1][2] == 0, "copy bits unused at t=1,2");
    expect(bt->target[1][3] == bt->innovation[1] && bt->copied[1][3] == 1, "HALF direct copy");
    expect(bt->target[1][4] == bt->innovation[4] && bt->copied[1][4] == 0, "HALF new innovation");
    expect(bt->target[1][5] == bt->innovation[1] && bt->copied[1][5] == 1, "HALF chained copy");
    expect(bt->target[0][3] == bt->innovation[3] && bt->target[0][5] == bt->innovation[5] && bt->copied[0][5] == 0,
           "ZERO shares innovations");
    passed.push_back("target_law_pairing");
}

void selftest_tournament(std::vector<std::string>& passed) {
    std::uint64_t loss[kCandidates];
    std::uint64_t tie[kCandidates];
    for (unsigned c = 0; c < kCandidates; ++c) {
        loss[c] = 100;
        tie[c] = 50;
    }
    const std::uint8_t same[4] = {7, 7, 7, 7};
    expect(tournament_winner(same, loss, tie) == 7, "tournament duplicate entries");
    const std::uint8_t mixed[4] = {70, 40, 3, 40};
    expect(tournament_winner(mixed, loss, tie) == 3, "tournament candidate-index fallback");
    tie[70] = 10;
    expect(tournament_winner(mixed, loss, tie) == 70, "tournament tie key");
    loss[40] = 99;
    expect(tournament_winner(mixed, loss, tie) == 40, "tournament loss before tie key");
    const std::uint64_t l3[3] = {5, 5, 5};
    const std::uint64_t k_eq[3] = {4, 4, 4};
    const std::uint64_t k_mid[3] = {9, 3, 3};
    const std::uint64_t k_last[3] = {9, 9, 3};
    expect(choose_donor(l3, k_eq) == 0 && choose_donor(l3, k_mid) == 1 && choose_donor(l3, k_last) == 2,
           "donor tie order");
    passed.push_back("tournament_and_donor_order");
}

// Compact one- and two-update hand trace with injected draws.
void selftest_hand_trace(std::vector<std::string>& passed) {
    Pheno zero;
    zero.fill(0);
    Pheno anti;
    anti.fill(32768);
    auto pre = std::make_unique<CellState>();
    auto post = std::make_unique<CellState>();
    auto post2 = std::make_unique<CellState>();
    auto sham = std::make_unique<CellState>();
    auto d = std::make_unique<UpdateDraws>();
    auto o = std::make_unique<StepOut>();
    for (unsigned i = 0; i < kPop; ++i) {
        pre->x[i] = anti;
        pre->label[i] = 0;
        pre->cache_valid[i] = 0;
        pre->cache[i] = zero;
        d->fresh[i] = anti;
        d->scout[i] = anti;
        d->value[i] = zero;
        d->flag[i].fill(1);
        for (unsigned f = 0; f < 3; ++f) d->donor_key[i][f] = 0;
    }
    pre->label[0] = 1;  // memory-better: valid cache equals target
    pre->cache_valid[0] = 1;
    pre->cache[0] = zero;
    pre->label[1] = 1;  // ACTIVE-M with invalid cache
    pre->label[2] = 1;  // cache equal to parent phenotype
    pre->cache_valid[2] = 1;
    pre->cache[2] = pre->x[2];
    d->flag[3][0] = 0;  // flagged; replacement equals donor value
    d->value[3][0] = 32768;
    d->flag[3][1] = 64;  // flagged; replacement differs
    d->value[3][1] = 5;
    d->donor_key[4][0] = 9;
    d->donor_key[4][1] = 3;
    d->donor_key[4][2] = 3;
    d->donor_key[5][0] = 9;
    d->donor_key[5][1] = 9;
    d->donor_key[5][2] = 3;
    for (unsigned s = 0; s < kPop; ++s) {
        d->entry[s][0] = 32;
        d->entry[s][1] = 96;
        d->entry[s][2] = 5;
        d->entry[s][3] = 5;
        d->mutate[s] = 0;
    }
    d->mutate[0] = 1;
    for (unsigned c = 0; c < kCandidates; ++c) d->tie[c] = 1000 + c;
    d->tie[32] = 1;
    d->tie[96] = 2;

    step_cell(*pre, *d, zero, true, *o, *post);
    expect(o->queries == 128, "128 objective queries");
    expect(o->probe_read[0] == 1 && o->cand[32] == zero && o->loss[32] == 0, "ACTIVE-M reads valid cache");
    expect(o->probe_read[1] == 0 && o->cand[33] == d->fresh[1], "invalid cache uses the fresh vector");
    expect(o->probe_read[2] == 1 && o->cand[34] == o->cand[2] && o->loss[34] == o->loss[2],
           "duplicate cache and parent are separate candidates");
    expect(o->loss[0] == (static_cast<std::uint64_t>(1) << 35), "antipodal parent loss");
    expect(o->donor_family[0] == 1 && o->cand[96] == zero && o->loss[96] == 0, "memory-better donor and child");
    expect(o->donor_family[4] == 1 && o->donor_family[5] == 2 && o->donor_family[6] == 0, "donor key and family ties");
    expect(o->flagged[3] == 2 && o->changed[3] == 1 && o->local_flagged == 2 && o->local_changed == 1,
           "flagged replacement equal to donor is not a change");
    expect(o->loss[99] == 31ull * (1ull << 30) + 25ull, "local child exact loss");
    bool all32 = true;
    for (unsigned s = 0; s < kPop; ++s) all32 = all32 && o->winner[s] == 32 && o->dup[s] == 1;
    expect(all32 && o->won[32] == 32 && o->distinct == 1 && o->dup_tournaments == 32, "tournament winners");
    expect(o->pop_loss == 0 && o->valid_pre == 2 && o->probe_uses == 2 && o->probe_winner_slots == 32,
           "update-level cache counts");
    expect(o->m_count == 31 && o->m_to_f == 1 && o->f_to_m == 0, "label mutation counts");
    expect(post->label[0] == 0 && post->cache_valid[0] == 0 && post->cache[0] == zero, "mutated F gets invalid cache");
    expect(post->label[1] == 1 && post->cache_valid[1] == 1 && post->cache[1] == anti,
           "M caches producing parent's pre-update phenotype, not the probe");
    expect(post->x[0] == zero && post->x[31] == zero, "survivors take the winning phenotype");

    for (unsigned s = 0; s < kPop; ++s) d->mutate[s] = 0;
    step_cell(*post, *d, zero, true, *o, *post2);
    bool all96 = true;
    for (unsigned s = 0; s < kPop; ++s) all96 = all96 && o->winner[s] == 96;
    expect(all96 && o->valid_pre == 31 && o->probe_uses == 31 && o->probe_winner_slots == 0, "second update");
    expect(o->m_count == 0 && o->f_to_m == 0 && o->m_to_f == 0 && post2->cache_valid[5] == 0,
           "second update inherits F from the winning lineage");

    step_cell(*pre, *d, zero, false, *o, *sham);
    bool sham_ok = o->probe_uses == 0 && o->probe_winner_slots == 0;
    for (unsigned i = 0; i < kPop; ++i) sham_ok = sham_ok && o->probe_read[i] == 0 && o->cand[32 + i] == d->fresh[i];
    expect(sham_ok && o->winner[0] == 32, "SHAM never reads labels or caches");
    passed.push_back("hand_trace_one_and_two_updates");
}

void run_selftests(std::vector<std::string>& passed) {
    selftest_sha256(passed);
    selftest_key_derivation(passed);
    selftest_threefry(passed);
    selftest_loss_and_rank(passed);
    selftest_target_law(passed);
    selftest_tournament(passed);
    selftest_hand_trace(passed);
}

// ---------------------------------------------------------------------------
// Replay driver.

struct ReplayCounts {
    std::uint64_t blocks = 0, paths = 0, path_updates = 0, candidate_rows = 0, entry_rows = 0, context_rows = 0,
                  update_rows = 0, path_rows = 0, block_rows = 0;
};

class Sink {
public:
    virtual ~Sink() = default;
    virtual void context(const std::uint8_t* row) = 0;
    virtual void candidate(const std::uint8_t* row) = 0;
    virtual void entry(const std::uint8_t* row) = 0;
    virtual void updates(const std::vector<std::uint8_t>& rows) = 0;  // 8 cells x 256 updates, cell-major
    virtual void paths(const std::vector<std::uint8_t>& rows) = 0;    // 8 cells
    virtual void block(const std::uint8_t* row) = 0;
};

void run_replay(const Keyring& kr, std::uint32_t nblocks, Sink& sink, ReplayCounts& n) {
    auto targets = std::make_unique<BlockTargets>();
    auto draws = std::make_unique<UpdateDraws>();
    auto out = std::make_unique<StepOut>();
    std::vector<CellState> cur(8);
    std::vector<CellState> nxt(8);
    std::vector<std::uint8_t> ctx(4240), cand(104), ent(16), urow(24), brow(136);
    std::vector<std::uint8_t> update_rows(8 * kUpdates * 24), path_rows(8 * 224);
    std::vector<Pheno> initial(kPop);
    for (std::uint32_t b = 0; b < nblocks; ++b) {
        draw_targets(kr, b, *targets);
        for (unsigned i = 0; i < kPop; ++i) kr.vector32(Purpose::InitialVector, b, 0, i, initial[i].data());
        for (unsigned cell = 0; cell < 8; ++cell) {
            init_cell(cur[cell], initial.data(), static_cast<std::uint8_t>(cell & 1u));
        }
        std::vector<PathAcc> acc(8);
        for (unsigned t = 1; t <= kUpdates; ++t) {
            draw_update(kr, b, t, *draws);
            for (unsigned cell = 0; cell < 8; ++cell) {
                const unsigned arm = cell >> 2;
                const unsigned law = (cell >> 1) & 1u;
                const Pheno& target = targets->target[law][t];
                encode_context(ctx.data(), b, t, cell, targets->copy_bit[t], targets->copied[law][t], target, cur[cell]);
                sink.context(ctx.data());
                ++n.context_rows;
                step_cell(cur[cell], *draws, target, arm == 0, *out, nxt[cell]);
                for (unsigned c = 0; c < kCandidates; ++c) {
                    encode_candidate(cand.data(), b, t, cell, c, cur[cell], *draws, *out);
                    sink.candidate(cand.data());
                }
                n.candidate_rows += kCandidates;
                for (unsigned s = 0; s < kPop; ++s) {
                    for (unsigned e = 0; e < kEntries; ++e) {
                        encode_entry(ent.data(), b, t, cell, s, e, *draws, *out, nxt[cell]);
                        sink.entry(ent.data());
                    }
                }
                n.entry_rows += kPop * kEntries;
                encode_update(urow.data(), t, *out);
                std::memcpy(update_rows.data() + (cell * kUpdates + (t - 1)) * 24, urow.data(), 24);
                acc[cell].add(urow.data(), t, *out, nxt[cell]);
                std::swap(cur[cell], nxt[cell]);
                ++n.path_updates;
            }
        }
        sink.updates(update_rows);
        n.update_rows += 8 * kUpdates;
        for (unsigned cell = 0; cell < 8; ++cell) {
            acc[cell].finalize();
            encode_path(path_rows.data() + cell * 224, b, cell, acc[cell], final_state_digest(cur[cell]));
        }
        sink.paths(path_rows);
        n.path_rows += 8;
        n.paths += 8;
        const std::uint32_t flags = identity_flags(acc);
        encode_block(brow.data(), b, acc, flags);
        sink.block(brow.data());
        ++n.block_rows;
        if (flags != 0x3Fu) throw AuditFailure("REPLAY_IDENTITY_N1_N2", "block " + std::to_string(b));
        ++n.blocks;
    }
}

struct Layout {
    std::string ns;
    std::uint32_t blocks;
    HeaderSpec spec(RecType t) const {
        const std::uint64_t groups = static_cast<std::uint64_t>(blocks) * kUpdates * 8;
        switch (t) {
            case RecType::Update: return HeaderSpec{static_cast<std::uint64_t>(blocks) * 8 * kUpdates, ns, 0, 0, blocks};
            case RecType::Path: return HeaderSpec{static_cast<std::uint64_t>(blocks) * 8, ns, 0, 0, blocks};
            case RecType::Block: return HeaderSpec{blocks, ns, 0, 0, blocks};
            case RecType::Candidate: return HeaderSpec{groups * kCandidates, ns, kNoChunk, 0, blocks};
            case RecType::Entry: return HeaderSpec{groups * kPop * kEntries, ns, kNoChunk, 0, blocks};
            case RecType::Context: return HeaderSpec{groups, ns, kNoChunk, 0, blocks};
            case RecType::Estimate: break;
        }
        throw std::logic_error("no replay layout for estimates");
    }
};

struct InputFile {
    RecType type;
    const char* rel;
};
const InputFile kReplayInputs[] = {
    {RecType::Update, "updates/chunk_00000.t3u"},  {RecType::Path, "paths/chunk_00000.t3p"},
    {RecType::Block, "blocks/chunk_00000.t3b"},    {RecType::Candidate, "audit/audit_candidates.t3c"},
    {RecType::Entry, "audit/audit_entries.t3e"},   {RecType::Context, "audit/audit_context.t3x"},
};

class VerifySink : public Sink {
public:
    VerifySink(const fs::path& root, const Layout& layout) {
        for (const InputFile& f : kReplayInputs) {
            streams_.emplace_back(new RowStream(checked_input(root, f.rel), f.rel, f.type, layout.spec(f.type)));
        }
    }
    void context(const std::uint8_t* row) override { check(5, RecType::Context, row); }
    void candidate(const std::uint8_t* row) override { check(3, RecType::Candidate, row); }
    void entry(const std::uint8_t* row) override { check(4, RecType::Entry, row); }
    void updates(const std::vector<std::uint8_t>& rows) override {
        for (std::size_t i = 0; i < rows.size(); i += 24) check(0, RecType::Update, rows.data() + i);
    }
    void paths(const std::vector<std::uint8_t>& rows) override {
        for (std::size_t i = 0; i < rows.size(); i += 224) check(1, RecType::Path, rows.data() + i);
    }
    void block(const std::uint8_t* row) override { check(2, RecType::Block, row); }
    void finish() {
        for (auto& s : streams_) s->finish();
    }

private:
    void check(std::size_t which, RecType type, const std::uint8_t* expected) {
        RowStream& s = *streams_[which];
        const std::uint8_t* got = s.next();
        compare_row(type, s.label(), expected, got, s.consumed() - 1);
    }
    std::vector<std::unique_ptr<RowStream>> streams_;
};

class EmitSink : public Sink {
public:
    EmitSink(const fs::path& out, const Layout& layout) {
        for (const InputFile& f : kReplayInputs) {
            writers_.emplace_back(new RowWriter(out / f.rel, f.type, layout.spec(f.type)));
        }
    }
    void context(const std::uint8_t* row) override { writers_[5]->write(row); }
    void candidate(const std::uint8_t* row) override { writers_[3]->write(row); }
    void entry(const std::uint8_t* row) override { writers_[4]->write(row); }
    void updates(const std::vector<std::uint8_t>& rows) override {
        for (std::size_t i = 0; i < rows.size(); i += 24) writers_[0]->write(rows.data() + i);
    }
    void paths(const std::vector<std::uint8_t>& rows) override {
        for (std::size_t i = 0; i < rows.size(); i += 224) writers_[1]->write(rows.data() + i);
    }
    void block(const std::uint8_t* row) override { writers_[2]->write(row); }
    void close() {
        for (auto& w : writers_) w->close();
    }

private:
    std::vector<std::unique_ptr<RowWriter>> writers_;
};

// ---------------------------------------------------------------------------
// Receipts and identities.

struct Receipt {
    std::string mode;
    std::string verdict = "INVALID";
    std::string code;
    std::string where;
    std::string ns;
    std::uint32_t blocks = 0;
    std::string started;
    std::string finished;
    std::string manifest_path;
    std::string manifest_sha;
    std::string binary_sha;
    std::vector<std::pair<std::string, std::string>> sources;
    std::vector<std::string> selftests;
    std::vector<InputIdentity> inputs;
    ReplayCounts counts;
    bool have_counts = false;

    std::string json() const {
        std::ostringstream o;
        o << "{\n";
        o << "  \"schema\": \"PHASE2-TORUS-MEMORY-003-AUDITOR-CPP-RECEIPT-v1\",\n";
        o << "  \"tool\": \"" << kToolName << "\",\n";
        o << "  \"tool_version\": \"" << kToolVersion << "\",\n";
        o << "  \"mode\": \"" << json_escape(mode) << "\",\n";
        o << "  \"verdict\": \"" << json_escape(verdict) << "\",\n";
        if (verdict == "PASS") {
            o << "  \"failure\": null,\n";
        } else {
            o << "  \"failure\": {\"code\": \"" << json_escape(code) << "\", \"where\": \"" << json_escape(where)
              << "\"},\n";
        }
        o << "  \"namespace\": \"" << json_escape(ns) << "\",\n";
        o << "  \"blocks\": " << blocks << ",\n";
        o << "  \"compiler\": \"" << json_escape(__VERSION__) << "\",\n";
        o << "  \"cplusplus\": " << __cplusplus << ",\n";
        o << "  \"binary_sha256\": \"" << json_escape(binary_sha) << "\",\n";
        o << "  \"source_files\": [";
        for (std::size_t i = 0; i < sources.size(); ++i) {
            o << (i ? ", " : "") << "{\"path\": \"" << json_escape(sources[i].first) << "\", \"sha256\": \""
              << json_escape(sources[i].second) << "\"}";
        }
        o << "],\n";
        o << "  \"selftests_passed\": [";
        for (std::size_t i = 0; i < selftests.size(); ++i) o << (i ? ", " : "") << "\"" << json_escape(selftests[i]) << "\"";
        o << "],\n";
        o << "  \"manifest\": {\"path\": \"" << json_escape(manifest_path) << "\", \"sha256\": \""
          << json_escape(manifest_sha) << "\"},\n";
        o << "  \"inputs\": [";
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            o << (i ? ", " : "") << "{\"path\": \"" << json_escape(inputs[i].rel) << "\", \"sha256\": \""
              << inputs[i].sha256 << "\", \"bytes\": " << inputs[i].bytes << "}";
        }
        o << "],\n";
        if (have_counts) {
            o << "  \"checked\": {\"blocks\": " << counts.blocks << ", \"paths\": " << counts.paths
              << ", \"path_updates\": " << counts.path_updates << ", \"candidate_rows\": " << counts.candidate_rows
              << ", \"entry_rows\": " << counts.entry_rows << ", \"context_rows\": " << counts.context_rows
              << ", \"update_rows\": " << counts.update_rows << ", \"path_rows\": " << counts.path_rows
              << ", \"block_rows\": " << counts.block_rows << "},\n";
        } else {
            o << "  \"checked\": null,\n";
        }
        o << "  \"inputs_modified\": false,\n";
        o << "  \"producer_code_invoked\": false,\n";
        o << "  \"started_utc\": \"" << started << "\",\n";
        o << "  \"finished_utc\": \"" << finished << "\"\n";
        o << "}\n";
        return o.str();
    }
};

std::vector<std::pair<std::string, std::string>> source_identity(const fs::path& src) {
    std::vector<std::pair<std::string, std::string>> out;
    for (const char* rel : kSourceFiles) {
        out.emplace_back(rel, to_hex(hash_file(checked_input(src, rel), rel)));
    }
    return out;
}

std::string binary_identity() {
    std::error_code ec;
    const fs::path self("/proc/self/exe");
    if (!fs::exists(self, ec) || ec) return "unavailable";
    try {
        return to_hex(hash_file(self, "binary"));
    } catch (const AuditFailure&) {
        return "unavailable";
    }
}

std::vector<std::string> components(const fs::path& p) {
    std::vector<std::string> out;
    for (const fs::path& part : p) {
        const std::string s = part.string();
        if (!s.empty() && s != "/" && s != ".") out.push_back(s);
    }
    return out;
}

bool path_within(const fs::path& child, const fs::path& parent) {
    std::error_code ec;
    const fs::path c = fs::weakly_canonical(fs::absolute(child), ec);
    if (ec) return true;  // fail closed: treat as inside
    const fs::path p = fs::weakly_canonical(fs::absolute(parent), ec);
    if (ec) return true;
    const std::vector<std::string> cc = components(c);
    const std::vector<std::string> pc = components(p);
    if (pc.size() > cc.size()) return false;
    return std::equal(pc.begin(), pc.end(), cc.begin());
}

using Args = std::map<std::string, std::string>;

Args parse_args(int argc, char** argv, const std::set<std::string>& allowed) {
    Args a;
    for (int i = 2; i < argc; i += 2) {
        const std::string key = argv[i];
        if (key.size() < 3 || key.compare(0, 2, "--") != 0) throw UsageError("expected --option");
        const std::string name = key.substr(2);
        if (allowed.count(name) == 0) throw UsageError("unknown option --" + name);
        if (i + 1 >= argc) throw UsageError("missing value for --" + name);
        if (a.count(name) != 0) throw UsageError("duplicate option --" + name);
        a[name] = argv[i + 1];
    }
    return a;
}

const std::string& required(const Args& a, const std::string& name) {
    const auto it = a.find(name);
    if (it == a.end() || it->second.empty()) throw UsageError("missing required --" + name);
    return it->second;
}

std::uint32_t parse_blocks(const std::string& s) {
    if (s.empty() || s.size() > 5) throw UsageError("bad --blocks");
    std::uint32_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') throw UsageError("bad --blocks");
        v = v * 10 + static_cast<std::uint32_t>(c - '0');
    }
    return v;
}

void refuse_existing(const fs::path& p) {
    std::error_code ec;
    const fs::file_status st = fs::symlink_status(p, ec);
    if (st.type() != fs::file_type::not_found) throw UsageError("receipt path already exists");
}

int finish_receipt(Receipt& r, const fs::path& receipt) {
    r.finished = utc_now();
    if (!write_exclusive_text(receipt, r.json())) {
        std::cerr << "receipt could not be created exclusively\n";
        return kExitReceipt;
    }
    std::cout << kToolName << " " << r.mode << " verdict " << r.verdict;
    if (r.verdict != "PASS") std::cout << " " << r.code << " (" << r.where << ")";
    std::cout << "\n";
    return r.verdict == "PASS" ? kExitPass : kExitInvalid;
}

void record_failure(Receipt& r, const std::exception_ptr& ep) {
    r.verdict = "INVALID";
    try {
        std::rethrow_exception(ep);
    } catch (const AuditFailure& e) {
        r.code = e.code();
        r.where = e.where();
    } catch (const RangeRefusal&) {
        r.code = "RANGE_REFUSAL";
        r.where = "declared draw schema";
    } catch (const std::bad_alloc&) {
        r.code = "OUT_OF_MEMORY";
        r.where = "allocation";
    } catch (const std::exception&) {
        r.code = "INTERNAL_ERROR";
        r.where = "unclassified exception";
    }
}

int cmd_selftest(int argc, char** argv) {
    const Args a = parse_args(argc, argv, {"source-dir", "receipt"});
    const fs::path src = required(a, "source-dir");
    const fs::path receipt = required(a, "receipt");
    refuse_existing(receipt);
    Receipt r;
    r.mode = "selftest";
    r.started = utc_now();
    try {
        r.sources = source_identity(src);
        r.binary_sha = binary_identity();
        run_selftests(r.selftests);
        r.verdict = "PASS";
    } catch (...) {
        record_failure(r, std::current_exception());
    }
    return finish_receipt(r, receipt);
}

int cmd_replay(int argc, char** argv) {
    const Args a = parse_args(argc, argv, {"root", "manifest", "source-dir", "receipt", "namespace", "blocks"});
    const fs::path root = fs::absolute(required(a, "root"));
    const fs::path manifest = fs::absolute(required(a, "manifest"));
    const fs::path src = required(a, "source-dir");
    const fs::path receipt = required(a, "receipt");
    const std::string ns = a.count("namespace") ? a.at("namespace") : std::string("production-r1");
    std::uint32_t blocks = a.count("blocks") ? parse_blocks(a.at("blocks")) : kAuditBlocks;
    if (ns == "production-r1") {
        if (blocks != kAuditBlocks) throw UsageError("production replay covers exactly audit blocks 0..63");
    } else if (ns == "fixture-r1") {
        if (blocks < 1 || blocks > kAuditBlocks) throw UsageError("fixture replay needs --blocks in 1..64");
    } else {
        throw UsageError("namespace must be production-r1 or fixture-r1");
    }
    refuse_existing(receipt);
    if (path_within(receipt, root)) throw UsageError("receipt must lie outside the input root");
    const std::uint32_t block_limit = ns == "production-r1" ? kProductionBlocks : kAuditBlocks;

    Receipt r;
    r.mode = "replay";
    r.ns = ns;
    r.blocks = blocks;
    r.manifest_path = manifest.string();
    r.started = utc_now();
    try {
        r.sources = source_identity(src);
        r.binary_sha = binary_identity();
        run_selftests(r.selftests);
        const std::map<std::string, std::string> entries = parse_manifest(manifest, r.manifest_sha);
        for (const InputFile& f : kReplayInputs) {
            const auto it = entries.find(f.rel);
            if (it == entries.end()) throw AuditFailure("MANIFEST_MISSING_ENTRY", f.rel);
            InputIdentity id = identify_input(root, f.rel);
            if (id.sha256 != it->second) throw AuditFailure("MANIFEST_HASH_MISMATCH", f.rel);
            r.inputs.push_back(id);
        }
        const Keyring kr(ns, block_limit);
        const Layout layout{ns, blocks};
        {
            VerifySink sink(root, layout);
            r.have_counts = true;
            run_replay(kr, blocks, sink, r.counts);
            sink.finish();
        }
        for (const InputIdentity& before : r.inputs) {
            const InputIdentity after = identify_input(root, before.rel);
            if (after.sha256 != before.sha256 || after.bytes != before.bytes || after.mtime != before.mtime) {
                throw AuditFailure("INPUT_CHANGED", before.rel);
            }
        }
        std::string manifest_after;
        (void)parse_manifest(manifest, manifest_after);
        if (manifest_after != r.manifest_sha) throw AuditFailure("INPUT_CHANGED", "manifest");
        r.verdict = "PASS";
    } catch (...) {
        record_failure(r, std::current_exception());
    }
    return finish_receipt(r, receipt);
}

// Writes a complete non-scientific fixture record tree in namespace
// fixture-r1 for the corruption gate.  Production namespaces are refused.
int cmd_emit_fixture(int argc, char** argv) {
    const Args a = parse_args(argc, argv, {"out", "blocks"});
    const fs::path out = fs::absolute(required(a, "out"));
    const std::uint32_t blocks = parse_blocks(required(a, "blocks"));
    if (blocks < 1 || blocks > 8) throw UsageError("emit-fixture needs --blocks in 1..8");
    std::error_code ec;
    if (fs::symlink_status(out, ec).type() != fs::file_type::not_found) throw UsageError("output directory exists");
    try {
        if (!fs::create_directory(out)) throw AuditFailure("FIXTURE_OUTPUT_CREATE", "root");
        for (const char* d : {"updates", "paths", "blocks", "audit"}) {
            if (!fs::create_directory(out / d)) throw AuditFailure("FIXTURE_OUTPUT_CREATE", d);
        }
        const Keyring kr("fixture-r1", kAuditBlocks);
        const Layout layout{"fixture-r1", blocks};
        EmitSink sink(out, layout);
        ReplayCounts n;
        run_replay(kr, blocks, sink, n);
        sink.close();
        std::cout << kToolName << " emit-fixture fixture-r1 blocks " << blocks << " paths " << n.paths << "\n";
        return kExitPass;
    } catch (const AuditFailure& e) {
        std::cerr << "emit-fixture failed: " << e.code() << " (" << e.where() << ")\n";
        return kExitInvalid;
    } catch (const std::exception&) {
        std::cerr << "emit-fixture failed\n";
        return kExitInvalid;
    }
}

void usage() {
    std::cerr << "usage:\n"
                 "  torus_audit selftest --source-dir DIR --receipt FILE\n"
                 "  torus_audit replay --root DIR --manifest FILE --source-dir DIR --receipt FILE\n"
                 "                     [--namespace production-r1|fixture-r1] [--blocks N]\n"
                 "  torus_audit emit-fixture --out DIR --blocks N\n";
}

}  // namespace
}  // namespace t3a

int main(int argc, char** argv) {
    if (argc < 2) {
        t3a::usage();
        return t3a::kExitUsage;
    }
    const std::string cmd = argv[1];
    try {
        if (cmd == "selftest") return t3a::cmd_selftest(argc, argv);
        if (cmd == "replay") return t3a::cmd_replay(argc, argv);
        if (cmd == "emit-fixture") return t3a::cmd_emit_fixture(argc, argv);
        t3a::usage();
        return t3a::kExitUsage;
    } catch (const t3a::UsageError& e) {
        std::cerr << "refused: " << e.what() << "\n";
        t3a::usage();
        return t3a::kExitUsage;
    } catch (const std::exception&) {
        std::cerr << "refused: unexpected error before any receipt\n";
        return t3a::kExitUsage;
    }
}
