// Strict "--key value" argument parsing: unknown, repeated or missing
// arguments are fatal. There are no defaults for any path or scientific value.
#pragma once

#include <map>
#include <set>
#include <string>

#include "util.hpp"

namespace torus {

class Args {
 public:
  Args(int argc, char** argv, const std::set<std::string>& allowed) {
    for (int i = 1; i < argc; ++i) {
      std::string k = argv[i];
      TORUS_REQUIRE(k.size() > 2 && k.compare(0, 2, "--") == 0, "unexpected argument: " + k);
      k = k.substr(2);
      TORUS_REQUIRE(allowed.count(k) == 1, "unknown argument: --" + k);
      TORUS_REQUIRE(i + 1 < argc, "missing value for --" + k);
      TORUS_REQUIRE(values_.count(k) == 0, "repeated argument: --" + k);
      values_[k] = argv[++i];
    }
  }
  const std::string& req(const std::string& k) const {
    auto it = values_.find(k);
    TORUS_REQUIRE(it != values_.end() && !it->second.empty(), "required argument missing: --" + k);
    return it->second;
  }
  bool has(const std::string& k) const { return values_.count(k) == 1; }
  void forbid(const std::string& k, const std::string& why) const {
    TORUS_REQUIRE(!has(k), "--" + k + " is not permitted " + why);
  }

 private:
  std::map<std::string, std::string> values_;
};

}  // namespace torus
