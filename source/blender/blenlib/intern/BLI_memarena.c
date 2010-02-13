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
 * Efficient memory allocation for lots of similar small chunks.
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_memarena.h"
#include "BLI_linklist.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

struct MemArena {
	unsigned char *curbuf;
	int bufsize, cursize;
	
	int use_calloc;	
	int align;
	
	LinkNode *bufs;
};

MemArena *BLI_memarena_new(int bufsize) {
	MemArena *ma= MEM_callocN(sizeof(*ma), "memarena");
	ma->bufsize= bufsize;
	ma->align = 8;
	
	return ma;
}

void BLI_memarena_use_calloc(MemArena *ma) {
	ma->use_calloc= 1;
}

void BLI_memarena_use_malloc(MemArena *ma) {
	ma->use_calloc= 0;
}

void BLI_memarena_use_align(struct MemArena *ma, int align) {
	/* align should be a power of two */
	ma->align = align;
}

void BLI_memarena_free(MemArena *ma) {
	BLI_linklist_free(ma->bufs, (void(*)(void*)) MEM_freeN);
	MEM_freeN(ma);
}

	/* amt must be power of two */
#define PADUP(num, amt)	((num+(amt-1))&~(amt-1))

void *BLI_memarena_alloc(MemArena *ma, int size) {
	void *ptr;

		/* ensure proper alignment by rounding
		 * size up to multiple of 8 */	
	size= PADUP(size, ma->align);
	
	if (size>ma->cursize) {
		unsigned char *tmp;
		
		if(size > ma->bufsize - (ma->align - 1))
		{
			ma->cursize = PADUP(size+1, ma->align);
		}
		else ma->cursize = ma->bufsize;

		if(ma->use_calloc)
			ma->curbuf= MEM_callocN(ma->cursize, "memarena calloc");
		else
			ma->curbuf= MEM_mallocN(ma->cursize, "memarena malloc");
		
		BLI_linklist_prepend(&ma->bufs, ma->curbuf);

		/* align alloc'ed memory (needed if align > 8) */
		tmp = (unsigned char*)PADUP( (intptr_t) ma->curbuf, ma->align);
		ma->cursize -= (tmp - ma->curbuf);
		ma->curbuf = tmp;		
	}
	
	ptr= ma->curbuf;
	ma->curbuf+= size;
	ma->cursize-= size;
	
	return ptr;
}

