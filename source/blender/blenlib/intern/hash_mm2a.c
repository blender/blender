/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 * Copyright (C) 2014 Blender Foundation.
 */

/** \file
 * \ingroup bli
 *
 *  Functions to compute Murmur2A hash key.
 *
 * A very fast hash generating int32 result, with few collisions and good repartition.
 *
 * See also:
 * reference implementation:
 * - https://smhasher.googlecode.com/svn-history/r130/trunk/MurmurHash2.cpp
 * - http://programmers.stackexchange.com/questions/49550
 *
 * \warning Do not store that hash in files or such, it is not endian-agnostic,
 * so you should only use it for temporary data.
 */

#include "BLI_compiler_attrs.h"

#include "BLI_hash_mm2a.h" /* own include */

/* Helpers. */
#define MM2A_M 0x5bd1e995

#define MM2A_MIX(h, k) \
  { \
    (k) *= MM2A_M; \
    (k) ^= (k) >> 24; \
    (k) *= MM2A_M; \
    (h) = ((h)*MM2A_M) ^ (k); \
  } \
  (void)0

#define MM2A_MIX_FINALIZE(h) \
  { \
    (h) ^= (h) >> 13; \
    (h) *= MM2A_M; \
    (h) ^= (h) >> 15; \
  } \
  (void)0

static void mm2a_mix_tail(BLI_HashMurmur2A *mm2, const unsigned char **data, size_t *len)
{
  while (*len && ((*len < 4) || mm2->count)) {
    mm2->tail |= (uint32_t)(**data) << (mm2->count * 8);

    mm2->count++;
    (*len)--;
    (*data)++;

    if (mm2->count == 4) {
      MM2A_MIX(mm2->hash, mm2->tail);
      mm2->tail = 0;
      mm2->count = 0;
    }
  }
}

void BLI_hash_mm2a_init(BLI_HashMurmur2A *mm2, uint32_t seed)
{
  mm2->hash = seed;
  mm2->tail = 0;
  mm2->count = 0;
  mm2->size = 0;
}

void BLI_hash_mm2a_add(BLI_HashMurmur2A *mm2, const unsigned char *data, size_t len)
{
  mm2->size += (uint32_t)len;

  mm2a_mix_tail(mm2, &data, &len);

  for (; len >= 4; data += 4, len -= 4) {
    uint32_t k = *(const uint32_t *)data;

    MM2A_MIX(mm2->hash, k);
  }

  mm2a_mix_tail(mm2, &data, &len);
}

void BLI_hash_mm2a_add_int(BLI_HashMurmur2A *mm2, int data)
{
  BLI_hash_mm2a_add(mm2, (const unsigned char *)&data, sizeof(data));
}

uint32_t BLI_hash_mm2a_end(BLI_HashMurmur2A *mm2)
{
  MM2A_MIX(mm2->hash, mm2->tail);
  MM2A_MIX(mm2->hash, mm2->size);

  MM2A_MIX_FINALIZE(mm2->hash);

  return mm2->hash;
}

/* Non-incremental version, quicker for small keys. */
uint32_t BLI_hash_mm2(const unsigned char *data, size_t len, uint32_t seed)
{
  /* Initialize the hash to a 'random' value */
  uint32_t h = seed ^ len;

  /* Mix 4 bytes at a time into the hash */
  for (; len >= 4; data += 4, len -= 4) {
    uint32_t k = *(uint32_t *)data;

    MM2A_MIX(h, k);
  }

  /* Handle the last few bytes of the input array */
  switch (len) {
    case 3:
      h ^= data[2] << 16;
      ATTR_FALLTHROUGH;
    case 2:
      h ^= data[1] << 8;
      ATTR_FALLTHROUGH;
    case 1:
      h ^= data[0];
      h *= MM2A_M;
  }

  /* Do a few final mixes of the hash to ensure the last few bytes are well-incorporated. */
  MM2A_MIX_FINALIZE(h);

  return h;
}
