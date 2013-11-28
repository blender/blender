// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
// http://code.google.com/p/ceres-solver/
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
// Portable HashMap and HashSet, and a specialized overload for hashing pairs.

#ifndef CERES_INTERNAL_COLLECTIONS_PORT_H_
#define CERES_INTERNAL_COLLECTIONS_PORT_H_

#if defined(CERES_NO_UNORDERED_MAP)
#  include <map>
#  include <set>
#endif

#if defined(CERES_TR1_UNORDERED_MAP)
#  include <tr1/unordered_map>
#  include <tr1/unordered_set>
#  define CERES_HASH_NAMESPACE_START namespace std { namespace tr1 {
#  define CERES_HASH_NAMESPACE_END } }
#endif

#if defined(CERES_STD_UNORDERED_MAP)
#  include <unordered_map>
#  include <unordered_set>
#  define CERES_HASH_NAMESPACE_START namespace std {
#  define CERES_HASH_NAMESPACE_END }
#endif

#if defined(CERES_STD_UNORDERED_MAP_IN_TR1_NAMESPACE)
#  include <unordered_map>
#  include <unordered_set>
#  define CERES_HASH_NAMESPACE_START namespace std { namespace tr1 {
#  define CERES_HASH_NAMESPACE_END } }
#endif

#if !defined(CERES_NO_UNORDERED_MAP) && !defined(CERES_TR1_UNORDERED_MAP) && \
    !defined(CERES_STD_UNORDERED_MAP) && !defined(CERES_STD_UNORDERED_MAP_IN_TR1_NAMESPACE)  // NOLINT
#  error One of: CERES_NO_UNORDERED_MAP, CERES_TR1_UNORDERED_MAP,\
 CERES_STD_UNORDERED_MAP, CERES_STD_UNORDERED_MAP_IN_TR1_NAMESPACE must be defined!  // NOLINT
#endif

#include <utility>
#include "ceres/integral_types.h"
#include "ceres/internal/port.h"

// Some systems don't have access to unordered_map/unordered_set. In
// that case, substitute the hash map/set with normal map/set. The
// price to pay is slower speed for some operations.
#if defined(CERES_NO_UNORDERED_MAP)

namespace ceres {
namespace internal {

template<typename K, typename V>
struct HashMap : map<K, V> {};

template<typename K>
struct HashSet : set<K> {};

}  // namespace internal
}  // namespace ceres

#else

namespace ceres {
namespace internal {

#if defined(CERES_TR1_UNORDERED_MAP) || \
    defined(CERES_STD_UNORDERED_MAP_IN_TR1_NAMESPACE)
template<typename K, typename V>
struct HashMap : std::tr1::unordered_map<K, V> {};
template<typename K>
struct HashSet : std::tr1::unordered_set<K> {};
#endif

#if defined(CERES_STD_UNORDERED_MAP)
template<typename K, typename V>
struct HashMap : std::unordered_map<K, V> {};
template<typename K>
struct HashSet : std::unordered_set<K> {};
#endif

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
inline void hash_mix(uint32& a, uint32& b, uint32& c) {
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
inline void hash_mix(uint64& a, uint64& b, uint64& c) {
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

inline uint32 Hash32NumWithSeed(uint32 num, uint32 c) {
  // The golden ratio; an arbitrary value.
  uint32 b = 0x9e3779b9UL;
  hash_mix(num, b, c);
  return c;
}

inline uint64 Hash64NumWithSeed(uint64 num, uint64 c) {
  // More of the golden ratio.
  uint64 b = GG_ULONGLONG(0xe08c1d668b756f82);
  hash_mix(num, b, c);
  return c;
}

}  // namespace internal
}  // namespace ceres

// Since on some platforms this is a doubly-nested namespace (std::tr1) and
// others it is not, the entire namespace line must be in a macro.
CERES_HASH_NAMESPACE_START

// The outrageously annoying specializations below are for portability reasons.
// In short, it's not possible to have two overloads of hash<pair<T1, T2>

// Hasher for STL pairs. Requires hashers for both members to be defined.
template<typename T>
struct hash<pair<T, T> > {
  size_t operator()(const pair<T, T>& p) const {
    size_t h1 = hash<T>()(p.first);
    size_t h2 = hash<T>()(p.second);
    // The decision below is at compile time
    return (sizeof(h1) <= sizeof(ceres::internal::uint32)) ?
            ceres::internal::Hash32NumWithSeed(h1, h2) :
            ceres::internal::Hash64NumWithSeed(h1, h2);
  }
  // Less than operator for MSVC.
  bool operator()(const pair<T, T>& a,
                  const pair<T, T>& b) const {
    return a < b;
  }
  static const size_t bucket_size = 4;  // These are required by MSVC
  static const size_t min_buckets = 8;  // 4 and 8 are defaults.
};

CERES_HASH_NAMESPACE_END

#endif  // CERES_NO_UNORDERED_MAP
#endif  // CERES_INTERNAL_COLLECTIONS_PORT_H_
