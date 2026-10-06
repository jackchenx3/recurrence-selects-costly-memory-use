// Fail-closed helpers: fatal abort, little-endian serialization, file
// identities and the explicit, refuse-if-nonempty output guard.
#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "constants.hpp"
#include "sha256.hpp"

namespace torus {

[[noreturn]] void fatal(const std::string& msg);
#define TORUS_REQUIRE(cond, msg)                       \
  do {                                                 \
    if (!(cond)) ::torus::fatal(std::string(msg));     \
  } while (0)

// Append-only little-endian byte buffer. All binary records are produced
// through this type so that host endianness never affects saved bytes.
class ByteBuf {
 public:
  void u8v(u8 v) { b_.push_back(v); }
  void u16v(u16 v) { for (int i = 0; i < 2; ++i) b_.push_back(u8(v >> (8 * i))); }
  void u32v(u32 v) { for (int i = 0; i < 4; ++i) b_.push_back(u8(v >> (8 * i))); }
  void u64v(u64 v) { for (int i = 0; i < 8; ++i) b_.push_back(u8(v >> (8 * i))); }
  void i32v(i32 v) { u32v(u32(v)); }
  void i64v(i64 v) { u64v(u64(v)); }
  void bytes(const void* p, std::size_t n) {
    const u8* q = static_cast<const u8*>(p);
    b_.insert(b_.end(), q, q + n);
  }
  void zeros(std::size_t n) { b_.insert(b_.end(), n, 0); }
  void fixed_ascii(const std::string& s, std::size_t width);
  std::size_t size() const { return b_.size(); }
  const u8* data() const { return b_.data(); }
  void clear() { b_.clear(); }

 private:
  std::vector<u8> b_;
};

std::string join_path(const std::string& a, const std::string& b);
bool path_exists(const std::string& p);
bool is_directory(const std::string& p);

// Explicit output directory: create it if absent (parent must exist), or
// accept it only if it is an existing *empty* directory. Anything else is fatal.
void prepare_output_dir(const std::string& path);
void make_subdir(const std::string& path);

// Exclusive creation of a new file; fatal if it already exists.
std::FILE* create_new_file(const std::string& path);
void write_new_text_file(const std::string& path, const std::string& text);
void write_all(std::FILE* f, const void* p, std::size_t n, const std::string& what);
void close_checked(std::FILE* f, const std::string& what);

std::string read_text_file(const std::string& path);
Digest sha256_file(const std::string& path, u64* bytes_out = nullptr);
std::string hex64(u64 v);
std::string json_escape(const std::string& s);
std::vector<std::string> list_directory_sorted(const std::string& dir);

// Strict decimal parsing for CLI arguments; fatal on any junk.
u64 parse_u64_strict(const std::string& s, const std::string& what);

}  // namespace torus
