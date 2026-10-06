// Saved-record layout, fail-closed readers/writers, input identity and
// manifest handling for the PHASE2-TORUS-MEMORY-003 auditor.
//
// Byte layout follows BINARY_SCHEMAS.md version 1 (little-endian, 64-byte
// common header).  Implemented independently of the producer's records code.
#ifndef T3A_RECORDS_HPP
#define T3A_RECORDS_HPP

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "sha256.hpp"

namespace t3a {

namespace fs = std::filesystem;

// A bounded, value-redacted failure: a code plus a location, never a value.
class AuditFailure : public std::runtime_error {
public:
    AuditFailure(const std::string& code, const std::string& where)
        : std::runtime_error(code), code_(code), where_(bounded(where)) {}
    const std::string& code() const { return code_; }
    const std::string& where() const { return where_; }
    static std::string bounded(const std::string& s) {
        return s.size() <= 200 ? s : s.substr(0, 200) + "...";
    }

private:
    std::string code_;
    std::string where_;
};

inline void put_u16(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
}
inline void put_u32(std::uint8_t* p, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8u * i));
}
inline void put_u64(std::uint8_t* p, std::uint64_t v) {
    for (unsigned i = 0; i < 8; ++i) p[i] = static_cast<std::uint8_t>(v >> (8u * i));
}
inline void put_i32(std::uint8_t* p, std::int32_t v) { put_u32(p, static_cast<std::uint32_t>(v)); }
inline void put_i64(std::uint8_t* p, std::int64_t v) { put_u64(p, static_cast<std::uint64_t>(v)); }

enum class RecType : unsigned { Update = 0, Path, Block, Estimate, Candidate, Entry, Context };

struct TypeInfo {
    char magic[8];
    std::uint32_t row_size;
    const char* kind;
};

inline const TypeInfo& type_info(RecType t) {
    static const TypeInfo table[7] = {
        {{'T', '3', 'U', 'P', 'D', 'A', 'T', 'E'}, 24, "UPDATE"},
        {{'T', '3', 'P', 'A', 'T', 'H', 0, 0}, 224, "PATH"},
        {{'T', '3', 'B', 'L', 'O', 'C', 'K', 0}, 136, "BLOCK"},
        {{'T', '3', 'E', 'S', 'T', 'I', 'M', 0}, 256, "ESTIMATE"},
        {{'T', '3', 'C', 'A', 'N', 'D', 'I', 'D'}, 104, "CANDIDATE"},
        {{'T', '3', 'E', 'N', 'T', 'R', 'Y', 0}, 16, "ENTRY"},
        {{'T', '3', 'C', 'O', 'N', 'T', 'X', 'T'}, 4240, "CONTEXT"},
    };
    return table[static_cast<unsigned>(t)];
}

constexpr std::uint32_t kHeaderSize = 64;
constexpr std::uint32_t kSchemaVersion = 1;
constexpr std::uint32_t kEndianMarker = 0x01020304u;
constexpr std::uint32_t kNoChunk = 0xFFFFFFFFu;

struct FieldSpan {
    std::uint32_t off;
    std::uint32_t size;
    const char* name;
};

inline const std::vector<FieldSpan>& header_fields() {
    static const std::vector<FieldSpan> f = {
        {0, 8, "magic"}, {8, 4, "schema_version"}, {12, 4, "endian_marker"}, {16, 4, "row_size"},
        {20, 4, "header_size"}, {24, 8, "row_count"}, {32, 16, "namespace"}, {48, 4, "chunk_index"},
        {52, 4, "first_block"}, {56, 4, "block_count"}, {60, 4, "reserved"},
    };
    return f;
}

inline const std::vector<FieldSpan>& row_fields(RecType t) {
    static const std::vector<FieldSpan> update = {
        {0, 8, "pop_loss"}, {8, 2, "update"}, {10, 2, "query_count"}, {12, 2, "local_flagged"},
        {14, 2, "local_changed"}, {16, 1, "m_count"}, {17, 1, "valid_cache_pre"},
        {18, 1, "cache_probe_uses"}, {19, 1, "cache_probe_winner_slots"}, {20, 1, "f_to_m"},
        {21, 1, "m_to_f"}, {22, 1, "dup_entry_tournaments"}, {23, 1, "distinct_winners"},
    };
    static const std::vector<FieldSpan> path = {
        {0, 4, "block"}, {4, 1, "cell"}, {5, 1, "arm"}, {6, 1, "law"}, {7, 1, "start"},
        {8, 4, "late_m_sum"}, {12, 4, "total_queries"}, {16, 8, "late_pop_loss_sum"},
        {24, 8, "all_pop_loss_sum"}, {32, 4, "all_m_sum"}, {36, 4, "cache_probe_uses"},
        {40, 4, "cache_probe_winner_slots"}, {44, 4, "f_to_m"}, {48, 4, "m_to_f"},
        {52, 4, "dup_entry_tournaments"}, {56, 4, "local_flagged"}, {60, 4, "local_changed"},
        {64, 32, "final_state_sha256"}, {96, 32, "update_records_sha256"},
        {128, 32, "label_blind_sha256"}, {160, 32, "label_sha256"},
        {192, 32, "complement_label_sha256"},
    };
    static const std::vector<FieldSpan> block = {
        {0, 4, "block"}, {4, 4, "flags"}, {8, 32, "late_m_sum"}, {40, 64, "late_pop_loss_sum"},
        {104, 4, "c_abs_numerator"}, {108, 4, "c_rec_numerator"}, {112, 4, "d_half_numerator"},
        {116, 4, "d_zero_numerator"}, {120, 8, "p_abs_numerator"}, {128, 8, "p_rec_numerator"},
    };
    static const std::vector<FieldSpan> estimate = {
        {0, 2, "index"}, {2, 1, "family"}, {3, 1, "classification"}, {4, 4, "n_blocks"},
        {8, 16, "numerator_sum"}, {24, 16, "denominator"}, {40, 8, "range_length"},
        {48, 32, "name"}, {80, 44, "estimate"}, {124, 44, "half_width"}, {168, 44, "lower"},
        {212, 44, "upper"},
    };
    static const std::vector<FieldSpan> candidate = {
        {0, 4, "block"}, {4, 2, "update"}, {6, 1, "cell"}, {7, 1, "candidate_index"},
        {8, 1, "family"}, {9, 1, "parent_slot"}, {10, 1, "parent_label"}, {11, 1, "flags"},
        {12, 1, "donor_family"}, {13, 1, "local_flagged"}, {14, 1, "local_changed"},
        {15, 1, "slots_won"}, {16, 8, "loss"}, {24, 8, "donor_key"}, {32, 8, "tie_key"},
        {40, 64, "phenotype"},
    };
    static const std::vector<FieldSpan> entry = {
        {0, 4, "block"}, {4, 2, "update"}, {6, 1, "cell"}, {7, 1, "survivor_slot"},
        {8, 1, "entry"}, {9, 1, "entered_candidate"}, {10, 1, "winner"},
        {11, 1, "inherited_label"}, {12, 1, "post_label"}, {13, 1, "label_flipped"},
        {14, 1, "post_cache_valid"}, {15, 1, "zero"},
    };
    static const std::vector<FieldSpan> context = {
        {0, 4, "block"}, {4, 2, "update"}, {6, 1, "cell"}, {7, 1, "target_copy_bit"},
        {8, 1, "law_copied"}, {9, 7, "zero"}, {16, 64, "target"}, {80, 2048, "pre_phenotypes"},
        {2128, 32, "pre_labels"}, {2160, 32, "pre_cache_valid"}, {2192, 2048, "pre_caches"},
    };
    switch (t) {
        case RecType::Update: return update;
        case RecType::Path: return path;
        case RecType::Block: return block;
        case RecType::Estimate: return estimate;
        case RecType::Candidate: return candidate;
        case RecType::Entry: return entry;
        case RecType::Context: return context;
    }
    return update;
}

inline const char* field_at(const std::vector<FieldSpan>& fields, std::uint32_t off) {
    for (const FieldSpan& f : fields) {
        if (off >= f.off && off < f.off + f.size) return f.name;
    }
    return "unknown";
}

struct HeaderSpec {
    std::uint64_t row_count;
    std::string ns;
    std::uint32_t chunk_index;
    std::uint32_t first_block;
    std::uint32_t block_count;
};

inline void encode_header(std::uint8_t* out, RecType t, const HeaderSpec& s) {
    std::memset(out, 0, kHeaderSize);
    std::memcpy(out, type_info(t).magic, 8);
    put_u32(out + 8, kSchemaVersion);
    put_u32(out + 12, kEndianMarker);
    put_u32(out + 16, type_info(t).row_size);
    put_u32(out + 20, kHeaderSize);
    put_u64(out + 24, s.row_count);
    if (s.ns.size() > 16) throw std::logic_error("namespace longer than 16 bytes");
    std::memcpy(out + 32, s.ns.data(), s.ns.size());
    put_u32(out + 48, s.chunk_index);
    put_u32(out + 52, s.first_block);
    put_u32(out + 56, s.block_count);
}

// Byte-exact comparison of a replayed row against a saved row.  The first
// differing byte is mapped to a field name; no value is reported.
inline void compare_row(RecType type, const std::string& label, const std::uint8_t* expected,
                        const std::uint8_t* got, std::uint64_t row) {
    const std::uint32_t n = type_info(type).row_size;
    if (std::memcmp(expected, got, n) == 0) return;
    std::uint32_t i = 0;
    while (i < n && expected[i] == got[i]) ++i;
    throw AuditFailure(std::string("MISMATCH_") + type_info(type).kind + "_" + field_at(row_fields(type), i),
                       label + " row " + std::to_string(row));
}

// ---------------------------------------------------------------------------
// Input path safety: relative, no '.'/'..', restricted characters, no symlink
// in any component, intermediate components directories, final regular file.

inline bool rel_chars_ok(const std::string& rel) {
    if (rel.empty() || rel.front() == '/' || rel.back() == '/') return false;
    for (char c : rel) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                        c == '.' || c == '_' || c == '-' || c == '/';
        if (!ok) return false;
    }
    return true;
}

inline std::vector<std::string> split_rel(const std::string& rel) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : rel) {
        if (c == '/') {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    parts.push_back(cur);
    return parts;
}

inline void require_plain_directory(const fs::path& dir, const std::string& label) {
    std::error_code ec;
    const fs::file_status st = fs::symlink_status(dir, ec);
    if (ec || st.type() == fs::file_type::not_found) throw AuditFailure("INPUT_ROOT_MISSING", label);
    if (fs::is_symlink(st)) throw AuditFailure("INPUT_ROOT_SYMLINK", label);
    if (!fs::is_directory(st)) throw AuditFailure("INPUT_ROOT_NOT_DIRECTORY", label);
}

inline fs::path checked_input(const fs::path& root, const std::string& rel) {
    if (!rel_chars_ok(rel)) throw AuditFailure("INPUT_PATH_FORM", "input path");
    const std::vector<std::string> parts = split_rel(rel);
    for (const std::string& p : parts) {
        if (p.empty() || p == "." || p == "..") throw AuditFailure("INPUT_PATH_FORM", "input path");
    }
    require_plain_directory(root, "input root");
    fs::path cur = root;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        cur /= parts[i];
        std::error_code ec;
        const fs::file_status st = fs::symlink_status(cur, ec);
        if (ec || st.type() == fs::file_type::not_found) throw AuditFailure("INPUT_MISSING", rel);
        if (fs::is_symlink(st)) throw AuditFailure("INPUT_SYMLINK", rel);
        if (i + 1 < parts.size()) {
            if (!fs::is_directory(st)) throw AuditFailure("INPUT_NOT_DIRECTORY", rel);
        } else if (!fs::is_regular_file(st)) {
            throw AuditFailure("INPUT_IRREGULAR", rel);
        }
    }
    return cur;
}

inline Digest hash_file(const fs::path& p, const std::string& label) {
    std::ifstream in(p, std::ios::binary);
    if (!in) throw AuditFailure("INPUT_UNREADABLE", label);
    std::vector<char> buf(static_cast<std::size_t>(1) << 22);
    Sha256 h;
    while (in) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        const std::streamsize got = in.gcount();
        if (got > 0) h.update(buf.data(), static_cast<std::size_t>(got));
    }
    if (in.bad()) throw AuditFailure("INPUT_READ_ERROR", label);
    return h.finish();
}

struct InputIdentity {
    std::string rel;
    std::string sha256;
    std::uintmax_t bytes = 0;
    fs::file_time_type mtime{};
};

inline InputIdentity identify_input(const fs::path& root, const std::string& rel) {
    const fs::path p = checked_input(root, rel);
    InputIdentity id;
    id.rel = rel;
    std::error_code ec;
    id.bytes = fs::file_size(p, ec);
    if (ec) throw AuditFailure("INPUT_UNREADABLE", rel);
    id.mtime = fs::last_write_time(p, ec);
    if (ec) throw AuditFailure("INPUT_UNREADABLE", rel);
    id.sha256 = to_hex(hash_file(p, rel));
    const std::uintmax_t bytes_after = fs::file_size(p, ec);
    if (ec) throw AuditFailure("INPUT_UNREADABLE", rel);
    const fs::file_time_type mtime_after = fs::last_write_time(p, ec);
    if (ec) throw AuditFailure("INPUT_UNREADABLE", rel);
    if (bytes_after != id.bytes || mtime_after != id.mtime) throw AuditFailure("INPUT_CHANGED_DURING_HASH", rel);
    return id;
}

// ---------------------------------------------------------------------------
// Manifest: sha256sum text form, one "<64 lowercase hex><space><space|*><path>"
// per line, LF endings, final LF required, no duplicates, no empty manifest.
// A single leading "./" on a path is removed before validation.

inline std::map<std::string, std::string> parse_manifest(const fs::path& path, std::string& manifest_sha) {
    std::error_code ec;
    const fs::file_status st = fs::symlink_status(path, ec);
    if (ec || st.type() == fs::file_type::not_found) throw AuditFailure("MANIFEST_MISSING", "manifest");
    if (fs::is_symlink(st) || !fs::is_regular_file(st)) throw AuditFailure("MANIFEST_IRREGULAR", "manifest");
    std::ifstream in(path, std::ios::binary);
    if (!in) throw AuditFailure("MANIFEST_UNREADABLE", "manifest");
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) throw AuditFailure("MANIFEST_UNREADABLE", "manifest");
    manifest_sha = to_hex(sha256_bytes(text.data(), text.size()));
    if (text.empty() || text.back() != '\n') throw AuditFailure("MANIFEST_FORMAT", "final newline");
    std::map<std::string, std::string> entries;
    std::size_t pos = 0;
    std::size_t line_no = 0;
    while (pos < text.size()) {
        const std::size_t nl = text.find('\n', pos);
        const std::string line = text.substr(pos, nl - pos);
        pos = nl + 1;
        ++line_no;
        const std::string where = "manifest line " + std::to_string(line_no);
        if (line.size() < 67) throw AuditFailure("MANIFEST_FORMAT", where);
        for (std::size_t i = 0; i < 64; ++i) {
            const char c = line[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) throw AuditFailure("MANIFEST_FORMAT", where);
        }
        if (line[64] != ' ' || (line[65] != ' ' && line[65] != '*')) throw AuditFailure("MANIFEST_FORMAT", where);
        std::string rel = line.substr(66);
        if (rel.size() > 2 && rel.compare(0, 2, "./") == 0) rel = rel.substr(2);
        if (!rel_chars_ok(rel)) throw AuditFailure("MANIFEST_PATH_FORM", where);
        for (const std::string& p : split_rel(rel)) {
            if (p.empty() || p == "." || p == "..") throw AuditFailure("MANIFEST_PATH_FORM", where);
        }
        if (entries.count(rel) != 0) throw AuditFailure("MANIFEST_DUPLICATE", where);
        entries[rel] = line.substr(0, 64);
    }
    if (entries.empty()) throw AuditFailure("MANIFEST_EMPTY", "manifest");
    return entries;
}

// ---------------------------------------------------------------------------
// Sequential fail-closed row reader.

class RowStream {
public:
    RowStream(const fs::path& path, const std::string& label, RecType type, const HeaderSpec& spec)
        : label_(label), type_(type), row_size_(type_info(type).row_size), rows_(spec.row_count) {
        std::error_code ec;
        const std::uintmax_t size = fs::file_size(path, ec);
        if (ec) throw AuditFailure("FILE_UNREADABLE", label_);
        if (size != static_cast<std::uintmax_t>(kHeaderSize) + rows_ * row_size_) {
            throw AuditFailure("FILE_SIZE", label_);
        }
        in_.open(path, std::ios::binary);
        if (!in_) throw AuditFailure("FILE_UNREADABLE", label_);
        std::uint8_t got[kHeaderSize];
        std::uint8_t expected[kHeaderSize];
        read_exact(got, kHeaderSize);
        encode_header(expected, type_, spec);
        if (std::memcmp(got, expected, kHeaderSize) != 0) {
            std::uint32_t i = 0;
            while (i < kHeaderSize && got[i] == expected[i]) ++i;
            throw AuditFailure(std::string("HEADER_") + field_at(header_fields(), i), label_);
        }
        batch_ = std::max<std::uint64_t>(1, (static_cast<std::uint64_t>(1) << 22) / row_size_);
        buf_.resize(static_cast<std::size_t>(batch_ * row_size_));
    }

    const std::uint8_t* next() {
        if (consumed_ >= rows_) throw AuditFailure("ROW_COUNT_EXCEEDED", label_);
        if (pos_ == avail_) refill();
        const std::uint8_t* p = buf_.data() + static_cast<std::size_t>(pos_ * row_size_);
        ++pos_;
        ++consumed_;
        return p;
    }

    std::uint64_t consumed() const { return consumed_; }
    const std::string& label() const { return label_; }

    void finish() {
        if (consumed_ != rows_) throw AuditFailure("ROWS_UNCONSUMED", label_);
        char c = 0;
        in_.read(&c, 1);
        if (in_.gcount() != 0) throw AuditFailure("TRAILING_BYTES", label_);
    }

private:
    void read_exact(std::uint8_t* dst, std::uint64_t n) {
        in_.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(n));
        if (static_cast<std::uint64_t>(in_.gcount()) != n) throw AuditFailure("FILE_SHORT", label_);
    }
    void refill() {
        const std::uint64_t remaining = rows_ - consumed_;
        const std::uint64_t want = remaining < batch_ ? remaining : batch_;
        read_exact(buf_.data(), want * row_size_);
        avail_ = want;
        pos_ = 0;
    }

    std::string label_;
    RecType type_;
    std::uint64_t row_size_;
    std::uint64_t rows_;
    std::uint64_t consumed_ = 0;
    std::uint64_t batch_ = 1;
    std::uint64_t pos_ = 0;
    std::uint64_t avail_ = 0;
    std::vector<std::uint8_t> buf_;
    std::ifstream in_;
};

// Create-exclusive row writer, used only by the fixture emitter.
class RowWriter {
public:
    RowWriter(const fs::path& path, RecType type, const HeaderSpec& spec)
        : type_(type), rows_(spec.row_count) {
        f_ = std::fopen(path.c_str(), "wbx");
        if (f_ == nullptr) throw AuditFailure("FIXTURE_OUTPUT_CREATE", path.filename().string());
        std::uint8_t hdr[kHeaderSize];
        encode_header(hdr, type, spec);
        if (std::fwrite(hdr, 1, kHeaderSize, f_) != kHeaderSize) throw AuditFailure("FIXTURE_OUTPUT_WRITE", "header");
    }
    RowWriter(const RowWriter&) = delete;
    RowWriter& operator=(const RowWriter&) = delete;
    ~RowWriter() {
        if (f_ != nullptr) std::fclose(f_);
    }
    void write(const std::uint8_t* row) {
        if (written_ >= rows_) throw AuditFailure("FIXTURE_OUTPUT_COUNT", type_info(type_).kind);
        const std::size_t n = type_info(type_).row_size;
        if (std::fwrite(row, 1, n, f_) != n) throw AuditFailure("FIXTURE_OUTPUT_WRITE", type_info(type_).kind);
        ++written_;
    }
    void close() {
        if (written_ != rows_) throw AuditFailure("FIXTURE_OUTPUT_COUNT", type_info(type_).kind);
        const bool ok = std::fflush(f_) == 0;
        const bool closed = std::fclose(f_) == 0;
        f_ = nullptr;
        if (!ok || !closed) throw AuditFailure("FIXTURE_OUTPUT_WRITE", type_info(type_).kind);
    }

private:
    RecType type_;
    std::uint64_t rows_;
    std::uint64_t written_ = 0;
    std::FILE* f_ = nullptr;
};

// Create-exclusive text output (receipts).  Returns false if the file exists
// or cannot be fully written.
inline bool write_exclusive_text(const fs::path& path, const std::string& text) {
    std::FILE* f = std::fopen(path.c_str(), "wx");
    if (f == nullptr) return false;
    const bool wrote = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    const bool flushed = std::fflush(f) == 0;
    const bool closed = std::fclose(f) == 0;
    return wrote && flushed && closed;
}

inline std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\t': o += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    o += "?";
                } else {
                    o.push_back(c);
                }
        }
    }
    return o;
}

}  // namespace t3a

#endif  // T3A_RECORDS_HPP
