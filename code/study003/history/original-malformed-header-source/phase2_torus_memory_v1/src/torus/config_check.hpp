// Fail-closed configuration and identity checks. Any mismatch is fatal.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "constants.hpp"

namespace torus {

// SHA-256 self-test and the official Threefry4x64-20 KATs. Fatal on mismatch.
void run_startup_self_tests();

// Parses the restricted flat JSON config (string / integer / boolean values
// only) and requires the exact frozen key order and value tokens compiled
// into this binary. Returns the config file SHA-256 hex. Fatal on mismatch.
std::string verify_frozen_config(const std::string& path);
std::vector<std::pair<std::string, std::string>> parse_flat_json(const std::string& text);

// Pinned Random123 header and KAT file under <package_root>. Fatal on mismatch.
void verify_random123(const std::string& package_root);

// Frozen design text and design-GO record identities. Fatal on mismatch.
void verify_design_spec(const std::string& path);
void verify_design_go_record(const std::string& path);

// Separate future production authority record (see docs/PRODUCTION_GATES.md).
// Must state scientific_execution_authorized = true for this exact design,
// design-GO record, config SHA-256 and the production namespace. Returns its
// SHA-256 hex. Fatal otherwise.
std::string verify_execution_authorization(const std::string& path, const std::string& config_sha256);

// Hex-string helper: 64 lowercase hex characters.
bool is_sha256_hex(const std::string& s);

}  // namespace torus
