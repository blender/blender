/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * Parallel tasks over all elements in a container.
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_mempool.h"
#include "BLI_mempool_private.h"
#include "BLI_task.h"
#include "BLI_threads.h"

#include "atomic_ops.h"

/* -------------------------------------------------------------------- */
/** \name Macros
 * \{ */

/* Allows to avoid using malloc for userdata_chunk in tasks, when small enough. */
#define MALLOCA(_size) ((_size) <= 8192) ? alloca(_size) : MEM_mallocN((_size), __func__)
#define MALLOCA_FREE(_mem, _size) \
  if (((_mem) != NULL) && ((_size) > 8192)) { \
    MEM_freeN(_mem); \
  } \
  ((void)0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name MemPool Iteration
 * \{ */

typedef struct ParallelMempoolState {
  void *userdata;
  TaskParallelMempoolFunc func;
} ParallelMempoolState;

static void parallel_mempool_func(TaskPool *__restrict pool, void *taskdata)
{
  ParallelMempoolState *__restrict state = BLI_task_pool_user_data(pool);
  BLI_mempool_threadsafe_iter *iter = &((ParallelMempoolTaskData *)taskdata)->ts_iter;
  TaskParallelTLS *tls = &((ParallelMempoolTaskData *)taskdata)->tls;

  MempoolIterData *item;
  while ((item = mempool_iter_threadsafe_step(iter)) != NULL) {
    state->func(state->userdata, item, tls);
  }
}

void BLI_task_parallel_mempool(BLI_mempool *mempool,
                               void *userdata,
                               TaskParallelMempoolFunc func,
                               const TaskParallelSettings *settings)
{
  if (UNLIKELY(BLI_mempool_len(mempool) == 0)) {
    return;
  }

  void *userdata_chunk = settings->userdata_chunk;
  const size_t userdata_chunk_size = settings->userdata_chunk_size;
  void *userdata_chunk_array = NULL;
  const bool use_userdata_chunk = (userdata_chunk_size != 0) && (userdata_chunk != NULL);

  if (!settings->use_threading) {
    TaskParallelTLS tls = {NULL};
    if (use_userdata_chunk) {
      if (settings->func_init != NULL) {
        settings->func_init(userdata, userdata_chunk);
      }
      tls.userdata_chunk = userdata_chunk;
    }

    BLI_mempool_iter iter;
    BLI_mempool_iternew(mempool, &iter);

    void *item;
    while ((item = BLI_mempool_iterstep(&iter))) {
      func(userdata, item, &tls);
    }

    if (use_userdata_chunk) {
      if (settings->func_free != NULL) {
        /* `func_free` should only free data that was created during execution of `func`. */
        settings->func_free(userdata, userdata_chunk);
      }
    }

    return;
  }

  ParallelMempoolState state;
  TaskPool *task_pool = BLI_task_pool_create(&state, TASK_PRIORITY_HIGH);
  const int threads_num = BLI_task_scheduler_num_threads();

  /* The idea here is to prevent creating task for each of the loop iterations
   * and instead have tasks which are evenly distributed across CPU cores and
   * pull next item to be crunched using the threaded-aware BLI_mempool_iter.
   */
  const int tasks_num = threads_num + 2;

  state.userdata = userdata;
  state.func = func;

  if (use_userdata_chunk) {
    userdata_chunk_array = MALLOCA(userdata_chunk_size * tasks_num);
  }

  ParallelMempoolTaskData *mempool_iterator_data = mempool_iter_threadsafe_create(
      mempool, (size_t)tasks_num);

  for (int i = 0; i < tasks_num; i++) {
    void *userdata_chunk_local = NULL;
    if (use_userdata_chunk) {
      userdata_chunk_local = (char *)userdata_chunk_array + (userdata_chunk_size * i);
      memcpy(userdata_chunk_local, userdata_chunk, userdata_chunk_size);
      if (settings->func_init != NULL) {
        settings->func_init(userdata, userdata_chunk_local);
      }
    }
    mempool_iterator_data[i].tls.userdata_chunk = userdata_chunk_local;

    /* Use this pool's pre-allocated tasks. */
    BLI_task_pool_push(task_pool, parallel_mempool_func, &mempool_iterator_data[i], false, NULL);
  }

  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);

  if (use_userdata_chunk) {
    if ((settings->func_free != NULL) || (settings->func_reduce != NULL)) {
      for (int i = 0; i < tasks_num; i++) {
        if (settings->func_reduce) {
          settings->func_reduce(
              userdata, userdata_chunk, mempool_iterator_data[i].tls.userdata_chunk);
        }
        if (settings->func_free) {
          settings->func_free(userdata, mempool_iterator_data[i].tls.userdata_chunk);
        }
      }
    }
    MALLOCA_FREE(userdata_chunk_array, userdata_chunk_size * tasks_num);
  }

  mempool_iter_threadsafe_destroy(mempool_iterator_data);
}

#undef MALLOCA
#undef MALLOCA_FREE

/** \} */
