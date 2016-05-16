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

#include "DNA_listBase.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_threads.h"

#include "atomic_ops.h"

/* Define this to enable some detailed statistic print. */
#undef DEBUG_STATS

/* Types */

/* Number of per-thread pre-allocated tasks.
 *
 * For more details see description of TaskMemPool.
 */
#define MEMPOOL_SIZE 256

typedef struct Task {
	struct Task *next, *prev;

	TaskRunFunction run;
	void *taskdata;
	bool free_taskdata;
	TaskFreeFunction freedata;
	TaskPool *pool;
} Task;

/* This is a per-thread storage of pre-allocated tasks.
 *
 * The idea behind this is simple: reduce amount of malloc() calls when pushing
 * new task to the pool. This is done by keeping memory from the tasks which
 * were finished already, so instead of freeing that memory we put it to the
 * pool for the later re-use.
 *
 * The tricky part here is to avoid any inter-thread synchronization, hence no
 * lock must exist around this pool. The pool will become an owner of the pointer
 * from freed task, and only corresponding thread will be able to use this pool
 * (no memory stealing and such).
 *
 * This leads to the following use of the pool:
 *
 * - task_push() should provide proper thread ID from which the task is being
 *   pushed from.
 *
 * - Task allocation function which check corresponding memory pool and if there
 *   is any memory in there it'll mark memory as re-used, remove it from the pool
 *   and use that memory for the new task.
 *
 *   At this moment task queue owns the memory.
 *
 * - When task is done and task_free() is called the memory will be put to the
 *  pool which corresponds to a thread which handled the task.
 */
typedef struct TaskMemPool {
	/* Number of pre-allocated tasks in the pool. */
	int num_tasks;
	/* Pre-allocated task memory pointers. */
	Task *tasks[MEMPOOL_SIZE];
} TaskMemPool;

#ifdef DEBUG_STATS
typedef struct TaskMemPoolStats {
	/* Number of allocations. */
	int num_alloc;
	/* Number of avoided allocations (pointer was re-used from the pool). */
	int num_reuse;
	/* Number of discarded memory due to pool saturation, */
	int num_discard;
} TaskMemPoolStats;
#endif

struct TaskPool {
	TaskScheduler *scheduler;

	volatile size_t num;
	volatile size_t done;
	size_t num_threads;
	size_t currently_running_tasks;
	ThreadMutex num_mutex;
	ThreadCondition num_cond;

	void *userdata;
	ThreadMutex user_mutex;

	volatile bool do_cancel;

	/* If set, this pool may never be work_and_wait'ed, which means TaskScheduler
	 * has to use its special background fallback thread in case we are in
	 * single-threaded situation.
	 */
	bool run_in_background;

	/* This pool is used for caching task pointers for thread id 0.
	 * This could either point to a global scheduler's task_mempool[0] if the
	 * pool is handled form the main thread or point to task_mempool_local
	 * otherwise.
	 *
	 * This way we solve possible threading conflicts accessing same global
	 * memory pool from multiple threads from which wait_work() is called.
	 */
	TaskMemPool *task_mempool;
	TaskMemPool task_mempool_local;

#ifdef DEBUG_STATS
	TaskMemPoolStats *mempool_stats;
#endif
};

struct TaskScheduler {
	pthread_t *threads;
	struct TaskThread *task_threads;
	TaskMemPool *task_mempool;
	int num_threads;
	bool background_thread_only;

	ListBase queue;
	ThreadMutex queue_mutex;
	ThreadCondition queue_cond;

	volatile bool do_exit;
};

typedef struct TaskThread {
	TaskScheduler *scheduler;
	int id;
} TaskThread;

/* Helper */
static void task_data_free(Task *task, const int thread_id)
{
	if (task->free_taskdata) {
		if (task->freedata) {
			task->freedata(task->pool, task->taskdata, thread_id);
		}
		else {
			MEM_freeN(task->taskdata);
		}
	}
}

BLI_INLINE TaskMemPool *get_task_mempool(TaskPool *pool, const int thread_id)
{
	if (thread_id == 0) {
		return pool->task_mempool;
	}
	return &pool->scheduler->task_mempool[thread_id];
}

static Task *task_alloc(TaskPool *pool, const int thread_id)
{
	assert(thread_id <= pool->scheduler->num_threads);
	if (thread_id != -1) {
		assert(thread_id >= 0);
		TaskMemPool *mem_pool = get_task_mempool(pool, thread_id);
		/* Try to re-use task memory from a thread local storage. */
		if (mem_pool->num_tasks > 0) {
			--mem_pool->num_tasks;
			/* Success! We've just avoided task allocation. */
#ifdef DEBUG_STATS
			pool->mempool_stats[thread_id].num_reuse++;
#endif
			return mem_pool->tasks[mem_pool->num_tasks];
		}
		/* We are doomed to allocate new task data. */
#ifdef DEBUG_STATS
		pool->mempool_stats[thread_id].num_alloc++;
#endif
	}
	return MEM_mallocN(sizeof(Task), "New task");
}

static void task_free(TaskPool *pool, Task *task, const int thread_id)
{
	task_data_free(task, thread_id);
	assert(thread_id >= 0);
	assert(thread_id <= pool->scheduler->num_threads);
	TaskMemPool *mem_pool = get_task_mempool(pool, thread_id);
	if (mem_pool->num_tasks < MEMPOOL_SIZE - 1) {
		/* Successfully allowed the task to be re-used later. */
		mem_pool->tasks[mem_pool->num_tasks] = task;
		++mem_pool->num_tasks;
	}
	else {
		/* Local storage saturated, no other way than just discard
		 * the memory.
		 *
		 * TODO(sergey): We can perhaps store such pointer in a global
		 * scheduler pool, maybe it'll be faster than discarding and
		 * allocating again.
		 */
		MEM_freeN(task);
#ifdef DEBUG_STATS
		pool->mempool_stats[thread_id].num_discard++;
#endif
	}
}

/* Task Scheduler */

static void task_pool_num_decrease(TaskPool *pool, size_t done)
{
	BLI_mutex_lock(&pool->num_mutex);

	BLI_assert(pool->num >= done);

	pool->num -= done;
	atomic_sub_z(&pool->currently_running_tasks, done);
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

		/* Assuming we can only have a void queue in 'exit' case here seems logical (we should only be here after
		 * our worker thread has been woken up from a condition_wait(), which only happens after a new task was
		 * added to the queue), but it is wrong.
		 * Waiting on condition may wake up the thread even if condition is not signaled (spurious wake-ups), and some
		 * race condition may also empty the queue **after** condition has been signaled, but **before** awoken thread
		 * reaches this point...
		 * See http://stackoverflow.com/questions/8594591
		 *
		 * So we only abort here if do_exit is set.
		 */
		if (scheduler->do_exit) {
			BLI_mutex_unlock(&scheduler->queue_mutex);
			return false;
		}

		for (current_task = scheduler->queue.first;
		     current_task != NULL;
		     current_task = current_task->next)
		{
			TaskPool *pool = current_task->pool;

			if (scheduler->background_thread_only && !pool->run_in_background) {
				continue;
			}

			if (pool->num_threads == 0 ||
			    pool->currently_running_tasks < pool->num_threads)
			{
				*task = current_task;
				found_task = true;
				atomic_add_z(&pool->currently_running_tasks, 1);
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
		task_free(pool, task, thread_id);

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

	/* Add background-only thread if needed. */
	if (num_threads == 0) {
	    scheduler->background_thread_only = true;
	    num_threads = 1;
	}

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
			}
		}

		scheduler->task_mempool = MEM_callocN(sizeof(*scheduler->task_mempool) * (num_threads + 1),
		                                      "TaskScheduler task_mempool");
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

	/* Delete task memory pool */
	if (scheduler->task_mempool) {
		for (int i = 0; i <= scheduler->num_threads; ++i) {
			for (int j = 0; j < scheduler->task_mempool[i].num_tasks; ++j) {
				MEM_freeN(scheduler->task_mempool[i].tasks[j]);
			}
		}
		MEM_freeN(scheduler->task_mempool);
	}

	/* delete leftover tasks */
	for (task = scheduler->queue.first; task; task = task->next) {
		task_data_free(task, 0);
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
			task_data_free(task, 0);
			BLI_freelinkN(&scheduler->queue, task);

			done++;
		}
	}

	BLI_mutex_unlock(&scheduler->queue_mutex);

	/* notify done */
	task_pool_num_decrease(pool, done);
}

/* Task Pool */

static TaskPool *task_pool_create_ex(TaskScheduler *scheduler, void *userdata, const bool is_background)
{
	TaskPool *pool = MEM_mallocN(sizeof(TaskPool), "TaskPool");

#ifndef NDEBUG
	/* Assert we do not try to create a background pool from some parent task - those only work OK from main thread. */
	if (is_background) {
		const pthread_t thread_id = pthread_self();
		int i = scheduler->num_threads;

		while (i--) {
			BLI_assert(!pthread_equal(scheduler->threads[i], thread_id));
		}
	}
#endif

	pool->scheduler = scheduler;
	pool->num = 0;
	pool->done = 0;
	pool->num_threads = 0;
	pool->currently_running_tasks = 0;
	pool->do_cancel = false;
	pool->run_in_background = is_background;

	BLI_mutex_init(&pool->num_mutex);
	BLI_condition_init(&pool->num_cond);

	pool->userdata = userdata;
	BLI_mutex_init(&pool->user_mutex);

	if (BLI_thread_is_main()) {
		pool->task_mempool = scheduler->task_mempool;
	}
	else {
		pool->task_mempool = &pool->task_mempool_local;
		pool->task_mempool_local.num_tasks = 0;
	}

#ifdef DEBUG_STATS
	pool->mempool_stats =
	        MEM_callocN(sizeof(*pool->mempool_stats) * (scheduler->num_threads + 1),
	                    "per-taskpool mempool stats");
#endif

	/* Ensure malloc will go fine from threads,
	 *
	 * This is needed because we could be in main thread here
	 * and malloc could be non-threda safe at this point because
	 * no other jobs are running.
	 */
	BLI_begin_threaded_malloc();

	return pool;
}

/**
 * Create a normal task pool.
 * This means that in single-threaded context, it will not be executed at all until you call
 * \a BLI_task_pool_work_and_wait() on it.
 */
TaskPool *BLI_task_pool_create(TaskScheduler *scheduler, void *userdata)
{
	return task_pool_create_ex(scheduler, userdata, false);
}

/**
 * Create a background task pool.
 * In multi-threaded context, there is no differences with \a BLI_task_pool_create(), but in single-threaded case
 * it is ensured to have at least one worker thread to run on (i.e. you do not have to call
 * \a BLI_task_pool_work_and_wait() on it to be sure it will be processed).
 *
 * \note Background pools are non-recursive (that is, you should not create other background pools in tasks assigned
 *       to a background pool, they could end never being executed, since the 'fallback' background thread is already
 *       busy with parent task in single-threaded context).
 */
TaskPool *BLI_task_pool_create_background(TaskScheduler *scheduler, void *userdata)
{
	return task_pool_create_ex(scheduler, userdata, true);
}

void BLI_task_pool_free(TaskPool *pool)
{
	BLI_task_pool_stop(pool);

	BLI_mutex_end(&pool->num_mutex);
	BLI_condition_end(&pool->num_cond);

	BLI_mutex_end(&pool->user_mutex);

	/* Free local memory pool, those pointers are lost forever. */
	if (pool->task_mempool == &pool->task_mempool_local) {
		for (int i = 0; i < pool->task_mempool_local.num_tasks; i++) {
			MEM_freeN(pool->task_mempool_local.tasks[i]);
		}
	}

#ifdef DEBUG_STATS
	printf("Thread ID    Allocated   Reused   Discarded\n");
	for (int i = 0; i < pool->scheduler->num_threads + 1; ++i) {
		printf("%02d           %05d       %05d    %05d\n",
		       i,
		       pool->mempool_stats[i].num_alloc,
		       pool->mempool_stats[i].num_reuse,
		       pool->mempool_stats[i].num_discard);
	}
	MEM_freeN(pool->mempool_stats);
#endif

	MEM_freeN(pool);

	BLI_end_threaded_malloc();
}

static void task_pool_push(
        TaskPool *pool, TaskRunFunction run, void *taskdata,
        bool free_taskdata, TaskFreeFunction freedata, TaskPriority priority,
        int thread_id)
{
	Task *task = task_alloc(pool, thread_id);

	task->run = run;
	task->taskdata = taskdata;
	task->free_taskdata = free_taskdata;
	task->freedata = freedata;
	task->pool = pool;

	task_scheduler_push(pool->scheduler, task, priority);
}

void BLI_task_pool_push_ex(
        TaskPool *pool, TaskRunFunction run, void *taskdata,
        bool free_taskdata, TaskFreeFunction freedata, TaskPriority priority)
{
	task_pool_push(pool, run, taskdata, free_taskdata, freedata, priority, -1);
}

void BLI_task_pool_push(
        TaskPool *pool, TaskRunFunction run, void *taskdata, bool free_taskdata, TaskPriority priority)
{
	BLI_task_pool_push_ex(pool, run, taskdata, free_taskdata, NULL, priority);
}

void BLI_task_pool_push_from_thread(TaskPool *pool, TaskRunFunction run,
        void *taskdata, bool free_taskdata, TaskPriority priority, int thread_id)
{
	task_pool_push(pool, run, taskdata, free_taskdata, NULL, priority, thread_id);
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
			atomic_add_z(&pool->currently_running_tasks, 1);
			work_task->run(pool, work_task->taskdata, 0);

			/* delete task */
			task_free(pool, task, 0);

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

int BLI_pool_get_num_threads(TaskPool *pool)
{
	if (pool->num_threads != 0) {
		return pool->num_threads;
	}
	else {
		return BLI_task_scheduler_num_threads(pool->scheduler);
	}
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
 * - #BLI_task_parallel_listbase (#ListBase - double linked list)
 *
 * TODO:
 * - #BLI_task_parallel_foreach_link (#Link - single linked list)
 * - #BLI_task_parallel_foreach_ghash/gset (#GHash/#GSet - hash & set)
 * - #BLI_task_parallel_foreach_mempool (#BLI_mempool - iterate over mempools)
 *
 */

/* Allows to avoid using malloc for userdata_chunk in tasks, when small enough. */
#define MALLOCA(_size) ((_size) <= 8192) ? alloca((_size)) : MEM_mallocN((_size), __func__)
#define MALLOCA_FREE(_mem, _size) if (((_mem) != NULL) && ((_size) > 8192)) MEM_freeN((_mem))

typedef struct ParallelRangeState {
	int start, stop;
	void *userdata;
	void *userdata_chunk;
	size_t userdata_chunk_size;

	TaskParallelRangeFunc func;
	TaskParallelRangeFuncEx func_ex;

	int iter;
	int chunk_size;
} ParallelRangeState;

BLI_INLINE bool parallel_range_next_iter_get(
        ParallelRangeState * __restrict state,
        int * __restrict iter, int * __restrict count)
{
	uint32_t previter = atomic_fetch_and_add_uint32((uint32_t *)(&state->iter), state->chunk_size);

	*iter = (int)previter;
	*count = max_ii(0, min_ii(state->chunk_size, state->stop - previter));

	return (previter < state->stop);
}

static void parallel_range_func(
        TaskPool * __restrict pool,
        void *UNUSED(taskdata),
        int threadid)
{
	ParallelRangeState * __restrict state = BLI_task_pool_userdata(pool);
	int iter, count;

	const bool use_userdata_chunk = (state->func_ex != NULL) &&
	                                (state->userdata_chunk_size != 0) && (state->userdata_chunk != NULL);
	void *userdata_chunk = use_userdata_chunk ? MALLOCA(state->userdata_chunk_size) : NULL;

	while (parallel_range_next_iter_get(state, &iter, &count)) {
		int i;

		if (state->func_ex) {
			if (use_userdata_chunk) {
				memcpy(userdata_chunk, state->userdata_chunk, state->userdata_chunk_size);
			}

			for (i = 0; i < count; ++i) {
				state->func_ex(state->userdata, userdata_chunk, iter + i, threadid);
			}
		}
		else {
			for (i = 0; i < count; ++i) {
				state->func(state->userdata, iter + i);
			}
		}
	}

	MALLOCA_FREE(userdata_chunk, state->userdata_chunk_size);
}

/**
 * This function allows to parallelized for loops in a similar way to OpenMP's 'parallel for' statement.
 *
 * See public API doc for description of parameters.
 */
static void task_parallel_range_ex(
        int start, int stop,
        void *userdata,
        void *userdata_chunk,
        const size_t userdata_chunk_size,
        TaskParallelRangeFunc func,
        TaskParallelRangeFuncEx func_ex,
        const bool use_threading,
        const bool use_dynamic_scheduling)
{
	TaskScheduler *task_scheduler;
	TaskPool *task_pool;
	ParallelRangeState state;
	int i, num_threads, num_tasks;

	if (start == stop) {
		return;
	}

	BLI_assert(start < stop);
	if (userdata_chunk_size != 0) {
		BLI_assert(func_ex != NULL && func == NULL);
		BLI_assert(userdata_chunk != NULL);
	}

	/* If it's not enough data to be crunched, don't bother with tasks at all,
	 * do everything from the main thread.
	 */
	if (!use_threading) {
		if (func_ex) {
			const bool use_userdata_chunk = (userdata_chunk_size != 0) && (userdata_chunk != NULL);
			void *userdata_chunk_local = NULL;

			if (use_userdata_chunk) {
				userdata_chunk_local = MALLOCA(userdata_chunk_size);
				memcpy(userdata_chunk_local, userdata_chunk, userdata_chunk_size);
			}

			for (i = start; i < stop; ++i) {
				func_ex(userdata, userdata_chunk, i, 0);
			}

			MALLOCA_FREE(userdata_chunk_local, userdata_chunk_size);
		}
		else {
			for (i = start; i < stop; ++i) {
				func(userdata, i);
			}
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

	state.start = start;
	state.stop = stop;
	state.userdata = userdata;
	state.userdata_chunk = userdata_chunk;
	state.userdata_chunk_size = userdata_chunk_size;
	state.func = func;
	state.func_ex = func_ex;
	state.iter = start;
	if (use_dynamic_scheduling) {
		state.chunk_size = 32;
	}
	else {
		state.chunk_size = max_ii(1, (stop - start) / (num_tasks));
	}

	num_tasks = min_ii(num_tasks, (stop - start) / state.chunk_size);
	atomic_fetch_and_add_uint32((uint32_t *)(&state.iter), 0);

	for (i = 0; i < num_tasks; i++) {
		BLI_task_pool_push(task_pool,
		                   parallel_range_func,
		                   NULL, false,
		                   TASK_PRIORITY_HIGH);
	}

	BLI_task_pool_work_and_wait(task_pool);
	BLI_task_pool_free(task_pool);
}

/**
 * This function allows to parallelize for loops in a similar way to OpenMP's 'parallel for' statement.
 *
 * \param start First index to process.
 * \param stop Index to stop looping (excluded).
 * \param userdata Common userdata passed to all instances of \a func.
 * \param userdata_chunk Optional, each instance of looping chunks will get a copy of this data
 *                       (similar to OpenMP's firstprivate).
 * \param userdata_chunk_size Memory size of \a userdata_chunk.
 * \param func_ex Callback function (advanced version).
 * \param use_threading If \a true, actually split-execute loop in threads, else just do a sequential forloop
 *                      (allows caller to use any kind of test to switch on parallelization or not).
 * \param use_dynamic_scheduling If \a true, the whole range is divided in a lot of small chunks (of size 32 currently),
 *                               otherwise whole range is split in a few big chunks (num_threads * 2 chunks currently).
 */
void BLI_task_parallel_range_ex(
        int start, int stop,
        void *userdata,
        void *userdata_chunk,
        const size_t userdata_chunk_size,
        TaskParallelRangeFuncEx func_ex,
        const bool use_threading,
        const bool use_dynamic_scheduling)
{
	task_parallel_range_ex(
	            start, stop, userdata, userdata_chunk, userdata_chunk_size, NULL, func_ex,
	            use_threading, use_dynamic_scheduling);
}

/**
 * A simpler version of \a BLI_task_parallel_range_ex, which does not use \a use_dynamic_scheduling,
 * and does not handle 'firstprivate'-like \a userdata_chunk.
 *
 * \param start First index to process.
 * \param stop Index to stop looping (excluded).
 * \param userdata Common userdata passed to all instances of \a func.
 * \param func Callback function (simple version).
 * \param use_threading If \a true, actually split-execute loop in threads, else just do a sequential forloop
 *                      (allows caller to use any kind of test to switch on parallelization or not).
 */
void BLI_task_parallel_range(
        int start, int stop,
        void *userdata,
        TaskParallelRangeFunc func,
        const bool use_threading)
{
	task_parallel_range_ex(start, stop, userdata, NULL, 0, func, NULL, use_threading, false);
}

#undef MALLOCA
#undef MALLOCA_FREE

typedef struct ParallelListbaseState {
	void *userdata;
	TaskParallelListbaseFunc func;

	int chunk_size;
	int index;
	Link *link;
	SpinLock lock;
} ParallelListState;

BLI_INLINE Link *parallel_listbase_next_iter_get(
        ParallelListState * __restrict state,
        int * __restrict index,
        int * __restrict count)
{
	int task_count = 0;
	BLI_spin_lock(&state->lock);
	Link *result = state->link;
	if (LIKELY(result != NULL)) {
		*index = state->index;
		while (state->link != NULL && task_count < state->chunk_size) {
			++task_count;
			state->link = state->link->next;
		}
		state->index += task_count;
	}
	BLI_spin_unlock(&state->lock);
	*count = task_count;
	return result;
}

static void parallel_listbase_func(
        TaskPool * __restrict pool,
        void *UNUSED(taskdata),
        int UNUSED(threadid))
{
	ParallelListState * __restrict state = BLI_task_pool_userdata(pool);
	Link *link;
	int index, count;

	while ((link = parallel_listbase_next_iter_get(state, &index, &count)) != NULL) {
		for (int i = 0; i < count; ++i) {
			state->func(state->userdata, link, index + i);
			link = link->next;
		}
	}
}

/**
 * This function allows to parallelize for loops over ListBase items.
 *
 * \param listbase The double linked list to loop over.
 * \param userdata Common userdata passed to all instances of \a func.
 * \param func Callback function.
 * \param use_threading If \a true, actually split-execute loop in threads, else just do a sequential forloop
 *                      (allows caller to use any kind of test to switch on parallelization or not).
 *
 * \note There is no static scheduling here, since it would need another full loop over items to count them...
 */
void BLI_task_parallel_listbase(
        struct ListBase *listbase,
        void *userdata,
        TaskParallelListbaseFunc func,
        const bool use_threading)
{
	TaskScheduler *task_scheduler;
	TaskPool *task_pool;
	ParallelListState state;
	int i, num_threads, num_tasks;

	if (BLI_listbase_is_empty(listbase)) {
		return;
	}

	if (!use_threading) {
		i = 0;
		for (Link *link = listbase->first; link != NULL; link = link->next, ++i) {
			func(userdata, link, i);
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

	state.index = 0;
	state.link = listbase->first;
	state.userdata = userdata;
	state.func = func;
	state.chunk_size = 32;
	BLI_spin_init(&state.lock);

	for (i = 0; i < num_tasks; i++) {
		/* Use this pool's pre-allocated tasks. */
		BLI_task_pool_push_from_thread(task_pool,
		                               parallel_listbase_func,
		                               NULL, false,
		                               TASK_PRIORITY_HIGH, 0);
	}

	BLI_task_pool_work_and_wait(task_pool);
	BLI_task_pool_free(task_pool);

	BLI_spin_end(&state.lock);
}
