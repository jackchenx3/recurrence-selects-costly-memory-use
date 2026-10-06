// Deterministic file manifest (sorted relative paths, byte counts, SHA-256).
#pragma once

#include <string>
#include <vector>

#include "constants.hpp"

namespace torus {

struct ManifestEntry {
  std::string rel;
  u64 bytes;
  std::string sha256;
};

// Recursively lists regular files under root/<subdirs...>, sorted by path.
std::vector<ManifestEntry> build_manifest(const std::string& root, const std::vector<std::string>& subdirs);
std::string manifest_json_array(const std::vector<ManifestEntry>& m, const std::string& indent);

}  // namespace torus
