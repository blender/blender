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
 * Contributor(s): Daniel Dunbar, Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/edgehash.c
 *  \ingroup bli
 *
 * A general (pointer -> pointer) hash table ADT
 *
 * \note Based on 'BLI_ghash.c', make sure these stay in sync.
 */


#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_edgehash.h"
#include "BLI_mempool.h"

#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wsign-conversion"
#  if (__GNUC__ * 100 + __GNUC_MINOR__) >= 406  /* gcc4.6+ only */
#    pragma GCC diagnostic error "-Wsign-compare"
#    pragma GCC diagnostic error "-Wconversion"
#  endif
#endif

/**************inlined code************/
static const unsigned int _ehash_hashsizes[] = {
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

typedef struct EdgeEntry {
	struct EdgeEntry *next;
	unsigned int v0, v1;
	void *val;
} EdgeEntry;

struct EdgeHash {
	EdgeEntry **buckets;
	BLI_mempool *epool;
	unsigned int nbuckets, nentries;
	unsigned short cursize, flag;
};


/* -------------------------------------------------------------------- */
/* EdgeHash API */

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

/**
 * Insert edge (\a v0, \a v1) into hash with given value, does
 * not check for duplicates.
 */
void BLI_edgehash_insert(EdgeHash *eh, unsigned int v0, unsigned int v1, void *val)
{
	unsigned int hash;
	EdgeEntry *e = BLI_mempool_alloc(eh->epool);

	BLI_assert((eh->flag & EDGEHASH_FLAG_ALLOW_DUPES) || (BLI_edgehash_haskey(eh, v0, v1) == 0));

	/* this helps to track down errors with bad edge data */
	BLI_assert(v0 != v1);

	EDGE_ORD(v0, v1); /* ensure v0 is smaller */

	hash = EDGE_HASH(v0, v1) % eh->nbuckets;

	e->next = eh->buckets[hash];
	e->v0 = v0;
	e->v1 = v1;
	e->val = val;
	eh->buckets[hash] = e;

	if (UNLIKELY(++eh->nentries > eh->nbuckets / 2)) {
		EdgeEntry **old = eh->buckets;
		const unsigned int nold = eh->nbuckets;
		unsigned int i;

		eh->nbuckets = _ehash_hashsizes[++eh->cursize];
		eh->buckets = MEM_callocN(eh->nbuckets * sizeof(*eh->buckets), "eh buckets");

		for (i = 0; i < nold; i++) {
			EdgeEntry *e_next;
			for (e = old[i]; e; e = e_next) {
				e_next = e->next;
				hash = EDGE_HASH(e->v0, e->v1) % eh->nbuckets;
				e->next = eh->buckets[hash];
				eh->buckets[hash] = e;
			}
		}

		MEM_freeN(old);
	}
}

BLI_INLINE EdgeEntry *edgehash_lookup_entry(EdgeHash *eh, unsigned int v0, unsigned int v1)
{
	unsigned int hash;
	EdgeEntry *e;

	EDGE_ORD(v0, v1); /* ensure v0 is smaller */

	hash = EDGE_HASH(v0, v1) % eh->nbuckets;
	for (e = eh->buckets[hash]; e; e = e->next) {
		if (v0 == e->v0 && v1 == e->v1) {
			return e;
		}
	}
	return NULL;
}

/**
 * Return pointer to value for given edge (\a v0, \a v1),
 * or NULL if key does not exist in hash.
 */
void **BLI_edgehash_lookup_p(EdgeHash *eh, unsigned int v0, unsigned int v1)
{
	EdgeEntry *e = edgehash_lookup_entry(eh, v0, v1);
	return e ? &e->val : NULL;
}

/**
 * Return value for given edge (\a v0, \a v1), or NULL if
 * if key does not exist in hash. (If need exists
 * to differentiate between key-value being NULL and
 * lack of key then see BLI_edgehash_lookup_p().
 */
void *BLI_edgehash_lookup(EdgeHash *eh, unsigned int v0, unsigned int v1)
{
	EdgeEntry *e = edgehash_lookup_entry(eh, v0, v1);
	return e ? e->val : NULL;
}

/**
 * Return boolean true/false if edge (v0,v1) in hash.
 */
bool BLI_edgehash_haskey(EdgeHash *eh, unsigned int v0, unsigned int v1)
{
	return (edgehash_lookup_entry(eh, v0, v1) != NULL);
}

/**
 * Return number of keys in hash.
 */
int BLI_edgehash_size(EdgeHash *eh)
{
	return (int)eh->nentries;
}

/**
 * Remove all edges from hash.
 */
void BLI_edgehash_clear(EdgeHash *eh, EdgeHashFreeFP valfreefp)
{
	unsigned int i;
	
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


void BLI_edgehash_flag_set(EdgeHash *eh, unsigned short flag)
{
	eh->flag |= flag;
}

void BLI_edgehash_flag_clear(EdgeHash *eh, unsigned short flag)
{
	eh->flag &= (unsigned short)~flag;
}


/* -------------------------------------------------------------------- */
/* EdgeHash Iterator API */

struct EdgeHashIterator {
	EdgeHash *eh;
	unsigned int curBucket;
	EdgeEntry *curEntry;
};


/**
 * Create a new EdgeHashIterator. The hash table must not be mutated
 * while the iterator is in use, and the iterator will step exactly
 * BLI_edgehash_size(gh) times before becoming done.
 */
EdgeHashIterator *BLI_edgehashIterator_new(EdgeHash *eh)
{
	EdgeHashIterator *ehi = MEM_mallocN(sizeof(*ehi), "eh iter");
	ehi->eh = eh;
	ehi->curEntry = NULL;
	ehi->curBucket = UINT_MAX;  /* wraps to zero */
	while (!ehi->curEntry) {
		ehi->curBucket++;
		if (ehi->curBucket == ehi->eh->nbuckets)
			break;
		ehi->curEntry = ehi->eh->buckets[ehi->curBucket];
	}
	return ehi;
}

/**
 * Free an EdgeHashIterator.
 */
void BLI_edgehashIterator_free(EdgeHashIterator *ehi)
{
	MEM_freeN(ehi);
}

/**
 * Retrieve the key from an iterator.
 */
void BLI_edgehashIterator_getKey(EdgeHashIterator *ehi, unsigned int *v0_r, unsigned int *v1_r)
{
	if (ehi->curEntry) {
		*v0_r = ehi->curEntry->v0;
		*v1_r = ehi->curEntry->v1;
	}
}

/**
 * Retrieve the value from an iterator.
 */
void *BLI_edgehashIterator_getValue(EdgeHashIterator *ehi)
{
	return ehi->curEntry ? ehi->curEntry->val : NULL;
}

/**
 * Set the value for an iterator.
 */
void BLI_edgehashIterator_setValue(EdgeHashIterator *ehi, void *val)
{
	if (ehi->curEntry) {
		ehi->curEntry->val = val;
	}
}

/**
 * Steps the iterator to the next index.
 */
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

/**
 * Determine if an iterator is done.
 */
bool BLI_edgehashIterator_isDone(EdgeHashIterator *ehi)
{
	return (ehi->curEntry == NULL);
}
