"""PHASE2-TORUS-MEMORY-003 binary schema v1 (Python standard library only).

Mirrors src/torus/records.hpp and docs/BINARY_SCHEMAS.md. All integers are
little-endian. Every file begins with a 64-byte header.
"""

import hashlib
import struct

SCHEMA_VERSION = 1
ENDIAN_MARKER = 0x01020304
HEADER_BYTES = 64
NO_CHUNK = 0xFFFFFFFF

HEADER = struct.Struct("<8sIIIIQ16sIIII")
UPDATE = struct.Struct("<QHHHHBBBBBBBB")
PATH = struct.Struct("<IBBBBIIQQIIIIIIII32s32s32s32s32s")
BLOCK = struct.Struct("<II8I8Qiiiiqq")
CANDIDATE = struct.Struct("<IHBBBBBBBBBBQQQ32H")
ENTRY = struct.Struct("<IHBBBBBBBBBB")
CONTEXT_FIXED = struct.Struct("<IHBBB7x32H")  # followed by parents, labels, cache flags, caches
CONTEXT_BYTES = 4240
ESTIMATE_BYTES = 256

assert HEADER.size == 64
assert UPDATE.size == 24
assert PATH.size == 224
assert BLOCK.size == 136
assert CANDIDATE.size == 104
assert ENTRY.size == 16
assert CONTEXT_FIXED.size == 80

MAGIC = {
    "update": b"T3UPDATE",
    "path": b"T3PATH\x00\x00",
    "block": b"T3BLOCK\x00",
    "estimate": b"T3ESTIM\x00",
    "candidate": b"T3CANDID",
    "entry": b"T3ENTRY\x00",
    "context": b"T3CONTXT",
}
ROW_BYTES = {
    "update": 24,
    "path": 224,
    "block": 136,
    "estimate": 256,
    "candidate": 104,
    "entry": 16,
    "context": 4240,
}

# Study constants (also bound by config/torus_003_frozen_config.json).
BLOCKS = 41600
CELLS = 8
UPDATES = 256
LATE_FIRST = 193
POP = 32
CANDIDATES = 128
CHUNK_BLOCKS = 64
CHUNKS = 650
PATHS = 332800
PATH_UPDATES = 85196800
OBJECTIVE_QUERIES = 10905190400
AUDIT_CANDIDATE_ROWS = 16777216
AUDIT_ENTRY_ROWS = 16777216
AUDIT_CONTEXT_ROWS = 131072
MAX_POP_LOSS = 1 << 40

FLAG_N1_ZERO = 1 << 0
FLAG_N1_HALF = 1 << 1
FLAG_N2_ZERO = 1 << 2
FLAG_N2_HALF = 1 << 3
FLAG_N2_SUM = 1 << 4
FLAG_QUERIES = 1 << 5
FLAG_ALL_VALID = 0x3F

# Cell index = 4*arm + 2*law + start (arm ACTIVE=0/SHAM=1, law ZERO=0/HALF=1, start ALL_F=0/ALL_M=1).
CELL_NAMES = [
    "ACTIVE_ZERO_ALL_F", "ACTIVE_ZERO_ALL_M", "ACTIVE_HALF_ALL_F", "ACTIVE_HALF_ALL_M",
    "SHAM_ZERO_ALL_F", "SHAM_ZERO_ALL_M", "SHAM_HALF_ALL_F", "SHAM_HALF_ALL_M",
]

PURPOSES = [
    "INITIAL_VECTOR", "TARGET_INNOVATION_VECTOR", "TARGET_COPY", "FRESH_VECTOR", "SCOUT_VECTOR",
    "LOCAL_REPLACE_FLAG", "LOCAL_REPLACE_VALUE", "DONOR_KEY", "TOURNAMENT_ENTRY", "CANDIDATE_TIE_KEY",
    "POLICY_MUTATION",
]


class SchemaError(Exception):
    pass


def fixed_ascii(raw):
    s = raw.rstrip(b"\x00")
    if b"\x00" in s:
        raise SchemaError("embedded NUL in fixed ASCII field")
    return s.decode("ascii")


def parse_header(raw, kind, namespace, expected_rows=None, chunk_index=None, first_block=None, block_count=None):
    if len(raw) < HEADER_BYTES:
        raise SchemaError("truncated header")
    (magic, version, endian, row_size, header_size, row_count, ns, chunk, first, count, reserved) = HEADER.unpack_from(raw, 0)
    if magic != MAGIC[kind]:
        raise SchemaError("bad magic for %s: %r" % (kind, magic))
    if version != SCHEMA_VERSION or endian != ENDIAN_MARKER or header_size != HEADER_BYTES or reserved != 0:
        raise SchemaError("bad version/endianness/header size")
    if row_size != ROW_BYTES[kind]:
        raise SchemaError("bad row size for %s" % kind)
    if fixed_ascii(ns) != namespace:
        raise SchemaError("namespace mismatch: %r" % fixed_ascii(ns))
    if expected_rows is not None and row_count != expected_rows:
        raise SchemaError("row count mismatch for %s" % kind)
    if chunk_index is not None and chunk != chunk_index:
        raise SchemaError("chunk index mismatch")
    if first_block is not None and first != first_block:
        raise SchemaError("first block mismatch")
    if block_count is not None and count != block_count:
        raise SchemaError("block count mismatch")
    if len(raw) != HEADER_BYTES + row_count * row_size:
        raise SchemaError("payload size mismatch for %s" % kind)
    return row_count


def build_header(kind, row_count, namespace, chunk_index, first_block, block_count):
    ns = namespace.encode("ascii")
    if len(ns) > 16:
        raise SchemaError("namespace too long")
    return HEADER.pack(MAGIC[kind], SCHEMA_VERSION, ENDIAN_MARKER, ROW_BYTES[kind], HEADER_BYTES, row_count,
                       ns.ljust(16, b"\x00"), chunk_index, first_block, block_count, 0)


def purpose_key_words(namespace, purpose):
    digest = hashlib.sha256(("PHASE2-TORUS-MEMORY-003|%s|%s" % (namespace, purpose)).encode("utf-8")).digest()
    return [int.from_bytes(digest[8 * j: 8 * j + 8], "little") for j in range(4)]


def sha256_file(path, bufsize=1 << 22):
    h = hashlib.sha256()
    n = 0
    with open(path, "rb") as f:
        while True:
            b = f.read(bufsize)
            if not b:
                break
            h.update(b)
            n += len(b)
    return h.hexdigest(), n
