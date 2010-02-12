/**
 * Simple fast memory allocator
 * 
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef BLI_MEMPOOL_H
#define BLI_MEMPOOL_H

struct BLI_mempool;
typedef struct BLI_mempool BLI_mempool;

BLI_mempool *BLI_mempool_create(int esize, int tote, int pchunk, int use_sysmalloc);
void *BLI_mempool_alloc(BLI_mempool *pool);
void *BLI_mempool_calloc(BLI_mempool *pool);
void BLI_mempool_free(BLI_mempool *pool, void *addr);
void BLI_mempool_destroy(BLI_mempool *pool);


#endif
