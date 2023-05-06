/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

#pragma once

/** \file
 * \ingroup bli
 *
 * Shared logic for #BLI_task_parallel_mempool to create a threaded iterator,
 * without exposing the these functions publicly.
 */

#include "BLI_compiler_attrs.h"

#include "BLI_mempool.h"
#include "BLI_task.h"

typedef struct BLI_mempool_threadsafe_iter {
  BLI_mempool_iter iter;
  struct BLI_mempool_chunk **curchunk_threaded_shared;
} BLI_mempool_threadsafe_iter;

typedef struct ParallelMempoolTaskData {
  BLI_mempool_threadsafe_iter ts_iter;
  TaskParallelTLS tls;
} ParallelMempoolTaskData;

/**
 * Initialize an array of mempool iterators, #BLI_MEMPOOL_ALLOW_ITER flag must be set.
 *
 * This is used in threaded code, to generate as much iterators as needed
 * (each task should have its own),
 * such that each iterator goes over its own single chunk,
 * and only getting the next chunk to iterate over has to be
 * protected against concurrency (which can be done in a lock-less way).
 *
 * To be used when creating a task for each single item in the pool is totally overkill.
 *
 * See #BLI_task_parallel_mempool implementation for detailed usage example.
 */
ParallelMempoolTaskData *mempool_iter_threadsafe_create(BLI_mempool *pool,
                                                        size_t iter_num) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
void mempool_iter_threadsafe_destroy(ParallelMempoolTaskData *iter_arr) ATTR_NONNULL();

/**
 * A version of #BLI_mempool_iterstep that uses
 * #BLI_mempool_threadsafe_iter.curchunk_threaded_shared for threaded iteration support.
 * (threaded section noted in comments).
 */
void *mempool_iter_threadsafe_step(BLI_mempool_threadsafe_iter *iter);

#ifdef __cplusplus
}
#endif
