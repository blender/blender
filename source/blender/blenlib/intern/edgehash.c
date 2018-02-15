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
 * An (edge -> pointer) chaining hash table.
 * Using unordered int-pairs as keys.
 *
 * \note Based on 'BLI_ghash.c', which is a more generalized hash-table
 * make sure these stay in sync.
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_edgehash.h"
#include "BLI_mempool.h"
#include "BLI_strict_flags.h"

/**************inlined code************/
static const uint _ehash_hashsizes[] = {
	1, 3, 5, 11, 17, 37, 67, 131, 257, 521, 1031, 2053, 4099, 8209,
	16411, 32771, 65537, 131101, 262147, 524309, 1048583, 2097169,
	4194319, 8388617, 16777259, 33554467, 67108879, 134217757,
	268435459
};

/* internal flag to ensure sets values aren't used */
#ifndef NDEBUG
#  define EDGEHASH_FLAG_IS_SET (1 << 8)
#  define IS_EDGEHASH_ASSERT(eh) BLI_assert((eh->flag & EDGEHASH_FLAG_IS_SET) == 0)
// #  define IS_EDGESET_ASSERT(es) BLI_assert((es->flag & EDGEHASH_FLAG_IS_SET) != 0)
#else
#  define IS_EDGEHASH_ASSERT(eh)
// #  define IS_EDGESET_ASSERT(es)
#endif

/* ensure v0 is smaller */
#define EDGE_ORD(v0, v1)            \
	if (v0 > v1) {                  \
		SWAP(uint, v0, v1); \
	} (void)0

/***/

typedef struct EdgeEntry {
	struct EdgeEntry *next;
	uint v0, v1;
	void *val;
} EdgeEntry;

struct EdgeHash {
	EdgeEntry **buckets;
	BLI_mempool *epool;
	uint nbuckets, nentries;
	uint cursize, flag;
};


/* -------------------------------------------------------------------- */
/* EdgeHash API */

/** \name Internal Utility API
 * \{ */

/**
 * Compute the hash and get the bucket-index.
 */
BLI_INLINE uint edgehash_bucket_index(EdgeHash *eh, uint v0, uint v1)
{
	BLI_assert(v0 < v1);

	return ((v0 * 65) ^ (v1 * 31)) % eh->nbuckets;
}

/**
 * Check if the number of items in the GHash is large enough to require more buckets.
 */
BLI_INLINE bool edgehash_test_expand_buckets(const uint nentries, const uint nbuckets)
{
	return (nentries > nbuckets * 3);
}

/**
 * Expand buckets to the next size up.
 */
BLI_INLINE void edgehash_resize_buckets(EdgeHash *eh, const uint nbuckets)
{
	EdgeEntry **buckets_old = eh->buckets;
	EdgeEntry **buckets_new;
	const uint nbuckets_old = eh->nbuckets;
	uint i;

	BLI_assert(eh->nbuckets != nbuckets);

	eh->nbuckets = nbuckets;
	buckets_new = MEM_callocN(eh->nbuckets * sizeof(*eh->buckets), "eh buckets");

	for (i = 0; i < nbuckets_old; i++) {
		for (EdgeEntry *e = buckets_old[i], *e_next; e; e = e_next) {
			const unsigned bucket_index = edgehash_bucket_index(eh, e->v0, e->v1);
			e_next = e->next;
			e->next = buckets_new[bucket_index];
			buckets_new[bucket_index] = e;
		}
	}

	eh->buckets = buckets_new;
	MEM_freeN(buckets_old);
}

/**
 * Increase initial bucket size to match a reserved amount.
 */
BLI_INLINE void edgehash_buckets_reserve(EdgeHash *eh, const uint nentries_reserve)
{
	while (edgehash_test_expand_buckets(nentries_reserve, eh->nbuckets)) {
		eh->nbuckets = _ehash_hashsizes[++eh->cursize];
	}
}

/**
 * Internal lookup function.
 * Takes a \a bucket_index argument to avoid calling #edgehash_bucket_index multiple times.
 */
BLI_INLINE EdgeEntry *edgehash_lookup_entry_ex(
        EdgeHash *eh, uint v0, uint v1,
        const uint bucket_index)
{
	EdgeEntry *e;
	BLI_assert(v0 < v1);
	for (e = eh->buckets[bucket_index]; e; e = e->next) {
		if (UNLIKELY(v0 == e->v0 && v1 == e->v1)) {
			return e;
		}
	}
	return NULL;
}

/**
 * Internal lookup function, returns previous entry of target one too.
 * Takes bucket_index argument to avoid calling #edgehash_bucket_index multiple times.
 * Useful when modifying buckets somehow (like removing an entry...).
 */
BLI_INLINE EdgeEntry *edgehash_lookup_entry_prev_ex(
        EdgeHash *eh, uint v0, uint v1,
        EdgeEntry **r_e_prev, const uint bucket_index)
{
	BLI_assert(v0 < v1);
	for (EdgeEntry *e_prev = NULL, *e = eh->buckets[bucket_index]; e; e_prev = e, e = e->next) {
		if (UNLIKELY(v0 == e->v0 && v1 == e->v1)) {
			*r_e_prev = e_prev;
			return e;
		}
	}

	*r_e_prev = NULL;
	return NULL;
}

/**
 * Internal lookup function. Only wraps #edgehash_lookup_entry_ex
 */
BLI_INLINE EdgeEntry *edgehash_lookup_entry(EdgeHash *eh, uint v0, uint v1)
{
	EDGE_ORD(v0, v1);
	const uint bucket_index = edgehash_bucket_index(eh, v0, v1);
	return edgehash_lookup_entry_ex(eh, v0, v1, bucket_index);
}


static EdgeHash *edgehash_new(const char *info,
                              const uint nentries_reserve,
                              const uint entry_size)
{
	EdgeHash *eh = MEM_mallocN(sizeof(*eh), info);

	eh->nbuckets = _ehash_hashsizes[0];  /* eh->cursize */
	eh->nentries = 0;
	eh->cursize = 0;
	eh->flag = 0;

	/* if we have reserved the number of elements that this hash will contain */
	if (nentries_reserve) {
		edgehash_buckets_reserve(eh, nentries_reserve);
	}

	eh->buckets = MEM_callocN(eh->nbuckets * sizeof(*eh->buckets), "eh buckets");
	eh->epool = BLI_mempool_create(entry_size, nentries_reserve, 512, BLI_MEMPOOL_NOP);

	return eh;
}

/**
 * Internal insert function.
 * Takes a \a bucket_index argument to avoid calling #edgehash_bucket_index multiple times.
 */
BLI_INLINE void edgehash_insert_ex(
        EdgeHash *eh, uint v0, uint v1, void *val,
        const uint bucket_index)
{
	EdgeEntry *e = BLI_mempool_alloc(eh->epool);

	BLI_assert((eh->flag & EDGEHASH_FLAG_ALLOW_DUPES) || (BLI_edgehash_haskey(eh, v0, v1) == 0));
	IS_EDGEHASH_ASSERT(eh);

	/* this helps to track down errors with bad edge data */
	BLI_assert(v0 < v1);
	BLI_assert(v0 != v1);

	e->next = eh->buckets[bucket_index];
	e->v0 = v0;
	e->v1 = v1;
	e->val = val;
	eh->buckets[bucket_index] = e;

	if (UNLIKELY(edgehash_test_expand_buckets(++eh->nentries, eh->nbuckets))) {
		edgehash_resize_buckets(eh, _ehash_hashsizes[++eh->cursize]);
	}
}

/**
 * Insert function that doesn't set the value (use for EdgeSet)
 */
BLI_INLINE void edgehash_insert_ex_keyonly(
        EdgeHash *eh, uint v0, uint v1,
        const uint bucket_index)
{
	EdgeEntry *e = BLI_mempool_alloc(eh->epool);

	BLI_assert((eh->flag & EDGEHASH_FLAG_ALLOW_DUPES) || (BLI_edgehash_haskey(eh, v0, v1) == 0));

	/* this helps to track down errors with bad edge data */
	BLI_assert(v0 < v1);
	BLI_assert(v0 != v1);

	e->next = eh->buckets[bucket_index];
	e->v0 = v0;
	e->v1 = v1;
	eh->buckets[bucket_index] = e;

	if (UNLIKELY(edgehash_test_expand_buckets(++eh->nentries, eh->nbuckets))) {
		edgehash_resize_buckets(eh, _ehash_hashsizes[++eh->cursize]);
	}
}

/**
 * Insert function that doesn't set the value (use for EdgeSet)
 */
BLI_INLINE void edgehash_insert_ex_keyonly_entry(
        EdgeHash *eh, uint v0, uint v1,
        const uint bucket_index,
        EdgeEntry *e)
{
	BLI_assert((eh->flag & EDGEHASH_FLAG_ALLOW_DUPES) || (BLI_edgehash_haskey(eh, v0, v1) == 0));

	/* this helps to track down errors with bad edge data */
	BLI_assert(v0 < v1);
	BLI_assert(v0 != v1);

	e->next = eh->buckets[bucket_index];
	e->v0 = v0;
	e->v1 = v1;
	/* intentionally leave value unset */
	eh->buckets[bucket_index] = e;

	if (UNLIKELY(edgehash_test_expand_buckets(++eh->nentries, eh->nbuckets))) {
		edgehash_resize_buckets(eh, _ehash_hashsizes[++eh->cursize]);
	}
}

BLI_INLINE void edgehash_insert(EdgeHash *eh, uint v0, uint v1, void *val)
{
	EDGE_ORD(v0, v1);
	const uint bucket_index = edgehash_bucket_index(eh, v0, v1);
	edgehash_insert_ex(eh, v0, v1, val, bucket_index);
}

/**
 * Remove the entry and return it, caller must free from eh->epool.
 */
BLI_INLINE EdgeEntry *edgehash_remove_ex(
        EdgeHash *eh, uint v0, uint v1,
        EdgeHashFreeFP valfreefp,
        const uint bucket_index)
{
	EdgeEntry *e_prev;
	EdgeEntry *e = edgehash_lookup_entry_prev_ex(eh, v0, v1, &e_prev, bucket_index);

	BLI_assert(v0 < v1);

	if (e) {
		EdgeEntry *e_next = e->next;

		if (valfreefp) {
			valfreefp(e->val);
		}

		if (e_prev) {
			e_prev->next = e_next;
		}
		else {
			eh->buckets[bucket_index] = e_next;
		}

		eh->nentries--;
		return e;
	}

	return e;
}

/**
 * Run free callbacks for freeing entries.
 */
static void edgehash_free_cb(EdgeHash *eh, EdgeHashFreeFP valfreefp)
{
	uint i;

	BLI_assert(valfreefp);

	for (i = 0; i < eh->nbuckets; i++) {
		EdgeEntry *e;

		for (e = eh->buckets[i]; e; ) {
			EdgeEntry *e_next = e->next;

			valfreefp(e->val);

			e = e_next;
		}
	}
}

/** \} */


/** \name Public API
 * \{ */

/* Public API */

EdgeHash *BLI_edgehash_new_ex(const char *info,
                              const uint nentries_reserve)
{
	return edgehash_new(info,
	                    nentries_reserve,
	                    sizeof(EdgeEntry));
}

EdgeHash *BLI_edgehash_new(const char *info)
{
	return BLI_edgehash_new_ex(info, 0);
}

/**
 * Insert edge (\a v0, \a v1) into hash with given value, does
 * not check for duplicates.
 */
void BLI_edgehash_insert(EdgeHash *eh, uint v0, uint v1, void *val)
{
	edgehash_insert(eh, v0, v1, val);
}

/**
 * Assign a new value to a key that may already be in edgehash.
 */
bool BLI_edgehash_reinsert(EdgeHash *eh, uint v0, uint v1, void *val)
{
	IS_EDGEHASH_ASSERT(eh);

	EDGE_ORD(v0, v1);
	const uint bucket_index = edgehash_bucket_index(eh, v0, v1);

	EdgeEntry *e = edgehash_lookup_entry_ex(eh, v0, v1, bucket_index);
	if (e) {
		e->val = val;
		return false;
	}
	else {
		edgehash_insert_ex(eh, v0, v1, val, bucket_index);
		return true;
	}
}

/**
 * Return pointer to value for given edge (\a v0, \a v1),
 * or NULL if key does not exist in hash.
 */
void **BLI_edgehash_lookup_p(EdgeHash *eh, uint v0, uint v1)
{
	EdgeEntry *e = edgehash_lookup_entry(eh, v0, v1);
	IS_EDGEHASH_ASSERT(eh);
	return e ? &e->val : NULL;
}

/**
 * Ensure \a (v0, v1) is exists in \a eh.
 *
 * This handles the common situation where the caller needs ensure a key is added to \a eh,
 * constructing a new value in the case the key isn't found.
 * Otherwise use the existing value.
 *
 * Such situations typically incur multiple lookups, however this function
 * avoids them by ensuring the key is added,
 * returning a pointer to the value so it can be used or initialized by the caller.
 *
 * \returns true when the value didn't need to be added.
 * (when false, the caller _must_ initialize the value).
 */
bool BLI_edgehash_ensure_p(EdgeHash *eh, uint v0, uint v1, void ***r_val)
{
	EDGE_ORD(v0, v1);
	const uint bucket_index = edgehash_bucket_index(eh, v0, v1);
	EdgeEntry *e = edgehash_lookup_entry_ex(eh, v0, v1, bucket_index);
	const bool haskey = (e != NULL);

	if (!haskey) {
		e = BLI_mempool_alloc(eh->epool);
		edgehash_insert_ex_keyonly_entry(eh, v0, v1, bucket_index, e);
	}

	*r_val = &e->val;
	return haskey;
}

/**
 * Return value for given edge (\a v0, \a v1), or NULL if
 * if key does not exist in hash. (If need exists
 * to differentiate between key-value being NULL and
 * lack of key then see BLI_edgehash_lookup_p().
 */
void *BLI_edgehash_lookup(EdgeHash *eh, uint v0, uint v1)
{
	EdgeEntry *e = edgehash_lookup_entry(eh, v0, v1);
	IS_EDGEHASH_ASSERT(eh);
	return e ? e->val : NULL;
}

/**
 * A version of #BLI_edgehash_lookup which accepts a fallback argument.
 */
void *BLI_edgehash_lookup_default(EdgeHash *eh, uint v0, uint v1, void *val_default)
{
	EdgeEntry *e = edgehash_lookup_entry(eh, v0, v1);
	IS_EDGEHASH_ASSERT(eh);
	return e ? e->val : val_default;
}

/**
 * Remove \a key (v0, v1) from \a eh, or return false if the key wasn't found.
 *
 * \param v0, v1: The key to remove.
 * \param valfreefp  Optional callback to free the value.
 * \return true if \a key was removed from \a eh.
 */
bool BLI_edgehash_remove(EdgeHash *eh, uint v0, uint v1, EdgeHashFreeFP valfreefp)
{
	EDGE_ORD(v0, v1);
	const uint bucket_index = edgehash_bucket_index(eh, v0, v1);
	EdgeEntry *e = edgehash_remove_ex(eh, v0, v1, valfreefp, bucket_index);
	if (e) {
		BLI_mempool_free(eh->epool, e);
		return true;
	}
	else {
		return false;
	}
}

/* same as above but return the value,
 * no free value argument since it will be returned */
/**
 * Remove \a key (v0, v1) from \a eh, returning the value or NULL if the key wasn't found.
 *
 * \param v0, v1: The key to remove.
 * \return the value of \a key int \a eh or NULL.
 */
void *BLI_edgehash_popkey(EdgeHash *eh, uint v0, uint v1)
{
	EDGE_ORD(v0, v1);
	const uint bucket_index = edgehash_bucket_index(eh, v0, v1);
	EdgeEntry *e = edgehash_remove_ex(eh, v0, v1, NULL, bucket_index);
	IS_EDGEHASH_ASSERT(eh);
	if (e) {
		void *val = e->val;
		BLI_mempool_free(eh->epool, e);
		return val;
	}
	else {
		return NULL;
	}
}

/**
 * Return boolean true/false if edge (v0,v1) in hash.
 */
bool BLI_edgehash_haskey(EdgeHash *eh, uint v0, uint v1)
{
	return (edgehash_lookup_entry(eh, v0, v1) != NULL);
}

/**
 * Return number of keys in hash.
 */
int BLI_edgehash_len(EdgeHash *eh)
{
	return (int)eh->nentries;
}

/**
 * Remove all edges from hash.
 */
void BLI_edgehash_clear_ex(EdgeHash *eh, EdgeHashFreeFP valfreefp,
                           const uint nentries_reserve)
{
	if (valfreefp)
		edgehash_free_cb(eh, valfreefp);

	eh->nbuckets = _ehash_hashsizes[0];  /* eh->cursize */
	eh->nentries = 0;
	eh->cursize = 0;

	if (nentries_reserve) {
		edgehash_buckets_reserve(eh, nentries_reserve);
	}

	MEM_freeN(eh->buckets);
	eh->buckets = MEM_callocN(eh->nbuckets * sizeof(*eh->buckets), "eh buckets");

	BLI_mempool_clear_ex(eh->epool, nentries_reserve ? (int)nentries_reserve : -1);
}

/**
 * Wraps #BLI_edgehash_clear_ex with zero entries reserved.
 */
void BLI_edgehash_clear(EdgeHash *eh, EdgeHashFreeFP valfreefp)
{
	BLI_edgehash_clear_ex(eh, valfreefp, 0);
}

void BLI_edgehash_free(EdgeHash *eh, EdgeHashFreeFP valfreefp)
{
	BLI_assert((int)eh->nentries == BLI_mempool_len(eh->epool));

	if (valfreefp)
		edgehash_free_cb(eh, valfreefp);

	BLI_mempool_destroy(eh->epool);

	MEM_freeN(eh->buckets);
	MEM_freeN(eh);
}


void BLI_edgehash_flag_set(EdgeHash *eh, uint flag)
{
	eh->flag |= flag;
}

void BLI_edgehash_flag_clear(EdgeHash *eh, uint flag)
{
	eh->flag &= ~flag;
}

/** \} */


/* -------------------------------------------------------------------- */
/* EdgeHash Iterator API */

/** \name Iterator API
 * \{ */

/**
 * Create a new EdgeHashIterator. The hash table must not be mutated
 * while the iterator is in use, and the iterator will step exactly
 * BLI_edgehash_len(eh) times before becoming done.
 */
EdgeHashIterator *BLI_edgehashIterator_new(EdgeHash *eh)
{
	EdgeHashIterator *ehi = MEM_mallocN(sizeof(*ehi), "eh iter");
	BLI_edgehashIterator_init(ehi, eh);
	return ehi;
}

/**
 * Init an already allocated EdgeHashIterator. The hash table must not
 * be mutated while the iterator is in use, and the iterator will
 * step exactly BLI_edgehash_len(eh) times before becoming done.
 *
 * \param ehi The EdgeHashIterator to initialize.
 * \param eh The EdgeHash to iterate over.
 */
void BLI_edgehashIterator_init(EdgeHashIterator *ehi, EdgeHash *eh)
{
	ehi->eh = eh;
	ehi->curEntry = NULL;
	ehi->curBucket = UINT_MAX;  /* wraps to zero */
	if (eh->nentries) {
		do {
			ehi->curBucket++;
			if (UNLIKELY(ehi->curBucket == ehi->eh->nbuckets)) {
				break;
			}

			ehi->curEntry = ehi->eh->buckets[ehi->curBucket];
		} while (!ehi->curEntry);
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
			if (UNLIKELY(ehi->curBucket == ehi->eh->nbuckets)) {
				break;
			}

			ehi->curEntry = ehi->eh->buckets[ehi->curBucket];
		}
	}
}

/**
 * Free an EdgeHashIterator.
 */
void BLI_edgehashIterator_free(EdgeHashIterator *ehi)
{
	MEM_freeN(ehi);
}

/* inline functions now */
#if 0
/**
 * Retrieve the key from an iterator.
 */
void BLI_edgehashIterator_getKey(EdgeHashIterator *ehi, uint *r_v0, uint *r_v1)
{
	*r_v0 = ehi->curEntry->v0;
	*r_v1 = ehi->curEntry->v1;
}

/**
 * Retrieve the value from an iterator.
 */
void *BLI_edgehashIterator_getValue(EdgeHashIterator *ehi)
{
	return ehi->curEntry->val;
}

/**
 * Retrieve the pointer to the value from an iterator.
 */
void **BLI_edgehashIterator_getValue_p(EdgeHashIterator *ehi)
{
	return &ehi->curEntry->val;
}

/**
 * Set the value for an iterator.
 */
void BLI_edgehashIterator_setValue(EdgeHashIterator *ehi, void *val)
{
	ehi->curEntry->val = val;
}

/**
 * Determine if an iterator is done.
 */
bool BLI_edgehashIterator_isDone(EdgeHashIterator *ehi)
{
	return (ehi->curEntry == NULL);
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/* EdgeSet API */

/* Use edgehash API to give 'set' functionality */

/** \name EdgeSet Functions
 * \{ */
EdgeSet *BLI_edgeset_new_ex(const char *info,
                                  const uint nentries_reserve)
{
	EdgeSet *es = (EdgeSet *)edgehash_new(info,
	                                      nentries_reserve,
	                                      sizeof(EdgeEntry) - sizeof(void *));
#ifndef NDEBUG
	((EdgeHash *)es)->flag |= EDGEHASH_FLAG_IS_SET;
#endif
	return es;
}

EdgeSet *BLI_edgeset_new(const char *info)
{
	return BLI_edgeset_new_ex(info, 0);
}

int BLI_edgeset_len(EdgeSet *es)
{
	return (int)((EdgeHash *)es)->nentries;
}

/**
 * Adds the key to the set (no checks for unique keys!).
 * Matching #BLI_edgehash_insert
 */
void BLI_edgeset_insert(EdgeSet *es, uint v0, uint v1)
{
	EDGE_ORD(v0, v1);
	const uint bucket_index = edgehash_bucket_index((EdgeHash *)es, v0, v1);
	edgehash_insert_ex_keyonly((EdgeHash *)es, v0, v1, bucket_index);
}

/**
 * A version of BLI_edgeset_insert which checks first if the key is in the set.
 * \returns true if a new key has been added.
 *
 * \note EdgeHash has no equivalent to this because typically the value would be different.
 */
bool BLI_edgeset_add(EdgeSet *es, uint v0, uint v1)
{
	EDGE_ORD(v0, v1);
	const uint bucket_index = edgehash_bucket_index((EdgeHash *)es, v0, v1);

	EdgeEntry *e = edgehash_lookup_entry_ex((EdgeHash *)es, v0, v1, bucket_index);
	if (e) {
		return false;
	}
	else {
		edgehash_insert_ex_keyonly((EdgeHash *)es, v0, v1, bucket_index);
		return true;
	}
}

bool BLI_edgeset_haskey(EdgeSet *es, uint v0, uint v1)
{
	return (edgehash_lookup_entry((EdgeHash *)es, v0, v1) != NULL);
}


void BLI_edgeset_free(EdgeSet *es)
{
	BLI_edgehash_free((EdgeHash *)es, NULL);
}

void BLI_edgeset_flag_set(EdgeSet *es, uint flag)
{
	((EdgeHash *)es)->flag |= flag;
}

void BLI_edgeset_flag_clear(EdgeSet *es, uint flag)
{
	((EdgeHash *)es)->flag &= ~flag;
}

/** \} */

/** \name Debugging & Introspection
 * \{ */
#ifdef DEBUG

/**
 * Measure how well the hash function performs
 * (1.0 is approx as good as random distribution).
 */
double BLI_edgehash_calc_quality(EdgeHash *eh)
{
	uint64_t sum = 0;
	uint i;

	if (eh->nentries == 0)
		return -1.0;

	for (i = 0; i < eh->nbuckets; i++) {
		uint64_t count = 0;
		EdgeEntry *e;
		for (e = eh->buckets[i]; e; e = e->next) {
			count += 1;
		}
		sum += count * (count + 1);
	}
	return ((double)sum * (double)eh->nbuckets /
	        ((double)eh->nentries * (eh->nentries + 2 * eh->nbuckets - 1)));
}
double BLI_edgeset_calc_quality(EdgeSet *es)
{
	return BLI_edgehash_calc_quality((EdgeHash *)es);
}

#endif
/** \} */
