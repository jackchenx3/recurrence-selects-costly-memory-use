// Literal known-answer vectors.
//
// Philox4x32-10: official Random123 known-answer vectors, rows beginning
// "philox4x32 10" in the Random123 distribution file tests/kat_vectors
// (D. E. Shaw Research, Random123 1.x, e.g. 1.14.0; format: counter[4] key[2]
// expected[4], hexadecimal 32-bit words). They were transcribed into this
// file without network access; a reviewer must compare these three rows
// byte-for-byte with the upstream file before the fixture result is relied
// on (listed as pending in IMPLEMENTATION_NOT_RUN.json).
//
// SHA-256: FIPS 180-2 / NIST CSRC example vectors.
#pragma once

#include <cstddef>

#include "mm/philox.hpp"

namespace mm {

struct PhiloxKat {
  PhiloxCounter counter;
  PhiloxKey key;
  PhiloxOutput expected;
  const char* label;
};

inline const PhiloxKat* philox_kats(std::size_t& n) {
  static const PhiloxKat kKats[3] = {
      {{{0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U}},
       {0x00000000U, 0x00000000U},
       {{0x6627e8d5U, 0xe169c58dU, 0xbc57ac4cU, 0x9b00dbd8U}},
       "random123 kat_vectors philox4x32 10 zero"},
      {{{0xffffffffU, 0xffffffffU, 0xffffffffU, 0xffffffffU}},
       {0xffffffffU, 0xffffffffU},
       {{0x408f276dU, 0x41c83b0eU, 0xa20bc7c6U, 0x6d5451fdU}},
       "random123 kat_vectors philox4x32 10 all-ones"},
      {{{0x243f6a88U, 0x85a308d3U, 0x13198a2eU, 0x03707344U}},
       {0xa4093822U, 0x299f31d0U},
       {{0xd16cfe09U, 0x94fdccebU, 0x5001e420U, 0x24126ea1U}},
       "random123 kat_vectors philox4x32 10 pi-digits"},
  };
  n = 3U;
  return kKats;
}

struct Sha256Kat {
  const char* message;  // repeated `repeat` times
  std::size_t repeat;
  const char* expected_hex;
};

inline const Sha256Kat* sha256_kats(std::size_t& n) {
  static const Sha256Kat kKats[4] = {
      {"", 1U, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
      {"abc", 1U, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
      {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 1U,
       "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
      {"a", 1000000U, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"},
  };
  n = 4U;
  return kKats;
}

}  // namespace mm
