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

/** \file
 * \ingroup bli
 *
 * A generic task system which can be used for any task based subsystem.
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_mempool.h"
#include "BLI_task.h"
#include "BLI_threads.h"

#include "atomic_ops.h"

/* Parallel range routines */

/**
 *
 * Main functions:
 * - #BLI_task_parallel_range
 * - #BLI_task_parallel_listbase (#ListBase - double linked list)
 *
 * TODO:
 * - #BLI_task_parallel_foreach_link (#Link - single linked list)
 * - #BLI_task_parallel_foreach_ghash/gset (#GHash/#GSet - hash & set)
 * - #BLI_task_parallel_foreach_mempool (#BLI_mempool - iterate over mempools)
 */

/* Allows to avoid using malloc for userdata_chunk in tasks, when small enough. */
#define MALLOCA(_size) ((_size) <= 8192) ? alloca((_size)) : MEM_mallocN((_size), __func__)
#define MALLOCA_FREE(_mem, _size) \
  if (((_mem) != NULL) && ((_size) > 8192)) \
  MEM_freeN((_mem))

/* Stores all needed data to perform a parallelized iteration,
 * with a same operation (callback function).
 * It can be chained with other tasks in a single-linked list way. */
typedef struct TaskParallelRangeState {
  struct TaskParallelRangeState *next;

  /* Start and end point of integer value iteration. */
  int start, stop;

  /* User-defined data, shared between all worker threads. */
  void *userdata_shared;
  /* User-defined callback function called for each value in [start, stop[ specified range. */
  TaskParallelRangeFunc func;

  /* Each instance of looping chunks will get a copy of this data
   * (similar to OpenMP's firstprivate).
   */
  void *initial_tls_memory; /* Pointer to actual user-defined 'tls' data. */
  size_t tls_data_size;     /* Size of that data.  */

  void *flatten_tls_storage; /* 'tls' copies of initial_tls_memory for each running task. */
  /* Number of 'tls' copies in the array, i.e. number of worker threads. */
  size_t num_elements_in_tls_storage;

  /* Function called to join user data chunk into another, to reduce
   * the result to the original userdata_chunk memory.
   * The reduce functions should have no side effects, so that they
   * can be run on any thread. */
  TaskParallelReduceFunc func_reduce;
  /* Function called to free data created by TaskParallelRangeFunc. */
  TaskParallelFreeFunc func_free;

  /* Current value of the iterator, shared between all threads (atomically updated). */
  int iter_value;
  int iter_chunk_num; /* Amount of iterations to process in a single step. */
} TaskParallelRangeState;

/* Stores all the parallel tasks for a single pool. */
typedef struct TaskParallelRangePool {
  /* The workers' task pool. */
  TaskPool *pool;
  /* The number of worker tasks we need to create. */
  int num_tasks;
  /* The total number of iterations in all the added ranges. */
  int num_total_iters;
  /* The size (number of items) processed at once by a worker task. */
  int chunk_size;

  /* Linked list of range tasks to process. */
  TaskParallelRangeState *parallel_range_states;
  /* Current range task beeing processed, swapped atomically. */
  TaskParallelRangeState *current_state;
  /* Scheduling settings common to all tasks. */
  TaskParallelSettings *settings;
} TaskParallelRangePool;

BLI_INLINE void task_parallel_calc_chunk_size(const TaskParallelSettings *settings,
                                              const int tot_items,
                                              int num_tasks,
                                              int *r_chunk_size)
{
  int chunk_size = 0;

  if (!settings->use_threading) {
    /* Some users of this helper will still need a valid chunk size in case processing is not
     * threaded. We can use a bigger one than in default threaded case then. */
    chunk_size = 1024;
    num_tasks = 1;
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
     * First values are: 1 if num_tasks < 16;
     *              else 2 if num_tasks < 32;
     *              else 3 if num_tasks < 48;
     *              else 4 if num_tasks < 64;
     *                   etc.
     * Note: If we wanted to keep the 'power of two' multiplier, we'd need something like:
     *     1 << max_ii(0, (int)(sizeof(int) * 8) - 1 - bitscan_reverse_i(num_tasks) - 3)
     */
    const int num_tasks_factor = max_ii(1, num_tasks >> 3);

    /* We could make that 'base' 32 number configurable in TaskParallelSettings too, or maybe just
     * always use that heuristic using TaskParallelSettings.min_iter_per_thread as basis? */
    chunk_size = 32 * num_tasks_factor;

    /* Basic heuristic to avoid threading on low amount of items.
     * We could make that limit configurable in settings too. */
    if (tot_items > 0 && tot_items < max_ii(256, chunk_size * 2)) {
      chunk_size = tot_items;
    }
  }

  BLI_assert(chunk_size > 0);

  if (tot_items > 0) {
    switch (settings->scheduling_mode) {
      case TASK_SCHEDULING_STATIC:
        *r_chunk_size = max_ii(chunk_size, tot_items / num_tasks);
        break;
      case TASK_SCHEDULING_DYNAMIC:
        *r_chunk_size = chunk_size;
        break;
    }
  }
  else {
    /* If total amount of items is unknown, we can only use dynamic scheduling. */
    *r_chunk_size = chunk_size;
  }
}

BLI_INLINE void task_parallel_range_calc_chunk_size(TaskParallelRangePool *range_pool)
{
  int num_iters = 0;
  int min_num_iters = INT_MAX;
  for (TaskParallelRangeState *state = range_pool->parallel_range_states; state != NULL;
       state = state->next) {
    const int ni = state->stop - state->start;
    num_iters += ni;
    if (min_num_iters > ni) {
      min_num_iters = ni;
    }
  }
  range_pool->num_total_iters = num_iters;
  /* Note: Passing min_num_iters here instead of num_iters kind of partially breaks the 'static'
   * scheduling, but pooled range iterator is inherently non-static anyway, so adding a small level
   * of dynamic scheduling here should be fine. */
  task_parallel_calc_chunk_size(
      range_pool->settings, min_num_iters, range_pool->num_tasks, &range_pool->chunk_size);
}

BLI_INLINE bool parallel_range_next_iter_get(TaskParallelRangePool *__restrict range_pool,
                                             int *__restrict r_iter,
                                             int *__restrict r_count,
                                             TaskParallelRangeState **__restrict r_state)
{
  /* We need an atomic op here as well to fetch the initial state, since some other thread might
   * have already updated it. */
  TaskParallelRangeState *current_state = atomic_cas_ptr(
      (void **)&range_pool->current_state, NULL, NULL);

  int previter = INT32_MAX;

  while (current_state != NULL && previter >= current_state->stop) {
    previter = atomic_fetch_and_add_int32(&current_state->iter_value, range_pool->chunk_size);
    *r_iter = previter;
    *r_count = max_ii(0, min_ii(range_pool->chunk_size, current_state->stop - previter));

    if (previter >= current_state->stop) {
      /* At this point the state we got is done, we need to go to the next one. In case some other
       * thread already did it, then this does nothing, and we'll just get current valid state
       * at start of the next loop. */
      TaskParallelRangeState *current_state_from_atomic_cas = atomic_cas_ptr(
          (void **)&range_pool->current_state, current_state, current_state->next);

      if (current_state == current_state_from_atomic_cas) {
        /* The atomic CAS operation was successful, we did update range_pool->current_state, so we
         * can safely switch to next state. */
        current_state = current_state->next;
      }
      else {
        /* The atomic CAS operation failed, but we still got range_pool->current_state value out of
         * it, just use it as our new current state. */
        current_state = current_state_from_atomic_cas;
      }
    }
  }

  *r_state = current_state;
  return (current_state != NULL && previter < current_state->stop);
}

static void parallel_range_func(TaskPool *__restrict pool, void *tls_data_idx, int thread_id)
{
  TaskParallelRangePool *__restrict range_pool = BLI_task_pool_user_data(pool);
  TaskParallelTLS tls = {
      .thread_id = thread_id,
      .userdata_chunk = NULL,
  };
  TaskParallelRangeState *state;
  int iter, count;
  while (parallel_range_next_iter_get(range_pool, &iter, &count, &state)) {
    tls.userdata_chunk = (char *)state->flatten_tls_storage +
                         (((size_t)POINTER_AS_INT(tls_data_idx)) * state->tls_data_size);
    for (int i = 0; i < count; i++) {
      state->func(state->userdata_shared, iter + i, &tls);
    }
  }
}

static void parallel_range_single_thread(TaskParallelRangePool *range_pool)
{
  for (TaskParallelRangeState *state = range_pool->parallel_range_states; state != NULL;
       state = state->next) {
    const int start = state->start;
    const int stop = state->stop;
    void *userdata = state->userdata_shared;
    TaskParallelRangeFunc func = state->func;

    void *initial_tls_memory = state->initial_tls_memory;
    const size_t tls_data_size = state->tls_data_size;
    const bool use_tls_data = (tls_data_size != 0) && (initial_tls_memory != NULL);
    TaskParallelTLS tls = {
        .thread_id = 0,
        .userdata_chunk = initial_tls_memory,
    };
    for (int i = start; i < stop; i++) {
      func(userdata, i, &tls);
    }
    if (use_tls_data && state->func_free != NULL) {
      /* `func_free` should only free data that was created during execution of `func`. */
      state->func_free(userdata, initial_tls_memory);
    }
  }
}

/**
 * This function allows to parallelized for loops in a similar way to OpenMP's
 * 'parallel for' statement.
 *
 * See public API doc of ParallelRangeSettings for description of all settings.
 */
void BLI_task_parallel_range(const int start,
                             const int stop,
                             void *userdata,
                             TaskParallelRangeFunc func,
                             TaskParallelSettings *settings)
{
  if (start == stop) {
    return;
  }

  BLI_assert(start < stop);

  TaskParallelRangeState state = {
      .next = NULL,
      .start = start,
      .stop = stop,
      .userdata_shared = userdata,
      .func = func,
      .iter_value = start,
      .initial_tls_memory = settings->userdata_chunk,
      .tls_data_size = settings->userdata_chunk_size,
      .func_free = settings->func_free,
  };
  TaskParallelRangePool range_pool = {
      .pool = NULL, .parallel_range_states = &state, .current_state = NULL, .settings = settings};
  int i, num_threads, num_tasks;

  void *tls_data = settings->userdata_chunk;
  const size_t tls_data_size = settings->userdata_chunk_size;
  if (tls_data_size != 0) {
    BLI_assert(tls_data != NULL);
  }
  const bool use_tls_data = (tls_data_size != 0) && (tls_data != NULL);
  void *flatten_tls_storage = NULL;

  /* If it's not enough data to be crunched, don't bother with tasks at all,
   * do everything from the current thread.
   */
  if (!settings->use_threading) {
    parallel_range_single_thread(&range_pool);
    return;
  }

  TaskScheduler *task_scheduler = BLI_task_scheduler_get();
  num_threads = BLI_task_scheduler_num_threads(task_scheduler);

  /* The idea here is to prevent creating task for each of the loop iterations
   * and instead have tasks which are evenly distributed across CPU cores and
   * pull next iter to be crunched using the queue.
   */
  range_pool.num_tasks = num_tasks = num_threads + 2;

  task_parallel_range_calc_chunk_size(&range_pool);
  range_pool.num_tasks = num_tasks = min_ii(num_tasks,
                                            max_ii(1, (stop - start) / range_pool.chunk_size));

  if (num_tasks == 1) {
    parallel_range_single_thread(&range_pool);
    return;
  }

  TaskPool *task_pool = range_pool.pool = BLI_task_pool_create_suspended(
      task_scheduler, &range_pool, TASK_PRIORITY_HIGH);

  range_pool.current_state = &state;

  if (use_tls_data) {
    state.flatten_tls_storage = flatten_tls_storage = MALLOCA(tls_data_size * (size_t)num_tasks);
    state.tls_data_size = tls_data_size;
  }

  const int thread_id = BLI_task_pool_creator_thread_id(task_pool);
  for (i = 0; i < num_tasks; i++) {
    if (use_tls_data) {
      void *userdata_chunk_local = (char *)flatten_tls_storage + (tls_data_size * (size_t)i);
      memcpy(userdata_chunk_local, tls_data, tls_data_size);
    }
    /* Use this pool's pre-allocated tasks. */
    BLI_task_pool_push_from_thread(
        task_pool, parallel_range_func, POINTER_FROM_INT(i), false, NULL, thread_id);
  }

  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);

  if (use_tls_data && (settings->func_free != NULL || settings->func_reduce != NULL)) {
    for (i = 0; i < num_tasks; i++) {
      void *userdata_chunk_local = (char *)flatten_tls_storage + (tls_data_size * (size_t)i);
      if (settings->func_reduce) {
        settings->func_reduce(userdata, tls_data, userdata_chunk_local);
      }
      if (settings->func_free) {
        /* `func_free` should only free data that was created during execution of `func`. */
        settings->func_free(userdata, userdata_chunk_local);
      }
    }
    MALLOCA_FREE(flatten_tls_storage, tls_data_size * (size_t)num_tasks);
  }
}

/**
 * Initialize a task pool to parallelize several for loops at the same time.
 *
 * See public API doc of ParallelRangeSettings for description of all settings.
 * Note that loop-specific settings (like 'tls' data or reduce/free functions) must be left NULL
 * here. Only settings controlling how iteration is parallelized must be defined, as those will
 * affect all loops added to that pool.
 */
TaskParallelRangePool *BLI_task_parallel_range_pool_init(const TaskParallelSettings *settings)
{
  TaskParallelRangePool *range_pool = MEM_callocN(sizeof(*range_pool), __func__);

  BLI_assert(settings->userdata_chunk == NULL);
  BLI_assert(settings->func_reduce == NULL);
  BLI_assert(settings->func_free == NULL);
  range_pool->settings = MEM_mallocN(sizeof(*range_pool->settings), __func__);
  *range_pool->settings = *settings;

  return range_pool;
}

/**
 * Add a loop task to the pool. It does not execute it at all.
 *
 * See public API doc of ParallelRangeSettings for description of all settings.
 * Note that only 'tls'-related data are used here.
 */
void BLI_task_parallel_range_pool_push(TaskParallelRangePool *range_pool,
                                       const int start,
                                       const int stop,
                                       void *userdata,
                                       TaskParallelRangeFunc func,
                                       const TaskParallelSettings *settings)
{
  BLI_assert(range_pool->pool == NULL);

  if (start == stop) {
    return;
  }

  BLI_assert(start < stop);
  if (settings->userdata_chunk_size != 0) {
    BLI_assert(settings->userdata_chunk != NULL);
  }

  TaskParallelRangeState *state = MEM_callocN(sizeof(*state), __func__);
  state->start = start;
  state->stop = stop;
  state->userdata_shared = userdata;
  state->func = func;
  state->iter_value = start;
  state->initial_tls_memory = settings->userdata_chunk;
  state->tls_data_size = settings->userdata_chunk_size;
  state->func_reduce = settings->func_reduce;
  state->func_free = settings->func_free;

  state->next = range_pool->parallel_range_states;
  range_pool->parallel_range_states = state;
}

static void parallel_range_func_finalize(TaskPool *__restrict pool,
                                         void *v_state,
                                         int UNUSED(thread_id))
{
  TaskParallelRangePool *__restrict range_pool = BLI_task_pool_user_data(pool);
  TaskParallelRangeState *state = v_state;

  for (int i = 0; i < range_pool->num_tasks; i++) {
    void *tls_data = (char *)state->flatten_tls_storage + (state->tls_data_size * (size_t)i);
    if (state->func_reduce != NULL) {
      state->func_reduce(state->userdata_shared, state->initial_tls_memory, tls_data);
    }
    if (state->func_free != NULL) {
      /* `func_free` should only free data that was created during execution of `func`. */
      state->func_free(state->userdata_shared, tls_data);
    }
  }
}

/**
 * Run all tasks pushed to the range_pool.
 *
 * Note that the range pool is re-usable (you may push new tasks into it and call this function
 * again).
 */
void BLI_task_parallel_range_pool_work_and_wait(TaskParallelRangePool *range_pool)
{
  BLI_assert(range_pool->pool == NULL);

  /* If it's not enough data to be crunched, don't bother with tasks at all,
   * do everything from the current thread.
   */
  if (!range_pool->settings->use_threading) {
    parallel_range_single_thread(range_pool);
    return;
  }

  TaskScheduler *task_scheduler = BLI_task_scheduler_get();
  const int num_threads = BLI_task_scheduler_num_threads(task_scheduler);

  /* The idea here is to prevent creating task for each of the loop iterations
   * and instead have tasks which are evenly distributed across CPU cores and
   * pull next iter to be crunched using the queue.
   */
  int num_tasks = num_threads + 2;
  range_pool->num_tasks = num_tasks;

  task_parallel_range_calc_chunk_size(range_pool);
  range_pool->num_tasks = num_tasks = min_ii(
      num_tasks, max_ii(1, range_pool->num_total_iters / range_pool->chunk_size));

  if (num_tasks == 1) {
    parallel_range_single_thread(range_pool);
    return;
  }

  /* We create all 'tls' data here in a single loop. */
  for (TaskParallelRangeState *state = range_pool->parallel_range_states; state != NULL;
       state = state->next) {
    void *userdata_chunk = state->initial_tls_memory;
    const size_t userdata_chunk_size = state->tls_data_size;
    if (userdata_chunk_size == 0) {
      BLI_assert(userdata_chunk == NULL);
      continue;
    }

    void *userdata_chunk_array = NULL;
    state->flatten_tls_storage = userdata_chunk_array = MALLOCA(userdata_chunk_size *
                                                                (size_t)num_tasks);
    for (int i = 0; i < num_tasks; i++) {
      void *userdata_chunk_local = (char *)userdata_chunk_array +
                                   (userdata_chunk_size * (size_t)i);
      memcpy(userdata_chunk_local, userdata_chunk, userdata_chunk_size);
    }
  }

  TaskPool *task_pool = range_pool->pool = BLI_task_pool_create_suspended(
      task_scheduler, range_pool, TASK_PRIORITY_HIGH);

  range_pool->current_state = range_pool->parallel_range_states;
  const int thread_id = BLI_task_pool_creator_thread_id(task_pool);
  for (int i = 0; i < num_tasks; i++) {
    BLI_task_pool_push_from_thread(
        task_pool, parallel_range_func, POINTER_FROM_INT(i), false, NULL, thread_id);
  }

  BLI_task_pool_work_and_wait(task_pool);

  BLI_assert(atomic_cas_ptr((void **)&range_pool->current_state, NULL, NULL) == NULL);

  /* Finalize all tasks. */
  for (TaskParallelRangeState *state = range_pool->parallel_range_states; state != NULL;
       state = state->next) {
    const size_t userdata_chunk_size = state->tls_data_size;
    void *userdata_chunk_array = state->flatten_tls_storage;
    UNUSED_VARS_NDEBUG(userdata_chunk_array);
    if (userdata_chunk_size == 0) {
      BLI_assert(userdata_chunk_array == NULL);
      continue;
    }

    if (state->func_reduce != NULL || state->func_free != NULL) {
      BLI_task_pool_push_from_thread(
          task_pool, parallel_range_func_finalize, state, false, NULL, thread_id);
    }
  }

  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);
  range_pool->pool = NULL;

  /* Cleanup all tasks. */
  TaskParallelRangeState *state_next;
  for (TaskParallelRangeState *state = range_pool->parallel_range_states; state != NULL;
       state = state_next) {
    state_next = state->next;

    const size_t userdata_chunk_size = state->tls_data_size;
    void *userdata_chunk_array = state->flatten_tls_storage;
    if (userdata_chunk_size != 0) {
      BLI_assert(userdata_chunk_array != NULL);
      MALLOCA_FREE(userdata_chunk_array, userdata_chunk_size * (size_t)num_tasks);
    }

    MEM_freeN(state);
  }
  range_pool->parallel_range_states = NULL;
}

/**
 * Clear/free given \a range_pool.
 */
void BLI_task_parallel_range_pool_free(TaskParallelRangePool *range_pool)
{
  TaskParallelRangeState *state_next = NULL;
  for (TaskParallelRangeState *state = range_pool->parallel_range_states; state != NULL;
       state = state_next) {
    state_next = state->next;
    MEM_freeN(state);
  }
  MEM_freeN(range_pool->settings);
  MEM_freeN(range_pool);
}

typedef struct TaskParallelIteratorState {
  void *userdata;
  TaskParallelIteratorIterFunc iter_func;
  TaskParallelIteratorFunc func;

  /* *** Data used to 'acquire' chunks of items from the iterator. *** */
  /* Common data also passed to the generator callback. */
  TaskParallelIteratorStateShared iter_shared;
  /* Total number of items. If unknown, set it to a negative number. */
  int tot_items;
} TaskParallelIteratorState;

BLI_INLINE void task_parallel_iterator_calc_chunk_size(const TaskParallelSettings *settings,
                                                       const int num_tasks,
                                                       TaskParallelIteratorState *state)
{
  task_parallel_calc_chunk_size(
      settings, state->tot_items, num_tasks, &state->iter_shared.chunk_size);
}

static void parallel_iterator_func_do(TaskParallelIteratorState *__restrict state,
                                      void *userdata_chunk,
                                      int threadid)
{
  TaskParallelTLS tls = {
      .thread_id = threadid,
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

static void parallel_iterator_func(TaskPool *__restrict pool, void *userdata_chunk, int threadid)
{
  TaskParallelIteratorState *__restrict state = BLI_task_pool_user_data(pool);

  parallel_iterator_func_do(state, userdata_chunk, threadid);
}

static void task_parallel_iterator_no_threads(const TaskParallelSettings *settings,
                                              TaskParallelIteratorState *state)
{
  /* Prepare user's TLS data. */
  void *userdata_chunk = settings->userdata_chunk;
  const size_t userdata_chunk_size = settings->userdata_chunk_size;
  void *userdata_chunk_local = NULL;
  const bool use_userdata_chunk = (userdata_chunk_size != 0) && (userdata_chunk != NULL);
  if (use_userdata_chunk) {
    userdata_chunk_local = MALLOCA(userdata_chunk_size);
    memcpy(userdata_chunk_local, userdata_chunk, userdata_chunk_size);
  }

  /* Also marking it as non-threaded for the iterator callback. */
  state->iter_shared.spin_lock = NULL;

  parallel_iterator_func_do(state, userdata_chunk, 0);

  if (use_userdata_chunk && settings->func_free != NULL) {
    /* `func_free` should only free data that was created during execution of `func`. */
    settings->func_free(state->userdata, userdata_chunk_local);
  }
}

static void task_parallel_iterator_do(const TaskParallelSettings *settings,
                                      TaskParallelIteratorState *state)
{
  TaskScheduler *task_scheduler = BLI_task_scheduler_get();
  const int num_threads = BLI_task_scheduler_num_threads(task_scheduler);

  task_parallel_iterator_calc_chunk_size(settings, num_threads, state);

  if (!settings->use_threading) {
    task_parallel_iterator_no_threads(settings, state);
    return;
  }

  const int chunk_size = state->iter_shared.chunk_size;
  const int tot_items = state->tot_items;
  const size_t num_tasks = tot_items >= 0 ?
                               (size_t)min_ii(num_threads, state->tot_items / chunk_size) :
                               (size_t)num_threads;

  BLI_assert(num_tasks > 0);
  if (num_tasks == 1) {
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

  TaskPool *task_pool = BLI_task_pool_create_suspended(task_scheduler, state, TASK_PRIORITY_HIGH);

  if (use_userdata_chunk) {
    userdata_chunk_array = MALLOCA(userdata_chunk_size * num_tasks);
  }

  const int thread_id = BLI_task_pool_creator_thread_id(task_pool);
  for (size_t i = 0; i < num_tasks; i++) {
    if (use_userdata_chunk) {
      userdata_chunk_local = (char *)userdata_chunk_array + (userdata_chunk_size * i);
      memcpy(userdata_chunk_local, userdata_chunk, userdata_chunk_size);
    }
    /* Use this pool's pre-allocated tasks. */
    BLI_task_pool_push_from_thread(
        task_pool, parallel_iterator_func, userdata_chunk_local, false, NULL, thread_id);
  }

  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);

  if (use_userdata_chunk && (settings->func_reduce != NULL || settings->func_free != NULL)) {
    for (size_t i = 0; i < num_tasks; i++) {
      userdata_chunk_local = (char *)userdata_chunk_array + (userdata_chunk_size * i);
      if (settings->func_reduce != NULL) {
        settings->func_reduce(state->userdata, userdata_chunk, userdata_chunk_local);
      }
      if (settings->func_free != NULL) {
        settings->func_free(state->userdata, userdata_chunk_local);
      }
    }
    MALLOCA_FREE(userdata_chunk_array, userdata_chunk_size * num_tasks);
  }

  BLI_spin_end(&spin_lock);
  state->iter_shared.spin_lock = NULL;
}

/**
 * This function allows to parallelize for loops using a generic iterator.
 *
 * \param userdata: Common userdata passed to all instances of \a func.
 * \param iter_func: Callback function used to generate chunks of items.
 * \param init_item: The initial item, if necessary (may be NULL if unused).
 * \param init_index: The initial index.
 * \param tot_items: The total amount of items to iterate over
 *                   (if unknown, set it to a negative number).
 * \param func: Callback function.
 * \param settings: See public API doc of TaskParallelSettings for description of all settings.
 *
 * \note Static scheduling is only available when \a tot_items is >= 0.
 */

void BLI_task_parallel_iterator(void *userdata,
                                TaskParallelIteratorIterFunc iter_func,
                                void *init_item,
                                const int init_index,
                                const int tot_items,
                                TaskParallelIteratorFunc func,
                                const TaskParallelSettings *settings)
{
  TaskParallelIteratorState state = {0};

  state.tot_items = tot_items;
  state.iter_shared.next_index = init_index;
  state.iter_shared.next_item = init_item;
  state.iter_shared.is_finished = false;
  state.userdata = userdata;
  state.iter_func = iter_func;
  state.func = func;

  task_parallel_iterator_do(settings, &state);
}

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

/**
 * This function allows to parallelize for loops over ListBase items.
 *
 * \param listbase: The double linked list to loop over.
 * \param userdata: Common userdata passed to all instances of \a func.
 * \param func: Callback function.
 * \param settings: See public API doc of ParallelRangeSettings for description of all settings.
 *
 * \note There is no static scheduling here,
 * since it would need another full loop over items to count them.
 */
void BLI_task_parallel_listbase(ListBase *listbase,
                                void *userdata,
                                TaskParallelIteratorFunc func,
                                const TaskParallelSettings *settings)
{
  if (BLI_listbase_is_empty(listbase)) {
    return;
  }

  TaskParallelIteratorState state = {0};

  state.tot_items = BLI_listbase_count(listbase);
  state.iter_shared.next_index = 0;
  state.iter_shared.next_item = listbase->first;
  state.iter_shared.is_finished = false;
  state.userdata = userdata;
  state.iter_func = task_parallel_listbase_get;
  state.func = func;

  task_parallel_iterator_do(settings, &state);
}

#undef MALLOCA
#undef MALLOCA_FREE

typedef struct ParallelMempoolState {
  void *userdata;
  TaskParallelMempoolFunc func;
} ParallelMempoolState;

static void parallel_mempool_func(TaskPool *__restrict pool, void *taskdata, int UNUSED(threadid))
{
  ParallelMempoolState *__restrict state = BLI_task_pool_user_data(pool);
  BLI_mempool_iter *iter = taskdata;
  MempoolIterData *item;

  while ((item = BLI_mempool_iterstep(iter)) != NULL) {
    state->func(state->userdata, item);
  }
}

/**
 * This function allows to parallelize for loops over Mempool items.
 *
 * \param mempool: The iterable BLI_mempool to loop over.
 * \param userdata: Common userdata passed to all instances of \a func.
 * \param func: Callback function.
 * \param use_threading: If \a true, actually split-execute loop in threads,
 * else just do a sequential for loop
 * (allows caller to use any kind of test to switch on parallelization or not).
 *
 * \note There is no static scheduling here.
 */
void BLI_task_parallel_mempool(BLI_mempool *mempool,
                               void *userdata,
                               TaskParallelMempoolFunc func,
                               const bool use_threading)
{
  TaskScheduler *task_scheduler;
  TaskPool *task_pool;
  ParallelMempoolState state;
  int i, num_threads, num_tasks;

  if (BLI_mempool_len(mempool) == 0) {
    return;
  }

  if (!use_threading) {
    BLI_mempool_iter iter;
    BLI_mempool_iternew(mempool, &iter);

    for (void *item = BLI_mempool_iterstep(&iter); item != NULL;
         item = BLI_mempool_iterstep(&iter)) {
      func(userdata, item);
    }
    return;
  }

  task_scheduler = BLI_task_scheduler_get();
  task_pool = BLI_task_pool_create_suspended(task_scheduler, &state, TASK_PRIORITY_HIGH);
  num_threads = BLI_task_scheduler_num_threads(task_scheduler);

  /* The idea here is to prevent creating task for each of the loop iterations
   * and instead have tasks which are evenly distributed across CPU cores and
   * pull next item to be crunched using the threaded-aware BLI_mempool_iter.
   */
  num_tasks = num_threads + 2;

  state.userdata = userdata;
  state.func = func;

  BLI_mempool_iter *mempool_iterators = BLI_mempool_iter_threadsafe_create(mempool,
                                                                           (size_t)num_tasks);

  const int thread_id = BLI_task_pool_creator_thread_id(task_pool);
  for (i = 0; i < num_tasks; i++) {
    /* Use this pool's pre-allocated tasks. */
    BLI_task_pool_push_from_thread(
        task_pool, parallel_mempool_func, &mempool_iterators[i], false, NULL, thread_id);
  }

  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);

  BLI_mempool_iter_threadsafe_free(mempool_iterators);
}
