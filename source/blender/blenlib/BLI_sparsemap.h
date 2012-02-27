#if 0
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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * Contributor(s): Joseph Eagar (original author)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_SPARSEMAP_H__
#define __BLI_SPARSEMAP_H__

/** \file BLI_sparsemap.h
 *  \ingroup bli
 */

#include "BLI_math_inline.h"

typedef struct SparseMap {
	int max;
	int blocksize;
	void **blocks;
	int totblock;
} SparseMap;

MALWAYS_INLINE SparseMap *BLI_sparsemap_new(int blocksize, char *name)
{
	SparseMap *sm = MEM_callocN(sizeof(SparseMap), name);

	sm->blocksize = blocksize;
	return sm;
}

MALWAYS_INLINE void BLI_sparsemap_free(SparseMap *sm)
{
	if (sm->blocks)
		MEM_freeN(sm->blocks);
	
	MEM_freeN(sm);
}

MALWAYS_INLINE void BLI_sparsemap_set(SparseMap *sm, int index, void *ptr)
{
	if (index >= sm->max || (sm->blocks && !sm->blocks[index/sm->blocksize])) {
		int totblock = MAX2((index+1)/sm->blocksize, 2);
		void *blocks = MEM_callocN(sizeof(void*)*totblock);
		
		if (sm->blocks)
			memcpy(blocks, sm->blocks, sizeof(void*)*sm->totblock);
		sm->totblock = totblock;
		MEM_freeN(sm->blocks);
		sm->blocks = blocks;
	}
	
	if (!sm->blocks[index/sm->blocksize]) {
		sm->blocks[index/sm->blocksize] = MEM_mallocN(sizeof(void*)*sm->blocksize);
	}
	
	sm->blocks[index/sm->blocksize] = ptr;
}

#endif /* __BLI_SPARSEMAP_H__ */

#endif
