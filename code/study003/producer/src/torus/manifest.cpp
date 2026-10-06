#include "manifest.hpp"

#include <sys/stat.h>

#include <algorithm>
#include <sstream>

#include "util.hpp"

namespace torus {

namespace {

void walk(const std::string& root, const std::string& rel, std::vector<ManifestEntry>& out) {
  const std::string abs = join_path(root, rel);
  for (const std::string& name : list_directory_sorted(abs)) {
    const std::string r = rel.empty() ? name : join_path(rel, name);
    const std::string a = join_path(root, r);
    struct stat st;
    TORUS_REQUIRE(::lstat(a.c_str(), &st) == 0, "stat failed: " + a);
    TORUS_REQUIRE(!S_ISLNK(st.st_mode), "symlink not allowed in manifest: " + a);
    if (S_ISDIR(st.st_mode)) {
      walk(root, r, out);
    } else {
      TORUS_REQUIRE(S_ISREG(st.st_mode), "non-regular file in manifest: " + a);
      ManifestEntry e;
      e.rel = r;
      e.sha256 = digest_hex(sha256_file(a, &e.bytes));
      out.push_back(e);
    }
  }
}

}  // namespace

std::vector<ManifestEntry> build_manifest(const std::string& root, const std::vector<std::string>& subdirs) {
  std::vector<ManifestEntry> out;
  for (const std::string& s : subdirs) walk(root, s, out);
  std::sort(out.begin(), out.end(), [](const ManifestEntry& a, const ManifestEntry& b) { return a.rel < b.rel; });
  return out;
}

std::string manifest_json_array(const std::vector<ManifestEntry>& m, const std::string& indent) {
  std::ostringstream js;
  js << "[";
  for (std::size_t i = 0; i < m.size(); ++i) {
    js << (i ? "," : "") << "\n" << indent << "  {\"path\": \"" << json_escape(m[i].rel) << "\", \"bytes\": " << m[i].bytes
       << ", \"sha256\": \"" << m[i].sha256 << "\"}";
  }
  js << "\n" << indent << "]";
  return js.str();
}

}  // namespace torus
