/**
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
 * A general (pointer -> pointer) hash table ADT
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "BLI_edgehash.h"

/***/

static unsigned int hashsizes[]= {
	1, 3, 5, 11, 17, 37, 67, 131, 257, 521, 1031, 2053, 4099, 8209, 
	16411, 32771, 65537, 131101, 262147, 524309, 1048583, 2097169, 
	4194319, 8388617, 16777259, 33554467, 67108879, 134217757, 
	268435459
};

#define EDGEHASH(v0,v1)		((v0*39)^(v1*31))

/***/

typedef struct Entry Entry;
struct Entry {
	Entry *next;
	int v0, v1;
	void *val;
};

struct EdgeHash {
	Entry **buckets;
	int nbuckets, nentries, cursize;
};

/***/

EdgeHash *BLI_edgehash_new(void) {
	EdgeHash *eh= MEM_mallocN(sizeof(*eh), "EdgeHash");
	eh->cursize= 0;
	eh->nentries= 0;
	eh->nbuckets= hashsizes[eh->cursize];
	
	eh->buckets= malloc(eh->nbuckets*sizeof(*eh->buckets));
	memset(eh->buckets, 0, eh->nbuckets*sizeof(*eh->buckets));
	
	return eh;
}

void BLI_edgehash_insert(EdgeHash *eh, int v0, int v1, void *val) {
	unsigned int hash;
	Entry *e= malloc(sizeof(*e));

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
		Entry *e, **old= eh->buckets;
		int i, nold= eh->nbuckets;
		
		eh->nbuckets= hashsizes[++eh->cursize];
		eh->buckets= malloc(eh->nbuckets*sizeof(*eh->buckets));
		memset(eh->buckets, 0, eh->nbuckets*sizeof(*eh->buckets));
		
		for (i=0; i<nold; i++) {
			for (e= old[i]; e;) {
				Entry *n= e->next;
				
				hash= EDGEHASH(e->v0,e->v1)%eh->nbuckets;
				e->next= eh->buckets[hash];
				eh->buckets[hash]= e;
				
				e= n;
			}
		}
		
		free(old);
	}
}

void** BLI_edgehash_lookup_p(EdgeHash *eh, int v0, int v1) {
	unsigned int hash;
	Entry *e;

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

void* BLI_edgehash_lookup(EdgeHash *eh, int v0, int v1) {
	void **value_p = BLI_edgehash_lookup_p(eh,v0,v1);

	return value_p?*value_p:NULL;
}

int BLI_edgehash_haskey(EdgeHash *eh, int v0, int v1) {
	return BLI_edgehash_lookup_p(eh, v0, v1)!=NULL;
}

int BLI_edgehash_size(EdgeHash *eh) {
	return eh->nentries;
}

void BLI_edgehash_clear(EdgeHash *eh, EdgeHashFreeFP valfreefp) {
	int i;
	
	for (i=0; i<eh->nbuckets; i++) {
		Entry *e;
		
		for (e= eh->buckets[i]; e; ) {
			Entry *n= e->next;
			
			if (valfreefp) valfreefp(e->val);
			free(e);
			
			e= n;
		}
		eh->buckets[i]= NULL;
	}

	eh->nentries= 0;
}

void BLI_edgehash_free(EdgeHash *eh, EdgeHashFreeFP valfreefp) {
	BLI_edgehash_clear(eh, valfreefp);
	
	free(eh->buckets);
	MEM_freeN(eh);
}


/***/

struct EdgeHashIterator {
	EdgeHash *eh;
	int curBucket;
	Entry *curEntry;
};

EdgeHashIterator *BLI_edgehashIterator_new(EdgeHash *eh) {
	EdgeHashIterator *ehi= malloc(sizeof(*ehi));
	ehi->eh= eh;
	ehi->curEntry= NULL;
	ehi->curBucket= -1;
	while (!ehi->curEntry) {
		ehi->curBucket++;
		if (ehi->curBucket==ehi->eh->nbuckets)
			break;
		ehi->curEntry= ehi->eh->buckets[ehi->curBucket];
	}
	return ehi;
}
void BLI_edgehashIterator_free(EdgeHashIterator *ehi) {
	free(ehi);
}

void BLI_edgehashIterator_getKey(EdgeHashIterator *ehi, int *v0_r, int *v1_r) {
	if (ehi->curEntry) {
		*v0_r = ehi->curEntry->v0;
		*v1_r = ehi->curEntry->v1;
	}
}
void *BLI_edgehashIterator_getValue(EdgeHashIterator *ehi) {
	return ehi->curEntry?ehi->curEntry->val:NULL;
}

void BLI_edgehashIterator_setValue(EdgeHashIterator *ehi, void *val) {
	if(ehi->curEntry)
		ehi->curEntry->val= val;
}

void BLI_edgehashIterator_step(EdgeHashIterator *ehi) {
	if (ehi->curEntry) {
        ehi->curEntry= ehi->curEntry->next;
		while (!ehi->curEntry) {
			ehi->curBucket++;
			if (ehi->curBucket==ehi->eh->nbuckets)
				break;
			ehi->curEntry= ehi->eh->buckets[ehi->curBucket];
		}
	}
}
int BLI_edgehashIterator_isDone(EdgeHashIterator *ehi) {
	return !ehi->curEntry;
}

