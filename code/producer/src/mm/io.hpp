// Small I/O and JSON-text helpers.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include "mm/fatal.hpp"

namespace mm {

inline std::string json_escape(const std::string& s) {
  std::string out;
  for (char ch : s) {
    const unsigned char c = static_cast<unsigned char>(ch);
    if (c == '"') {
      out += "\\\"";
    } else if (c == '\\') {
      out += "\\\\";
    } else if (c < 0x20U) {
      char buf[8];
      std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(c));
      out += buf;
    } else {
      out += ch;
    }
  }
  return out;
}

inline std::string jstr(const std::string& s) { return "\"" + json_escape(s) + "\""; }

inline std::string hex32(std::uint32_t v) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%08x", v);
  return buf;
}

inline bool read_file(const std::string& path, std::string& out) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) return false;
  out.clear();
  char buf[65536];
  for (;;) {
    const std::size_t n = std::fread(buf, 1U, sizeof buf, f);
    out.append(buf, n);
    if (n < sizeof buf) break;
  }
  const bool bad = std::ferror(f) != 0;
  std::fclose(f);
  return !bad;
}

// Opens a file that must not already exist ("x" mode, C11/C++17).
inline std::FILE* open_new_file(const std::string& path) {
  std::FILE* f = std::fopen(path.c_str(), "wbx");
  if (f == nullptr) fatal(("cannot create new file (exists or unwritable): " + path).c_str());
  return f;
}

inline void write_all(std::FILE* f, const void* data, std::size_t n) {
  if (n != 0U && std::fwrite(data, 1U, n, f) != n) fatal("short write");
}

inline void close_file(std::FILE* f) {
  if (std::fflush(f) != 0 || std::fclose(f) != 0) fatal("close failed");
}

inline void write_new_text_file(const std::string& path, const std::string& text) {
  std::FILE* f = open_new_file(path);
  write_all(f, text.data(), text.size());
  close_file(f);
}

}  // namespace mm
