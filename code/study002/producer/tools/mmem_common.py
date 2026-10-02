"""Shared constants, binary layouts and config checks (Python 3.6+, stdlib only).

PHASE2-PERFORMANCE-CONVERSION-002 revision 1. Mirrors src/mm/constants.hpp,
src/mm/records.hpp and src/mm/config_check.hpp. Imports no producer code.
"""
import hashlib
import json
import os
import re
import struct

STUDY_ID = "PHASE2-PERFORMANCE-CONVERSION-002"
SPEC_SHA256 = "58ee414d753fab042e6cccee1a4e6b95dd7f167633b4f33c1491687d9afcb895"
REVIEW_SHA256 = "76b0c6172e61f60befecef183e83d9e43ea50c1b6bc1e93299668b76f55be60f"
# Sentinel until a reviewer hashes the exact DESIGN_002_GO.json bytes (see config design_go_record).
DESIGN_GO_SHA256 = "f4a6180109e0d212cbe784a7722a64576e5e7cb0fbfd4b23c757bb5e7ab84d6a"
DESIGN_GO_RECORD = "PHASE2-PERFORMANCE-CONVERSION-002-DESIGN-GO-1"
PREDECESSOR_STUDY_ID = "PHASE2-MUTABLE-MEMORY-001"
PRODUCTION_NAMESPACE = "production-r1"
FIXTURE_NAMESPACE = "fixture-r1-nonscientific"
PURPOSES = ("INITIAL_GENOTYPE", "TARGET_INNOVATION", "TARGET_COPY", "FRESH_MASK", "SCOUT_MASK",
            "LOCAL_BIT", "DONOR_KEY", "SURVIVAL_UNIFORM", "POLICY_MUTATION", "DECOY_PERMUTATION")
ARMS = ("INFO", "NONINFO", "SHAM")
INFO, NONINFO, SHAM = 0, 1, 2
CELL_NAMES = ("INFO|ALL_F", "INFO|ALL_M", "NONINFO|ALL_F", "NONINFO|ALL_M", "SHAM|ALL_F", "SHAM|ALL_M")
SRC_FRESH, SRC_TRUE_CACHE, SRC_DECOY = 0, 1, 2

POP = 32
CANDIDATES = 128
UPDATES = 256
LATE_FIRST = 193
LATE_LAST = 256
N_BLOCKS = 41600
CELLS = 6
TOTAL_PATHS = 249600
TOTAL_PATH_UPDATES = 63897600
TOTAL_QUERIES = 8178892800
AUDIT_BLOCKS = 64
AUDIT_PATHS = 384
AUDIT_ROWS = 12582912
AUDIT_PERM_RECORDS = 16384
PERM_STEPS = 31
SHARDS = 32
BLOCKS_PER_SHARD = 1300
SAVED_ESTIMATES = 19
NA = 255

assert N_BLOCKS * CELLS == TOTAL_PATHS
assert TOTAL_PATHS * UPDATES == TOTAL_PATH_UPDATES
assert TOTAL_PATH_UPDATES * CANDIDATES == TOTAL_QUERIES
assert AUDIT_BLOCKS * CELLS == AUDIT_PATHS
assert AUDIT_PATHS * UPDATES * CANDIDATES == AUDIT_ROWS
assert AUDIT_BLOCKS * UPDATES == AUDIT_PERM_RECORDS
assert SHARDS * BLOCKS_PER_SHARD == N_BLOCKS
assert 3 + 2 + 2 + 2 * CELLS == SAVED_ESTIMATES


def cell_arm(c):
    return c >> 1


def cell_start(c):
    return c & 1


# 0 block u32, 1 update u16, 2 cell u8, 3 m_count u8, 4 total_mismatch u16, 5 valid_m_cache u8,
# 6 cache_probe_use u8, 7 cache_probe_survivors u8, 8 f_to_m u8, 9 m_to_f u8, 10 query_count u8,
# 11 survival_retries u64, 12 recurrence_applied u8, 13 true_cache_use u8, 14 decoy_use u8,
# 15 true_cache_survivors u8, 16 decoy_survivors u8, 17 valid_m_disp_w0 u8, 18 valid_m_disp_w32 u8,
# 19 decoy_w0 u8, 20 decoy_w32 u8, 21 decoy_identical u8, 22 reserved u16, 23 perm_retry_total u32,
# 24 perm_fnv1a u32, 25 reserved u32
UPDATE_REC = struct.Struct("<IHBBHBBBBBBQBBBBBBBBBBHIII")
# 0 block u32, 1 cell u8, 2 arm u8, 3 start u8, 4 reserved u8, 5 late_m_sum, 6 late_mismatch_sum,
# 7 total_queries, 8 f_to_m, 9 m_to_f, 10 cache_probe_use, 11 cache_probe_survivors (u32 each),
# 12 survival_retries u64, 13 fixation, 14 extinction, 15 late_fixation, 16 late_extinction (u16 each),
# 17 final_state_sha256[32], 18 final_m_count u32, 19 final_total_mismatch u32, 20 reserved u32,
# 21 paired_trajectory_sha256[32], 22 true_cache_use, 23 decoy_use, 24 true_cache_survivors,
# 25 decoy_survivors, 26 valid_m_disp_w0, 27 valid_m_disp_w32, 28 decoy_w0, 29 decoy_w32,
# 30 decoy_identical, 31 recurrent_updates (u32 each), 32 perm_retry_total u64
PATH_REC = struct.Struct("<IBBBBIIIIIIIQHHHH32sIII32sIIIIIIIIIIQ")
# 0 block u32, 1 n1 u8, 2 n2 u8, 3 query_ok u8, 4 audit_block u8, 5..11 delta_p, d_info, d_noninfo,
# e_info, e_noninfo, b_info, b_noninfo (i32 each), 12..17 late_m_sum[6] u32, 18..23 late_mismatch_sum[6]
# u32, 24 c1_ok u8, 25 reserved u8, 26 first_decoupling_ALL_F u16, 27 first_decoupling_ALL_M u16,
# 28 recurrent_updates u16, 29 perm_retry_total u32
BLOCK_REC = struct.Struct("<IBBBB7i6I6IBBHHHI")
# 0 block u32, 1 update u16, 2 cell u8, 3 candidate u8, 4 family u8, 5 parent u8, 6 parent_label u8,
# 7 parent_cache_valid u8, 8 parent_cache u32, 9 genotype u32, 10 target u32, 11 mismatch u8,
# 12 recurrence_bit u8, 13 recurrence_applied u8, 14 probe_source u8, 15 weight u64, 16 selected_rank u8,
# 17 post_label u8, 18 policy_flip u8, 19 donor_family u8, 20 W u64, 21 x u64, 22 Z u64, 23 retry u32,
# 24 perm_ref u32, 25 parent_genotype u32, 26 true_displacement u32, 27 parent_distance u8,
# 28 displacement_weight u8, 29 reserved u16, 30 applied_mask u32
AUDIT_ROW = struct.Struct("<IHBBBBBBIIIBBBBQBBBBQQQIIIIBBHI")
# 0 block u32, 1 update u16, 2 retry_steps u8, 3 reserved u8, 4 retry_total u32, 5 perm[32] bytes,
# 6..36 accepted x for steps i = 31..1 (u64), 37..67 accepted retry for steps i = 31..1 (u32)
AUDIT_PERM = struct.Struct("<IHBBI32s31Q31I")

assert UPDATE_REC.size == 48 and PATH_REC.size == 176 and BLOCK_REC.size == 96
assert AUDIT_ROW.size == 88 and AUDIT_PERM.size == 416
TOTAL_RECORD_BYTES = (TOTAL_PATH_UPDATES * UPDATE_REC.size + TOTAL_PATHS * PATH_REC.size
                      + N_BLOCKS * BLOCK_REC.size + AUDIT_ROWS * AUDIT_ROW.size
                      + AUDIT_PERM_RECORDS * AUDIT_PERM.size)
assert TOTAL_RECORD_BYTES == 4229120000

AUDIT_FILE = "audit_rows_blocks_0000_0063.bin"
AUDIT_PERM_FILE = "audit_permutations_blocks_0000_0063.bin"

EXPECTED_SCALARS = {
    "study_id": STUDY_ID, "specification_revision": 1,
    "input_hashes.specification_sha256": SPEC_SHA256,
    "input_hashes.terminal_review_sha256": REVIEW_SHA256,
    "input_hashes.design_go_sha256": DESIGN_GO_SHA256,
    "design_go_record.record": DESIGN_GO_RECORD, "design_go_record.status": "DESIGN_GO",
    "design_go_record.scientific_execution_authorized": False,
    "design_go_record.production_authorized": False,
    "predecessor.accepted_study": PREDECESSOR_STUDY_ID,
    "population.size": 32, "population.genotype_bits": 32,
    "population.lineage_identifiers_read_by_operators": False,
    "updates.first": 1, "updates.last": 256, "updates.late_window_first": 193,
    "updates.late_window_last": 256, "updates.late_window_length": 64,
    "target_law.name": "HALF", "target_law.only_law": True,
    "target_law.innovation_and_copy_bit_generated_every_update": True,
    "candidates.per_parent": 4, "candidates.per_update": 128,
    "candidates.operator_reads_recurrence_event": True, "candidates.allele_reads_recurrence_event": False,
    "candidates.donor_families": 3,
    "candidates.local_bit_flip_probability_numerator": 1,
    "candidates.local_bit_flip_probability_denominator": 32,
    "candidates.genotype_deduplication": False,
    "decoy_permutation.generated_unconditionally": True, "decoy_permutation.fisher_yates_steps": 31,
    "selection.survivors": 32, "selection.weight_exponent_base": 32,
    "selection.max_weight_sum_log2": 39, "selection.floating_point_used": False,
    "policy_mutation.mu_numerator": 1, "policy_mutation.mu_denominator": 32,
    "policy_mutation.symmetric": True, "policy_mutation.flip_rule": "(word0 & 31U) == 0U",
    "design.blocks": 41600, "design.paths_per_block": 6, "design.total_paths": 249600,
    "design.updates_per_path": 256, "design.total_path_updates": 63897600,
    "design.queries_per_update": 128, "design.total_objective_queries": 8178892800,
    "rng.generator": "Philox4x32-10", "rng.rounds": 10,
    "rng.key_text_template": "PHASE2-PERFORMANCE-CONVERSION-002|production-r1|<purpose>",
    "rng.key_namespace": PRODUCTION_NAMESPACE,
    "rng.fixture_key_namespace_non_scientific": FIXTURE_NAMESPACE,
    "rng.max_survival_retry_subindex": 4294967295, "rng.max_decoy_retry_subindex": 4294967295,
    "rng.collision_enumeration_retry_depth": 4,
    "inference.n_blocks": 41600, "inference.delta_numerator": 1, "inference.delta_denominator": 32,
    "inference.primary_family.familywise_alpha": "0.05", "inference.primary_family.family_size": 3,
    "inference.primary_family.alpha_each": "0.05/3",
    "inference.allele_family.familywise_alpha": "0.05", "inference.allele_family.family_size": 2,
    "inference.allele_family.alpha_each": "0.025",
    "inference.allele_family.may_alter_primary_decision": False,
    "inference.performance_family.familywise_alpha": "0.05", "inference.performance_family.family_size": 2,
    "inference.performance_family.alpha_each": "0.025",
    "inference.performance_family.may_alter_primary_decision": False,
    "saved_records.primary": 3, "saved_records.allele_secondary": 2,
    "saved_records.performance_secondary": 2, "saved_records.absolute_cell_means": 12,
    "saved_records.total": 19,
    "audit.first_block": 0, "audit.last_block": 63, "audit.audit_paths": 384,
    "audit.audit_rows": 12582912, "audit.audit_permutation_records": 16384,
    "resources.allocations": 1, "resources.max_cpus": 32, "resources.max_mem_gib": 64,
    "resources.max_wall_hours": 6, "resources.max_new_output_gib": 100,
    "output.byte_order": "little-endian", "output.shards": 32, "output.blocks_per_shard": 1300,
    "output.update_record_bytes": 48, "output.path_record_bytes": 176,
    "output.block_record_bytes": 96, "output.audit_row_bytes": 88,
    "output.audit_permutation_record_bytes": 416, "output.total_record_bytes": 4229120000,
}
EXPECTED_ARRAYS = {
    "candidates.families_in_order": ["PARENT", "POLICY_PROBE", "GLOBAL_SCOUT", "LOCAL_CHILD"],
    "design.arms": list(ARMS),
    "design.starts": ["ALL_F", "ALL_M"],
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


def is_sha256_hex(s):
    return isinstance(s, str) and re.fullmatch(r"[0-9a-f]{64}", s) is not None


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def purpose_key(namespace, purpose, study=STUDY_ID):
    text = "%s|%s|%s" % (study, namespace, purpose)
    d = hashlib.sha256(text.encode("utf-8")).digest()
    return text, int.from_bytes(d[0:4], "little"), int.from_bytes(d[4:8], "little")


def shard_dir(prod_dir, shard):
    return os.path.join(prod_dir, "shards", "shard_%02d" % shard)


def popcount(x):
    return bin(x).count("1")


def permute_bits(mask, perm):
    """Source bit s of mask moves to destination perm[s]."""
    out = 0
    for s in range(32):
        if (mask >> s) & 1:
            out |= 1 << perm[s]
    return out


def fnv1a32(data):
    h = 0x811C9DC5
    for b in data:
        h ^= b
        h = (h * 0x01000193) & 0xFFFFFFFF
    return h


def lemire_threshold(n):
    return ((1 << 64) - n) % n


def replay_fisher_yates(xs_desc, n=32):
    """Replay Fisher-Yates from accepted x values ordered for steps i = n-1, ..., 1.

    Returns (perm, ok) where ok is False if any stored x would have been rejected.
    """
    perm = list(range(n))
    ok = True
    for k, i in enumerate(range(n - 1, 0, -1)):
        bound = i + 1
        prod = xs_desc[k] * bound
        if (prod & ((1 << 64) - 1)) < lemire_threshold(bound):
            ok = False
        j = prod >> 64
        if j > i:
            ok = False
            j = i
        perm[i], perm[j] = perm[j], perm[i]
    return perm, ok
