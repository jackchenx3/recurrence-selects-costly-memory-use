// Refuses any mismatch between config/frozen_config.json and the compiled
// frozen constants. There is no override.
#pragma once

#include <string>
#include <vector>

#include "mm/constants.hpp"
#include "mm/io.hpp"
#include "mm/json_min.hpp"

namespace mm {

struct ScalarExpectation {
  const char* path;
  char kind;  // 'n' number token, 's' string, 'b' boolean
  std::string value;
};

struct ArrayExpectation {
  const char* path;
  std::vector<std::string> values;
};

inline std::vector<ScalarExpectation> scalar_expectations() {
  return {
      {"study_id", 's', kStudyId},
      {"specification_revision", 'n', "1"},
      {"input_hashes.specification_sha256", 's', kSpecSha256},
      {"input_hashes.terminal_review_sha256", 's', kTerminalReviewSha256},
      {"input_hashes.design_go_sha256", 's', kDesignGoSha256},
      {"design_go_record.record", 's', kDesignGoRecord},
      {"design_go_record.status", 's', "DESIGN_GO"},
      {"design_go_record.scientific_execution_authorized", 'b', "false"},
      {"design_go_record.production_authorized", 'b', "false"},
      {"predecessor.accepted_study", 's', kPredecessorStudyId},
      {"population.size", 'n', "32"},
      {"population.genotype_bits", 'n', "32"},
      {"population.lineage_identifiers_read_by_operators", 'b', "false"},
      {"updates.first", 'n', "1"},
      {"updates.last", 'n', "256"},
      {"updates.late_window_first", 'n', "193"},
      {"updates.late_window_last", 'n', "256"},
      {"updates.late_window_length", 'n', "64"},
      {"target_law.name", 's', "HALF"},
      {"target_law.only_law", 'b', "true"},
      {"target_law.innovation_and_copy_bit_generated_every_update", 'b', "true"},
      {"candidates.per_parent", 'n', "4"},
      {"candidates.per_update", 'n', "128"},
      {"candidates.operator_reads_recurrence_event", 'b', "true"},
      {"candidates.allele_reads_recurrence_event", 'b', "false"},
      {"candidates.donor_families", 'n', "3"},
      {"candidates.local_bit_flip_probability_numerator", 'n', "1"},
      {"candidates.local_bit_flip_probability_denominator", 'n', "32"},
      {"candidates.genotype_deduplication", 'b', "false"},
      {"decoy_permutation.generated_unconditionally", 'b', "true"},
      {"decoy_permutation.fisher_yates_steps", 'n', "31"},
      {"selection.survivors", 'n', "32"},
      {"selection.weight_exponent_base", 'n', "32"},
      {"selection.max_weight_sum_log2", 'n', "39"},
      {"selection.floating_point_used", 'b', "false"},
      {"policy_mutation.mu_numerator", 'n', "1"},
      {"policy_mutation.mu_denominator", 'n', "32"},
      {"policy_mutation.symmetric", 'b', "true"},
      {"policy_mutation.flip_rule", 's', "(word0 & 31U) == 0U"},
      {"design.blocks", 'n', "41600"},
      {"design.paths_per_block", 'n', "6"},
      {"design.total_paths", 'n', "249600"},
      {"design.updates_per_path", 'n', "256"},
      {"design.total_path_updates", 'n', "63897600"},
      {"design.queries_per_update", 'n', "128"},
      {"design.total_objective_queries", 'n', "8178892800"},
      {"rng.generator", 's', "Philox4x32-10"},
      {"rng.rounds", 'n', "10"},
      {"rng.key_text_template", 's', "PHASE2-PERFORMANCE-CONVERSION-002|production-r1|<purpose>"},
      {"rng.key_namespace", 's', kProductionNamespace},
      {"rng.fixture_key_namespace_non_scientific", 's', kFixtureNamespace},
      {"rng.max_survival_retry_subindex", 'n', "4294967295"},
      {"rng.max_decoy_retry_subindex", 'n', "4294967295"},
      {"rng.collision_enumeration_retry_depth", 'n', "4"},
      {"inference.n_blocks", 'n', "41600"},
      {"inference.delta_numerator", 'n', "1"},
      {"inference.delta_denominator", 'n', "32"},
      {"inference.primary_family.familywise_alpha", 's', "0.05"},
      {"inference.primary_family.family_size", 'n', "3"},
      {"inference.primary_family.alpha_each", 's', "0.05/3"},
      {"inference.allele_family.familywise_alpha", 's', "0.05"},
      {"inference.allele_family.family_size", 'n', "2"},
      {"inference.allele_family.alpha_each", 's', "0.025"},
      {"inference.allele_family.may_alter_primary_decision", 'b', "false"},
      {"inference.performance_family.familywise_alpha", 's', "0.05"},
      {"inference.performance_family.family_size", 'n', "2"},
      {"inference.performance_family.alpha_each", 's', "0.025"},
      {"inference.performance_family.may_alter_primary_decision", 'b', "false"},
      {"saved_records.primary", 'n', "3"},
      {"saved_records.allele_secondary", 'n', "2"},
      {"saved_records.performance_secondary", 'n', "2"},
      {"saved_records.absolute_cell_means", 'n', "12"},
      {"saved_records.total", 'n', "19"},
      {"audit.first_block", 'n', "0"},
      {"audit.last_block", 'n', "63"},
      {"audit.audit_paths", 'n', "384"},
      {"audit.audit_rows", 'n', "12582912"},
      {"audit.audit_permutation_records", 'n', "16384"},
      {"resources.allocations", 'n', "1"},
      {"resources.max_cpus", 'n', "32"},
      {"resources.max_mem_gib", 'n', "64"},
      {"resources.max_wall_hours", 'n', "6"},
      {"resources.max_new_output_gib", 'n', "100"},
      {"output.byte_order", 's', "little-endian"},
      {"output.shards", 'n', "32"},
      {"output.blocks_per_shard", 'n', "1300"},
      {"output.update_record_bytes", 'n', "48"},
      {"output.path_record_bytes", 'n', "176"},
      {"output.block_record_bytes", 'n', "96"},
      {"output.audit_row_bytes", 'n', "88"},
      {"output.audit_permutation_record_bytes", 'n', "416"},
      {"output.total_record_bytes", 'n', "4229120000"},
  };
}

inline std::vector<ArrayExpectation> array_expectations() {
  return {
      {"candidates.families_in_order", {"PARENT", "POLICY_PROBE", "GLOBAL_SCOUT", "LOCAL_CHILD"}},
      {"design.arms", {"INFO", "NONINFO", "SHAM"}},
      {"design.starts", {"ALL_F", "ALL_M"}},
      {"design.cells_in_index_order",
       {"INFO|ALL_F", "INFO|ALL_M", "NONINFO|ALL_F", "NONINFO|ALL_M", "SHAM|ALL_F", "SHAM|ALL_M"}},
      {"rng.multipliers_hex", {"D2511F53", "CD9E8D57"}},
      {"rng.weyl_hex", {"9E3779B9", "BB67AE85"}},
      {"rng.counter_words", {"block", "update", "entity", "subindex"}},
      {"rng.purposes_in_order",
       {"INITIAL_GENOTYPE", "TARGET_INNOVATION", "TARGET_COPY", "FRESH_MASK", "SCOUT_MASK", "LOCAL_BIT", "DONOR_KEY",
        "SURVIVAL_UNIFORM", "POLICY_MUTATION", "DECOY_PERMUTATION"}},
  };
}

inline bool check_frozen_config(const JsonValue& root, std::vector<std::string>& errors) {
  for (const ScalarExpectation& e : scalar_expectations()) {
    const JsonValue* v = json_lookup(root, e.path);
    bool ok = v != nullptr;
    if (ok && e.kind == 'n') ok = v->kind == JsonValue::Kind::kNumber && v->text == e.value;
    if (ok && e.kind == 's') ok = v->kind == JsonValue::Kind::kString && v->text == e.value;
    if (ok && e.kind == 'b') ok = v->kind == JsonValue::Kind::kBool && (v->boolean ? "true" : "false") == e.value;
    if (!ok) errors.push_back(std::string("config mismatch at ") + e.path + " (expected " + e.value + ")");
  }
  for (const ArrayExpectation& e : array_expectations()) {
    const JsonValue* v = json_lookup(root, e.path);
    bool ok = v != nullptr && v->kind == JsonValue::Kind::kArray && v->items.size() == e.values.size();
    for (std::size_t i = 0; ok && i < e.values.size(); ++i)
      ok = v->items[i].kind == JsonValue::Kind::kString && v->items[i].text == e.values[i];
    if (!ok) errors.push_back(std::string("config array mismatch at ") + e.path);
  }
  return errors.empty();
}

inline bool load_and_check_config(const std::string& path, std::vector<std::string>& errors) {
  std::string raw;
  if (!read_file(path, raw)) {
    errors.push_back("cannot read config " + path);
    return false;
  }
  JsonValue root;
  std::string err;
  JsonParser parser(raw);
  if (!parser.parse(root, err)) {
    errors.push_back("config JSON parse error: " + err);
    return false;
  }
  return check_frozen_config(root, errors);
}

// True for exactly 64 lowercase hexadecimal digits.
inline bool is_sha256_hex(const std::string& s) {
  if (s.size() != 64U) return false;
  for (char c : s)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

// Checks the design-GO record against the compiled identities. The SHA-256 of
// the file itself is compared separately by the caller.
inline bool check_design_go_record(const std::string& path, std::vector<std::string>& errors) {
  std::string raw;
  if (!read_file(path, raw)) {
    errors.push_back("cannot read design-GO record " + path);
    return false;
  }
  JsonValue root;
  std::string err;
  JsonParser parser(raw);
  if (!parser.parse(root, err)) {
    errors.push_back("design-GO JSON parse error: " + err);
    return false;
  }
  const struct {
    const char* key;
    std::string value;
  } want[] = {{"record", kDesignGoRecord},
              {"study_id", kStudyId},
              {"status", "DESIGN_GO"},
              {"design_sha256", kSpecSha256},
              {"terminal_review_sha256", kTerminalReviewSha256},
              {"terminal_verdict", "DESIGN_GO"}};
  for (const auto& w : want) {
    const JsonValue* v = json_lookup(root, w.key);
    if (v == nullptr || v->kind != JsonValue::Kind::kString || v->text != w.value)
      errors.push_back(std::string("design-GO record mismatch at ") + w.key);
  }
  return errors.empty();
}

}  // namespace mm
