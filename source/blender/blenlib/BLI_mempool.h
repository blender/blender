/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation. All rights reserved. */

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

BLI_mempool *BLI_mempool_create_ex(unsigned int esize,
                                   unsigned int totelem,
                                   unsigned int pchunk,
                                   unsigned int flag,
                                   const char *tag)
    ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL;
;

//#define DEBUG_MEMPOOL_LEAKS

#ifdef DEBUG_MEMPOOL_LEAKS
ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL;

#  define BLI_mempool_create(esize, totelem, pchunk, flag) \
    BLI_mempool_create_ex(esize, totelem, pchunk, flag, __func__)
#else
BLI_mempool *BLI_mempool_create(unsigned int esize,
                                unsigned int elem_num,
                                unsigned int pchunk,
                                unsigned int flag)
    ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL;
#endif

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

  /* allow random access, implies BLI_MEMPOOL_ALLOW_ITER since we
     need the freewords to detect free state of elements*/
  BLI_MEMPOOL_RANDOM_ACCESS = (1 << 1) | (1 << 0),
  BLI_MEMPOOL_IGNORE_FREE = (1 << 2), /* Used for debugging. */
};

/**
 * Initialize a new mempool iterator, #BLI_MEMPOOL_ALLOW_ITER flag must be set.
 */
void BLI_mempool_iternew(BLI_mempool *pool, BLI_mempool_iter *iter) ATTR_NONNULL();
/**
 * Step over the iterator, returning the mempool item or NULL.
 */
void *BLI_mempool_iterstep(BLI_mempool_iter *iter) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/*
This preallocates a mempool suitable for threading.  totelem elements are preallocated
in chunks of size pchunk, and returned in r_chunks.
*/

BLI_mempool *BLI_mempool_create_for_tasks(const unsigned int esize,
                                          int totelem,
                                          const int pchunk,
                                          void ***r_chunks,
                                          int *r_totchunk,
                                          int *r_esize,
                                          int flag);

// memory coherence stuff
int BLI_mempool_find_elems_fuzzy(
    BLI_mempool *pool, int idx, int range, void **r_elems, int r_elems_size);

int BLI_mempool_get_size(BLI_mempool *pool);
int BLI_mempool_find_real_index(BLI_mempool *pool, void *ptr);

/* Sets BLI_MEMPOOL_IGNORE_FREE flag, ignores call to BLI_mempool_free.
 * Used for debugging.
 */
void BLI_mempool_ignore_free(BLI_mempool *pool);

#ifdef __cplusplus
}
#endif
