/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

/**
 * \param chunk_size_min: Should be a power of two and
 * significantly larger than the average element size used.
 *
 * While allocations of any size are supported, they won't be efficient
 * (effectively becoming a single-linked list).
 *
 * Its intended that many elements can be stored per chunk.
 */
BLI_memiter *BLI_memiter_create(unsigned int chunk_size_min)
    ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL;
void *BLI_memiter_alloc(BLI_memiter *mi, unsigned int elem_size)
    /* WARNING: `ATTR_MALLOC` attribute on #BLI_memiter_alloc causes crash, see: D2756. */
    ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL ATTR_NONNULL(1);
void BLI_memiter_alloc_from(BLI_memiter *mi, uint elem_size, const void *data_from)
    ATTR_NONNULL(1, 3);
void *BLI_memiter_calloc(BLI_memiter *mi,
                         unsigned int elem_size) ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL
    ATTR_NONNULL(1);
void BLI_memiter_destroy(BLI_memiter *mi) ATTR_NONNULL(1);
void BLI_memiter_clear(BLI_memiter *mi) ATTR_NONNULL(1);
unsigned int BLI_memiter_count(const BLI_memiter *mi) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

/* Utilities. */

/**
 * Support direct lookup for the first item.
 */
void *BLI_memiter_elem_first(BLI_memiter *mi);
void *BLI_memiter_elem_first_size(BLI_memiter *mi, unsigned int *r_size);

/** Private structure. */
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
