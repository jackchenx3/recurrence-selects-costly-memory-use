#include "config_check.hpp"

#include "sha256.hpp"
#include "threefry_rng.hpp"
#include "util.hpp"

namespace torus {

namespace {

// Exact frozen key order and raw JSON value tokens of
// config/torus_003_frozen_config.json.
const char* const kExpected[][2] = {
    {"schema", "\"PHASE2-TORUS-MEMORY-003-CONFIG-v1\""},
    {"study", "\"PHASE2-TORUS-MEMORY-003\""},
    {"class_name", "\"32-locus, 2^16-allele torus with graded circular loss and tournament survival\""},
    {"design_file", "\"PHASE2-TORUS-MEMORY-003-REV1.md\""},
    {"design_sha256", "\"0ff662dc31807b0b2cdcae371d884473ed398e2583272d94331bac8c8f9ff0b0\""},
    {"design_go_record_sha256", "\"e5dc485dd858f18eb9652cfe6b63f142e2d09fb6b069d640438cd8dbe63a786d\""},
    {"design_go_arithmetic_sha256", "\"fa90bf23484d383e0c6362f1e1a9de5ae70cbeee802c7a3c0b7d0aba1b504a8a\""},
    {"terminal_review_sha256", "\"68ccffc3c9a4d013590e7b5c8e5f4200ce955b77a76554bd58bad4bcc7dcc04a\""},
    {"terminal_verdict", "\"TORUS_DESIGN_GO\""},
    {"seed_receipt_sha256", "\"3fe28150db873b0d6ee1e7313de14f271d27d81e7e3ecf1e97f1677aba66dd53\""},
    {"random123_commit", "\"9545ff6413f258be2f04c1d319d99aaef7521150\""},
    {"random123_threefry_h_sha256", "\"4c210b32b5ba605b059c54d5edd6f01bf04190de49a0abeecec76420cd072a72\""},
    {"random123_kat_vectors_sha256", "\"aab5ebabf40003f63d6d87b24cbd2c8a02652e00cf8bad64226fd50586929183\""},
    {"threefry_variant", "\"Threefry4x64-20\""},
    {"threefry_rounds", "20"},
    {"threefry_rotations", "\"(14,16),(52,57),(23,40),(5,37),(25,33),(46,12),(58,22),(32,32)\""},
    {"threefry_parity", "\"0x1BD11BDAA9FC1A22\""},
    {"kat_zero", "\"09218ebde6c85537,55941f5266d86105,4bd25e16282434dc,ee29ec846bd2e40b\""},
    {"kat_ones", "\"29c24097942bba1b,0371bbfb0f6f4e11,3c231ffa33f83a1c,cd29113fde32d168\""},
    {"key_text_format", "\"PHASE2-TORUS-MEMORY-003|<NAMESPACE>|<PURPOSE>\""},
    {"key_word_rule", "\"SHA-256 digest bytes 8j..8j+7 little-endian = key word j\""},
    {"counter_rule", "\"(block,update,entity,subindex)\""},
    {"vector_extraction_rule", "\"coordinate 16*q+4*word+lane = (word >> 16*lane) & 0xffff\""},
    {"production_namespace", "\"production-r1\""},
    {"fixture_namespace", "\"fixture-r1\""},
    {"timing_namespace", "\"timing-r1\""},
    {"purposes",
     "\"INITIAL_VECTOR,TARGET_INNOVATION_VECTOR,TARGET_COPY,FRESH_VECTOR,SCOUT_VECTOR,LOCAL_REPLACE_FLAG,"
     "LOCAL_REPLACE_VALUE,DONOR_KEY,TOURNAMENT_ENTRY,CANDIDATE_TIE_KEY,POLICY_MUTATION\""},
    {"population", "32"},
    {"loci", "32"},
    {"allele_bits", "16"},
    {"updates", "256"},
    {"late_first", "193"},
    {"late_last", "256"},
    {"families", "\"PARENT,POLICY_PROBE,SCOUT,LOCAL_CHILD\""},
    {"candidate_index_rule", "\"32*family+parent_slot\""},
    {"candidates_per_update", "128"},
    {"survivor_slots", "32"},
    {"tournament_entries", "4"},
    {"tournament_entry_mask", "127"},
    {"tournament_order", "\"loss,candidate_tie_key,candidate_index\""},
    {"donor_order", "\"loss,donor_key,family\""},
    {"local_replace_mask", "31"},
    {"policy_mutation_mask", "31"},
    {"policy_mutation_rate", "\"1/32\""},
    {"arms", "\"ACTIVE,SHAM\""},
    {"target_laws", "\"ZERO,HALF\""},
    {"policy_starts", "\"ALL_F,ALL_M\""},
    {"cell_index_rule", "\"4*arm+2*law+start\""},
    {"blocks", "41600"},
    {"cells", "8"},
    {"paths", "332800"},
    {"path_updates", "85196800"},
    {"objective_queries", "10905190400"},
    {"declared_draws_per_block", "164672"},
    {"declared_draws_total", "6850355200"},
    {"chunk_blocks", "64"},
    {"chunks", "650"},
    {"audit_block_first", "0"},
    {"audit_block_last", "63"},
    {"audit_paths", "512"},
    {"audit_path_updates", "131072"},
    {"audit_candidate_rows", "16777216"},
    {"audit_entry_rows", "16777216"},
    {"primary_estimands", "\"C_abs,C_rec,D_HALF,D_ZERO\""},
    {"performance_estimands", "\"P_abs,P_rec\""},
    {"absolute_cell_means", "16"},
    {"estimate_records", "22"},
    {"delta", "\"1/32\""},
    {"delta_p", "\"1/32\""},
    {"primary_alpha_each", "\"1/80\""},
    {"performance_alpha_each", "\"1/40\""},
    {"h_c_abs", "\"0.007810229527949401\""},
    {"h_c_rec_d", "\"0.015620459055898803\""},
    {"h_p_abs", "\"0.014514625638859732\""},
    {"h_p_rec", "\"0.029029251277719464\""},
    {"resource_cpus", "32"},
    {"resource_mem_gib", "64"},
    {"resource_wall_hours", "12"},
    {"resource_output_gib", "150"},
    {"schema_version", "1"},
    {"endianness", "\"little\""},
    {"file_header_bytes", "64"},
    {"update_row_bytes", "24"},
    {"path_row_bytes", "224"},
    {"block_row_bytes", "136"},
    {"estimate_row_bytes", "256"},
    {"candidate_row_bytes", "104"},
    {"entry_row_bytes", "16"},
    {"context_row_bytes", "4240"},
    {"config_grants_execution", "false"},
};

bool is_ws(char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }

bool contains(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }

}  // namespace

void run_startup_self_tests() {
  TORUS_REQUIRE(sha256_self_test(), "SHA-256 self-test failed");
  TORUS_REQUIRE(threefry_design_kats(), "Threefry4x64-20 known-answer test failed");
}

std::vector<std::pair<std::string, std::string>> parse_flat_json(const std::string& t) {
  std::vector<std::pair<std::string, std::string>> out;
  std::size_t i = 0;
  auto ws = [&]() {
    while (i < t.size() && is_ws(t[i])) ++i;
  };
  auto expect = [&](char c) {
    ws();
    TORUS_REQUIRE(i < t.size() && t[i] == c, std::string("config parse: expected '") + c + "'");
    ++i;
  };
  auto string_token = [&]() -> std::string {
    ws();
    TORUS_REQUIRE(i < t.size() && t[i] == '"', "config parse: expected string");
    std::size_t s = i++;
    while (i < t.size() && t[i] != '"') {
      TORUS_REQUIRE(t[i] != '\\' && static_cast<unsigned char>(t[i]) >= 0x20, "config parse: escapes not allowed");
      ++i;
    }
    TORUS_REQUIRE(i < t.size(), "config parse: unterminated string");
    ++i;
    return t.substr(s, i - s);
  };
  expect('{');
  for (;;) {
    std::string key = string_token();
    expect(':');
    ws();
    std::string val;
    if (i < t.size() && t[i] == '"') {
      val = string_token();
    } else {
      std::size_t s = i;
      while (i < t.size() && ((t[i] >= '0' && t[i] <= '9') || (t[i] >= 'a' && t[i] <= 'z'))) ++i;
      val = t.substr(s, i - s);
      TORUS_REQUIRE(!val.empty(), "config parse: empty value");
    }
    out.emplace_back(key.substr(1, key.size() - 2), val);
    ws();
    TORUS_REQUIRE(i < t.size(), "config parse: truncated");
    if (t[i] == ',') {
      ++i;
      continue;
    }
    TORUS_REQUIRE(t[i] == '}', "config parse: expected ',' or '}'");
    ++i;
    break;
  }
  ws();
  TORUS_REQUIRE(i == t.size(), "config parse: trailing content");
  return out;
}

std::string verify_frozen_config(const std::string& path) {
  const std::string text = read_text_file(path);
  const auto kv = parse_flat_json(text);
  const std::size_t n = sizeof(kExpected) / sizeof(kExpected[0]);
  TORUS_REQUIRE(kv.size() == n, "config key count differs from the frozen configuration");
  for (std::size_t k = 0; k < n; ++k) {
    TORUS_REQUIRE(kv[k].first == kExpected[k][0], "config key mismatch at position " + std::to_string(k));
    TORUS_REQUIRE(kv[k].second == kExpected[k][1], "config value mismatch for " + kv[k].first);
  }
  return digest_hex(sha256_bytes(text.data(), text.size()));
}

void verify_random123(const std::string& root) {
  TORUS_REQUIRE(digest_hex(sha256_file(join_path(root, kThreefryHeaderRel))) == kThreefryHeaderSha256,
                "pinned Random123 threefry.h hash mismatch");
  TORUS_REQUIRE(digest_hex(sha256_file(join_path(root, kKatVectorsRel))) == kKatVectorsSha256,
                "pinned Random123 kat_vectors hash mismatch");
}

void verify_design_spec(const std::string& path) {
  TORUS_REQUIRE(digest_hex(sha256_file(path)) == kDesignSha256, "frozen design SHA-256 mismatch");
}

void verify_design_go_record(const std::string& path) {
  TORUS_REQUIRE(digest_hex(sha256_file(path)) == kDesignGoSha256, "design-GO record SHA-256 mismatch");
  const std::string t = read_text_file(path);
  TORUS_REQUIRE(contains(t, "\"terminal_verdict\": \"TORUS_DESIGN_GO\""), "design-GO record lacks TORUS_DESIGN_GO");
  TORUS_REQUIRE(contains(t, std::string("\"design_sha256\": \"") + kDesignSha256 + "\""),
                "design-GO record does not bind the frozen design");
  TORUS_REQUIRE(contains(t, std::string("\"terminal_review_sha256\": \"") + kTerminalReviewSha256 + "\""),
                "design-GO record does not bind the terminal review");
}

std::string verify_execution_authorization(const std::string& path, const std::string& config_sha256) {
  const std::string t = read_text_file(path);
  TORUS_REQUIRE(contains(t, "\"scientific_execution_authorized\": true"),
                "execution authorization record does not authorize scientific execution");
  TORUS_REQUIRE(contains(t, std::string("\"design_sha256\": \"") + kDesignSha256 + "\""),
                "execution authorization does not bind the frozen design");
  TORUS_REQUIRE(contains(t, std::string("\"design_go_record_sha256\": \"") + kDesignGoSha256 + "\""),
                "execution authorization does not bind the design-GO record");
  TORUS_REQUIRE(contains(t, "\"config_sha256\": \"" + config_sha256 + "\""),
                "execution authorization does not bind this configuration");
  TORUS_REQUIRE(contains(t, std::string("\"namespace\": \"") + kProductionNamespace + "\""),
                "execution authorization does not bind the production namespace");
  return digest_hex(sha256_file(path));
}

bool is_sha256_hex(const std::string& s) {
  if (s.size() != 64) return false;
  for (char c : s)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

}  // namespace torus
