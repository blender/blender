/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Copyright (C) 2018 Blender Foundation.
 *
 */

/** \file blender/blenlib/intern/hash_mm3.c
 *  \ingroup bli
 *
 *  Functions to compute Murmur3 hash key.
 *
 * This Code is based on alShaders/Cryptomatte/MurmurHash3.h:
 *
 * MurmurHash3 was written by Austin Appleby, and is placed in the public
 * domain. The author hereby disclaims copyright to this source code.
 *
 */

#include "BLI_compiler_compat.h"
#include "BLI_compiler_attrs.h"
#include "BLI_hash_mm3.h"  /* own include */

#if defined(_MSC_VER)
#  include <stdlib.h>
#  define ROTL32(x,y) _rotl(x,y)
#  define BIG_CONSTANT(x) (x)

/* Other compilers */
#else	/* defined(_MSC_VER) */
static inline uint32_t rotl32(uint32_t x, int8_t r)
{
	return (x << r) | (x >> (32 - r));
}
#  define ROTL32(x,y) rotl32(x,y)
#  define BIG_CONSTANT(x) (x##LLU)
#endif /* !defined(_MSC_VER) */

/* Block read - if your platform needs to do endian-swapping or can only
 * handle aligned reads, do the conversion here
 */

BLI_INLINE uint32_t getblock32(const uint32_t * p, int i)
{
	return p[i];
}

BLI_INLINE uint64_t getblock64(const uint64_t * p, int i)
{
	return p[i];
}

/* Finalization mix - force all bits of a hash block to avalanche */

BLI_INLINE uint32_t fmix32(uint32_t h)
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

BLI_INLINE uint64_t fmix64(uint64_t k)
{
	k ^= k >> 33;
	k *= BIG_CONSTANT(0xff51afd7ed558ccd);
	k ^= k >> 33;
	k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
	k ^= k >> 33;

	return k;
}

uint32_t BLI_hash_mm3(const unsigned char *in, size_t len, uint32_t seed)
{
	const uint8_t *data = (const uint8_t*)in;
	const int nblocks = len / 4;

	uint32_t h1 = seed;

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	/* body */

	const uint32_t *blocks = (const uint32_t *)(data + nblocks*4);

	for (int i = -nblocks; i; i++) {
		uint32_t k1 = getblock32(blocks,i);

		k1 *= c1;
		k1 = ROTL32(k1,15);
		k1 *= c2;

		h1 ^= k1;
		h1 = ROTL32(h1,13);
		h1 = h1*5+0xe6546b64;
	}

	/* tail */

	const uint8_t *tail = (const uint8_t*)(data + nblocks*4);

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
			k1 = ROTL32(k1,15);
			k1 *= c2;
			h1 ^= k1;
	};

	/* finalization */

	h1 ^= len;

	h1 = fmix32(h1);

	return h1;
}
