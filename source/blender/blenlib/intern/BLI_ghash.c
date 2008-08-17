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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "BLI_ghash.h"

#include "BLO_sys_types.h" // for intptr_t support

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/***/

static unsigned int hashsizes[]= {
	1, 3, 5, 11, 17, 37, 67, 131, 257, 521, 1031, 2053, 4099, 8209, 
	16411, 32771, 65537, 131101, 262147, 524309, 1048583, 2097169, 
	4194319, 8388617, 16777259, 33554467, 67108879, 134217757, 
	268435459
};

/***/

typedef struct Entry Entry;
struct Entry {
	Entry *next;
	
	void *key, *val;
};

struct GHash {
	GHashHashFP	hashfp;
	GHashCmpFP	cmpfp;
	
	Entry **buckets;
	int nbuckets, nentries, cursize;
};

/***/

GHash *BLI_ghash_new(GHashHashFP hashfp, GHashCmpFP cmpfp) {
	GHash *gh= MEM_mallocN(sizeof(*gh), "GHash");
	gh->hashfp= hashfp;
	gh->cmpfp= cmpfp;
	
	gh->cursize= 0;
	gh->nentries= 0;
	gh->nbuckets= hashsizes[gh->cursize];
	
	gh->buckets= malloc(gh->nbuckets*sizeof(*gh->buckets));
	memset(gh->buckets, 0, gh->nbuckets*sizeof(*gh->buckets));
	
	return gh;
}

void BLI_ghash_insert(GHash *gh, void *key, void *val) {
	unsigned int hash= gh->hashfp(key)%gh->nbuckets;
	Entry *e= malloc(sizeof(*e));

	e->key= key;
	e->val= val;
	e->next= gh->buckets[hash];
	gh->buckets[hash]= e;
	
	if (++gh->nentries>gh->nbuckets*3) {
		Entry *e, **old= gh->buckets;
		int i, nold= gh->nbuckets;
		
		gh->nbuckets= hashsizes[++gh->cursize];
		gh->buckets= malloc(gh->nbuckets*sizeof(*gh->buckets));
		memset(gh->buckets, 0, gh->nbuckets*sizeof(*gh->buckets));
		
		for (i=0; i<nold; i++) {
			for (e= old[i]; e;) {
				Entry *n= e->next;
				
				hash= gh->hashfp(e->key)%gh->nbuckets;
				e->next= gh->buckets[hash];
				gh->buckets[hash]= e;
				
				e= n;
			}
		}
		
		free(old);
	}
}

void* BLI_ghash_lookup(GHash *gh, void *key) 
{
	if(gh) {
		unsigned int hash= gh->hashfp(key)%gh->nbuckets;
		Entry *e;
		
		for (e= gh->buckets[hash]; e; e= e->next)
			if (gh->cmpfp(key, e->key)==0)
				return e->val;
	}	
	return NULL;
}

int BLI_ghash_remove (GHash *gh, void *key, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp)
{
	unsigned int hash= gh->hashfp(key)%gh->nbuckets;
	Entry *e;
	Entry *p = 0;

	for (e= gh->buckets[hash]; e; e= e->next) {
		if (gh->cmpfp(key, e->key)==0) {
			Entry *n= e->next;

			if (keyfreefp) keyfreefp(e->key);
			if (valfreefp) valfreefp(e->val);
			free(e);


			e= n;
			if (p)
				p->next = n;
			else
				gh->buckets[hash] = n;

			--gh->nentries;
			return 1;
		}
		p = e;
	}
 
	return 0;
}

int BLI_ghash_haskey(GHash *gh, void *key) {
	unsigned int hash= gh->hashfp(key)%gh->nbuckets;
	Entry *e;
	
	for (e= gh->buckets[hash]; e; e= e->next)
		if (gh->cmpfp(key, e->key)==0)
			return 1;
	
	return 0;
}

int BLI_ghash_size(GHash *gh) {
	return gh->nentries;
}

void BLI_ghash_free(GHash *gh, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp) {
	int i;
	
	for (i=0; i<gh->nbuckets; i++) {
		Entry *e;
		
		for (e= gh->buckets[i]; e; ) {
			Entry *n= e->next;
			
			if (keyfreefp) keyfreefp(e->key);
			if (valfreefp) valfreefp(e->val);
			free(e);
			
			e= n;
		}
	}
	
	free(gh->buckets);
	gh->buckets = 0;
	gh->nentries = 0;
	gh->nbuckets = 0;
	MEM_freeN(gh);
}

/***/

struct GHashIterator {
	GHash *gh;
	int curBucket;
	Entry *curEntry;
};

GHashIterator *BLI_ghashIterator_new(GHash *gh) {
	GHashIterator *ghi= malloc(sizeof(*ghi));
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
void BLI_ghashIterator_free(GHashIterator *ghi) {
	free(ghi);
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
	return (unsigned int) key;
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
