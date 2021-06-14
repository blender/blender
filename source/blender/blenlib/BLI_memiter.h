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
 */

#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 512kb, good default for small elems. */
#define BLI_MEMITER_DEFAULT_SIZE (1 << 19)

struct BLI_memiter;

typedef struct BLI_memiter BLI_memiter;

/* warning, ATTR_MALLOC flag on BLI_memiter_alloc causes crash, see: D2756 */
BLI_memiter *BLI_memiter_create(unsigned int chunk_size)
    ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL;
void *BLI_memiter_alloc(BLI_memiter *mi, unsigned int size)
    ATTR_RETURNS_NONNULL ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL ATTR_NONNULL(1);
void BLI_memiter_alloc_from(BLI_memiter *mi, uint elem_size, const void *data_from)
    ATTR_NONNULL(1, 3);
void *BLI_memiter_calloc(BLI_memiter *mi, unsigned int size)
    ATTR_RETURNS_NONNULL ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL ATTR_NONNULL(1);
void BLI_memiter_destroy(BLI_memiter *mi) ATTR_NONNULL(1);
void BLI_memiter_clear(BLI_memiter *mi) ATTR_NONNULL(1);
unsigned int BLI_memiter_count(const BLI_memiter *mi) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

/* utils */
void *BLI_memiter_elem_first(BLI_memiter *mi);
void *BLI_memiter_elem_first_size(BLI_memiter *mi, unsigned int *r_size);

/* private structure */
typedef struct BLI_memiter_handle {
  struct BLI_memiter_elem *elem;
  uint elem_left;
} BLI_memiter_handle;

void BLI_memiter_iter_init(BLI_memiter *mi, BLI_memiter_handle *iter) ATTR_NONNULL(1, 2);
bool BLI_memiter_iter_done(const BLI_memiter_handle *iter) ATTR_NONNULL(1);
void *BLI_memiter_iter_step(BLI_memiter_handle *iter) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void *BLI_memiter_iter_step_size(BLI_memiter_handle *iter, uint *r_size) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);

#ifdef __cplusplus
}
#endif
