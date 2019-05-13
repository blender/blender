/*
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
 */

#ifndef __BLI_MEMBLOCK_H__
#define __BLI_MEMBLOCK_H__

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_compiler_attrs.h"

struct BLI_memblock;

typedef struct BLI_memblock BLI_memblock;
typedef void (*MemblockValFreeFP)(void *val);

BLI_memblock *BLI_memblock_create(uint elem_size,
                                  const bool clear_alloc) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT;
void *BLI_memblock_alloc(BLI_memblock *mblk) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void BLI_memblock_clear(BLI_memblock *mblk, MemblockValFreeFP valfreefp) ATTR_NONNULL(1);
void BLI_memblock_destroy(BLI_memblock *mblk, MemblockValFreeFP free_callback) ATTR_NONNULL(1);

typedef struct BLI_memblock_iter {
  BLI_memblock *mblk;
  int current_index;
  int elem_per_chunk;
} BLI_memblock_iter;

void BLI_memblock_iternew(BLI_memblock *pool, BLI_memblock_iter *iter) ATTR_NONNULL();
void *BLI_memblock_iterstep(BLI_memblock_iter *iter) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

#ifdef __cplusplus
}
#endif

#endif /* __BLI_MEMBLOCK_H__ */
