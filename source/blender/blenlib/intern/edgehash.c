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
 * Contributor(s): Daniel Dunbar, Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 * A general (pointer -> pointer) hash table ADT
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "BLI_edgehash.h"
#include "BLI_mempool.h"

/***/

EdgeHash *BLI_edgehash_new(void) {
	EdgeHash *eh= MEM_callocN(sizeof(*eh), "EdgeHash");
	eh->cursize= 0;
	eh->nentries= 0;
	eh->nbuckets= _ehash_hashsizes[eh->cursize];
	
	eh->buckets= MEM_callocN(eh->nbuckets*sizeof(*eh->buckets), "eh buckets 2");
	eh->epool = BLI_mempool_create(sizeof(EdgeEntry), 512, 512, 1);

	return eh;
}

int BLI_edgehash_size(EdgeHash *eh) {
	return eh->nentries;
}

void BLI_edgehash_clear(EdgeHash *eh, EdgeHashFreeFP valfreefp) {
	int i;
	
	for (i=0; i<eh->nbuckets; i++) {
		EdgeEntry *e;
		
		for (e= eh->buckets[i]; e; ) {
			EdgeEntry *n= e->next;
			
			if (valfreefp) valfreefp(e->val);
			BLI_mempool_free(eh->epool, e);
			
			e= n;
		}
		eh->buckets[i]= NULL;
	}

	eh->nentries= 0;
}

void BLI_edgehash_free(EdgeHash *eh, EdgeHashFreeFP valfreefp) {
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

EdgeHashIterator *BLI_edgehashIterator_new(EdgeHash *eh) {
	EdgeHashIterator *ehi= MEM_mallocN(sizeof(*ehi), "eh iter");
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
	MEM_freeN(ehi);
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

