/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_TASK_H__
#define __BLI_TASK_H__ 

struct Link;
struct ListBase;

/** \file BLI_task.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_threads.h"
#include "BLI_utildefines.h"

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
	TASK_SCHEDULER_SINGLE_THREAD = 1
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
	TASK_PRIORITY_HIGH
} TaskPriority;

typedef struct TaskPool TaskPool;
typedef void (*TaskRunFunction)(TaskPool *__restrict pool, void *taskdata, int threadid);
typedef void (*TaskFreeFunction)(TaskPool *__restrict pool, void *taskdata, int threadid);

TaskPool *BLI_task_pool_create(TaskScheduler *scheduler, void *userdata);
TaskPool *BLI_task_pool_create_background(TaskScheduler *scheduler, void *userdata);
TaskPool *BLI_task_pool_create_suspended(TaskScheduler *scheduler, void *userdata);
void BLI_task_pool_free(TaskPool *pool);

void BLI_task_pool_push_ex(
        TaskPool *pool, TaskRunFunction run, void *taskdata,
        bool free_taskdata, TaskFreeFunction freedata, TaskPriority priority);
void BLI_task_pool_push(TaskPool *pool, TaskRunFunction run,
        void *taskdata, bool free_taskdata, TaskPriority priority);
void BLI_task_pool_push_from_thread(TaskPool *pool, TaskRunFunction run,
        void *taskdata, bool free_taskdata, TaskPriority priority, int thread_id);

/* work and wait until all tasks are done */
void BLI_task_pool_work_and_wait(TaskPool *pool);
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
typedef void (*TaskParallelRangeFunc)(void *userdata, const int iter);
typedef void (*TaskParallelRangeFuncEx)(void *userdata, void *userdata_chunk, const int iter, const int thread_id);
typedef void (*TaskParallelRangeFuncFinalize)(void *userdata, void *userdata_chunk);
void BLI_task_parallel_range_ex(
        int start, int stop,
        void *userdata,
        void *userdata_chunk,
        const size_t userdata_chunk_size,
        TaskParallelRangeFuncEx func_ex,
        const bool use_threading,
        const bool use_dynamic_scheduling);
void BLI_task_parallel_range(
        int start, int stop,
        void *userdata,
        TaskParallelRangeFunc func,
        const bool use_threading);

void BLI_task_parallel_range_finalize(
        int start, int stop,
        void *userdata,
        void *userdata_chunk,
        const size_t userdata_chunk_size,
        TaskParallelRangeFuncEx func_ex,
        TaskParallelRangeFuncFinalize func_finalize,
        const bool use_threading,
        const bool use_dynamic_scheduling);

typedef void (*TaskParallelListbaseFunc)(void *userdata,
                                         struct Link *iter,
                                         int index);
void BLI_task_parallel_listbase(
        struct ListBase *listbase,
        void *userdata,
        TaskParallelListbaseFunc func,
        const bool use_threading);

#ifdef __cplusplus
}
#endif

#endif

