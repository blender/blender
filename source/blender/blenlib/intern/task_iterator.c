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
/** \name Generic Iteration
 * \{ */

BLI_INLINE void task_parallel_calc_chunk_size(const TaskParallelSettings *settings,
                                              const int items_num,
                                              int tasks_num,
                                              int *r_chunk_size)
{
  int chunk_size = 0;

  if (!settings->use_threading) {
    /* Some users of this helper will still need a valid chunk size in case processing is not
     * threaded. We can use a bigger one than in default threaded case then. */
    chunk_size = 1024;
    tasks_num = 1;
  }
  else if (settings->min_iter_per_thread > 0) {
    /* Already set by user, no need to do anything here. */
    chunk_size = settings->min_iter_per_thread;
  }
  else {
    /* Multiplier used in heuristics below to define "optimal" chunk size.
     * The idea here is to increase the chunk size to compensate for a rather measurable threading
     * overhead caused by fetching tasks. With too many CPU threads we are starting
     * to spend too much time in those overheads.
     * First values are: 1 if tasks_num < 16;
     *              else 2 if tasks_num < 32;
     *              else 3 if tasks_num < 48;
     *              else 4 if tasks_num < 64;
     *                   etc.
     * NOTE: If we wanted to keep the 'power of two' multiplier, we'd need something like:
     *     1 << max_ii(0, (int)(sizeof(int) * 8) - 1 - bitscan_reverse_i(tasks_num) - 3)
     */
    const int tasks_num_factor = max_ii(1, tasks_num >> 3);

    /* We could make that 'base' 32 number configurable in TaskParallelSettings too, or maybe just
     * always use that heuristic using TaskParallelSettings.min_iter_per_thread as basis? */
    chunk_size = 32 * tasks_num_factor;

    /* Basic heuristic to avoid threading on low amount of items.
     * We could make that limit configurable in settings too. */
    if (items_num > 0 && items_num < max_ii(256, chunk_size * 2)) {
      chunk_size = items_num;
    }
  }

  BLI_assert(chunk_size > 0);
  *r_chunk_size = chunk_size;
}

typedef struct TaskParallelIteratorState {
  void *userdata;
  TaskParallelIteratorIterFunc iter_func;
  TaskParallelIteratorFunc func;

  /* *** Data used to 'acquire' chunks of items from the iterator. *** */
  /* Common data also passed to the generator callback. */
  TaskParallelIteratorStateShared iter_shared;
  /* Total number of items. If unknown, set it to a negative number. */
  int items_num;
} TaskParallelIteratorState;

static void parallel_iterator_func_do(TaskParallelIteratorState *__restrict state,
                                      void *userdata_chunk)
{
  TaskParallelTLS tls = {
      .userdata_chunk = userdata_chunk,
  };

  void **current_chunk_items;
  int *current_chunk_indices;
  int current_chunk_size;

  const size_t items_size = sizeof(*current_chunk_items) * (size_t)state->iter_shared.chunk_size;
  const size_t indices_size = sizeof(*current_chunk_indices) *
                              (size_t)state->iter_shared.chunk_size;

  current_chunk_items = MALLOCA(items_size);
  current_chunk_indices = MALLOCA(indices_size);
  current_chunk_size = 0;

  for (bool do_abort = false; !do_abort;) {
    if (state->iter_shared.spin_lock != NULL) {
      BLI_spin_lock(state->iter_shared.spin_lock);
    }

    /* Get current status. */
    int index = state->iter_shared.next_index;
    void *item = state->iter_shared.next_item;
    int i;

    /* 'Acquire' a chunk of items from the iterator function. */
    for (i = 0; i < state->iter_shared.chunk_size && !state->iter_shared.is_finished; i++) {
      current_chunk_indices[i] = index;
      current_chunk_items[i] = item;
      state->iter_func(state->userdata, &tls, &item, &index, &state->iter_shared.is_finished);
    }

    /* Update current status. */
    state->iter_shared.next_index = index;
    state->iter_shared.next_item = item;
    current_chunk_size = i;

    do_abort = state->iter_shared.is_finished;

    if (state->iter_shared.spin_lock != NULL) {
      BLI_spin_unlock(state->iter_shared.spin_lock);
    }

    for (i = 0; i < current_chunk_size; ++i) {
      state->func(state->userdata, current_chunk_items[i], current_chunk_indices[i], &tls);
    }
  }

  MALLOCA_FREE(current_chunk_items, items_size);
  MALLOCA_FREE(current_chunk_indices, indices_size);
}

static void parallel_iterator_func(TaskPool *__restrict pool, void *userdata_chunk)
{
  TaskParallelIteratorState *__restrict state = BLI_task_pool_user_data(pool);

  parallel_iterator_func_do(state, userdata_chunk);
}

static void task_parallel_iterator_no_threads(const TaskParallelSettings *settings,
                                              TaskParallelIteratorState *state)
{
  /* Prepare user's TLS data. */
  void *userdata_chunk = settings->userdata_chunk;
  if (userdata_chunk) {
    if (settings->func_init != NULL) {
      settings->func_init(state->userdata, userdata_chunk);
    }
  }

  /* Also marking it as non-threaded for the iterator callback. */
  state->iter_shared.spin_lock = NULL;

  parallel_iterator_func_do(state, userdata_chunk);

  if (userdata_chunk) {
    if (settings->func_free != NULL) {
      /* `func_free` should only free data that was created during execution of `func`. */
      settings->func_free(state->userdata, userdata_chunk);
    }
  }
}

static void task_parallel_iterator_do(const TaskParallelSettings *settings,
                                      TaskParallelIteratorState *state)
{
  const int threads_num = BLI_task_scheduler_num_threads();

  task_parallel_calc_chunk_size(
      settings, state->items_num, threads_num, &state->iter_shared.chunk_size);

  if (!settings->use_threading) {
    task_parallel_iterator_no_threads(settings, state);
    return;
  }

  const int chunk_size = state->iter_shared.chunk_size;
  const int items_num = state->items_num;
  const size_t tasks_num = items_num >= 0 ?
                               (size_t)min_ii(threads_num, state->items_num / chunk_size) :
                               (size_t)threads_num;

  BLI_assert(tasks_num > 0);
  if (tasks_num == 1) {
    task_parallel_iterator_no_threads(settings, state);
    return;
  }

  SpinLock spin_lock;
  BLI_spin_init(&spin_lock);
  state->iter_shared.spin_lock = &spin_lock;

  void *userdata_chunk = settings->userdata_chunk;
  const size_t userdata_chunk_size = settings->userdata_chunk_size;
  void *userdata_chunk_local = NULL;
  void *userdata_chunk_array = NULL;
  const bool use_userdata_chunk = (userdata_chunk_size != 0) && (userdata_chunk != NULL);

  TaskPool *task_pool = BLI_task_pool_create(state, TASK_PRIORITY_HIGH);

  if (use_userdata_chunk) {
    userdata_chunk_array = MALLOCA(userdata_chunk_size * tasks_num);
  }

  for (size_t i = 0; i < tasks_num; i++) {
    if (use_userdata_chunk) {
      userdata_chunk_local = (char *)userdata_chunk_array + (userdata_chunk_size * i);
      memcpy(userdata_chunk_local, userdata_chunk, userdata_chunk_size);
      if (settings->func_init != NULL) {
        settings->func_init(state->userdata, userdata_chunk_local);
      }
    }
    /* Use this pool's pre-allocated tasks. */
    BLI_task_pool_push(task_pool, parallel_iterator_func, userdata_chunk_local, false, NULL);
  }

  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);

  if (use_userdata_chunk) {
    if (settings->func_reduce != NULL || settings->func_free != NULL) {
      for (size_t i = 0; i < tasks_num; i++) {
        userdata_chunk_local = (char *)userdata_chunk_array + (userdata_chunk_size * i);
        if (settings->func_reduce != NULL) {
          settings->func_reduce(state->userdata, userdata_chunk, userdata_chunk_local);
        }
        if (settings->func_free != NULL) {
          settings->func_free(state->userdata, userdata_chunk_local);
        }
      }
    }
    MALLOCA_FREE(userdata_chunk_array, userdata_chunk_size * tasks_num);
  }

  BLI_spin_end(&spin_lock);
  state->iter_shared.spin_lock = NULL;
}

void BLI_task_parallel_iterator(void *userdata,
                                TaskParallelIteratorIterFunc iter_func,
                                void *init_item,
                                const int init_index,
                                const int items_num,
                                TaskParallelIteratorFunc func,
                                const TaskParallelSettings *settings)
{
  TaskParallelIteratorState state = {0};

  state.items_num = items_num;
  state.iter_shared.next_index = init_index;
  state.iter_shared.next_item = init_item;
  state.iter_shared.is_finished = false;
  state.userdata = userdata;
  state.iter_func = iter_func;
  state.func = func;

  task_parallel_iterator_do(settings, &state);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ListBase Iteration
 * \{ */

static void task_parallel_listbase_get(void *__restrict UNUSED(userdata),
                                       const TaskParallelTLS *__restrict UNUSED(tls),
                                       void **r_next_item,
                                       int *r_next_index,
                                       bool *r_do_abort)
{
  /* Get current status. */
  Link *link = *r_next_item;

  if (link->next == NULL) {
    *r_do_abort = true;
  }
  *r_next_item = link->next;
  (*r_next_index)++;
}

void BLI_task_parallel_listbase(ListBase *listbase,
                                void *userdata,
                                TaskParallelIteratorFunc func,
                                const TaskParallelSettings *settings)
{
  if (BLI_listbase_is_empty(listbase)) {
    return;
  }

  TaskParallelIteratorState state = {0};

  state.items_num = BLI_listbase_count(listbase);
  state.iter_shared.next_index = 0;
  state.iter_shared.next_item = listbase->first;
  state.iter_shared.is_finished = false;
  state.userdata = userdata;
  state.iter_func = task_parallel_listbase_get;
  state.func = func;

  task_parallel_iterator_do(settings, &state);
}

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
