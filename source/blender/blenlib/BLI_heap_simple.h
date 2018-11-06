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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_HEAP_SIMPLE_H__
#define __BLI_HEAP_SIMPLE_H__

/** \file BLI_heap_simple.h
 *  \ingroup bli
 *  \brief A min-heap / priority queue ADT
 */


struct FastHeap;
typedef struct FastHeap FastHeap;

typedef void (*HeapSimpleFreeFP)(void *ptr);

FastHeap   *BLI_fastheap_new_ex(unsigned int tot_reserve) ATTR_WARN_UNUSED_RESULT;
FastHeap   *BLI_fastheap_new(void) ATTR_WARN_UNUSED_RESULT;
void        BLI_fastheap_clear(FastHeap *heap, HeapSimpleFreeFP ptrfreefp) ATTR_NONNULL(1);
void        BLI_fastheap_free(FastHeap *heap, HeapSimpleFreeFP ptrfreefp) ATTR_NONNULL(1);
void        BLI_fastheap_insert(FastHeap *heap, float value, void *ptr) ATTR_NONNULL(1);
bool        BLI_fastheap_is_empty(const FastHeap *heap) ATTR_NONNULL(1);
uint        BLI_fastheap_len(const FastHeap *heap) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
float       BLI_fastheap_top_value(const FastHeap *heap) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void       *BLI_fastheap_pop_min(FastHeap *heap) ATTR_NONNULL(1);

#endif  /* __BLI_HEAP_SIMPLE_H__ */
