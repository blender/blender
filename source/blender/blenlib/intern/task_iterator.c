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
 * Parallel tasks over all elements in a container.
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

/* Allows to avoid using malloc for userdata_chunk in tasks, when small enough. */
#define MALLOCA(_size) ((_size) <= 8192) ? alloca((_size)) : MEM_mallocN((_size), __func__)
#define MALLOCA_FREE(_mem, _size) \
  if (((_mem) != NULL) && ((_size) > 8192)) \
  MEM_freeN((_mem))

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
  int tot_items;
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
  const size_t userdata_chunk_size = settings->userdata_chunk_size;
  void *userdata_chunk_local = NULL;
  const bool use_userdata_chunk = (userdata_chunk_size != 0) && (userdata_chunk != NULL);
  if (use_userdata_chunk) {
    userdata_chunk_local = MALLOCA(userdata_chunk_size);
    memcpy(userdata_chunk_local, userdata_chunk, userdata_chunk_size);
  }

  /* Also marking it as non-threaded for the iterator callback. */
  state->iter_shared.spin_lock = NULL;

  parallel_iterator_func_do(state, userdata_chunk);

  if (use_userdata_chunk && settings->func_free != NULL) {
    /* `func_free` should only free data that was created during execution of `func`. */
    settings->func_free(state->userdata, userdata_chunk_local);
  }
}

static void task_parallel_iterator_do(const TaskParallelSettings *settings,
                                      TaskParallelIteratorState *state)
{
  const int num_threads = BLI_task_scheduler_num_threads();

  task_parallel_calc_chunk_size(
      settings, state->tot_items, num_threads, &state->iter_shared.chunk_size);

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

  TaskPool *task_pool = BLI_task_pool_create(state, TASK_PRIORITY_HIGH);

  if (use_userdata_chunk) {
    userdata_chunk_array = MALLOCA(userdata_chunk_size * num_tasks);
  }

  for (size_t i = 0; i < num_tasks; i++) {
    if (use_userdata_chunk) {
      userdata_chunk_local = (char *)userdata_chunk_array + (userdata_chunk_size * i);
      memcpy(userdata_chunk_local, userdata_chunk, userdata_chunk_size);
    }
    /* Use this pool's pre-allocated tasks. */
    BLI_task_pool_push(task_pool, parallel_iterator_func, userdata_chunk_local, false, NULL);
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

static void parallel_mempool_func(TaskPool *__restrict pool, void *taskdata)
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

  task_pool = BLI_task_pool_create(&state, TASK_PRIORITY_HIGH);
  num_threads = BLI_task_scheduler_num_threads();

  /* The idea here is to prevent creating task for each of the loop iterations
   * and instead have tasks which are evenly distributed across CPU cores and
   * pull next item to be crunched using the threaded-aware BLI_mempool_iter.
   */
  num_tasks = num_threads + 2;

  state.userdata = userdata;
  state.func = func;

  BLI_mempool_iter *mempool_iterators = BLI_mempool_iter_threadsafe_create(mempool,
                                                                           (size_t)num_tasks);

  for (i = 0; i < num_tasks; i++) {
    /* Use this pool's pre-allocated tasks. */
    BLI_task_pool_push(task_pool, parallel_mempool_func, &mempool_iterators[i], false, NULL);
  }

  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);

  BLI_mempool_iter_threadsafe_free(mempool_iterators);
}
