#include "threefry_rng.hpp"

#include <Random123/threefry.h>

#include <tuple>

#include "sha256.hpp"
#include "util.hpp"

namespace torus {

namespace {

const char* const kPurposeNames[kPurposes] = {
    "INITIAL_VECTOR",     "TARGET_INNOVATION_VECTOR", "TARGET_COPY",      "FRESH_VECTOR",
    "SCOUT_VECTOR",       "LOCAL_REPLACE_FLAG",       "LOCAL_REPLACE_VALUE", "DONOR_KEY",
    "TOURNAMENT_ENTRY",   "CANDIDATE_TIE_KEY",        "POLICY_MUTATION",
};

thread_local std::vector<DrawAddress>* t_recorder = nullptr;

}  // namespace

const char* purpose_name(int p) {
  TORUS_REQUIRE(p >= 0 && p < kPurposes, "purpose out of range");
  return kPurposeNames[p];
}

std::string key_text(const std::string& ns, int purpose) {
  return std::string(kStudyTag) + "|" + ns + "|" + purpose_name(purpose);
}

KeySet derive_keys(const std::string& ns) {
  TORUS_REQUIRE(ns == kProductionNamespace || ns == kFixtureNamespace || ns == kTimingNamespace,
                "unknown RNG namespace: " + ns);
  KeySet ks;
  ks.ns = ns;
  for (int p = 0; p < kPurposes; ++p) {
    Digest d = sha256_string(key_text(ns, p));
    for (int j = 0; j < 4; ++j) {
      u64 w = 0;
      for (int b = 0; b < 8; ++b) w |= u64(d[8 * j + b]) << (8 * b);
      ks.key[p][j] = w;
    }
  }
  // Purpose separation requires pairwise distinct keys.
  for (int a = 0; a < kPurposes; ++a)
    for (int b = a + 1; b < kPurposes; ++b)
      TORUS_REQUIRE(ks.key[a] != ks.key[b], "purpose key collision");
  return ks;
}

std::string key_derivation_json(const KeySet& ks) {
  std::string js = "{\n  \"schema\": \"PHASE2-TORUS-MEMORY-003-KEY-DERIVATION-v1\",\n";
  js += "  \"namespace\": \"" + json_escape(ks.ns) + "\",\n";
  js += "  \"rule\": \"key word j = little-endian u64 of SHA-256(text) bytes 8j..8j+7\",\n  \"purposes\": [";
  for (int p = 0; p < kPurposes; ++p) {
    const std::string text = key_text(ks.ns, p);
    js += std::string(p ? "," : "") + "\n    {\"purpose\": \"" + purpose_name(p) + "\", \"text\": \"" +
          json_escape(text) + "\", \"sha256\": \"" + digest_hex(sha256_string(text)) + "\", \"key_words_hex\": [";
    for (int j = 0; j < 4; ++j) js += std::string(j ? ", " : "") + "\"" + hex64(ks.key[p][j]) + "\"";
    js += "]}";
  }
  js += "\n  ]\n}\n";
  return js;
}

Word4 threefry4x64_20(const Word4& ctr, const Word4& key) {
  typedef r123::Threefry4x64_R<20> G;
  G g;
  G::ctr_type c;
  G::key_type k;
  for (int i = 0; i < 4; ++i) {
    c.v[i] = ctr[i];
    k.v[i] = key[i];
  }
  G::ctr_type r = g(c, k);
  return Word4{{r.v[0], r.v[1], r.v[2], r.v[3]}};
}

bool threefry_design_kats() {
  const u64 ones = ~0ULL;
  Word4 z = threefry4x64_20(Word4{{0, 0, 0, 0}}, Word4{{0, 0, 0, 0}});
  Word4 ez{{0x09218ebde6c85537ULL, 0x55941f5266d86105ULL, 0x4bd25e16282434dcULL, 0xee29ec846bd2e40bULL}};
  Word4 o = threefry4x64_20(Word4{{ones, ones, ones, ones}}, Word4{{ones, ones, ones, ones}});
  Word4 eo{{0x29c24097942bba1bULL, 0x0371bbfb0f6f4e11ULL, 0x3c231ffa33f83a1cULL, 0xcd29113fde32d168ULL}};
  return z == ez && o == eo;
}

void check_address_domain(int purpose, u64 block, u64 update, u64 entity, u64 sub) {
  TORUS_REQUIRE(block < kBlocks, "draw address: block out of range");
  bool ok = false;
  const bool upd = update >= 1 && update <= u64(kUpdates);
  switch (purpose) {
    case P_INITIAL_VECTOR: ok = update == 0 && entity < 32 && sub < 2; break;
    case P_TARGET_INNOVATION_VECTOR: ok = upd && entity == 0 && sub < 2; break;
    case P_TARGET_COPY: ok = upd && entity == 0 && sub == 0; break;
    case P_FRESH_VECTOR:
    case P_SCOUT_VECTOR:
    case P_LOCAL_REPLACE_FLAG:
    case P_LOCAL_REPLACE_VALUE: ok = upd && entity < 32 && sub < 2; break;
    case P_DONOR_KEY: ok = upd && entity < u64(kDonorEntities) && sub == 0; break;
    case P_TOURNAMENT_ENTRY: ok = upd && entity < u64(kPop) && sub < u64(kEntries); break;
    case P_CANDIDATE_TIE_KEY: ok = upd && entity < u64(kCandidates) && sub == 0; break;
    case P_POLICY_MUTATION: ok = upd && entity < u64(kPop) && sub == 0; break;
    default: ok = false;
  }
  TORUS_REQUIRE(ok, std::string("draw address outside declared domain for ") +
                        (purpose >= 0 && purpose < kPurposes ? kPurposeNames[purpose] : "?"));
}

Word4 draw(const KeySet& ks, int purpose, u64 block, u64 update, u64 entity, u64 sub) {
  check_address_domain(purpose, block, update, entity, sub);
  if (t_recorder) {
    DrawAddress a;
    a.purpose = u8(purpose);
    a.ctr[0] = block;
    a.ctr[1] = update;
    a.ctr[2] = entity;
    a.ctr[3] = sub;
    t_recorder->push_back(a);
  }
  return threefry4x64_20(Word4{{block, update, entity, sub}}, ks.key[purpose]);
}

void draw_vector32(const KeySet& ks, int purpose, u64 block, u64 update, u64 entity, u16 out[32]) {
  for (int q = 0; q < 2; ++q) {
    Word4 w = draw(ks, purpose, block, update, entity, u64(q));
    for (int word = 0; word < 4; ++word) {
      for (int lane = 0; lane < 4; ++lane) {
        int coord = 16 * q + 4 * word + lane;
        TORUS_REQUIRE(coord >= 0 && coord < kLoci, "coordinate out of range");
        out[coord] = u16((w[word] >> (16 * lane)) & 0xffffULL);
      }
    }
  }
}

bool DrawAddress::operator<(const DrawAddress& o) const {
  return std::tie(purpose, ctr[0], ctr[1], ctr[2], ctr[3]) <
         std::tie(o.purpose, o.ctr[0], o.ctr[1], o.ctr[2], o.ctr[3]);
}

bool DrawAddress::operator==(const DrawAddress& o) const {
  return purpose == o.purpose && ctr[0] == o.ctr[0] && ctr[1] == o.ctr[1] && ctr[2] == o.ctr[2] &&
         ctr[3] == o.ctr[3];
}

void set_thread_draw_recorder(std::vector<DrawAddress>* rec) { t_recorder = rec; }

}  // namespace torus
