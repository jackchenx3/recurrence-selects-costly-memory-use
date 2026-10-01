"""Shared constants, binary layouts and config checks (Python 3.6+, stdlib only).

Mirrors src/mm/constants.hpp, src/mm/records.hpp and src/mm/config_check.hpp.
Imports no producer code from studies 001-064.
"""
import hashlib
import json
import os
import struct

STUDY_ID = "PHASE2-MUTABLE-MEMORY-001"
SPEC_SHA256 = "c17a3a1e9ac9cf2260f6743eaf08e4c2808080a767af16d2f9e9d2b9ad3e821d"
REVIEW_SHA256 = "787cf32a20312f2682c761243f3dec04c5eb8036d14d6efc444e9830db37960a"
PRODUCTION_NAMESPACE = "production-r1"
FIXTURE_NAMESPACE = "fixture-r1-nonscientific"
PURPOSES = ("INITIAL_GENOTYPE", "TARGET_INNOVATION", "TARGET_COPY", "FRESH_MASK", "SCOUT_MASK",
            "LOCAL_BIT", "DONOR_KEY", "SURVIVAL_UNIFORM", "POLICY_MUTATION")
CELL_NAMES = ("ACTIVE|ZERO|ALL_F", "ACTIVE|ZERO|ALL_M", "ACTIVE|HALF|ALL_F", "ACTIVE|HALF|ALL_M",
              "SHAM|ZERO|ALL_F", "SHAM|ZERO|ALL_M", "SHAM|HALF|ALL_F", "SHAM|HALF|ALL_M")

POP = 32
CANDIDATES = 128
UPDATES = 256
LATE_FIRST = 193
LATE_LAST = 256
N_BLOCKS = 41600
CELLS = 8
TOTAL_PATHS = 332800
TOTAL_PATH_UPDATES = 85196800
TOTAL_QUERIES = 10905190400
AUDIT_BLOCKS = 64
AUDIT_ROWS = 16777216
SHARDS = 32
BLOCKS_PER_SHARD = 1300
NA = 255

assert N_BLOCKS * CELLS == TOTAL_PATHS
assert TOTAL_PATHS * UPDATES == TOTAL_PATH_UPDATES
assert TOTAL_PATH_UPDATES * CANDIDATES == TOTAL_QUERIES
assert AUDIT_BLOCKS * CELLS * UPDATES * CANDIDATES == AUDIT_ROWS
assert SHARDS * BLOCKS_PER_SHARD == N_BLOCKS

# block u32, update u16, cell u8, m_count u8, total_mismatch u16, valid_m_cache u8,
# cache_probe_use u8, cache_probe_survivors u8, f_to_m u8, m_to_f u8, query_count u8,
# survival_retries u64 (sum of 32 accepted u32 retry indices)
UPDATE_REC = struct.Struct("<IHBBHBBBBBBQ")
# block u32, cell u8, arm u8, law u8, start u8, late_m_sum, late_mismatch_sum, total_queries,
# f_to_m, m_to_f, cache_probe_use, cache_probe_survivors (u32 each), survival_retries u64,
# fixation, extinction, late_fixation, late_extinction (u16 each), final_state_sha256[32],
# final_m_count u32, final_total_mismatch u32, reserved u32, paired_trajectory_sha256[32]
PATH_REC = struct.Struct("<IBBBBIIIIIIIQHHHH32sIII32s")
# block u32, n1 u8, n2 u8, query_ok u8, audit_block u8, c_abs, c_rec, d_half, d_zero, p_abs,
# p_rec (i32 each), late_m_sum[8] u32, late_mismatch_sum[8] u32
BLOCK_REC = struct.Struct("<IBBBB6i8I8I")
# block u32, update u16, cell u8, candidate u8, family u8, parent u8, parent_label u8,
# parent_cache_valid u8, parent_cache u32, genotype u32, target u32, mismatch u8, copy_bit u8,
# recurrence_applied u8, probe_source u8, weight u64, selected_rank u8, post_label u8,
# policy_flip u8, donor_family u8, W u64, x u64, Z u64, retry u32, reserved u32
AUDIT_ROW = struct.Struct("<IHBBBBBBIIIBBBBQBBBBQQQII")

assert UPDATE_REC.size == 24 and PATH_REC.size == 128 and BLOCK_REC.size == 96 and AUDIT_ROW.size == 72
TOTAL_RECORD_BYTES = (TOTAL_PATH_UPDATES * UPDATE_REC.size + TOTAL_PATHS * PATH_REC.size
                      + N_BLOCKS * BLOCK_REC.size + AUDIT_ROWS * AUDIT_ROW.size)
assert TOTAL_RECORD_BYTES == 3299274752

EXPECTED_SCALARS = {
    "study_id": STUDY_ID, "specification_revision": 1,
    "input_hashes.specification_sha256": SPEC_SHA256,
    "input_hashes.terminal_review_sha256": REVIEW_SHA256,
    "population.size": 32, "population.genotype_bits": 32,
    "population.lineage_identifiers_read_by_operators": False,
    "updates.first": 1, "updates.last": 256, "updates.late_window_first": 193,
    "updates.late_window_last": 256, "updates.late_window_length": 64,
    "candidates.per_parent": 4, "candidates.per_update": 128, "candidates.donor_families": 3,
    "candidates.local_bit_flip_probability_numerator": 1,
    "candidates.local_bit_flip_probability_denominator": 32,
    "candidates.genotype_deduplication": False,
    "selection.survivors": 32, "selection.weight_exponent_base": 32,
    "selection.max_weight_sum_log2": 39, "selection.floating_point_used": False,
    "policy_mutation.mu_numerator": 1, "policy_mutation.mu_denominator": 32,
    "policy_mutation.symmetric": True, "policy_mutation.flip_rule": "(word0 & 31U) == 0U",
    "design.blocks": 41600, "design.paths_per_block": 8, "design.total_paths": 332800,
    "design.updates_per_path": 256, "design.total_path_updates": 85196800,
    "design.queries_per_update": 128, "design.total_objective_queries": 10905190400,
    "rng.generator": "Philox4x32-10", "rng.rounds": 10,
    "rng.key_text_template": "PHASE2-MUTABLE-MEMORY-001|production-r1|<purpose>",
    "rng.key_namespace": PRODUCTION_NAMESPACE,
    "rng.fixture_key_namespace_non_scientific": FIXTURE_NAMESPACE,
    "rng.max_survival_retry_subindex": 4294967295, "rng.collision_enumeration_retry_depth": 4,
    "inference.n_blocks": 41600, "inference.Delta_numerator": 1, "inference.Delta_denominator": 32,
    "inference.primary_family.familywise_alpha": "0.05", "inference.primary_family.family_size": 4,
    "inference.primary_family.alpha_each": "0.0125",
    "inference.secondary_family.familywise_alpha": "0.05", "inference.secondary_family.family_size": 2,
    "inference.secondary_family.alpha_each": "0.025",
    "inference.secondary_family.Delta_P_numerator": 1,
    "inference.secondary_family.Delta_P_denominator": 32,
    "inference.secondary_family.may_alter_primary_decision": False,
    "saved_records.primary": 4, "saved_records.performance": 2,
    "saved_records.absolute_cell_means": 16, "saved_records.total": 22,
    "audit.first_block": 0, "audit.last_block": 63, "audit.audit_paths": 512,
    "audit.audit_rows": 16777216,
    "resources.allocations": 1, "resources.max_cpus": 32, "resources.max_mem_gib": 64,
    "resources.max_wall_hours": 6, "resources.max_new_output_gib": 100,
    "output.byte_order": "little-endian", "output.shards": 32, "output.blocks_per_shard": 1300,
    "output.update_record_bytes": 24, "output.path_record_bytes": 128,
    "output.block_record_bytes": 96, "output.audit_row_bytes": 72,
}
EXPECTED_ARRAYS = {
    "candidates.families_in_order": ["PARENT", "POLICY_PROBE", "GLOBAL_SCOUT", "LOCAL_CHILD"],
    "design.cells_in_index_order": list(CELL_NAMES),
    "rng.multipliers_hex": ["D2511F53", "CD9E8D57"],
    "rng.weyl_hex": ["9E3779B9", "BB67AE85"],
    "rng.counter_words": ["block", "update", "entity", "subindex"],
    "rng.purposes_in_order": list(PURPOSES),
}


def lookup(cfg, dotted):
    cur = cfg
    for part in dotted.split("."):
        if not isinstance(cur, dict) or part not in cur:
            raise KeyError(dotted)
        cur = cur[part]
    return cur


def load_config(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def check_config(cfg):
    """Return a list of mismatches; empty means the config equals the frozen values."""
    errors = []
    for path, expected in EXPECTED_SCALARS.items():
        try:
            value = lookup(cfg, path)
        except KeyError:
            errors.append("missing " + path)
            continue
        if type(value) is not type(expected) or value != expected:
            errors.append("mismatch at %s: %r != %r" % (path, value, expected))
    for path, expected in EXPECTED_ARRAYS.items():
        try:
            value = lookup(cfg, path)
        except KeyError:
            errors.append("missing " + path)
            continue
        if value != expected:
            errors.append("array mismatch at " + path)
    return errors


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def purpose_key(namespace, purpose):
    text = "%s|%s|%s" % (STUDY_ID, namespace, purpose)
    d = hashlib.sha256(text.encode("utf-8")).digest()
    return text, int.from_bytes(d[0:4], "little"), int.from_bytes(d[4:8], "little")


def shard_dir(prod_dir, shard):
    return os.path.join(prod_dir, "shards", "shard_%02d" % shard)


AUDIT_FILE = "audit_rows_blocks_0000_0063.bin"
