// Independent model replay for PHASE2-TORUS-MEMORY-003 (specification
// sections 3-9), plus the expected saved-row encoders used for byte-exact
// comparison.  Written from the frozen specification only.
#ifndef T3A_REPLAY_HPP
#define T3A_REPLAY_HPP

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "records.hpp"
#include "sha256.hpp"
#include "threefry.hpp"

namespace t3a {

using Pheno = std::array<std::uint16_t, kLoci>;

// d_i = min(u, 65536 - u) with u = (x_i - t_i) mod 2^16.
inline std::uint32_t circular_distance(std::uint16_t a, std::uint16_t b) {
    const std::uint32_t u =
        static_cast<std::uint16_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
    return u <= 32768u ? u : 65536u - u;
}

// Q(x,t) = sum_i d_i^2, exact, at most 2^35.
inline std::uint64_t torus_loss(const Pheno& x, const Pheno& t) {
    std::uint64_t q = 0;
    for (unsigned i = 0; i < kLoci; ++i) {
        const std::uint64_t d = circular_distance(x[i], t[i]);
        q += d * d;
    }
    return q;
}

inline void put_pheno(std::uint8_t* p, const Pheno& x) {
    for (unsigned k = 0; k < kLoci; ++k) put_u16(p + 2 * k, x[k]);
}

// ---------------------------------------------------------------------------
// Draws shared by every cell of one (block, update).

struct UpdateDraws {
    Pheno fresh[kPop];
    Pheno scout[kPop];
    Pheno flag[kPop];   // raw LOCAL_REPLACE_FLAG lanes; replace iff (lane & 31) == 0
    Pheno value[kPop];  // LOCAL_REPLACE_VALUE
    std::uint64_t donor_key[kPop][3];
    std::uint8_t entry[kPop][kEntries];
    std::uint64_t tie[kCandidates];
    std::uint8_t mutate[kPop];
};

inline void draw_update(const Keyring& kr, std::uint32_t block, unsigned t, UpdateDraws& d) {
    for (unsigned i = 0; i < kPop; ++i) {
        kr.vector32(Purpose::FreshVector, block, t, i, d.fresh[i].data());
        kr.vector32(Purpose::ScoutVector, block, t, i, d.scout[i].data());
        kr.vector32(Purpose::LocalReplaceFlag, block, t, i, d.flag[i].data());
        kr.vector32(Purpose::LocalReplaceValue, block, t, i, d.value[i].data());
        for (unsigned f = 0; f < 3; ++f) d.donor_key[i][f] = kr.word0(Purpose::DonorKey, block, t, 3u * i + f, 0);
    }
    for (unsigned s = 0; s < kPop; ++s) {
        for (unsigned e = 0; e < kEntries; ++e) {
            d.entry[s][e] = static_cast<std::uint8_t>(kr.word0(Purpose::TournamentEntry, block, t, s, e) & 127u);
        }
        d.mutate[s] = (kr.word0(Purpose::PolicyMutation, block, t, s, 0) & 31u) == 0 ? 1 : 0;
    }
    for (unsigned c = 0; c < kCandidates; ++c) d.tie[c] = kr.word0(Purpose::CandidateTieKey, block, t, c, 0);
}

// ---------------------------------------------------------------------------
// Target laws (section 5).  Index 0 is unused.

struct BlockTargets {
    Pheno innovation[kUpdates + 1];
    std::uint8_t copy_bit[kUpdates + 1];
    Pheno target[2][kUpdates + 1];   // [law: 0 ZERO, 1 HALF]
    std::uint8_t copied[2][kUpdates + 1];
};

inline void apply_target_laws(BlockTargets& bt) {
    bt.target[0][0].fill(0);
    bt.target[1][0].fill(0);
    bt.copied[0][0] = 0;
    bt.copied[1][0] = 0;
    for (unsigned t = 1; t <= kUpdates; ++t) {
        bt.target[0][t] = bt.innovation[t];
        bt.copied[0][t] = 0;
        const bool copy = t >= 3 && bt.copy_bit[t] == 1;
        bt.target[1][t] = copy ? bt.target[1][t - 2] : bt.innovation[t];
        bt.copied[1][t] = copy ? 1 : 0;
    }
}

inline void draw_targets(const Keyring& kr, std::uint32_t block, BlockTargets& bt) {
    bt.innovation[0].fill(0);
    bt.copy_bit[0] = 0;
    for (unsigned t = 1; t <= kUpdates; ++t) {
        kr.vector32(Purpose::TargetInnovationVector, block, t, 0, bt.innovation[t].data());
        bt.copy_bit[t] = static_cast<std::uint8_t>(kr.word0(Purpose::TargetCopy, block, t, 0, 0) & 1u);
    }
    apply_target_laws(bt);
}

// ---------------------------------------------------------------------------
// Population state of one cell.  Labels: F = 0, M = 1.  Invalid caches are
// held as all-zero phenotypes.

struct CellState {
    Pheno x[kPop];
    std::uint8_t label[kPop];
    std::uint8_t cache_valid[kPop];
    Pheno cache[kPop];
};

inline void init_cell(CellState& s, const Pheno* initial, std::uint8_t start_label) {
    for (unsigned i = 0; i < kPop; ++i) {
        s.x[i] = initial[i];
        s.label[i] = start_label;
        s.cache_valid[i] = 0;
        s.cache[i].fill(0);
    }
}

struct StepOut {
    Pheno cand[kCandidates];
    std::uint64_t loss[kCandidates];
    std::uint8_t probe_read[kPop];
    std::uint8_t donor_family[kPop];
    std::uint8_t flagged[kPop];
    std::uint8_t changed[kPop];
    std::uint8_t winner[kPop];
    std::uint8_t dup[kPop];
    std::uint8_t won[kCandidates];
    std::uint8_t inherited[kPop];
    std::uint8_t post_label[kPop];
    std::uint8_t flipped[kPop];
    std::uint64_t pop_loss;
    std::uint32_t queries;
    std::uint32_t local_flagged;
    std::uint32_t local_changed;
    std::uint32_t m_count;
    std::uint32_t valid_pre;
    std::uint32_t probe_uses;
    std::uint32_t probe_winner_slots;
    std::uint32_t f_to_m;
    std::uint32_t m_to_f;
    std::uint32_t dup_tournaments;
    std::uint32_t distinct;
};

// Donor: smallest loss, then lower unsigned donor key, then lower family.
inline unsigned choose_donor(const std::uint64_t* loss3, const std::uint64_t* key3) {
    unsigned best = 0;
    for (unsigned f = 1; f < 3; ++f) {
        if (loss3[f] < loss3[best] || (loss3[f] == loss3[best] && key3[f] < key3[best])) best = f;
    }
    return best;
}

// Tournament order: lowest loss, then lowest candidate tie key, then lowest
// candidate index.  Labels, caches, lineage and entry position never enter.
inline bool candidate_precedes(unsigned a, unsigned b, const std::uint64_t* loss, const std::uint64_t* tie) {
    if (loss[a] != loss[b]) return loss[a] < loss[b];
    if (tie[a] != tie[b]) return tie[a] < tie[b];
    return a < b;
}

inline unsigned tournament_winner(const std::uint8_t* entries, const std::uint64_t* loss, const std::uint64_t* tie) {
    unsigned best = entries[0];
    for (unsigned e = 1; e < kEntries; ++e) {
        if (candidate_precedes(entries[e], best, loss, tie)) best = entries[e];
    }
    return best;
}

// One update of one cell (sections 6 and 7).  pre and post must be distinct.
inline void step_cell(const CellState& pre, const UpdateDraws& d, const Pheno& target, bool active, StepOut& o,
                      CellState& post) {
    o = StepOut{};
    for (unsigned i = 0; i < kPop; ++i) {
        const bool reads = active && pre.label[i] == 1 && pre.cache_valid[i] == 1;
        o.probe_read[i] = reads ? 1 : 0;
        o.cand[i] = pre.x[i];
        o.cand[32 + i] = reads ? pre.cache[i] : d.fresh[i];
        o.cand[64 + i] = d.scout[i];
        std::uint64_t loss3[3];
        for (unsigned f = 0; f < 3; ++f) {
            loss3[f] = torus_loss(o.cand[32 * f + i], target);
            o.loss[32 * f + i] = loss3[f];
            ++o.queries;
        }
        const unsigned donor = choose_donor(loss3, d.donor_key[i]);
        o.donor_family[i] = static_cast<std::uint8_t>(donor);
        Pheno child = o.cand[32 * donor + i];
        unsigned flagged = 0;
        unsigned changed = 0;
        for (unsigned k = 0; k < kLoci; ++k) {
            if ((d.flag[i][k] & 31u) == 0) {
                ++flagged;
                if (d.value[i][k] != child[k]) ++changed;
                child[k] = d.value[i][k];
            }
        }
        o.flagged[i] = static_cast<std::uint8_t>(flagged);
        o.changed[i] = static_cast<std::uint8_t>(changed);
        o.cand[96 + i] = child;
        o.loss[96 + i] = torus_loss(child, target);
        ++o.queries;
        o.local_flagged += flagged;
        o.local_changed += changed;
        if (pre.cache_valid[i] == 1) ++o.valid_pre;
        if (reads) ++o.probe_uses;
    }
    for (unsigned s = 0; s < kPop; ++s) {
        const std::uint8_t* ent = d.entry[s];
        for (unsigned e = 0; e < kEntries; ++e) {
            if (ent[e] >= kCandidates) throw RangeRefusal("tournament entry out of range");
        }
        const unsigned w = tournament_winner(ent, o.loss, d.tie);
        bool dup = false;
        for (unsigned a = 0; a < kEntries; ++a) {
            for (unsigned b = a + 1; b < kEntries; ++b) dup = dup || ent[a] == ent[b];
        }
        o.winner[s] = static_cast<std::uint8_t>(w);
        o.dup[s] = dup ? 1 : 0;
        if (dup) ++o.dup_tournaments;
        ++o.won[w];
        o.pop_loss += o.loss[w];
        if ((w >> 5) == 1 && o.probe_read[w & 31u] == 1) ++o.probe_winner_slots;
    }
    for (unsigned c = 0; c < kCandidates; ++c) {
        if (o.won[c] > 0) ++o.distinct;
    }
    // Event order: inherit producing parent's pre-update label, flip with
    // POLICY_MUTATION, then F -> invalid cache, M -> cache of the producing
    // parent's pre-update phenotype.
    for (unsigned s = 0; s < kPop; ++s) {
        const unsigned w = o.winner[s];
        const unsigned p = w & 31u;
        const std::uint8_t inherited = pre.label[p];
        const std::uint8_t flip = d.mutate[s];
        const std::uint8_t lab = static_cast<std::uint8_t>(inherited ^ flip);
        post.x[s] = o.cand[w];
        post.label[s] = lab;
        if (lab == 1) {
            post.cache_valid[s] = 1;
            post.cache[s] = pre.x[p];
        } else {
            post.cache_valid[s] = 0;
            post.cache[s].fill(0);
        }
        o.inherited[s] = inherited;
        o.post_label[s] = lab;
        o.flipped[s] = flip;
        if (lab == 1) ++o.m_count;
        if (inherited == 0 && lab == 1) ++o.f_to_m;
        if (inherited == 1 && lab == 0) ++o.m_to_f;
    }
}

// ---------------------------------------------------------------------------
// Expected saved rows.

inline void encode_context(std::uint8_t* r, std::uint32_t block, unsigned t, unsigned cell, std::uint8_t copy_bit,
                           std::uint8_t copied, const Pheno& target, const CellState& pre) {
    std::memset(r, 0, 4240);
    put_u32(r, block);
    put_u16(r + 4, static_cast<std::uint16_t>(t));
    r[6] = static_cast<std::uint8_t>(cell);
    r[7] = copy_bit;
    r[8] = copied;
    put_pheno(r + 16, target);
    for (unsigned s = 0; s < kPop; ++s) {
        put_pheno(r + 80 + 64 * s, pre.x[s]);
        r[2128 + s] = pre.label[s];
        r[2160 + s] = pre.cache_valid[s];
        if (pre.cache_valid[s] == 1) put_pheno(r + 2192 + 64 * s, pre.cache[s]);
    }
}

// Interpretation (documented in docs/INPUT_CONTRACT.md): flags bit0 is the
// producing parent's pre-update cache validity on all four of its rows; bit1
// is set only on the family-1 row whose probe read the cache; bit2 only on the
// family-0..2 row chosen as donor.
inline void encode_candidate(std::uint8_t* r, std::uint32_t block, unsigned t, unsigned cell, unsigned c,
                             const CellState& pre, const UpdateDraws& d, const StepOut& o) {
    std::memset(r, 0, 104);
    const unsigned family = c >> 5;
    const unsigned parent = c & 31u;
    put_u32(r, block);
    put_u16(r + 4, static_cast<std::uint16_t>(t));
    r[6] = static_cast<std::uint8_t>(cell);
    r[7] = static_cast<std::uint8_t>(c);
    r[8] = static_cast<std::uint8_t>(family);
    r[9] = static_cast<std::uint8_t>(parent);
    r[10] = pre.label[parent];
    std::uint8_t flags = pre.cache_valid[parent] == 1 ? 1 : 0;
    if (family == 1 && o.probe_read[parent] == 1) flags = static_cast<std::uint8_t>(flags | 2u);
    if (family < 3 && o.donor_family[parent] == family) flags = static_cast<std::uint8_t>(flags | 4u);
    r[11] = flags;
    r[12] = family == 3 ? o.donor_family[parent] : 255;
    r[13] = family == 3 ? o.flagged[parent] : 0;
    r[14] = family == 3 ? o.changed[parent] : 0;
    r[15] = o.won[c];
    put_u64(r + 16, o.loss[c]);
    put_u64(r + 24, family < 3 ? d.donor_key[parent][family] : 0);
    put_u64(r + 32, d.tie[c]);
    put_pheno(r + 40, o.cand[c]);
}

inline void encode_entry(std::uint8_t* r, std::uint32_t block, unsigned t, unsigned cell, unsigned s, unsigned e,
                         const UpdateDraws& d, const StepOut& o, const CellState& post) {
    put_u32(r, block);
    put_u16(r + 4, static_cast<std::uint16_t>(t));
    r[6] = static_cast<std::uint8_t>(cell);
    r[7] = static_cast<std::uint8_t>(s);
    r[8] = static_cast<std::uint8_t>(e);
    r[9] = d.entry[s][e];
    r[10] = o.winner[s];
    r[11] = o.inherited[s];
    r[12] = o.post_label[s];
    r[13] = o.flipped[s];
    r[14] = post.cache_valid[s];
    r[15] = 0;
}

inline void encode_update(std::uint8_t* r, unsigned t, const StepOut& o) {
    put_u64(r, o.pop_loss);
    put_u16(r + 8, static_cast<std::uint16_t>(t));
    put_u16(r + 10, static_cast<std::uint16_t>(o.queries));
    put_u16(r + 12, static_cast<std::uint16_t>(o.local_flagged));
    put_u16(r + 14, static_cast<std::uint16_t>(o.local_changed));
    r[16] = static_cast<std::uint8_t>(o.m_count);
    r[17] = static_cast<std::uint8_t>(o.valid_pre);
    r[18] = static_cast<std::uint8_t>(o.probe_uses);
    r[19] = static_cast<std::uint8_t>(o.probe_winner_slots);
    r[20] = static_cast<std::uint8_t>(o.f_to_m);
    r[21] = static_cast<std::uint8_t>(o.m_to_f);
    r[22] = static_cast<std::uint8_t>(o.dup_tournaments);
    r[23] = static_cast<std::uint8_t>(o.distinct);
}

// Final state digest: 32 phenotypes (u16 LE, slot order), 32 label bytes,
// 32 cache-valid bytes, 32 caches (u16 LE, zero when invalid).
inline Digest final_state_digest(const CellState& s) {
    std::vector<std::uint8_t> buf(kPop * 64 + kPop + kPop + kPop * 64);
    std::uint8_t* p = buf.data();
    for (unsigned i = 0; i < kPop; ++i, p += 64) put_pheno(p, s.x[i]);
    for (unsigned i = 0; i < kPop; ++i) *p++ = s.label[i];
    for (unsigned i = 0; i < kPop; ++i) *p++ = s.cache_valid[i];
    for (unsigned i = 0; i < kPop; ++i, p += 64) {
        Pheno c = s.cache[i];
        if (s.cache_valid[i] == 0) c.fill(0);
        put_pheno(p, c);
    }
    return sha256_bytes(buf.data(), buf.size());
}

// Per-path accumulation of summaries and hashes.
struct PathAcc {
    Sha256 update_hash;
    Sha256 blind_hash;
    Sha256 label_hash;
    Sha256 complement_hash;
    Digest update_digest{};
    Digest blind_digest{};
    Digest label_digest{};
    Digest complement_digest{};
    std::uint64_t late_m = 0, queries = 0, late_loss = 0, all_loss = 0, all_m = 0;
    std::uint64_t probe_uses = 0, probe_winners = 0, f_to_m = 0, m_to_f = 0, dup = 0, flagged = 0, changed = 0;
    std::vector<std::uint64_t> pop_loss_seq;
    std::vector<std::uint8_t> m_seq;
    std::vector<std::uint8_t> blind_buf = std::vector<std::uint8_t>(kCandidates * 64 + kCandidates * 8 + kPop + kPop * 64);

    void add(const std::uint8_t* update_row, unsigned t, const StepOut& o, const CellState& post) {
        update_hash.update(update_row, 24);
        std::uint8_t* p = blind_buf.data();
        for (unsigned c = 0; c < kCandidates; ++c, p += 64) put_pheno(p, o.cand[c]);
        for (unsigned c = 0; c < kCandidates; ++c, p += 8) put_u64(p, o.loss[c]);
        for (unsigned s = 0; s < kPop; ++s) *p++ = o.winner[s];
        for (unsigned s = 0; s < kPop; ++s, p += 64) put_pheno(p, post.x[s]);
        blind_hash.update(blind_buf.data(), blind_buf.size());
        std::uint8_t lab[kPop];
        std::uint8_t comp[kPop];
        for (unsigned s = 0; s < kPop; ++s) {
            lab[s] = post.label[s];
            comp[s] = static_cast<std::uint8_t>(1u - post.label[s]);
        }
        label_hash.update(lab, kPop);
        complement_hash.update(comp, kPop);
        queries += o.queries;
        all_loss += o.pop_loss;
        all_m += o.m_count;
        if (t >= kLateFirst) {
            late_m += o.m_count;
            late_loss += o.pop_loss;
        }
        probe_uses += o.probe_uses;
        probe_winners += o.probe_winner_slots;
        f_to_m += o.f_to_m;
        m_to_f += o.m_to_f;
        dup += o.dup_tournaments;
        flagged += o.local_flagged;
        changed += o.local_changed;
        pop_loss_seq.push_back(o.pop_loss);
        m_seq.push_back(static_cast<std::uint8_t>(o.m_count));
    }

    void finalize() {
        update_digest = update_hash.finish();
        blind_digest = blind_hash.finish();
        label_digest = label_hash.finish();
        complement_digest = complement_hash.finish();
    }
};

inline void encode_path(std::uint8_t* r, std::uint32_t block, unsigned cell, const PathAcc& a, const Digest& final_state) {
    std::memset(r, 0, 224);
    put_u32(r, block);
    r[4] = static_cast<std::uint8_t>(cell);
    r[5] = static_cast<std::uint8_t>(cell >> 2);
    r[6] = static_cast<std::uint8_t>((cell >> 1) & 1u);
    r[7] = static_cast<std::uint8_t>(cell & 1u);
    put_u32(r + 8, static_cast<std::uint32_t>(a.late_m));
    put_u32(r + 12, static_cast<std::uint32_t>(a.queries));
    put_u64(r + 16, a.late_loss);
    put_u64(r + 24, a.all_loss);
    put_u32(r + 32, static_cast<std::uint32_t>(a.all_m));
    put_u32(r + 36, static_cast<std::uint32_t>(a.probe_uses));
    put_u32(r + 40, static_cast<std::uint32_t>(a.probe_winners));
    put_u32(r + 44, static_cast<std::uint32_t>(a.f_to_m));
    put_u32(r + 48, static_cast<std::uint32_t>(a.m_to_f));
    put_u32(r + 52, static_cast<std::uint32_t>(a.dup));
    put_u32(r + 56, static_cast<std::uint32_t>(a.flagged));
    put_u32(r + 60, static_cast<std::uint32_t>(a.changed));
    std::memcpy(r + 64, final_state.data(), 32);
    std::memcpy(r + 96, a.update_digest.data(), 32);
    std::memcpy(r + 128, a.blind_digest.data(), 32);
    std::memcpy(r + 160, a.label_digest.data(), 32);
    std::memcpy(r + 192, a.complement_digest.data(), 32);
}

// Section 9 identities, recomputed from the replay: bit0/bit1 N1 (label-blind
// trajectory identical between SHAM starts), bit2/bit3 N2 (labels exact
// complements), bit4 per-update M counts sum to 32 and late sums to 2048,
// bit5 every update made exactly 128 objective queries.
inline std::uint32_t identity_flags(const std::vector<PathAcc>& a) {
    auto n1 = [&](unsigned x, unsigned y) {
        return a[x].blind_digest == a[y].blind_digest && a[x].pop_loss_seq == a[y].pop_loss_seq;
    };
    auto n2 = [&](unsigned x, unsigned y) {
        return a[x].label_digest == a[y].complement_digest && a[y].label_digest == a[x].complement_digest &&
               a[x].f_to_m == a[y].m_to_f && a[x].m_to_f == a[y].f_to_m;
    };
    bool sums = a[4].late_m + a[5].late_m == 2048 && a[6].late_m + a[7].late_m == 2048 &&
                a[4].m_seq.size() == kUpdates && a[6].m_seq.size() == kUpdates;
    for (unsigned t = 0; sums && t < kUpdates; ++t) {
        sums = a[4].m_seq[t] + a[5].m_seq[t] == 32 && a[6].m_seq[t] + a[7].m_seq[t] == 32;
    }
    bool queries = true;
    for (unsigned c = 0; c < 8; ++c) queries = queries && a[c].queries == 128ull * kUpdates;
    std::uint32_t flags = 0;
    if (n1(4, 5)) flags |= 1u;
    if (n1(6, 7)) flags |= 2u;
    if (n2(4, 5)) flags |= 4u;
    if (n2(6, 7)) flags |= 8u;
    if (sums) flags |= 16u;
    if (queries) flags |= 32u;
    return flags;
}

inline void encode_block(std::uint8_t* r, std::uint32_t block, const std::vector<PathAcc>& a, std::uint32_t flags) {
    std::memset(r, 0, 136);
    put_u32(r, block);
    put_u32(r + 4, flags);
    std::int64_t s[8];
    std::int64_t l[8];
    for (unsigned c = 0; c < 8; ++c) {
        s[c] = static_cast<std::int64_t>(a[c].late_m);
        l[c] = static_cast<std::int64_t>(a[c].late_loss);
        put_u32(r + 8 + 4 * c, static_cast<std::uint32_t>(a[c].late_m));
        put_u64(r + 40 + 8 * c, a[c].late_loss);
    }
    put_i32(r + 104, static_cast<std::int32_t>(s[2] + s[3] - 2048));
    put_i32(r + 108, static_cast<std::int32_t>((s[2] + s[3]) - (s[0] + s[1])));
    put_i32(r + 112, static_cast<std::int32_t>(2 * (s[3] - s[2])));
    put_i32(r + 116, static_cast<std::int32_t>(2 * (s[1] - s[0])));
    const std::int64_t p_abs = (l[6] + l[7]) - (l[2] + l[3]);
    put_i64(r + 120, p_abs);
    put_i64(r + 128, p_abs - ((l[4] + l[5]) - (l[0] + l[1])));
}

}  // namespace t3a

#endif  // T3A_REPLAY_HPP
