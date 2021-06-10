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

ParallelMempoolTaskData *mempool_iter_threadsafe_create(BLI_mempool *pool, const size_t num_iter)
    ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void mempool_iter_threadsafe_destroy(ParallelMempoolTaskData *iter_arr) ATTR_NONNULL();

void *mempool_iter_threadsafe_step(BLI_mempool_threadsafe_iter *iter);

#ifdef __cplusplus
}
#endif
