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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * A general (pointer -> pointer) hash table ADT
 */


#include "BLI_ghash.h"
#include "BLO_sys_types.h" // for intptr_t support

/***/

unsigned int hashsizes[]= {
	5, 11, 17, 37, 67, 131, 257, 521, 1031, 2053, 4099, 8209, 
	16411, 32771, 65537, 131101, 262147, 524309, 1048583, 2097169, 
	4194319, 8388617, 16777259, 33554467, 67108879, 134217757, 
	268435459
};

/***/

/***/

GHash *BLI_ghash_new(GHashHashFP hashfp, GHashCmpFP cmpfp) {
	GHash *gh= MEM_mallocN(sizeof(*gh), "GHash");
	gh->hashfp= hashfp;
	gh->cmpfp= cmpfp;
	gh->entrypool = BLI_mempool_create(sizeof(Entry), 64, 64, 0);

	gh->cursize= 0;
	gh->nentries= 0;
	gh->nbuckets= hashsizes[gh->cursize];
	
	gh->buckets= MEM_mallocN(gh->nbuckets*sizeof(*gh->buckets), "buckets");
	memset(gh->buckets, 0, gh->nbuckets*sizeof(*gh->buckets));
	
	return gh;
}

#ifdef BLI_ghash_insert
#undef BLI_ghash_insert
#endif

int BLI_ghash_size(GHash *gh) {
	return gh->nentries;
}

void BLI_ghash_free(GHash *gh, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp) {
	int i;
	
	if (keyfreefp || valfreefp) {
		for (i=0; i<gh->nbuckets; i++) {
			Entry *e;
			
			for (e= gh->buckets[i]; e; ) {
				Entry *n= e->next;
				
				if (keyfreefp) keyfreefp(e->key);
				if (valfreefp) valfreefp(e->val);

				e= n;
			}
		}
	}
	
	MEM_freeN(gh->buckets);
	BLI_mempool_destroy(gh->entrypool);
	gh->buckets = 0;
	gh->nentries = 0;
	gh->nbuckets = 0;
	MEM_freeN(gh);
}

/***/

GHashIterator *BLI_ghashIterator_new(GHash *gh) {
	GHashIterator *ghi= MEM_mallocN(sizeof(*ghi), "ghash iterator");
	ghi->gh= gh;
	ghi->curEntry= NULL;
	ghi->curBucket= -1;
	while (!ghi->curEntry) {
		ghi->curBucket++;
		if (ghi->curBucket==ghi->gh->nbuckets)
			break;
		ghi->curEntry= ghi->gh->buckets[ghi->curBucket];
	}
	return ghi;
}
void BLI_ghashIterator_init(GHashIterator *ghi, GHash *gh) {
	ghi->gh= gh;
	ghi->curEntry= NULL;
	ghi->curBucket= -1;
	while (!ghi->curEntry) {
		ghi->curBucket++;
		if (ghi->curBucket==ghi->gh->nbuckets)
			break;
		ghi->curEntry= ghi->gh->buckets[ghi->curBucket];
	}
}
void BLI_ghashIterator_free(GHashIterator *ghi) {
	MEM_freeN(ghi);
}

void *BLI_ghashIterator_getKey(GHashIterator *ghi) {
	return ghi->curEntry?ghi->curEntry->key:NULL;
}
void *BLI_ghashIterator_getValue(GHashIterator *ghi) {
	return ghi->curEntry?ghi->curEntry->val:NULL;
}

void BLI_ghashIterator_step(GHashIterator *ghi) {
	if (ghi->curEntry) {
		ghi->curEntry= ghi->curEntry->next;
		while (!ghi->curEntry) {
			ghi->curBucket++;
			if (ghi->curBucket==ghi->gh->nbuckets)
				break;
			ghi->curEntry= ghi->gh->buckets[ghi->curBucket];
		}
	}
}
int BLI_ghashIterator_isDone(GHashIterator *ghi) {
	return !ghi->curEntry;
}

/***/

unsigned int BLI_ghashutil_ptrhash(void *key) {
	return (unsigned int)(intptr_t)key;
}
int BLI_ghashutil_ptrcmp(void *a, void *b) {
	if (a==b)
		return 0;
	else
		return (a<b)?-1:1;
}

unsigned int BLI_ghashutil_inthash(void *ptr) {
	uintptr_t key = (uintptr_t)ptr;

	key += ~(key << 16);
	key ^=  (key >>  5);
	key +=  (key <<  3);
	key ^=  (key >> 13);
	key += ~(key <<  9);
	key ^=  (key >> 17);

	  return (unsigned int)(key & 0xffffffff);
}

int BLI_ghashutil_intcmp(void *a, void *b) {
	if (a==b)
		return 0;
	else
		return (a<b)?-1:1;
}

unsigned int BLI_ghashutil_strhash(void *ptr) {
	char *s= ptr;
	unsigned int i= 0;
	unsigned char c;
	
	while ( (c= *s++) )
		i= i*37 + c;
		
	return i;
}
int BLI_ghashutil_strcmp(void *a, void *b) {
	return strcmp(a, b);
}
