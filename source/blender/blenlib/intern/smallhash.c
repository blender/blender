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

#include <string.h>

#include "MEM_guardedalloc.h"
#include "BLI_utildefines.h"

#include "BLI_smallhash.h"

/* SMHASH_CELL_UNUSED means this cell is inside a key series,
 * while SMHASH_CELL_FREE means this cell terminates a key series.
 *
 * no chance of anyone shoving INT32_MAX-2 into a *val pointer, I
 * imagine.  hopefully.
 *
 * note: these have the SMHASH suffix because we may want to make them public.
 */
#define SMHASH_CELL_UNUSED	((void *)0x7FFFFFFF)
#define SMHASH_CELL_FREE	((void *)0x7FFFFFFD)

#define SMHASH_NONZERO(n) ((n) + !(n))
#define SMHASH_NEXT(h, hoff) ABS(((h) + ((hoff = SMHASH_NONZERO(hoff * 2) + 1), hoff)))

extern unsigned int hashsizes[];

void BLI_smallhash_init(SmallHash *hash)
{
	int i;

	memset(hash, 0, sizeof(*hash));

	hash->table = hash->_stacktable;
	hash->curhash = 2;
	hash->size = hashsizes[hash->curhash];

	hash->copytable = hash->_copytable;
	hash->stacktable = hash->_stacktable;

	for (i = 0; i < hash->size; i++) {
		hash->table[i].val = SMHASH_CELL_FREE;
	}
}

/*NOTE: does *not* free *hash itself!  only the direct data!*/
void BLI_smallhash_release(SmallHash *hash)
{
	if (hash == NULL) {
		return;
	}

	if (hash->table != hash->stacktable) {
		MEM_freeN(hash->table);
	}
}

void BLI_smallhash_insert(SmallHash *hash, uintptr_t key, void *item)
{
	int h, hoff=1;

	if (hash->size < hash->used * 3) {
		int newsize = hashsizes[++hash->curhash];
		SmallHashEntry *tmp;
		int i = 0;

		if (hash->table != hash->stacktable || newsize > SMSTACKSIZE) {
			tmp = MEM_callocN(sizeof(*hash->table) * newsize, __func__);
		}
		else {
			SWAP(SmallHashEntry *, hash->stacktable, hash->copytable);
			tmp = hash->stacktable;
		}

		SWAP(SmallHashEntry *, tmp, hash->table);

		hash->size = newsize;

		for (i = 0; i < hash->size; i++) {
			hash->table[i].val = SMHASH_CELL_FREE;
		}

		for (i = 0; i<hashsizes[hash->curhash - 1]; i++) {
			if (ELEM(tmp[i].val, SMHASH_CELL_UNUSED, SMHASH_CELL_FREE)) {
				continue;
			}

			h = ABS((int)(tmp[i].key));
			hoff = 1;
			while (!ELEM(hash->table[h % newsize].val, SMHASH_CELL_UNUSED, SMHASH_CELL_FREE)) {
				h = SMHASH_NEXT(h, hoff);
			}

			h %= newsize;

			hash->table[h].key = tmp[i].key;
			hash->table[h].val = tmp[i].val;
		}

		if (tmp != hash->stacktable && tmp != hash->copytable) {
			MEM_freeN(tmp);
		}
	}

	h = ABS((int)key);
	hoff = 1;

	while (!ELEM(hash->table[h % hash->size].val, SMHASH_CELL_UNUSED, SMHASH_CELL_FREE)) {
		h = SMHASH_NEXT(h, hoff);
	}

	h %= hash->size;
	hash->table[h].key = key;
	hash->table[h].val = item;

	hash->used++;
}

void BLI_smallhash_remove(SmallHash *hash, uintptr_t key)
{
	int h, hoff=1;

	h = ABS((int)key);

	while ((hash->table[h % hash->size].key != key) ||
	       (hash->table[h % hash->size].val == SMHASH_CELL_UNUSED))
	{
		if (hash->table[h % hash->size].val == SMHASH_CELL_FREE) {
			return;
		}

		h = SMHASH_NEXT(h, hoff);
	}

	h %= hash->size;
	hash->table[h].key = 0;
	hash->table[h].val = SMHASH_CELL_UNUSED;
}

void *BLI_smallhash_lookup(SmallHash *hash, uintptr_t key)
{
	int h, hoff=1;
	void *v;

	h = ABS((int)key);

	if (hash->table == NULL) {
		return NULL;
	}

	while ((hash->table[h % hash->size].key != key) ||
	       (hash->table[h % hash->size].val == SMHASH_CELL_UNUSED))
	{
		if (hash->table[h % hash->size].val == SMHASH_CELL_FREE) {
			return NULL;
		}

		h = SMHASH_NEXT(h, hoff);
	}

	v = hash->table[h % hash->size].val;
	if (ELEM(v, SMHASH_CELL_UNUSED, SMHASH_CELL_FREE)) {
		return NULL;
	}

	return v;
}


int BLI_smallhash_haskey(SmallHash *hash, uintptr_t key)
{
	int h = ABS((int)key);
	int hoff =1;

	if (hash->table == NULL) {
		return 0;
	}

	while ((hash->table[h % hash->size].key != key) ||
	       (hash->table[h % hash->size].val == SMHASH_CELL_UNUSED))
	{
		if (hash->table[h % hash->size].val == SMHASH_CELL_FREE) {
			return 0;
		}

		h = SMHASH_NEXT(h, hoff);
	}

	return !ELEM(hash->table[h % hash->size].val, SMHASH_CELL_UNUSED, SMHASH_CELL_FREE);
}

int BLI_smallhash_count(SmallHash *hash)
{
	return hash->used;
}

void *BLI_smallhash_iternext(SmallHashIter *iter, uintptr_t *key)
{
	while (iter->i < iter->hash->size) {
		if (    (iter->hash->table[iter->i].val != SMHASH_CELL_UNUSED) &&
		        (iter->hash->table[iter->i].val != SMHASH_CELL_FREE))
		{
			if (key) {
				*key = iter->hash->table[iter->i].key;
			}

			iter->i++;

			return iter->hash->table[iter->i - 1].val;
		}

		iter->i++;
	}

	return NULL;
}

void *BLI_smallhash_iternew(SmallHash *hash, SmallHashIter *iter, uintptr_t *key)
{
	iter->hash = hash;
	iter->i = 0;

	return BLI_smallhash_iternext(iter, key);
}

/* note, this was called _print_smhash in knifetool.c
 * it may not be intended for general use - campbell */
#if 0
void BLI_smallhash_print(SmallHash *hash)
{
	int i, linecol=79, c=0;

	printf("{");
	for (i=0; i<hash->size; i++) {
		if (hash->table[i].val == SMHASH_CELL_UNUSED) {
			printf("--u-");
		}
		else if (hash->table[i].val == SMHASH_CELL_FREE) {
			printf("--f-");
		}
		else	{
			printf("%2x", (unsigned int)hash->table[i].key);
		}

		if (i != hash->size-1)
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
