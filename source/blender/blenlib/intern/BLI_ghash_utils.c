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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/BLI_ghash_utils.c
 *  \ingroup bli
 *
 * Helper functions and implementations of standard data types for #GHash
 * (not it's implementation).
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_hash_mm2a.h"
#include "BLI_ghash.h"  /* own include */

/* keep last */
#include "BLI_strict_flags.h"

/* -------------------------------------------------------------------- */
/** \name Generic Key Hash & Comparison Functions
 * \{ */

#if 0
/* works but slower */
uint BLI_ghashutil_ptrhash(const void *key)
{
	return (uint)(intptr_t)key;
}
#else
/* based python3.3's pointer hashing function */
uint BLI_ghashutil_ptrhash(const void *key)
{
	size_t y = (size_t)key;
	/* bottom 3 or 4 bits are likely to be 0; rotate y by 4 to avoid
	 * excessive hash collisions for dicts and sets */
	y = (y >> 4) | (y << (8 * sizeof(void *) - 4));
	return (uint)y;
}
#endif
bool BLI_ghashutil_ptrcmp(const void *a, const void *b)
{
	return (a != b);
}

uint BLI_ghashutil_uinthash_v4(const uint key[4])
{
	uint hash;
	hash  = key[0];
	hash *= 37;
	hash += key[1];
	hash *= 37;
	hash += key[2];
	hash *= 37;
	hash += key[3];
	return hash;
}
uint BLI_ghashutil_uinthash_v4_murmur(const uint key[4])
{
	return BLI_hash_mm2((const unsigned char *)key, sizeof(int) * 4  /* sizeof(key) */, 0);
}

bool BLI_ghashutil_uinthash_v4_cmp(const void *a, const void *b)
{
	return (memcmp(a, b, sizeof(uint[4])) != 0);
}

uint BLI_ghashutil_uinthash(uint key)
{
	key += ~(key << 16);
	key ^=  (key >>  5);
	key +=  (key <<  3);
	key ^=  (key >> 13);
	key += ~(key <<  9);
	key ^=  (key >> 17);

	return key;
}

uint BLI_ghashutil_inthash_p(const void *ptr)
{
	uintptr_t key = (uintptr_t)ptr;

	key += ~(key << 16);
	key ^=  (key >>  5);
	key +=  (key <<  3);
	key ^=  (key >> 13);
	key += ~(key <<  9);
	key ^=  (key >> 17);

	return (uint)(key & 0xffffffff);
}

uint BLI_ghashutil_inthash_p_murmur(const void *ptr)
{
	uintptr_t key = (uintptr_t)ptr;

	return BLI_hash_mm2((const unsigned char *)&key, sizeof(key), 0);
}

uint BLI_ghashutil_inthash_p_simple(const void *ptr)
{
	return POINTER_AS_UINT(ptr);
}

bool BLI_ghashutil_intcmp(const void *a, const void *b)
{
	return (a != b);
}

size_t BLI_ghashutil_combine_hash(size_t hash_a, size_t hash_b)
{
	return hash_a ^ (hash_b + 0x9e3779b9 + (hash_a << 6) + (hash_a >> 2));
}

/**
 * This function implements the widely used "djb" hash apparently posted
 * by Daniel Bernstein to comp.lang.c some time ago.  The 32 bit
 * unsigned hash value starts at 5381 and for each byte 'c' in the
 * string, is updated: ``hash = hash * 33 + c``.  This
 * function uses the signed value of each byte.
 *
 * note: this is the same hash method that glib 2.34.0 uses.
 */
uint BLI_ghashutil_strhash_n(const char *key, size_t n)
{
	const signed char *p;
	uint h = 5381;

	for (p = (const signed char *)key; n-- && *p != '\0'; p++) {
		h = (uint)((h << 5) + h) + (uint)*p;
	}

	return h;
}
uint BLI_ghashutil_strhash_p(const void *ptr)
{
	const signed char *p;
	uint h = 5381;

	for (p = ptr; *p != '\0'; p++) {
		h = (uint)((h << 5) + h) + (uint)*p;
	}

	return h;
}
uint BLI_ghashutil_strhash_p_murmur(const void *ptr)
{
	const unsigned char *key = ptr;

	return BLI_hash_mm2(key, strlen((const char *)key) + 1, 0);
}
bool BLI_ghashutil_strcmp(const void *a, const void *b)
{
	return (a == b) ? false : !STREQ(a, b);
}

GHashPair *BLI_ghashutil_pairalloc(const void *first, const void *second)
{
	GHashPair *pair = MEM_mallocN(sizeof(GHashPair), "GHashPair");
	pair->first = first;
	pair->second = second;
	return pair;
}

uint BLI_ghashutil_pairhash(const void *ptr)
{
	const GHashPair *pair = ptr;
	uint hash = BLI_ghashutil_ptrhash(pair->first);
	return hash ^ BLI_ghashutil_ptrhash(pair->second);
}

bool BLI_ghashutil_paircmp(const void *a, const void *b)
{
	const GHashPair *A = a;
	const GHashPair *B = b;

	return (BLI_ghashutil_ptrcmp(A->first, B->first) ||
	        BLI_ghashutil_ptrcmp(A->second, B->second));
}

void BLI_ghashutil_pairfree(void *ptr)
{
	MEM_freeN(ptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Convenience GHash Creation Functions
 * \{ */

GHash *BLI_ghash_ptr_new_ex(const char *info, const uint nentries_reserve)
{
	return BLI_ghash_new_ex(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, info, nentries_reserve);
}
GHash *BLI_ghash_ptr_new(const char *info)
{
	return BLI_ghash_ptr_new_ex(info, 0);
}

GHash *BLI_ghash_str_new_ex(const char *info, const uint nentries_reserve)
{
	return BLI_ghash_new_ex(BLI_ghashutil_strhash_p, BLI_ghashutil_strcmp, info, nentries_reserve);
}
GHash *BLI_ghash_str_new(const char *info)
{
	return BLI_ghash_str_new_ex(info, 0);
}

GHash *BLI_ghash_int_new_ex(const char *info, const uint nentries_reserve)
{
	return BLI_ghash_new_ex(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, info, nentries_reserve);
}
GHash *BLI_ghash_int_new(const char *info)
{
	return BLI_ghash_int_new_ex(info, 0);
}

GHash *BLI_ghash_pair_new_ex(const char *info, const uint nentries_reserve)
{
	return BLI_ghash_new_ex(BLI_ghashutil_pairhash, BLI_ghashutil_paircmp, info, nentries_reserve);
}
GHash *BLI_ghash_pair_new(const char *info)
{
	return BLI_ghash_pair_new_ex(info, 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Convenience GSet Creation Functions
 * \{ */

GSet *BLI_gset_ptr_new_ex(const char *info, const uint nentries_reserve)
{
	return BLI_gset_new_ex(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, info, nentries_reserve);
}
GSet *BLI_gset_ptr_new(const char *info)
{
	return BLI_gset_ptr_new_ex(info, 0);
}

GSet *BLI_gset_str_new_ex(const char *info, const uint nentries_reserve)
{
	return BLI_gset_new_ex(BLI_ghashutil_strhash_p, BLI_ghashutil_strcmp, info, nentries_reserve);
}
GSet *BLI_gset_str_new(const char *info)
{
	return BLI_gset_str_new_ex(info, 0);
}

GSet *BLI_gset_pair_new_ex(const char *info, const uint nentries_reserve)
{
	return BLI_gset_new_ex(BLI_ghashutil_pairhash, BLI_ghashutil_paircmp, info, nentries_reserve);
}
GSet *BLI_gset_pair_new(const char *info)
{
	return BLI_gset_pair_new_ex(info, 0);
}

/** \} */
