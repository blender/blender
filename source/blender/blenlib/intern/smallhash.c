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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/smallhash.c
 *  \ingroup bli
 *
 * A light stack-friendly hash library, it uses stack space for smallish hash tables
 * but falls back to heap memory once the stack limits reached.
 *
 * based on a doubling non-chaining approach
 */

#include <string.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"
#include "BLI_utildefines.h"

#include "BLI_smallhash.h"

#include "BLI_strict_flags.h"

/* SMHASH_CELL_UNUSED means this cell is inside a key series,
 * while SMHASH_CELL_FREE means this cell terminates a key series.
 *
 * no chance of anyone shoving INT32_MAX-2 into a *val pointer, I
 * imagine.  hopefully.
 *
 * note: these have the SMHASH prefix because we may want to make them public.
 */
#define SMHASH_CELL_UNUSED  ((void *)0x7FFFFFFF)
#define SMHASH_CELL_FREE    ((void *)0x7FFFFFFD)
#define SMHASH_KEY_UNUSED ((uintptr_t)-1)

/* typically this re-assigns 'h' */
#define SMHASH_NEXT(h, hoff)  ( \
	CHECK_TYPE_INLINE(&(h),    unsigned int *), \
	CHECK_TYPE_INLINE(&(hoff), unsigned int *), \
	((h) + (((hoff) = ((hoff) * 2) + 1), (hoff))) \
	)

extern const unsigned int hashsizes[];

/**
 * Check if the number of items in the smallhash is large enough to require more buckets.
 */
BLI_INLINE bool smallhash_test_expand_buckets(const unsigned int nentries, const unsigned int nbuckets)
{
	return nentries * 3 > nbuckets;
}

BLI_INLINE void smallhash_init_empty(SmallHash *sh)
{
	unsigned int i;

	for (i = 0; i < sh->nbuckets; i++) {
		sh->buckets[i].key = SMHASH_KEY_UNUSED;
		sh->buckets[i].val = SMHASH_CELL_FREE;
	}
}

BLI_INLINE SmallHashEntry *smallhash_lookup(SmallHash *sh, const uintptr_t key)
{
	SmallHashEntry *e;
	unsigned int h = (unsigned int)key;
	unsigned int hoff = 1;

	BLI_assert(key != SMHASH_KEY_UNUSED);

	/* note: there are always more buckets then entries,
	 * so we know there will always be a free bucket if the key isn't found. */
	for (e = &sh->buckets[h % sh->nbuckets];
	     e->val != SMHASH_CELL_FREE;
	     h = SMHASH_NEXT(h, hoff), e = &sh->buckets[h % sh->nbuckets])
	{
		if (e->key == key) {
			/* should never happen because unused keys are zero'd */
			BLI_assert(e->val != SMHASH_CELL_UNUSED);
			return e;
		}
	}

	return NULL;
}

BLI_INLINE SmallHashEntry *smallhash_lookup_first_free(SmallHash *sh, const uintptr_t key)
{
	SmallHashEntry *e;
	unsigned int h = (unsigned int)key;
	unsigned int hoff = 1;

	for (e = &sh->buckets[h % sh->nbuckets];
	     !ELEM(e->val, SMHASH_CELL_UNUSED, SMHASH_CELL_FREE);
	     h = SMHASH_NEXT(h, hoff), e = &sh->buckets[h % sh->nbuckets])
	{
		/* pass */
	}

	return e;
}

BLI_INLINE void smallhash_resize_buckets(SmallHash *sh, const unsigned int nbuckets)
{
	SmallHashEntry *buckets_old = sh->buckets;
	const unsigned int nbuckets_old = sh->nbuckets;
	unsigned int i = 0;

	BLI_assert(sh->nbuckets != nbuckets);

	if (buckets_old == sh->buckets_stack && nbuckets <= SMSTACKSIZE) {
		SWAP(SmallHashEntry *, sh->buckets_stack, sh->buckets_copy);
		sh->buckets = sh->buckets_stack;
	}
	else {
		sh->buckets = MEM_mallocN(sizeof(*sh->buckets) * nbuckets, __func__);
	}

	sh->nbuckets = nbuckets;

	smallhash_init_empty(sh);

	for (i = 0; i < nbuckets_old; i++) {
		if (!ELEM(buckets_old[i].val, SMHASH_CELL_UNUSED, SMHASH_CELL_FREE)) {
			SmallHashEntry *e = smallhash_lookup_first_free(sh, buckets_old[i].key);
			e->key = buckets_old[i].key;
			e->val = buckets_old[i].val;
		}
	}

	if (buckets_old != sh->buckets_stack && buckets_old != sh->buckets_copy) {
		MEM_freeN(buckets_old);
	}
}

void BLI_smallhash_init(SmallHash *sh)
{
	/* assume 'sh' is uninitialized */

	sh->nentries = 0;
	sh->cursize = 2;
	sh->nbuckets = hashsizes[sh->cursize];

	sh->buckets         = sh->_buckets_stack;
	sh->buckets_copy    = sh->_buckets_copy;
	sh->buckets_stack   = sh->_buckets_stack;

	smallhash_init_empty(sh);
}

/*NOTE: does *not* free *sh itself!  only the direct data!*/
void BLI_smallhash_release(SmallHash *sh)
{
	if (sh->buckets != sh->buckets_stack) {
		MEM_freeN(sh->buckets);
	}
}

void BLI_smallhash_insert(SmallHash *sh, uintptr_t key, void *val)
{
	SmallHashEntry *e;

	BLI_assert(key != SMHASH_KEY_UNUSED);
	BLI_assert(!ELEM(val, SMHASH_CELL_UNUSED, SMHASH_CELL_FREE));
	BLI_assert(BLI_smallhash_haskey(sh, key) == false);

	if (UNLIKELY(smallhash_test_expand_buckets(++sh->nentries, sh->nbuckets))) {
		smallhash_resize_buckets(sh, hashsizes[++sh->cursize]);
	}

	e = smallhash_lookup_first_free(sh, key);
	e->key = key;
	e->val = val;
}

bool BLI_smallhash_remove(SmallHash *sh, uintptr_t key)
{
	SmallHashEntry *e = smallhash_lookup(sh, key);

	if (e) {
		e->key = SMHASH_KEY_UNUSED;
		e->val = SMHASH_CELL_UNUSED;
		sh->nentries--;

		return true;
	}
	else {
		return false;
	}
}

void *BLI_smallhash_lookup(SmallHash *sh, uintptr_t key)
{
	SmallHashEntry *e = smallhash_lookup(sh, key);

	return e ? e->val : NULL;
}

void **BLI_smallhash_lookup_p(SmallHash *sh, uintptr_t key)
{
	SmallHashEntry *e = smallhash_lookup(sh, key);

	return e ? &e->val : NULL;
}

bool BLI_smallhash_haskey(SmallHash *sh, uintptr_t key)
{
	SmallHashEntry *e = smallhash_lookup(sh, key);

	return (e != NULL);
}

int BLI_smallhash_count(SmallHash *sh)
{
	return (int)sh->nentries;
}

void *BLI_smallhash_iternext(SmallHashIter *iter, uintptr_t *key)
{
	while (iter->i < iter->sh->nbuckets) {
		if ((iter->sh->buckets[iter->i].val != SMHASH_CELL_UNUSED) &&
		    (iter->sh->buckets[iter->i].val != SMHASH_CELL_FREE))
		{
			if (key) {
				*key = iter->sh->buckets[iter->i].key;
			}

			return iter->sh->buckets[iter->i++].val;
		}

		iter->i++;
	}

	return NULL;
}

void *BLI_smallhash_iternew(SmallHash *sh, SmallHashIter *iter, uintptr_t *key)
{
	iter->sh = sh;
	iter->i = 0;

	return BLI_smallhash_iternext(iter, key);
}

/* note, this was called _print_smhash in knifetool.c
 * it may not be intended for general use - campbell */
#if 0
void BLI_smallhash_print(SmallHash *sh)
{
	unsigned int i, linecol = 79, c = 0;

	printf("{");
	for (i = 0; i < sh->nbuckets; i++) {
		if (sh->buckets[i].val == SMHASH_CELL_UNUSED) {
			printf("--u-");
		}
		else if (sh->buckets[i].val == SMHASH_CELL_FREE) {
			printf("--f-");
		}
		else {
			printf("%2x", (unsigned int)sh->buckets[i].key);
		}

		if (i != sh->nbuckets - 1)
			printf(", ");

		c += 6;

		if (c >= linecol) {
			printf("\n ");
			c = 0;
		}
	}

	fflush(stdout);
}
#endif
