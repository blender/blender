// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2018 Google Inc. All rights reserved.
// http://ceres-solver.org/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: keir@google.com (Keir Mierle)
//
// A hasher for std::pair<T, T>.

#ifndef CERES_INTERNAL_PAIR_HASH_H_
#define CERES_INTERNAL_PAIR_HASH_H_

#include "ceres/internal/port.h"
#include <cstdint>
#include <utility>

namespace ceres {
namespace internal {

#if defined(_WIN32) && !defined(__MINGW64__) && !defined(__MINGW32__)
#define GG_LONGLONG(x) x##I64
#define GG_ULONGLONG(x) x##UI64
#else
#define GG_LONGLONG(x) x##LL
#define GG_ULONGLONG(x) x##ULL
#endif

// The hash function is due to Bob Jenkins (see
// http://burtleburtle.net/bob/hash/index.html). Each mix takes 36 instructions,
// in 18 cycles if you're lucky. On x86 architectures, this requires 45
// instructions in 27 cycles, if you're lucky.
//
// 32bit version
inline void hash_mix(uint32_t& a, uint32_t& b, uint32_t& c) {
  a -= b; a -= c; a ^= (c>>13);
  b -= c; b -= a; b ^= (a<<8);
  c -= a; c -= b; c ^= (b>>13);
  a -= b; a -= c; a ^= (c>>12);
  b -= c; b -= a; b ^= (a<<16);
  c -= a; c -= b; c ^= (b>>5);
  a -= b; a -= c; a ^= (c>>3);
  b -= c; b -= a; b ^= (a<<10);
  c -= a; c -= b; c ^= (b>>15);
}

// 64bit version
inline void hash_mix(uint64_t& a, uint64_t& b, uint64_t& c) {
  a -= b; a -= c; a ^= (c>>43);
  b -= c; b -= a; b ^= (a<<9);
  c -= a; c -= b; c ^= (b>>8);
  a -= b; a -= c; a ^= (c>>38);
  b -= c; b -= a; b ^= (a<<23);
  c -= a; c -= b; c ^= (b>>5);
  a -= b; a -= c; a ^= (c>>35);
  b -= c; b -= a; b ^= (a<<49);
  c -= a; c -= b; c ^= (b>>11);
}

inline uint32_t Hash32NumWithSeed(uint32_t num, uint32_t c) {
  // The golden ratio; an arbitrary value.
  uint32_t b = 0x9e3779b9UL;
  hash_mix(num, b, c);
  return c;
}

inline uint64_t Hash64NumWithSeed(uint64_t num, uint64_t c) {
  // More of the golden ratio.
  uint64_t b = GG_ULONGLONG(0xe08c1d668b756f82);
  hash_mix(num, b, c);
  return c;
}

// Hasher for STL pairs. Requires hashers for both members to be defined.
struct pair_hash {
 public:
  template <typename T>
  std::size_t operator()(const std::pair<T, T>& p) const {
    const std::size_t h1 = std::hash<T>()(p.first);
    const std::size_t h2 = std::hash<T>()(p.second);
    // The decision below is at compile time
    return (sizeof(h1) <= sizeof(uint32_t)) ? Hash32NumWithSeed(h1, h2)
                                            : Hash64NumWithSeed(h1, h2);
  }
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_PAIR_HASH_H_
