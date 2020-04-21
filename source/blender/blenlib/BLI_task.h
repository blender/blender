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

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_threads.h"
#include "BLI_utildefines.h"

struct BLI_mempool;

/* Task Scheduler
 *
 * Central scheduler that holds running threads ready to execute tasks. A single
 * queue holds the task from all pools.
 *
 * Init/exit must be called before/after any task pools are created/freed, and
 * must be called from the main threads. All other scheduler and pool functions
 * are thread-safe. */

typedef struct TaskScheduler TaskScheduler;

TaskScheduler *BLI_task_scheduler_create(int num_threads);
void BLI_task_scheduler_free(TaskScheduler *scheduler);

int BLI_task_scheduler_num_threads(TaskScheduler *scheduler);

/* Task Pool
 *
 * Pool of tasks that will be executed by the central TaskScheduler. For each
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
typedef void (*TaskRunFunction)(TaskPool *__restrict pool, void *taskdata, int threadid);
typedef void (*TaskFreeFunction)(TaskPool *__restrict pool, void *taskdata);

TaskPool *BLI_task_pool_create(TaskScheduler *scheduler, void *userdata, TaskPriority priority);
TaskPool *BLI_task_pool_create_background(TaskScheduler *scheduler,
                                          void *userdata,
                                          TaskPriority priority);
TaskPool *BLI_task_pool_create_suspended(TaskScheduler *scheduler,
                                         void *userdata,
                                         TaskPriority priority);
void BLI_task_pool_free(TaskPool *pool);

void BLI_task_pool_push(TaskPool *pool,
                        TaskRunFunction run,
                        void *taskdata,
                        bool free_taskdata,
                        TaskFreeFunction freedata);
void BLI_task_pool_push_from_thread(TaskPool *pool,
                                    TaskRunFunction run,
                                    void *taskdata,
                                    bool free_taskdata,
                                    TaskFreeFunction freedata,
                                    int thread_id);

/* work and wait until all tasks are done */
void BLI_task_pool_work_and_wait(TaskPool *pool);
/* work and wait until all tasks are done, then reset to the initial suspended state */
void BLI_task_pool_work_wait_and_reset(TaskPool *pool);
/* cancel all tasks, keep worker threads running */
void BLI_task_pool_cancel(TaskPool *pool);

/* for worker threads, test if canceled */
bool BLI_task_pool_canceled(TaskPool *pool);

/* optional userdata pointer to pass along to run function */
void *BLI_task_pool_user_data(TaskPool *pool);

/* optional mutex to use from run function */
ThreadMutex *BLI_task_pool_user_mutex(TaskPool *pool);

/* Thread ID of thread that created the task pool. */
int BLI_task_pool_creator_thread_id(TaskPool *pool);

/* Delayed push, use that to reduce thread overhead by accumulating
 * all new tasks into local queue first and pushing it to scheduler
 * from within a single mutex lock.
 */
void BLI_task_pool_delayed_push_begin(TaskPool *pool, int thread_id);
void BLI_task_pool_delayed_push_end(TaskPool *pool, int thread_id);

/* Parallel for routines */

typedef enum eTaskSchedulingMode {
  /* Task scheduler will divide overall work into equal chunks, scheduling
   * even chunks to all worker threads.
   * Least run time benefit, ideal for cases when each task requires equal
   * amount of compute power.
   */
  TASK_SCHEDULING_STATIC,
  /* Task scheduler will schedule small amount of work to each worker thread.
   * Has more run time overhead, but deals much better with cases when each
   * part of the work requires totally different amount of compute power.
   */
  TASK_SCHEDULING_DYNAMIC,
} eTaskSchedulingMode;

/* Per-thread specific data passed to the callback. */
typedef struct TaskParallelTLS {
  /* Identifier of the thread who this data belongs to. */
  int thread_id;
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
  /* Scheduling mode to use for this parallel range invocation. */
  eTaskSchedulingMode scheduling_mode;
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
                             TaskParallelSettings *settings);

typedef struct TaskParallelRangePool TaskParallelRangePool;
struct TaskParallelRangePool *BLI_task_parallel_range_pool_init(
    const struct TaskParallelSettings *settings);
void BLI_task_parallel_range_pool_push(struct TaskParallelRangePool *range_pool,
                                       const int start,
                                       const int stop,
                                       void *userdata,
                                       TaskParallelRangeFunc func,
                                       const struct TaskParallelSettings *settings);
void BLI_task_parallel_range_pool_work_and_wait(struct TaskParallelRangePool *range_pool);
void BLI_task_parallel_range_pool_free(struct TaskParallelRangePool *range_pool);

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
  settings->scheduling_mode = TASK_SCHEDULING_STATIC;
  /* Use default heuristic to define actual chunk size. */
  settings->min_iter_per_thread = 0;
}

#ifdef __cplusplus
}
#endif

#endif
