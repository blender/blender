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

struct Link;
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

enum {
  TASK_SCHEDULER_AUTO_THREADS = 0,
  TASK_SCHEDULER_SINGLE_THREAD = 1,
};

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
typedef void (*TaskFreeFunction)(TaskPool *__restrict pool, void *taskdata, int threadid);

TaskPool *BLI_task_pool_create(TaskScheduler *scheduler, void *userdata);
TaskPool *BLI_task_pool_create_background(TaskScheduler *scheduler, void *userdata);
TaskPool *BLI_task_pool_create_suspended(TaskScheduler *scheduler, void *userdata);
void BLI_task_pool_free(TaskPool *pool);

void BLI_task_pool_push_ex(TaskPool *pool,
                           TaskRunFunction run,
                           void *taskdata,
                           bool free_taskdata,
                           TaskFreeFunction freedata,
                           TaskPriority priority);
void BLI_task_pool_push(TaskPool *pool,
                        TaskRunFunction run,
                        void *taskdata,
                        bool free_taskdata,
                        TaskPriority priority);
void BLI_task_pool_push_from_thread(TaskPool *pool,
                                    TaskRunFunction run,
                                    void *taskdata,
                                    bool free_taskdata,
                                    TaskPriority priority,
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
void *BLI_task_pool_userdata(TaskPool *pool);

/* optional mutex to use from run function */
ThreadMutex *BLI_task_pool_user_mutex(TaskPool *pool);

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
typedef struct ParallelRangeTLS {
  /* Identifier of the thread who this data belongs to. */
  int thread_id;
  /* Copy of user-specifier chunk, which is copied from original chunk to all
   * worker threads. This is similar to OpenMP's firstprivate.
   */
  void *userdata_chunk;
} ParallelRangeTLS;

typedef void (*TaskParallelRangeFunc)(void *__restrict userdata,
                                      const int iter,
                                      const ParallelRangeTLS *__restrict tls);
typedef void (*TaskParallelRangeFuncFinalize)(void *__restrict userdata,
                                              void *__restrict userdata_chunk);

typedef struct ParallelRangeSettings {
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
  TaskParallelRangeFuncFinalize func_finalize;
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
} ParallelRangeSettings;

BLI_INLINE void BLI_parallel_range_settings_defaults(ParallelRangeSettings *settings);

void BLI_task_parallel_range(const int start,
                             const int stop,
                             void *userdata,
                             TaskParallelRangeFunc func,
                             const ParallelRangeSettings *settings);

typedef void (*TaskParallelListbaseFunc)(void *userdata, struct Link *iter, int index);
void BLI_task_parallel_listbase(struct ListBase *listbase,
                                void *userdata,
                                TaskParallelListbaseFunc func,
                                const bool use_threading);

typedef struct MempoolIterData MempoolIterData;
typedef void (*TaskParallelMempoolFunc)(void *userdata, MempoolIterData *iter);
void BLI_task_parallel_mempool(struct BLI_mempool *mempool,
                               void *userdata,
                               TaskParallelMempoolFunc func,
                               const bool use_threading);

/* TODO(sergey): Think of a better place for this. */
BLI_INLINE void BLI_parallel_range_settings_defaults(ParallelRangeSettings *settings)
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
