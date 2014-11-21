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

/** \file blender/blenlib/intern/task.c
 *  \ingroup bli
 *
 * A generic task system which can be used for any task based subsystem.
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_threads.h"

/* Types */

typedef struct Task {
	struct Task *next, *prev;

	TaskRunFunction run;
	void *taskdata;
	bool free_taskdata;
	TaskPool *pool;
} Task;

struct TaskPool {
	TaskScheduler *scheduler;

	volatile size_t num;
	volatile size_t done;
	volatile int num_threads;
	volatile int currently_running_tasks;
	ThreadMutex num_mutex;
	ThreadCondition num_cond;

	void *userdata;
	ThreadMutex user_mutex;

	volatile bool do_cancel;
};

struct TaskScheduler {
	pthread_t *threads;
	struct TaskThread *task_threads;
	int num_threads;

	ListBase queue;
	ThreadMutex queue_mutex;
	ThreadCondition queue_cond;

	volatile bool do_exit;
};

typedef struct TaskThread {
	TaskScheduler *scheduler;
	int id;
} TaskThread;

/* Task Scheduler */

static void task_pool_num_decrease(TaskPool *pool, size_t done)
{
	BLI_mutex_lock(&pool->num_mutex);

	BLI_assert(pool->num >= done);

	pool->num -= done;
	pool->currently_running_tasks -= done;
	pool->done += done;

	if (pool->num == 0)
		BLI_condition_notify_all(&pool->num_cond);

	BLI_mutex_unlock(&pool->num_mutex);
}

static void task_pool_num_increase(TaskPool *pool)
{
	BLI_mutex_lock(&pool->num_mutex);

	pool->num++;
	BLI_condition_notify_all(&pool->num_cond);

	BLI_mutex_unlock(&pool->num_mutex);
}

static bool task_scheduler_thread_wait_pop(TaskScheduler *scheduler, Task **task)
{
	bool found_task = false;
	BLI_mutex_lock(&scheduler->queue_mutex);

	while (!scheduler->queue.first && !scheduler->do_exit)
		BLI_condition_wait(&scheduler->queue_cond, &scheduler->queue_mutex);

	do {
		Task *current_task;
		if (!scheduler->queue.first) {
			BLI_mutex_unlock(&scheduler->queue_mutex);
			BLI_assert(scheduler->do_exit);
			return false;
		}
		for (current_task = scheduler->queue.first;
		     current_task != NULL;
		     current_task = current_task->next)
		{
			TaskPool *pool = current_task->pool;
			if (pool->num_threads == 0 ||
			    pool->currently_running_tasks < pool->num_threads)
			{
				*task = current_task;
				found_task = true;
				pool->currently_running_tasks++;
				BLI_remlink(&scheduler->queue, *task);
				break;
			}
		}
		if (!found_task)
			BLI_condition_wait(&scheduler->queue_cond, &scheduler->queue_mutex);
	} while (!found_task);

	BLI_mutex_unlock(&scheduler->queue_mutex);

	return true;
}

static void *task_scheduler_thread_run(void *thread_p)
{
	TaskThread *thread = (TaskThread *) thread_p;
	TaskScheduler *scheduler = thread->scheduler;
	int thread_id = thread->id;
	Task *task;

	/* keep popping off tasks */
	while (task_scheduler_thread_wait_pop(scheduler, &task)) {
		TaskPool *pool = task->pool;

		/* run task */
		task->run(pool, task->taskdata, thread_id);

		/* delete task */
		if (task->free_taskdata)
			MEM_freeN(task->taskdata);
		MEM_freeN(task);

		/* notify pool task was done */
		task_pool_num_decrease(pool, 1);
	}

	return NULL;
}

TaskScheduler *BLI_task_scheduler_create(int num_threads)
{
	TaskScheduler *scheduler = MEM_callocN(sizeof(TaskScheduler), "TaskScheduler");

	/* multiple places can use this task scheduler, sharing the same
	 * threads, so we keep track of the number of users. */
	scheduler->do_exit = false;

	BLI_listbase_clear(&scheduler->queue);
	BLI_mutex_init(&scheduler->queue_mutex);
	BLI_condition_init(&scheduler->queue_cond);

	if (num_threads == 0) {
		/* automatic number of threads will be main thread + num cores */
		num_threads = BLI_system_thread_count();
	}

	/* main thread will also work, so we count it too */
	num_threads -= 1;

	/* launch threads that will be waiting for work */
	if (num_threads > 0) {
		int i;

		scheduler->num_threads = num_threads;
		scheduler->threads = MEM_callocN(sizeof(pthread_t) * num_threads, "TaskScheduler threads");
		scheduler->task_threads = MEM_callocN(sizeof(TaskThread) * num_threads, "TaskScheduler task threads");

		for (i = 0; i < num_threads; i++) {
			TaskThread *thread = &scheduler->task_threads[i];
			thread->scheduler = scheduler;
			thread->id = i + 1;

			if (pthread_create(&scheduler->threads[i], NULL, task_scheduler_thread_run, thread) != 0) {
				fprintf(stderr, "TaskScheduler failed to launch thread %d/%d\n", i, num_threads);
				MEM_freeN(thread);
			}
		}
	}
	
	return scheduler;
}

void BLI_task_scheduler_free(TaskScheduler *scheduler)
{
	Task *task;

	/* stop all waiting threads */
	BLI_mutex_lock(&scheduler->queue_mutex);
	scheduler->do_exit = true;
	BLI_condition_notify_all(&scheduler->queue_cond);
	BLI_mutex_unlock(&scheduler->queue_mutex);

	/* delete threads */
	if (scheduler->threads) {
		int i;

		for (i = 0; i < scheduler->num_threads; i++) {
			if (pthread_join(scheduler->threads[i], NULL) != 0)
				fprintf(stderr, "TaskScheduler failed to join thread %d/%d\n", i, scheduler->num_threads);
		}

		MEM_freeN(scheduler->threads);
	}

	/* Delete task thread data */
	if (scheduler->task_threads) {
		MEM_freeN(scheduler->task_threads);
	}

	/* delete leftover tasks */
	for (task = scheduler->queue.first; task; task = task->next) {
		if (task->free_taskdata)
			MEM_freeN(task->taskdata);
	}
	BLI_freelistN(&scheduler->queue);

	/* delete mutex/condition */
	BLI_mutex_end(&scheduler->queue_mutex);
	BLI_condition_end(&scheduler->queue_cond);

	MEM_freeN(scheduler);
}

int BLI_task_scheduler_num_threads(TaskScheduler *scheduler)
{
	return scheduler->num_threads + 1;
}

static void task_scheduler_push(TaskScheduler *scheduler, Task *task, TaskPriority priority)
{
	task_pool_num_increase(task->pool);

	/* add task to queue */
	BLI_mutex_lock(&scheduler->queue_mutex);

	if (priority == TASK_PRIORITY_HIGH)
		BLI_addhead(&scheduler->queue, task);
	else
		BLI_addtail(&scheduler->queue, task);

	BLI_condition_notify_one(&scheduler->queue_cond);
	BLI_mutex_unlock(&scheduler->queue_mutex);
}

static void task_scheduler_clear(TaskScheduler *scheduler, TaskPool *pool)
{
	Task *task, *nexttask;
	size_t done = 0;

	BLI_mutex_lock(&scheduler->queue_mutex);

	/* free all tasks from this pool from the queue */
	for (task = scheduler->queue.first; task; task = nexttask) {
		nexttask = task->next;

		if (task->pool == pool) {
			if (task->free_taskdata)
				MEM_freeN(task->taskdata);
			BLI_freelinkN(&scheduler->queue, task);

			done++;
		}
	}

	BLI_mutex_unlock(&scheduler->queue_mutex);

	/* notify done */
	task_pool_num_decrease(pool, done);
}

/* Task Pool */

TaskPool *BLI_task_pool_create(TaskScheduler *scheduler, void *userdata)
{
	TaskPool *pool = MEM_callocN(sizeof(TaskPool), "TaskPool");

	pool->scheduler = scheduler;
	pool->num = 0;
	pool->num_threads = 0;
	pool->currently_running_tasks = 0;
	pool->do_cancel = false;

	BLI_mutex_init(&pool->num_mutex);
	BLI_condition_init(&pool->num_cond);

	pool->userdata = userdata;
	BLI_mutex_init(&pool->user_mutex);

	/* Ensure malloc will go fine from threads,
	 *
	 * This is needed because we could be in main thread here
	 * and malloc could be non-threda safe at this point because
	 * no other jobs are running.
	 */
	BLI_begin_threaded_malloc();

	return pool;
}

void BLI_task_pool_free(TaskPool *pool)
{
	BLI_task_pool_stop(pool);

	BLI_mutex_end(&pool->num_mutex);
	BLI_condition_end(&pool->num_cond);

	BLI_mutex_end(&pool->user_mutex);

	MEM_freeN(pool);

	BLI_end_threaded_malloc();
}

void BLI_task_pool_push(TaskPool *pool, TaskRunFunction run,
	void *taskdata, bool free_taskdata, TaskPriority priority)
{
	Task *task = MEM_callocN(sizeof(Task), "Task");

	task->run = run;
	task->taskdata = taskdata;
	task->free_taskdata = free_taskdata;
	task->pool = pool;

	task_scheduler_push(pool->scheduler, task, priority);
}

void BLI_task_pool_work_and_wait(TaskPool *pool)
{
	TaskScheduler *scheduler = pool->scheduler;

	BLI_mutex_lock(&pool->num_mutex);

	while (pool->num != 0) {
		Task *task, *work_task = NULL;
		bool found_task = false;

		BLI_mutex_unlock(&pool->num_mutex);

		BLI_mutex_lock(&scheduler->queue_mutex);

		/* find task from this pool. if we get a task from another pool,
		 * we can get into deadlock */

		if (pool->num_threads == 0 ||
		    pool->currently_running_tasks < pool->num_threads)
		{
			for (task = scheduler->queue.first; task; task = task->next) {
				if (task->pool == pool) {
					work_task = task;
					found_task = true;
					BLI_remlink(&scheduler->queue, task);
					break;
				}
			}
		}

		BLI_mutex_unlock(&scheduler->queue_mutex);

		/* if found task, do it, otherwise wait until other tasks are done */
		if (found_task) {
			/* run task */
			pool->currently_running_tasks++;
			work_task->run(pool, work_task->taskdata, 0);

			/* delete task */
			if (work_task->free_taskdata)
				MEM_freeN(work_task->taskdata);
			MEM_freeN(work_task);

			/* notify pool task was done */
			task_pool_num_decrease(pool, 1);
		}

		BLI_mutex_lock(&pool->num_mutex);
		if (pool->num == 0)
			break;

		if (!found_task)
			BLI_condition_wait(&pool->num_cond, &pool->num_mutex);
	}

	BLI_mutex_unlock(&pool->num_mutex);
}

void BLI_pool_set_num_threads(TaskPool *pool, int num_threads)
{
	/* NOTE: Don't try to modify threads while tasks are running! */
	pool->num_threads = num_threads;
}

void BLI_task_pool_cancel(TaskPool *pool)
{
	pool->do_cancel = true;

	task_scheduler_clear(pool->scheduler, pool);

	/* wait until all entries are cleared */
	BLI_mutex_lock(&pool->num_mutex);
	while (pool->num)
		BLI_condition_wait(&pool->num_cond, &pool->num_mutex);
	BLI_mutex_unlock(&pool->num_mutex);

	pool->do_cancel = false;
}

void BLI_task_pool_stop(TaskPool *pool)
{
	task_scheduler_clear(pool->scheduler, pool);

	BLI_assert(pool->num == 0);
}

bool BLI_task_pool_canceled(TaskPool *pool)
{
	return pool->do_cancel;
}

void *BLI_task_pool_userdata(TaskPool *pool)
{
	return pool->userdata;
}

ThreadMutex *BLI_task_pool_user_mutex(TaskPool *pool)
{
	return &pool->user_mutex;
}

size_t BLI_task_pool_tasks_done(TaskPool *pool)
{
	return pool->done;
}

/* Parallel range routines */

/**
 *
 * Main functions:
 * - #BLI_task_parallel_range
 *
 * TODO:
 * - #BLI_task_parallel_foreach_listbase (#ListBase - double linked list)
 * - #BLI_task_parallel_foreach_link (#Link - single linked list)
 * - #BLI_task_parallel_foreach_ghash/gset (#GHash/#GSet - hash & set)
 * - #BLI_task_parallel_foreach_mempool (#BLI_mempool - iterate over mempools)
 *
 * Possible improvements:
 *
 * - Chunk iterations to reduce number of spin locks.
 */

typedef struct ParallelRangeState {
	int start, stop;
	void *userdata;
	TaskParallelRangeFunc func;

	int iter;
	int chunk_size;
	SpinLock lock;
} ParallelRangeState;

BLI_INLINE bool parallel_range_next_iter_get(
        ParallelRangeState * __restrict state,
        int * __restrict iter, int * __restrict count)
{
	bool result = false;
	if (state->iter < state->stop) {
		BLI_spin_lock(&state->lock);
		if (state->iter < state->stop) {
			*count = min_ii(state->chunk_size, state->stop - state->iter);
			*iter = state->iter;
			state->iter += *count;
			result = true;
		}
		BLI_spin_unlock(&state->lock);
	}
	return result;
}

static void parallel_range_func(
        TaskPool * __restrict pool,
        void *UNUSED(taskdata),
        int UNUSED(threadid))
{
	ParallelRangeState * __restrict state = BLI_task_pool_userdata(pool);
	int iter, count;
	while (parallel_range_next_iter_get(state, &iter, &count)) {
		int i;
		for (i = 0; i < count; ++i) {
			state->func(state->userdata, iter + i);
		}
	}
}

void BLI_task_parallel_range_ex(
        int start, int stop,
        void *userdata,
        TaskParallelRangeFunc func,
        const int range_threshold,
        const bool use_dynamic_scheduling)
{
	TaskScheduler *task_scheduler;
	TaskPool *task_pool;
	ParallelRangeState state;
	int i, num_threads, num_tasks;

	BLI_assert(start < stop);

	/* If it's not enough data to be crunched, don't bother with tasks at all,
	 * do everything from the main thread.
	 */
	if (stop - start < range_threshold) {
		for (i = start; i < stop; ++i) {
			func(userdata, i);
		}
		return;
	}

	task_scheduler = BLI_task_scheduler_get();
	task_pool = BLI_task_pool_create(task_scheduler, &state);
	num_threads = BLI_task_scheduler_num_threads(task_scheduler);

	/* The idea here is to prevent creating task for each of the loop iterations
	 * and instead have tasks which are evenly distributed across CPU cores and
	 * pull next iter to be crunched using the queue.
	 */
	num_tasks = num_threads * 2;

	BLI_spin_init(&state.lock);
	state.start = start;
	state.stop = stop;
	state.userdata = userdata;
	state.func = func;
	state.iter = start;
	if (use_dynamic_scheduling) {
		state.chunk_size = 32;
	}
	else {
		state.chunk_size = (stop - start) / (num_tasks);
	}

	for (i = 0; i < num_tasks; i++) {
		BLI_task_pool_push(task_pool,
		                   parallel_range_func,
		                   NULL, false,
		                   TASK_PRIORITY_HIGH);
	}

	BLI_task_pool_work_and_wait(task_pool);
	BLI_task_pool_free(task_pool);

	BLI_spin_end(&state.lock);
}

void BLI_task_parallel_range(
        int start, int stop,
        void *userdata,
        TaskParallelRangeFunc func)
{
	BLI_task_parallel_range_ex(start, stop, userdata, func, 64, false);
}
