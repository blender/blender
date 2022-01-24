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

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BLI_mempool;
struct BLI_mempool_chunk;

typedef struct BLI_mempool BLI_mempool;

BLI_mempool *BLI_mempool_create(unsigned int esize,
                                unsigned int totelem,
                                unsigned int pchunk,
                                unsigned int flag)
    ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL;
void *BLI_mempool_alloc(BLI_mempool *pool) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL
    ATTR_NONNULL(1);
void *BLI_mempool_calloc(BLI_mempool *pool)
    ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL ATTR_NONNULL(1);
/**
 * Free an element from the mempool.
 *
 * \note doesn't protect against double frees, take care!
 */
void BLI_mempool_free(BLI_mempool *pool, void *addr) ATTR_NONNULL(1, 2);
/**
 * Empty the pool, as if it were just created.
 *
 * \param pool: The pool to clear.
 * \param totelem_reserve: Optionally reserve how many items should be kept from clearing.
 */
void BLI_mempool_clear_ex(BLI_mempool *pool, int totelem_reserve) ATTR_NONNULL(1);
/**
 * Wrap #BLI_mempool_clear_ex with no reserve set.
 */
void BLI_mempool_clear(BLI_mempool *pool) ATTR_NONNULL(1);
/**
 * Free the mempool itself (and all elements).
 */
void BLI_mempool_destroy(BLI_mempool *pool) ATTR_NONNULL(1);
int BLI_mempool_len(const BLI_mempool *pool) ATTR_NONNULL(1);
void *BLI_mempool_findelem(BLI_mempool *pool, unsigned int index) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);

/**
 * Fill in \a data with pointers to each element of the mempool,
 * to create lookup table.
 *
 * \param pool: Pool to create a table from.
 * \param data: array of pointers at least the size of 'pool->totused'
 */
void BLI_mempool_as_table(BLI_mempool *pool, void **data) ATTR_NONNULL(1, 2);
/**
 * A version of #BLI_mempool_as_table that allocates and returns the data.
 */
void **BLI_mempool_as_tableN(BLI_mempool *pool,
                             const char *allocstr) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);
/**
 * Fill in \a data with the contents of the mempool.
 */
void BLI_mempool_as_array(BLI_mempool *pool, void *data) ATTR_NONNULL(1, 2);
/**
 * A version of #BLI_mempool_as_array that allocates and returns the data.
 */
void *BLI_mempool_as_arrayN(BLI_mempool *pool,
                            const char *allocstr) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2);

#ifndef NDEBUG
void BLI_mempool_set_memory_debug(void);
#endif

/**
 * Iteration stuff.
 * \note this may easy to produce bugs with.
 */

/**  \note Private structure. */
typedef struct BLI_mempool_iter {
  BLI_mempool *pool;
  struct BLI_mempool_chunk *curchunk;
  unsigned int curindex;
} BLI_mempool_iter;

/** #BLI_mempool.flag */
enum {
  BLI_MEMPOOL_NOP = 0,
  /** allow iterating on this mempool.
   *
   * \note this requires that the first four bytes of the elements
   * never begin with 'free' (#FREEWORD).
   * \note order of iteration is only assured to be the
   * order of allocation when no chunks have been freed.
   */
  BLI_MEMPOOL_ALLOW_ITER = (1 << 0),
};

/**
 * Initialize a new mempool iterator, #BLI_MEMPOOL_ALLOW_ITER flag must be set.
 */
void BLI_mempool_iternew(BLI_mempool *pool, BLI_mempool_iter *iter) ATTR_NONNULL();
/**
 * Step over the iterator, returning the mempool item or NULL.
 */
void *BLI_mempool_iterstep(BLI_mempool_iter *iter) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

#ifdef __cplusplus
}
#endif
