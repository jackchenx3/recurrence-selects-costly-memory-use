// Mechanical-failure handling. Any failure terminates the process at once;
// a terminated production run cannot produce a complete manifest and is
// therefore classified INVALID by the analyzer.
#pragma once

#include <cstdio>
#include <cstdlib>

namespace mm {

[[noreturn]] inline void fatal(const char* what) {
  std::fprintf(stderr, "MMEM_FATAL: %s\n", what);
  std::fflush(stderr);
  std::_Exit(70);
}

inline void require(bool condition, const char* what) {
  if (!condition) fatal(what);
}

}  // namespace mm
