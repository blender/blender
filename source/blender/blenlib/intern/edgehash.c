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
 * The Original Code is: none of this file.
 *
 * Contributor(s): Daniel Dunbar, Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 * A general (pointer -> pointer) hash table ADT
 */

/** \file blender/blenlib/intern/edgehash.c
 *  \ingroup bli
 */


#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_edgehash.h"
#include "BLI_mempool.h"

/**************inlined code************/
static unsigned int _ehash_hashsizes[] = {
	1, 3, 5, 11, 17, 37, 67, 131, 257, 521, 1031, 2053, 4099, 8209,
	16411, 32771, 65537, 131101, 262147, 524309, 1048583, 2097169,
	4194319, 8388617, 16777259, 33554467, 67108879, 134217757,
	268435459
};

#define EDGE_HASH(v0, v1)  ((v0 * 39) ^ (v1 * 31))

/* ensure v0 is smaller */
#define EDGE_ORD(v0, v1) \
	if (v0 > v1) {       \
		v0 ^= v1;        \
		v1 ^= v0;        \
		v0 ^= v1;        \
	} (void)0

/***/

typedef struct EdgeEntry EdgeEntry;
struct EdgeEntry {
	EdgeEntry *next;
	unsigned int v0, v1;
	void *val;
};

struct EdgeHash {
	EdgeEntry **buckets;
	BLI_mempool *epool;
	int nbuckets, nentries, cursize;
};

/***/

EdgeHash *BLI_edgehash_new(void)
{
	EdgeHash *eh = MEM_callocN(sizeof(*eh), "EdgeHash");
	eh->cursize = 0;
	eh->nentries = 0;
	eh->nbuckets = _ehash_hashsizes[eh->cursize];
	
	eh->buckets = MEM_callocN(eh->nbuckets * sizeof(*eh->buckets), "eh buckets 2");
	eh->epool = BLI_mempool_create(sizeof(EdgeEntry), 512, 512, BLI_MEMPOOL_SYSMALLOC);

	return eh;
}


void BLI_edgehash_insert(EdgeHash *eh, unsigned int v0, unsigned int v1, void *val)
{
	unsigned int hash;
	EdgeEntry *e = BLI_mempool_alloc(eh->epool);

	/* this helps to track down errors with bad edge data */
	BLI_assert(v0 != v1);

	EDGE_ORD(v0, v1); /* ensure v0 is smaller */

	hash = EDGE_HASH(v0, v1) % eh->nbuckets;

	e->v0 = v0;
	e->v1 = v1;
	e->val = val;
	e->next = eh->buckets[hash];
	eh->buckets[hash] = e;

	if (++eh->nentries > eh->nbuckets * 3) {
		EdgeEntry *e, **old = eh->buckets;
		int i, nold = eh->nbuckets;

		eh->nbuckets = _ehash_hashsizes[++eh->cursize];
		eh->buckets = MEM_mallocN(eh->nbuckets * sizeof(*eh->buckets), "eh buckets");
		memset(eh->buckets, 0, eh->nbuckets * sizeof(*eh->buckets));

		for (i = 0; i < nold; i++) {
			for (e = old[i]; e; ) {
				EdgeEntry *n = e->next;

				hash = EDGE_HASH(e->v0, e->v1) % eh->nbuckets;
				e->next = eh->buckets[hash];
				eh->buckets[hash] = e;

				e = n;
			}
		}

		MEM_freeN(old);
	}
}

void **BLI_edgehash_lookup_p(EdgeHash *eh, unsigned int v0, unsigned int v1)
{
	unsigned int hash;
	EdgeEntry *e;

	EDGE_ORD(v0, v1); /* ensure v0 is smaller */

	hash = EDGE_HASH(v0, v1) % eh->nbuckets;
	for (e = eh->buckets[hash]; e; e = e->next)
		if (v0 == e->v0 && v1 == e->v1)
			return &e->val;

	return NULL;
}

void *BLI_edgehash_lookup(EdgeHash *eh, unsigned int v0, unsigned int v1)
{
	void **value_p = BLI_edgehash_lookup_p(eh, v0, v1);

	return value_p ? *value_p : NULL;
}

int BLI_edgehash_haskey(EdgeHash *eh, unsigned int v0, unsigned int v1)
{
	return BLI_edgehash_lookup_p(eh, v0, v1) != NULL;
}

int BLI_edgehash_size(EdgeHash *eh)
{
	return eh->nentries;
}

void BLI_edgehash_clear(EdgeHash *eh, EdgeHashFreeFP valfreefp)
{
	int i;
	
	for (i = 0; i < eh->nbuckets; i++) {
		EdgeEntry *e;
		
		for (e = eh->buckets[i]; e; ) {
			EdgeEntry *n = e->next;
			
			if (valfreefp) valfreefp(e->val);
			BLI_mempool_free(eh->epool, e);
			
			e = n;
		}
		eh->buckets[i] = NULL;
	}

	eh->nentries = 0;
}

void BLI_edgehash_free(EdgeHash *eh, EdgeHashFreeFP valfreefp)
{
	BLI_edgehash_clear(eh, valfreefp);

	BLI_mempool_destroy(eh->epool);

	MEM_freeN(eh->buckets);
	MEM_freeN(eh);
}


/***/

struct EdgeHashIterator {
	EdgeHash *eh;
	int curBucket;
	EdgeEntry *curEntry;
};

EdgeHashIterator *BLI_edgehashIterator_new(EdgeHash *eh)
{
	EdgeHashIterator *ehi = MEM_mallocN(sizeof(*ehi), "eh iter");
	ehi->eh = eh;
	ehi->curEntry = NULL;
	ehi->curBucket = -1;
	while (!ehi->curEntry) {
		ehi->curBucket++;
		if (ehi->curBucket == ehi->eh->nbuckets)
			break;
		ehi->curEntry = ehi->eh->buckets[ehi->curBucket];
	}
	return ehi;
}
void BLI_edgehashIterator_free(EdgeHashIterator *ehi)
{
	MEM_freeN(ehi);
}

void BLI_edgehashIterator_getKey(EdgeHashIterator *ehi, unsigned int *v0_r, unsigned int *v1_r)
{
	if (ehi->curEntry) {
		*v0_r = ehi->curEntry->v0;
		*v1_r = ehi->curEntry->v1;
	}
}
void *BLI_edgehashIterator_getValue(EdgeHashIterator *ehi)
{
	return ehi->curEntry ? ehi->curEntry->val : NULL;
}

void BLI_edgehashIterator_setValue(EdgeHashIterator *ehi, void *val)
{
	if (ehi->curEntry) {
		ehi->curEntry->val = val;
	}
}

void BLI_edgehashIterator_step(EdgeHashIterator *ehi)
{
	if (ehi->curEntry) {
		ehi->curEntry = ehi->curEntry->next;
		while (!ehi->curEntry) {
			ehi->curBucket++;
			if (ehi->curBucket == ehi->eh->nbuckets) {
				break;
			}

			ehi->curEntry = ehi->eh->buckets[ehi->curBucket];
		}
	}
}
int BLI_edgehashIterator_isDone(EdgeHashIterator *ehi)
{
	return !ehi->curEntry;
}

