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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef __BLI_MEMPOOL_H__
#define __BLI_MEMPOOL_H__

/** \file BLI_mempool.h
 *  \ingroup bli
 *  \author Geoffrey Bantle
 *  \brief Simple fast memory allocator.
 */

#ifdef __cplusplus
extern "C"
{
#endif

struct BLI_mempool;
struct BLI_mempool_chunk;

typedef struct BLI_mempool BLI_mempool;

/* allow_iter allows iteration on this mempool.  note: this requires that the
 * first four bytes of the elements never contain the character string
 * 'free'.  use with care.*/

BLI_mempool *BLI_mempool_create(int esize, int tote, int pchunk, int flag);
void *BLI_mempool_alloc(BLI_mempool *pool);
void *BLI_mempool_calloc(BLI_mempool *pool);
void  BLI_mempool_free(BLI_mempool *pool, void *addr);
void  BLI_mempool_destroy(BLI_mempool *pool);
int   BLI_mempool_count(BLI_mempool *pool);
void *BLI_mempool_findelem(BLI_mempool *pool, int index);

/** iteration stuff.  note: this may easy to produce bugs with **/
/*private structure*/
typedef struct BLI_mempool_iter {
	BLI_mempool *pool;
	struct BLI_mempool_chunk *curchunk;
	int curindex;
} BLI_mempool_iter;

/* flag */
enum {
	BLI_MEMPOOL_SYSMALLOC  = (1 << 0),
	BLI_MEMPOOL_ALLOW_ITER = (1 << 1)
};

void  BLI_mempool_iternew(BLI_mempool *pool, BLI_mempool_iter *iter);
void *BLI_mempool_iterstep(BLI_mempool_iter *iter);

#ifdef __cplusplus
}
#endif

#endif
