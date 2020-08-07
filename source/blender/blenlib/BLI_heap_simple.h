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
 */

#pragma once

/** \file
 * \ingroup bli
 * \brief A min-heap / priority queue ADT
 */

#ifdef __cplusplus
extern "C" {
#endif

struct HeapSimple;
typedef struct HeapSimple HeapSimple;

typedef void (*HeapSimpleFreeFP)(void *ptr);

HeapSimple *BLI_heapsimple_new_ex(unsigned int tot_reserve) ATTR_WARN_UNUSED_RESULT;
HeapSimple *BLI_heapsimple_new(void) ATTR_WARN_UNUSED_RESULT;
void BLI_heapsimple_clear(HeapSimple *heap, HeapSimpleFreeFP ptrfreefp) ATTR_NONNULL(1);
void BLI_heapsimple_free(HeapSimple *heap, HeapSimpleFreeFP ptrfreefp) ATTR_NONNULL(1);
void BLI_heapsimple_insert(HeapSimple *heap, float value, void *ptr) ATTR_NONNULL(1);
bool BLI_heapsimple_is_empty(const HeapSimple *heap) ATTR_NONNULL(1);
uint BLI_heapsimple_len(const HeapSimple *heap) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
float BLI_heapsimple_top_value(const HeapSimple *heap) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void *BLI_heapsimple_pop_min(HeapSimple *heap) ATTR_NONNULL(1);

#ifdef __cplusplus
}
#endif
