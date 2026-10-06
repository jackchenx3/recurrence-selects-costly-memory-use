#include "util.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace torus {

void fatal(const std::string& msg) {
  std::fprintf(stderr, "TORUS_FATAL: %s\n", msg.c_str());
  std::fflush(stderr);
  std::exit(2);
}

void ByteBuf::fixed_ascii(const std::string& s, std::size_t width) {
  TORUS_REQUIRE(s.size() <= width, "fixed ASCII field overflow: " + s);
  for (char c : s) TORUS_REQUIRE(static_cast<unsigned char>(c) >= 0x20 && static_cast<unsigned char>(c) < 0x7f,
                                 "non-printable ASCII in fixed field");
  bytes(s.data(), s.size());
  zeros(width - s.size());
}

std::string join_path(const std::string& a, const std::string& b) {
  if (a.empty()) return b;
  if (a.back() == '/') return a + b;
  return a + "/" + b;
}

bool path_exists(const std::string& p) {
  struct stat st;
  return ::lstat(p.c_str(), &st) == 0;
}

bool is_directory(const std::string& p) {
  struct stat st;
  return ::lstat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::vector<std::string> list_directory_sorted(const std::string& dir) {
  std::vector<std::string> out;
  DIR* d = ::opendir(dir.c_str());
  TORUS_REQUIRE(d != nullptr, "cannot open directory: " + dir);
  while (struct dirent* e = ::readdir(d)) {
    std::string n = e->d_name;
    if (n == "." || n == "..") continue;
    out.push_back(n);
  }
  ::closedir(d);
  std::sort(out.begin(), out.end());
  return out;
}

void prepare_output_dir(const std::string& path) {
  TORUS_REQUIRE(!path.empty(), "an explicit output path is required");
  if (path_exists(path)) {
    TORUS_REQUIRE(is_directory(path), "output path exists and is not a directory: " + path);
    TORUS_REQUIRE(list_directory_sorted(path).empty(), "refusing existing nonempty output directory: " + path);
    return;
  }
  TORUS_REQUIRE(::mkdir(path.c_str(), 0755) == 0, "cannot create output directory (parent must exist): " + path);
}

void make_subdir(const std::string& path) {
  TORUS_REQUIRE(!path_exists(path), "refusing existing output subdirectory: " + path);
  TORUS_REQUIRE(::mkdir(path.c_str(), 0755) == 0, "cannot create subdirectory: " + path);
}

std::FILE* create_new_file(const std::string& path) {
  int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
  TORUS_REQUIRE(fd >= 0, "refusing to overwrite or cannot create: " + path);
  std::FILE* f = ::fdopen(fd, "wb");
  TORUS_REQUIRE(f != nullptr, "fdopen failed: " + path);
  return f;
}

void write_all(std::FILE* f, const void* p, std::size_t n, const std::string& what) {
  if (n == 0) return;
  TORUS_REQUIRE(std::fwrite(p, 1, n, f) == n, "short write: " + what);
}

void close_checked(std::FILE* f, const std::string& what) {
  TORUS_REQUIRE(std::fflush(f) == 0, "flush failed: " + what);
  TORUS_REQUIRE(::fsync(::fileno(f)) == 0, "fsync failed: " + what);
  TORUS_REQUIRE(std::fclose(f) == 0, "close failed: " + what);
}

void write_new_text_file(const std::string& path, const std::string& text) {
  std::FILE* f = create_new_file(path);
  write_all(f, text.data(), text.size(), path);
  close_checked(f, path);
}

std::string read_text_file(const std::string& path) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  TORUS_REQUIRE(f != nullptr, "cannot read: " + path);
  std::string out;
  char buf[65536];
  std::size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
  TORUS_REQUIRE(!std::ferror(f), "read error: " + path);
  std::fclose(f);
  return out;
}

Digest sha256_file(const std::string& path, u64* bytes_out) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  TORUS_REQUIRE(f != nullptr, "cannot read for hashing: " + path);
  Sha256 s;
  std::vector<char> buf(1 << 20);
  u64 total = 0;
  std::size_t n;
  while ((n = std::fread(buf.data(), 1, buf.size(), f)) > 0) {
    s.update(buf.data(), n);
    total += n;
  }
  TORUS_REQUIRE(!std::ferror(f), "read error while hashing: " + path);
  std::fclose(f);
  if (bytes_out) *bytes_out = total;
  return s.finish();
}

std::string hex64(u64 v) {
  char b[17];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(v));
  return b;
}

std::string json_escape(const std::string& s) {
  std::string out;
  for (char c : s) {
    unsigned char u = static_cast<unsigned char>(c);
    if (c == '"' || c == '\\') {
      out.push_back('\\');
      out.push_back(c);
    } else if (u < 0x20) {
      char b[8];
      std::snprintf(b, sizeof b, "\\u%04x", u);
      out += b;
    } else {
      out.push_back(c);
    }
  }
  return out;
}

u64 parse_u64_strict(const std::string& s, const std::string& what) {
  TORUS_REQUIRE(!s.empty() && s.size() <= 19, "invalid integer for " + what);
  u64 v = 0;
  for (char c : s) {
    TORUS_REQUIRE(c >= '0' && c <= '9', "invalid integer for " + what);
    v = v * 10 + u64(c - '0');
  }
  return v;
}

}  // namespace torus
