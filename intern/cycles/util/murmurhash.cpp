/*
 * Copyright 2018 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* This is taken from alShaders/Cryptomatte/MurmurHash3.h:
 *
 * MurmurHash3 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code.
 */

#include <stdlib.h>
#include <string.h>

#include "util/math.h"
#include "util/murmurhash.h"

#if defined(_MSC_VER)
#  define ROTL32(x, y) _rotl(x, y)
#  define ROTL64(x, y) _rotl64(x, y)
#  define BIG_CONSTANT(x) (x)
#else
ccl_device_inline uint32_t rotl32(uint32_t x, int8_t r)
{
  return (x << r) | (x >> (32 - r));
}
#  define ROTL32(x, y) rotl32(x, y)
#  define BIG_CONSTANT(x) (x##LLU)
#endif

CCL_NAMESPACE_BEGIN

/* Block read - if your platform needs to do endian-swapping or can only
 * handle aligned reads, do the conversion here. */
ccl_device_inline uint32_t mm_hash_getblock32(const uint32_t *p, int i)
{
  return p[i];
}

/* Finalization mix - force all bits of a hash block to avalanche */
ccl_device_inline uint32_t mm_hash_fmix32(uint32_t h)
{
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}

uint32_t util_murmur_hash3(const void *key, int len, uint32_t seed)
{
  const uint8_t *data = (const uint8_t *)key;
  const int nblocks = len / 4;

  uint32_t h1 = seed;

  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;

  const uint32_t *blocks = (const uint32_t *)(data + nblocks * 4);

  for (int i = -nblocks; i; i++) {
    uint32_t k1 = mm_hash_getblock32(blocks, i);

    k1 *= c1;
    k1 = ROTL32(k1, 15);
    k1 *= c2;

    h1 ^= k1;
    h1 = ROTL32(h1, 13);
    h1 = h1 * 5 + 0xe6546b64;
  }

  const uint8_t *tail = (const uint8_t *)(data + nblocks * 4);

  uint32_t k1 = 0;

  switch (len & 3) {
    case 3:
      k1 ^= tail[2] << 16;
      ATTR_FALLTHROUGH;
    case 2:
      k1 ^= tail[1] << 8;
      ATTR_FALLTHROUGH;
    case 1:
      k1 ^= tail[0];
      k1 *= c1;
      k1 = ROTL32(k1, 15);
      k1 *= c2;
      h1 ^= k1;
  }

  h1 ^= len;
  h1 = mm_hash_fmix32(h1);
  return h1;
}

/* This is taken from the cryptomatte specification 1.0 */
float util_hash_to_float(uint32_t hash)
{
  uint32_t mantissa = hash & ((1 << 23) - 1);
  uint32_t exponent = (hash >> 23) & ((1 << 8) - 1);
  exponent = max(exponent, (uint32_t)1);
  exponent = min(exponent, (uint32_t)254);
  exponent = exponent << 23;
  uint32_t sign = (hash >> 31);
  sign = sign << 31;
  uint32_t float_bits = sign | exponent | mantissa;
  float f;
  memcpy(&f, &float_bits, sizeof(uint32_t));
  return f;
}

CCL_NAMESPACE_END
