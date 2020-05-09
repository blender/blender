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

#ifndef __BLI_TASK_H__
#define __BLI_TASK_H__

#include <string.h> /* for memset() */

struct ListBase;

/** \file
 * \ingroup bli
 */

#include "BLI_threads.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BLI_mempool;

/* Task Scheduler
 *
 * Central scheduler that holds running threads ready to execute tasks. A single
 * queue holds the task from all pools.
 *
 * Init/exit must be called before/after any task pools are created/freed, and
 * must be called from the main threads. All other scheduler and pool functions
 * are thread-safe. */

void BLI_task_scheduler_init(void);
void BLI_task_scheduler_exit(void);
int BLI_task_scheduler_num_threads(void);

/* Task Pool
 *
 * Pool of tasks that will be executed by the central task scheduler. For each
 * pool, we can wait for all tasks to be done, or cancel them before they are
 * done.
 *
 * Running tasks may spawn new tasks.
 *
 * Pools may be nested, i.e. a thread running a task can create another task
 * pool with smaller tasks. When other threads are busy they will continue
 * working on their own tasks, if not they will join in, no new threads will
 * be launched.
 */

typedef enum TaskPriority {
  TASK_PRIORITY_LOW,
  TASK_PRIORITY_HIGH,
} TaskPriority;

typedef struct TaskPool TaskPool;
typedef void (*TaskRunFunction)(TaskPool *__restrict pool, void *taskdata);
typedef void (*TaskFreeFunction)(TaskPool *__restrict pool, void *taskdata);

/* Regular task pool that immediately starts executing tasks as soon as they
 * are pushed, either on the current or another thread. */
TaskPool *BLI_task_pool_create(void *userdata, TaskPriority priority);

/* Background: always run tasks in a background thread, never immediately
 * execute them. For running background jobs. */
TaskPool *BLI_task_pool_create_background(void *userdata, TaskPriority priority);

/* Background Serial: run tasks one after the other in the background,
 * without parallelization between the tasks. */
TaskPool *BLI_task_pool_create_background_serial(void *userdata, TaskPriority priority);

/* Suspended: don't execute tasks until work_and_wait is called. This is slower
 * as threads can't immediately start working. But it can be used if the data
 * structures the threads operate on are not fully initialized until all tasks
 * are created. */
TaskPool *BLI_task_pool_create_suspended(void *userdata, TaskPriority priority);

/* No threads: immediately executes tasks on the same thread. For debugging. */
TaskPool *BLI_task_pool_create_no_threads(void *userdata);

void BLI_task_pool_free(TaskPool *pool);

void BLI_task_pool_push(TaskPool *pool,
                        TaskRunFunction run,
                        void *taskdata,
                        bool free_taskdata,
                        TaskFreeFunction freedata);

/* work and wait until all tasks are done */
void BLI_task_pool_work_and_wait(TaskPool *pool);
/* cancel all tasks, keep worker threads running */
void BLI_task_pool_cancel(TaskPool *pool);

/* for worker threads, test if canceled */
bool BLI_task_pool_canceled(TaskPool *pool);

/* optional userdata pointer to pass along to run function */
void *BLI_task_pool_user_data(TaskPool *pool);

/* optional mutex to use from run function */
ThreadMutex *BLI_task_pool_user_mutex(TaskPool *pool);

/* Parallel for routines */

/* Per-thread specific data passed to the callback. */
typedef struct TaskParallelTLS {
  /* Copy of user-specifier chunk, which is copied from original chunk to all
   * worker threads. This is similar to OpenMP's firstprivate.
   */
  void *userdata_chunk;
} TaskParallelTLS;

typedef void (*TaskParallelRangeFunc)(void *__restrict userdata,
                                      const int iter,
                                      const TaskParallelTLS *__restrict tls);
typedef void (*TaskParallelReduceFunc)(const void *__restrict userdata,
                                       void *__restrict chunk_join,
                                       void *__restrict chunk);

typedef void (*TaskParallelFreeFunc)(const void *__restrict userdata, void *__restrict chunk);

typedef struct TaskParallelSettings {
  /* Whether caller allows to do threading of the particular range.
   * Usually set by some equation, which forces threading off when threading
   * overhead becomes higher than speed benefit.
   * BLI_task_parallel_range() by itself will always use threading when range
   * is higher than a chunk size. As in, threading will always be performed.
   */
  bool use_threading;
  /* Each instance of looping chunks will get a copy of this data
   * (similar to OpenMP's firstprivate).
   */
  void *userdata_chunk;       /* Pointer to actual data. */
  size_t userdata_chunk_size; /* Size of that data.  */
  /* Function called from calling thread once whole range have been
   * processed.
   */
  /* Function called to join user data chunk into another, to reduce
   * the result to the original userdata_chunk memory.
   * The reduce functions should have no side effects, so that they
   * can be run on any thread. */
  TaskParallelReduceFunc func_reduce;
  /* Function called to free data created by TaskParallelRangeFunc. */
  TaskParallelFreeFunc func_free;
  /* Minimum allowed number of range iterators to be handled by a single
   * thread. This allows to achieve following:
   * - Reduce amount of threading overhead.
   * - Partially occupy thread pool with ranges which are computationally
   *   expensive, but which are smaller than amount of available threads.
   *   For example, it's possible to multi-thread [0 .. 64] range into 4
   *   thread which will be doing 16 iterators each.
   * This is a preferred way to tell scheduler when to start threading than
   * having a global use_threading switch based on just range size.
   */
  int min_iter_per_thread;
} TaskParallelSettings;

BLI_INLINE void BLI_parallel_range_settings_defaults(TaskParallelSettings *settings);

void BLI_task_parallel_range(const int start,
                             const int stop,
                             void *userdata,
                             TaskParallelRangeFunc func,
                             const TaskParallelSettings *settings);

/* This data is shared between all tasks, its access needs thread lock or similar protection.
 */
typedef struct TaskParallelIteratorStateShared {
  /* Maximum amount of items to acquire at once. */
  int chunk_size;
  /* Next item to be acquired. */
  void *next_item;
  /* Index of the next item to be acquired. */
  int next_index;
  /* Indicates that end of iteration has been reached. */
  bool is_finished;
  /* Helper lock to protect access to this data in iterator getter callback,
   * can be ignored (if the callback implements its own protection system, using atomics e.g.).
   * Will be NULL when iterator is actually processed in a single thread. */
  SpinLock *spin_lock;
} TaskParallelIteratorStateShared;

typedef void (*TaskParallelIteratorIterFunc)(void *__restrict userdata,
                                             const TaskParallelTLS *__restrict tls,
                                             void **r_next_item,
                                             int *r_next_index,
                                             bool *r_do_abort);

typedef void (*TaskParallelIteratorFunc)(void *__restrict userdata,
                                         void *item,
                                         int index,
                                         const TaskParallelTLS *__restrict tls);

void BLI_task_parallel_iterator(void *userdata,
                                TaskParallelIteratorIterFunc iter_func,
                                void *init_item,
                                const int init_index,
                                const int tot_items,
                                TaskParallelIteratorFunc func,
                                const TaskParallelSettings *settings);

void BLI_task_parallel_listbase(struct ListBase *listbase,
                                void *userdata,
                                TaskParallelIteratorFunc func,
                                const TaskParallelSettings *settings);

typedef struct MempoolIterData MempoolIterData;
typedef void (*TaskParallelMempoolFunc)(void *userdata, MempoolIterData *iter);
void BLI_task_parallel_mempool(struct BLI_mempool *mempool,
                               void *userdata,
                               TaskParallelMempoolFunc func,
                               const bool use_threading);

/* TODO(sergey): Think of a better place for this. */
BLI_INLINE void BLI_parallel_range_settings_defaults(TaskParallelSettings *settings)
{
  memset(settings, 0, sizeof(*settings));
  settings->use_threading = true;
  /* Use default heuristic to define actual chunk size. */
  settings->min_iter_per_thread = 0;
}

/* Don't use this, store any thread specific data in tls->userdata_chunk instead.
 * Only here for code to be removed. */
int BLI_task_parallel_thread_id(const TaskParallelTLS *tls);

#ifdef __cplusplus
}
#endif

#endif
