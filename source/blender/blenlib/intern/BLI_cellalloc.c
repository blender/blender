/**
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
 * The Original Code is Copyright (C) 2008 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/*
	Simple, fast memory allocator that uses many BLI_mempools for allocation.
	this is meant to be used by lots of relatively small objects.
*/

#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"

#include "DNA_listBase.h"
#include "BLI_mempool.h"

#include <string.h> 

static BLI_mempool **pools;
static int totpool = 0;
ListBase active_mem = {NULL, NULL};
static int celltotblock = 0;

#define MEMIDCHECK	('a' | ('b' << 4) | ('c' << 8) | ('d' << 12))

typedef struct MemHeader {
	struct MemHeader *next, *prev;

	int size;
	char *tag;
	int idcheck;
} MemHeader;

void *BLI_cellalloc_malloc(long size, char *tag)
{
	MemHeader *memh;
	int slot = size + sizeof(MemHeader);

	if (!slot) 
		return NULL;

	if (slot >= totpool) {
		void *tmp;

		tmp = calloc(1, sizeof(void*)*(slot+1));
		if (pools) {
			memcpy(tmp, pools, totpool*sizeof(void*));
		}

		pools = tmp;
		totpool = slot+1;
	}

	if (!pools[slot]) {
		pools[slot] = BLI_mempool_create(slot, 1, 128);
	}

	memh = BLI_mempool_alloc(pools[slot]);
	memh->size = size;
	memh->idcheck = MEMIDCHECK;
	memh->tag = tag;
	BLI_addtail(&active_mem, memh);
	celltotblock++;

	return memh + 1;
}

void *BLI_cellalloc_calloc(long size, char *tag)
{
	void *mem = BLI_cellalloc_malloc(size, tag);
	BMEMSET(mem, 0, size);

	return mem;
}

void BLI_cellalloc_free(void *mem)
{
	MemHeader *memh = mem;
	
	if (!memh)
		return;

	memh--;
	if (memh->idcheck != MEMIDCHECK) {
		printf("Error in BLI_cellalloc: attempt to free invalid block.\n");
		return;
	}
	
	if (memh->size > 0 && memh->size+sizeof(MemHeader) < totpool) {
		BLI_remlink(&active_mem, memh);
		BLI_mempool_free(pools[memh->size+sizeof(MemHeader)], memh);
		celltotblock--;
	} else {
		printf("Error in BLI_cellalloc: attempt to free corrupted block.\n");
	}
}

void BLI_cellalloc_printleaks(void)
{
	MemHeader *memh;

	if (!active_mem.first) return;

	for (memh=active_mem.first; memh; memh=memh->next) {
		printf("%s %d %p\n", memh->tag, memh->size, memh+1);
	}
}

int BLI_cellalloc_get_totblock(void)
{
	return celltotblock;
}

void BLI_cellalloc_destroy(void)
{
	int i;

	for (i=0; i<totpool; i++) {
		if (pools[i]) {
			BLI_mempool_destroy(pools[i]);
			pools[i] = NULL;
		}
	}
}