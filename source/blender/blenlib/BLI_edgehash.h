/**
 * A general unordered 2-int pair hash table ADT
 * 
 * $Id$
 *
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
 * Contributor(s): Daniel Dunbar
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef BLI_EDGEHASH_H
#define BLI_EDGEHASH_H

#include "MEM_guardedalloc.h"
#include "BKE_utildefines.h"
#include "BLI_mempool.h"

struct EdgeHash;
struct EdgeHashIterator;
typedef struct EdgeHash EdgeHash;
typedef struct EdgeHashIterator EdgeHashIterator;

typedef	void	(*EdgeHashFreeFP)(void *key);

EdgeHash*		BLI_edgehash_new		(void);
void			BLI_edgehash_free		(EdgeHash *eh, EdgeHashFreeFP valfreefp);

	/* Insert edge (v0,v1) into hash with given value, does
	 * not check for duplicates.
	 */
//void			BLI_edgehash_insert		(EdgeHash *eh, int v0, int v1, void *val);

	/* Return value for given edge (v0,v1), or NULL if
	 * if key does not exist in hash. (If need exists 
	 * to differentiate between key-value being NULL and 
	 * lack of key then see BLI_edgehash_lookup_p().
	 */
//void*			BLI_edgehash_lookup		(EdgeHash *eh, int v0, int v1);

	/* Return pointer to value for given edge (v0,v1),
	 * or NULL if key does not exist in hash.
	 */
//void**			BLI_edgehash_lookup_p	(EdgeHash *eh, int v0, int v1);

	/* Return boolean true/false if edge (v0,v1) in hash. */
//int				BLI_edgehash_haskey		(EdgeHash *eh, int v0, int v1);

	/* Return number of keys in hash. */
int				BLI_edgehash_size		(EdgeHash *eh);

	/* Remove all edges from hash. */
void			BLI_edgehash_clear		(EdgeHash *eh, EdgeHashFreeFP valfreefp);

/***/

	/**
	 * Create a new EdgeHashIterator. The hash table must not be mutated
	 * while the iterator is in use, and the iterator will step exactly
	 * BLI_edgehash_size(gh) times before becoming done.
	 */
EdgeHashIterator*	BLI_edgehashIterator_new		(EdgeHash *eh);

	/* Free an EdgeHashIterator. */
void				BLI_edgehashIterator_free		(EdgeHashIterator *ehi);

	/* Retrieve the key from an iterator. */
void 				BLI_edgehashIterator_getKey		(EdgeHashIterator *ehi, int *v0_r, int *v1_r);
	
	/* Retrieve the value from an iterator. */
void*				BLI_edgehashIterator_getValue	(EdgeHashIterator *ehi);

	/* Set the value for an iterator. */
void				BLI_edgehashIterator_setValue	(EdgeHashIterator *ehi, void *val);

	/* Steps the iterator to the next index. */
void				BLI_edgehashIterator_step		(EdgeHashIterator *ehi);

	/* Determine if an iterator is done. */
int					BLI_edgehashIterator_isDone		(EdgeHashIterator *ehi);

/**************inlined code************/
static unsigned int _ehash_hashsizes[]= {
	1, 3, 5, 11, 17, 37, 67, 131, 257, 521, 1031, 2053, 4099, 8209, 
	16411, 32771, 65537, 131101, 262147, 524309, 1048583, 2097169, 
	4194319, 8388617, 16777259, 33554467, 67108879, 134217757, 
	268435459
};

#define EDGEHASH(v0,v1)		((v0*39)^(v1*31))

/***/

typedef struct EdgeEntry EdgeEntry;
struct EdgeEntry {
	EdgeEntry *next;
	int v0, v1;
	void *val;
};

struct EdgeHash {
	EdgeEntry **buckets;
	BLI_mempool *epool;
	int nbuckets, nentries, cursize;
};


BM_INLINE void BLI_edgehash_insert(EdgeHash *eh, int v0, int v1, void *val) {
	unsigned int hash;
	EdgeEntry *e= BLI_mempool_alloc(eh->epool);

	if (v1<v0) {
		v0 ^= v1;
		v1 ^= v0;
		v0 ^= v1;
	}
 	hash = EDGEHASH(v0,v1)%eh->nbuckets;

	e->v0 = v0;
	e->v1 = v1;
	e->val = val;
	e->next= eh->buckets[hash];
	eh->buckets[hash]= e;
	
	if (++eh->nentries>eh->nbuckets*3) {
		EdgeEntry *e, **old= eh->buckets;
		int i, nold= eh->nbuckets;
		
		eh->nbuckets= _ehash_hashsizes[++eh->cursize];
		eh->buckets= MEM_mallocN(eh->nbuckets*sizeof(*eh->buckets), "eh buckets");
		BMEMSET(eh->buckets, 0, eh->nbuckets*sizeof(*eh->buckets));
		
		for (i=0; i<nold; i++) {
			for (e= old[i]; e;) {
				EdgeEntry *n= e->next;
				
				hash= EDGEHASH(e->v0,e->v1)%eh->nbuckets;
				e->next= eh->buckets[hash];
				eh->buckets[hash]= e;
				
				e= n;
			}
		}
		
		MEM_freeN(old);
	}
}

BM_INLINE void** BLI_edgehash_lookup_p(EdgeHash *eh, int v0, int v1) {
	unsigned int hash;
	EdgeEntry *e;

	if (v1<v0) {
		v0 ^= v1;
		v1 ^= v0;
		v0 ^= v1;
	}
	hash = EDGEHASH(v0,v1)%eh->nbuckets;
	for (e= eh->buckets[hash]; e; e= e->next)
		if (v0==e->v0 && v1==e->v1)
			return &e->val;
	
	return NULL;
}

BM_INLINE void* BLI_edgehash_lookup(EdgeHash *eh, int v0, int v1) {
	void **value_p = BLI_edgehash_lookup_p(eh,v0,v1);

	return value_p?*value_p:NULL;
}

BM_INLINE int BLI_edgehash_haskey(EdgeHash *eh, int v0, int v1) {
	return BLI_edgehash_lookup_p(eh, v0, v1)!=NULL;
}

#endif

