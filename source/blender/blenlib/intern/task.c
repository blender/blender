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

/* Define this to enable some detailed statistic print. */
#undef DEBUG_STATS

/* Types */

/* Number of per-thread pre-allocated tasks.
 *
 * For more details see description of TaskMemPool.
 */
#define MEMPOOL_SIZE 256

/* Number of tasks which are pushed directly to local thread queue.
 *
 * This allows thread to fetch next task without locking the whole queue.
 */
#define LOCAL_QUEUE_SIZE 1

/* Number of tasks which are allowed to be scheduled in a delayed manner.
 *
 * This allows to use less locks per graph node children schedule. More details
 * could be found at TaskThreadLocalStorage::do_delayed_push.
 */
#define DELAYED_QUEUE_SIZE 4096

#ifndef NDEBUG
#  define ASSERT_THREAD_ID(scheduler, thread_id) \
    do { \
      if (!BLI_thread_is_main()) { \
        TaskThread *thread = pthread_getspecific(scheduler->tls_id_key); \
        if (thread == NULL) { \
          BLI_assert(thread_id == 0); \
        } \
        else { \
          BLI_assert(thread_id == thread->id); \
        } \
      } \
      else { \
        BLI_assert(thread_id == 0); \
      } \
    } while (false)
#else
#  define ASSERT_THREAD_ID(scheduler, thread_id)
#endif

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
 *   pool which corresponds to a thread which handled the task.
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

typedef struct TaskThreadLocalStorage {
  /* Memory pool for faster task allocation.
   * The idea is to re-use memory of finished/discarded tasks by this thread.
   */
  TaskMemPool task_mempool;

  /* Local queue keeps thread alive by keeping small amount of tasks ready
   * to be picked up without causing global thread locks for synchronization.
   */
  int num_local_queue;
  Task *local_queue[LOCAL_QUEUE_SIZE];

  /* Thread can be marked for delayed tasks push. This is helpful when it's
   * know that lots of subsequent task pushed will happen from the same thread
   * without "interrupting" for task execution.
   *
   * We try to accumulate as much tasks as possible in a local queue without
   * any locks first, and then we push all of them into a scheduler's queue
   * from within a single mutex lock.
   */
  bool do_delayed_push;
  int num_delayed_queue;
  Task *delayed_queue[DELAYED_QUEUE_SIZE];
} TaskThreadLocalStorage;

struct TaskPool {
  TaskScheduler *scheduler;

  volatile size_t num;
  ThreadMutex num_mutex;
  ThreadCondition num_cond;

  void *userdata;
  ThreadMutex user_mutex;

  volatile bool do_cancel;
  volatile bool do_work;

  volatile bool is_suspended;
  bool start_suspended;
  ListBase suspended_queue;
  size_t num_suspended;

  /* If set, this pool may never be work_and_wait'ed, which means TaskScheduler
   * has to use its special background fallback thread in case we are in
   * single-threaded situation.
   */
  bool run_in_background;

  /* This is a task scheduler's ID of a thread at which pool was constructed.
   * It will be used to access task TLS.
   */
  int thread_id;

  /* For the pools which are created from non-main thread which is not a
   * scheduler worker thread we can't re-use any of scheduler's threads TLS
   * and have to use our own one.
   */
  bool use_local_tls;
  TaskThreadLocalStorage local_tls;
#ifndef NDEBUG
  pthread_t creator_thread_id;
#endif

#ifdef DEBUG_STATS
  TaskMemPoolStats *mempool_stats;
#endif
};

struct TaskScheduler {
  pthread_t *threads;
  struct TaskThread *task_threads;
  int num_threads;
  bool background_thread_only;

  ListBase queue;
  ThreadMutex queue_mutex;
  ThreadCondition queue_cond;

  ThreadMutex startup_mutex;
  ThreadCondition startup_cond;
  volatile int num_thread_started;

  volatile bool do_exit;

  /* NOTE: In pthread's TLS we store the whole TaskThread structure. */
  pthread_key_t tls_id_key;
};

typedef struct TaskThread {
  TaskScheduler *scheduler;
  int id;
  TaskThreadLocalStorage tls;
} TaskThread;

/* Helper */
BLI_INLINE void task_data_free(Task *task, const int thread_id)
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

BLI_INLINE void initialize_task_tls(TaskThreadLocalStorage *tls)
{
  memset(tls, 0, sizeof(TaskThreadLocalStorage));
}

BLI_INLINE TaskThreadLocalStorage *get_task_tls(TaskPool *pool, const int thread_id)
{
  TaskScheduler *scheduler = pool->scheduler;
  BLI_assert(thread_id >= 0);
  BLI_assert(thread_id <= scheduler->num_threads);
  if (pool->use_local_tls && thread_id == 0) {
    BLI_assert(pool->thread_id == 0);
    BLI_assert(!BLI_thread_is_main());
    BLI_assert(pthread_equal(pthread_self(), pool->creator_thread_id));
    return &pool->local_tls;
  }
  if (thread_id == 0) {
    BLI_assert(BLI_thread_is_main());
    return &scheduler->task_threads[pool->thread_id].tls;
  }
  return &scheduler->task_threads[thread_id].tls;
}

BLI_INLINE void free_task_tls(TaskThreadLocalStorage *tls)
{
  TaskMemPool *task_mempool = &tls->task_mempool;
  for (int i = 0; i < task_mempool->num_tasks; ++i) {
    MEM_freeN(task_mempool->tasks[i]);
  }
}

static Task *task_alloc(TaskPool *pool, const int thread_id)
{
  BLI_assert(thread_id <= pool->scheduler->num_threads);
  if (thread_id != -1) {
    BLI_assert(thread_id >= 0);
    BLI_assert(thread_id <= pool->scheduler->num_threads);
    TaskThreadLocalStorage *tls = get_task_tls(pool, thread_id);
    TaskMemPool *task_mempool = &tls->task_mempool;
    /* Try to re-use task memory from a thread local storage. */
    if (task_mempool->num_tasks > 0) {
      --task_mempool->num_tasks;
      /* Success! We've just avoided task allocation. */
#ifdef DEBUG_STATS
      pool->mempool_stats[thread_id].num_reuse++;
#endif
      return task_mempool->tasks[task_mempool->num_tasks];
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
  BLI_assert(thread_id >= 0);
  BLI_assert(thread_id <= pool->scheduler->num_threads);
  if (thread_id == 0) {
    BLI_assert(pool->use_local_tls || BLI_thread_is_main());
  }
  TaskThreadLocalStorage *tls = get_task_tls(pool, thread_id);
  TaskMemPool *task_mempool = &tls->task_mempool;
  if (task_mempool->num_tasks < MEMPOOL_SIZE - 1) {
    /* Successfully allowed the task to be re-used later. */
    task_mempool->tasks[task_mempool->num_tasks] = task;
    ++task_mempool->num_tasks;
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

  if (pool->num == 0) {
    BLI_condition_notify_all(&pool->num_cond);
  }

  BLI_mutex_unlock(&pool->num_mutex);
}

static void task_pool_num_increase(TaskPool *pool, size_t new)
{
  BLI_mutex_lock(&pool->num_mutex);

  pool->num += new;
  BLI_condition_notify_all(&pool->num_cond);

  BLI_mutex_unlock(&pool->num_mutex);
}

static bool task_scheduler_thread_wait_pop(TaskScheduler *scheduler, Task **task)
{
  bool found_task = false;
  BLI_mutex_lock(&scheduler->queue_mutex);

  while (!scheduler->queue.first && !scheduler->do_exit) {
    BLI_condition_wait(&scheduler->queue_cond, &scheduler->queue_mutex);
  }

  do {
    Task *current_task;

    /* Assuming we can only have a void queue in 'exit' case here seems logical
     * (we should only be here after our worker thread has been woken up from a
     * condition_wait(), which only happens after a new task was added to the queue),
     * but it is wrong.
     * Waiting on condition may wake up the thread even if condition is not signaled
     * (spurious wake-ups), and some race condition may also empty the queue **after**
     * condition has been signaled, but **before** awoken thread reaches this point...
     * See http://stackoverflow.com/questions/8594591
     *
     * So we only abort here if do_exit is set.
     */
    if (scheduler->do_exit) {
      BLI_mutex_unlock(&scheduler->queue_mutex);
      return false;
    }

    for (current_task = scheduler->queue.first; current_task != NULL;
         current_task = current_task->next) {
      TaskPool *pool = current_task->pool;

      if (scheduler->background_thread_only && !pool->run_in_background) {
        continue;
      }

      *task = current_task;
      found_task = true;
      BLI_remlink(&scheduler->queue, *task);
      break;
    }
    if (!found_task) {
      BLI_condition_wait(&scheduler->queue_cond, &scheduler->queue_mutex);
    }
  } while (!found_task);

  BLI_mutex_unlock(&scheduler->queue_mutex);

  return true;
}

BLI_INLINE void handle_local_queue(TaskThreadLocalStorage *tls, const int thread_id)
{
  BLI_assert(!tls->do_delayed_push);
  while (tls->num_local_queue > 0) {
    /* We pop task from queue before handling it so handler of the task can
     * push next job to the local queue.
     */
    tls->num_local_queue--;
    Task *local_task = tls->local_queue[tls->num_local_queue];
    /* TODO(sergey): Double-check work_and_wait() doesn't handle other's
     * pool tasks.
     */
    TaskPool *local_pool = local_task->pool;
    local_task->run(local_pool, local_task->taskdata, thread_id);
    task_free(local_pool, local_task, thread_id);
  }
  BLI_assert(!tls->do_delayed_push);
}

static void *task_scheduler_thread_run(void *thread_p)
{
  TaskThread *thread = (TaskThread *)thread_p;
  TaskThreadLocalStorage *tls = &thread->tls;
  TaskScheduler *scheduler = thread->scheduler;
  int thread_id = thread->id;
  Task *task;

  pthread_setspecific(scheduler->tls_id_key, thread);

  /* signal the main thread when all threads have started */
  BLI_mutex_lock(&scheduler->startup_mutex);
  scheduler->num_thread_started++;
  if (scheduler->num_thread_started == scheduler->num_threads) {
    BLI_condition_notify_one(&scheduler->startup_cond);
  }
  BLI_mutex_unlock(&scheduler->startup_mutex);

  /* keep popping off tasks */
  while (task_scheduler_thread_wait_pop(scheduler, &task)) {
    TaskPool *pool = task->pool;

    /* run task */
    BLI_assert(!tls->do_delayed_push);
    task->run(pool, task->taskdata, thread_id);
    BLI_assert(!tls->do_delayed_push);

    /* delete task */
    task_free(pool, task, thread_id);

    /* Handle all tasks from local queue. */
    handle_local_queue(tls, thread_id);

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

  BLI_mutex_init(&scheduler->startup_mutex);
  BLI_condition_init(&scheduler->startup_cond);
  scheduler->num_thread_started = 0;

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

  scheduler->task_threads = MEM_mallocN(sizeof(TaskThread) * (num_threads + 1),
                                        "TaskScheduler task threads");

  /* Initialize TLS for main thread. */
  initialize_task_tls(&scheduler->task_threads[0].tls);

  pthread_key_create(&scheduler->tls_id_key, NULL);

  /* launch threads that will be waiting for work */
  if (num_threads > 0) {
    int i;

    scheduler->num_threads = num_threads;
    scheduler->threads = MEM_callocN(sizeof(pthread_t) * num_threads, "TaskScheduler threads");

    for (i = 0; i < num_threads; i++) {
      TaskThread *thread = &scheduler->task_threads[i + 1];
      thread->scheduler = scheduler;
      thread->id = i + 1;
      initialize_task_tls(&thread->tls);

      if (pthread_create(&scheduler->threads[i], NULL, task_scheduler_thread_run, thread) != 0) {
        fprintf(stderr, "TaskScheduler failed to launch thread %d/%d\n", i, num_threads);
      }
    }
  }

  /* Wait for all worker threads to start before returning to caller to prevent the case where
   * threads are still starting and pthread_join is called, which causes a deadlock on pthreads4w.
   */
  BLI_mutex_lock(&scheduler->startup_mutex);
  /* NOTE: Use loop here to avoid false-positive everything-is-ready caused by spontaneous thread
   * wake up. */
  while (scheduler->num_thread_started != num_threads) {
    BLI_condition_wait(&scheduler->startup_cond, &scheduler->startup_mutex);
  }
  BLI_mutex_unlock(&scheduler->startup_mutex);

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

  pthread_key_delete(scheduler->tls_id_key);

  /* delete threads */
  if (scheduler->threads) {
    int i;

    for (i = 0; i < scheduler->num_threads; i++) {
      if (pthread_join(scheduler->threads[i], NULL) != 0) {
        fprintf(stderr, "TaskScheduler failed to join thread %d/%d\n", i, scheduler->num_threads);
      }
    }

    MEM_freeN(scheduler->threads);
  }

  /* Delete task thread data */
  if (scheduler->task_threads) {
    for (int i = 0; i < scheduler->num_threads + 1; ++i) {
      TaskThreadLocalStorage *tls = &scheduler->task_threads[i].tls;
      free_task_tls(tls);
    }

    MEM_freeN(scheduler->task_threads);
  }

  /* delete leftover tasks */
  for (task = scheduler->queue.first; task; task = task->next) {
    task_data_free(task, 0);
  }
  BLI_freelistN(&scheduler->queue);

  /* delete mutex/condition */
  BLI_mutex_end(&scheduler->queue_mutex);
  BLI_condition_end(&scheduler->queue_cond);
  BLI_mutex_end(&scheduler->startup_mutex);
  BLI_condition_end(&scheduler->startup_cond);

  MEM_freeN(scheduler);
}

int BLI_task_scheduler_num_threads(TaskScheduler *scheduler)
{
  return scheduler->num_threads + 1;
}

static void task_scheduler_push(TaskScheduler *scheduler, Task *task, TaskPriority priority)
{
  task_pool_num_increase(task->pool, 1);

  /* add task to queue */
  BLI_mutex_lock(&scheduler->queue_mutex);

  if (priority == TASK_PRIORITY_HIGH) {
    BLI_addhead(&scheduler->queue, task);
  }
  else {
    BLI_addtail(&scheduler->queue, task);
  }

  BLI_condition_notify_one(&scheduler->queue_cond);
  BLI_mutex_unlock(&scheduler->queue_mutex);
}

static void task_scheduler_push_all(TaskScheduler *scheduler,
                                    TaskPool *pool,
                                    Task **tasks,
                                    int num_tasks)
{
  if (num_tasks == 0) {
    return;
  }

  task_pool_num_increase(pool, num_tasks);

  BLI_mutex_lock(&scheduler->queue_mutex);

  for (int i = 0; i < num_tasks; i++) {
    BLI_addhead(&scheduler->queue, tasks[i]);
  }

  BLI_condition_notify_all(&scheduler->queue_cond);
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
      task_data_free(task, pool->thread_id);
      BLI_freelinkN(&scheduler->queue, task);

      done++;
    }
  }

  BLI_mutex_unlock(&scheduler->queue_mutex);

  /* notify done */
  task_pool_num_decrease(pool, done);
}

/* Task Pool */

static TaskPool *task_pool_create_ex(TaskScheduler *scheduler,
                                     void *userdata,
                                     const bool is_background,
                                     const bool is_suspended)
{
  TaskPool *pool = MEM_mallocN(sizeof(TaskPool), "TaskPool");

#ifndef NDEBUG
  /* Assert we do not try to create a background pool from some parent task -
   * those only work OK from main thread. */
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
  pool->do_cancel = false;
  pool->do_work = false;
  pool->is_suspended = is_suspended;
  pool->start_suspended = is_suspended;
  pool->num_suspended = 0;
  pool->suspended_queue.first = pool->suspended_queue.last = NULL;
  pool->run_in_background = is_background;
  pool->use_local_tls = false;

  BLI_mutex_init(&pool->num_mutex);
  BLI_condition_init(&pool->num_cond);

  pool->userdata = userdata;
  BLI_mutex_init(&pool->user_mutex);

  if (BLI_thread_is_main()) {
    pool->thread_id = 0;
  }
  else {
    TaskThread *thread = pthread_getspecific(scheduler->tls_id_key);
    if (thread == NULL) {
      /* NOTE: Task pool is created from non-main thread which is not
       * managed by the task scheduler. We identify ourselves as thread ID
       * 0 but we do not use scheduler's TLS storage and use our own
       * instead to avoid any possible threading conflicts.
       */
      pool->thread_id = 0;
      pool->use_local_tls = true;
#ifndef NDEBUG
      pool->creator_thread_id = pthread_self();
#endif
      initialize_task_tls(&pool->local_tls);
    }
    else {
      pool->thread_id = thread->id;
    }
  }

#ifdef DEBUG_STATS
  pool->mempool_stats = MEM_callocN(sizeof(*pool->mempool_stats) * (scheduler->num_threads + 1),
                                    "per-taskpool mempool stats");
#endif

  /* Ensure malloc will go fine from threads,
   *
   * This is needed because we could be in main thread here
   * and malloc could be non-thread safe at this point because
   * no other jobs are running.
   */
  BLI_threaded_malloc_begin();

  return pool;
}

/**
 * Create a normal task pool.
 * This means that in single-threaded context, it will not be executed at all until you call
 * \a BLI_task_pool_work_and_wait() on it.
 */
TaskPool *BLI_task_pool_create(TaskScheduler *scheduler, void *userdata)
{
  return task_pool_create_ex(scheduler, userdata, false, false);
}

/**
 * Create a background task pool.
 * In multi-threaded context, there is no differences with #BLI_task_pool_create(),
 * but in single-threaded case it is ensured to have at least one worker thread to run on
 * (i.e. you don't have to call #BLI_task_pool_work_and_wait
 * on it to be sure it will be processed).
 *
 * \note Background pools are non-recursive
 * (that is, you should not create other background pools in tasks assigned to a background pool,
 * they could end never being executed, since the 'fallback' background thread is already
 * busy with parent task in single-threaded context).
 */
TaskPool *BLI_task_pool_create_background(TaskScheduler *scheduler, void *userdata)
{
  return task_pool_create_ex(scheduler, userdata, true, false);
}

/**
 * Similar to BLI_task_pool_create() but does not schedule any tasks for execution
 * for until BLI_task_pool_work_and_wait() is called. This helps reducing threading
 * overhead when pushing huge amount of small initial tasks from the main thread.
 */
TaskPool *BLI_task_pool_create_suspended(TaskScheduler *scheduler, void *userdata)
{
  return task_pool_create_ex(scheduler, userdata, false, true);
}

void BLI_task_pool_free(TaskPool *pool)
{
  BLI_task_pool_cancel(pool);

  BLI_mutex_end(&pool->num_mutex);
  BLI_condition_end(&pool->num_cond);

  BLI_mutex_end(&pool->user_mutex);

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

  if (pool->use_local_tls) {
    free_task_tls(&pool->local_tls);
  }

  MEM_freeN(pool);

  BLI_threaded_malloc_end();
}

BLI_INLINE bool task_can_use_local_queues(TaskPool *pool, int thread_id)
{
  return (thread_id != -1 && (thread_id != pool->thread_id || pool->do_work));
}

static void task_pool_push(TaskPool *pool,
                           TaskRunFunction run,
                           void *taskdata,
                           bool free_taskdata,
                           TaskFreeFunction freedata,
                           TaskPriority priority,
                           int thread_id)
{
  /* Allocate task and fill it's properties. */
  Task *task = task_alloc(pool, thread_id);
  task->run = run;
  task->taskdata = taskdata;
  task->free_taskdata = free_taskdata;
  task->freedata = freedata;
  task->pool = pool;
  /* For suspended pools we put everything yo a global queue first
   * and exit as soon as possible.
   *
   * This tasks will be moved to actual execution when pool is
   * activated by work_and_wait().
   */
  if (pool->is_suspended) {
    BLI_addhead(&pool->suspended_queue, task);
    atomic_fetch_and_add_z(&pool->num_suspended, 1);
    return;
  }
  /* Populate to any local queue first, this is cheapest push ever. */
  if (task_can_use_local_queues(pool, thread_id)) {
    ASSERT_THREAD_ID(pool->scheduler, thread_id);
    TaskThreadLocalStorage *tls = get_task_tls(pool, thread_id);
    /* Try to push to a local execution queue.
     * These tasks will be picked up next.
     */
    if (tls->num_local_queue < LOCAL_QUEUE_SIZE) {
      tls->local_queue[tls->num_local_queue] = task;
      tls->num_local_queue++;
      return;
    }
    /* If we are in the delayed tasks push mode, we push tasks to a
     * temporary local queue first without any locks, and then move them
     * to global execution queue with a single lock.
     */
    if (tls->do_delayed_push && tls->num_delayed_queue < DELAYED_QUEUE_SIZE) {
      tls->delayed_queue[tls->num_delayed_queue] = task;
      tls->num_delayed_queue++;
      return;
    }
  }
  /* Do push to a global execution pool, slowest possible method,
   * causes quite reasonable amount of threading overhead.
   */
  task_scheduler_push(pool->scheduler, task, priority);
}

void BLI_task_pool_push_ex(TaskPool *pool,
                           TaskRunFunction run,
                           void *taskdata,
                           bool free_taskdata,
                           TaskFreeFunction freedata,
                           TaskPriority priority)
{
  task_pool_push(pool, run, taskdata, free_taskdata, freedata, priority, -1);
}

void BLI_task_pool_push(
    TaskPool *pool, TaskRunFunction run, void *taskdata, bool free_taskdata, TaskPriority priority)
{
  BLI_task_pool_push_ex(pool, run, taskdata, free_taskdata, NULL, priority);
}

void BLI_task_pool_push_from_thread(TaskPool *pool,
                                    TaskRunFunction run,
                                    void *taskdata,
                                    bool free_taskdata,
                                    TaskPriority priority,
                                    int thread_id)
{
  task_pool_push(pool, run, taskdata, free_taskdata, NULL, priority, thread_id);
}

void BLI_task_pool_work_and_wait(TaskPool *pool)
{
  TaskThreadLocalStorage *tls = get_task_tls(pool, pool->thread_id);
  TaskScheduler *scheduler = pool->scheduler;

  if (atomic_fetch_and_and_uint8((uint8_t *)&pool->is_suspended, 0)) {
    if (pool->num_suspended) {
      task_pool_num_increase(pool, pool->num_suspended);
      BLI_mutex_lock(&scheduler->queue_mutex);

      BLI_movelisttolist(&scheduler->queue, &pool->suspended_queue);

      BLI_condition_notify_all(&scheduler->queue_cond);
      BLI_mutex_unlock(&scheduler->queue_mutex);

      pool->num_suspended = 0;
    }
  }

  pool->do_work = true;

  ASSERT_THREAD_ID(pool->scheduler, pool->thread_id);

  handle_local_queue(tls, pool->thread_id);

  BLI_mutex_lock(&pool->num_mutex);

  while (pool->num != 0) {
    Task *task, *work_task = NULL;
    bool found_task = false;

    BLI_mutex_unlock(&pool->num_mutex);

    BLI_mutex_lock(&scheduler->queue_mutex);

    /* find task from this pool. if we get a task from another pool,
     * we can get into deadlock */

    for (task = scheduler->queue.first; task; task = task->next) {
      if (task->pool == pool) {
        work_task = task;
        found_task = true;
        BLI_remlink(&scheduler->queue, task);
        break;
      }
    }

    BLI_mutex_unlock(&scheduler->queue_mutex);

    /* if found task, do it, otherwise wait until other tasks are done */
    if (found_task) {
      /* run task */
      BLI_assert(!tls->do_delayed_push);
      work_task->run(pool, work_task->taskdata, pool->thread_id);
      BLI_assert(!tls->do_delayed_push);

      /* delete task */
      task_free(pool, task, pool->thread_id);

      /* Handle all tasks from local queue. */
      handle_local_queue(tls, pool->thread_id);

      /* notify pool task was done */
      task_pool_num_decrease(pool, 1);
    }

    BLI_mutex_lock(&pool->num_mutex);
    if (pool->num == 0) {
      break;
    }

    if (!found_task) {
      BLI_condition_wait(&pool->num_cond, &pool->num_mutex);
    }
  }

  BLI_mutex_unlock(&pool->num_mutex);

  BLI_assert(tls->num_local_queue == 0);
}

void BLI_task_pool_work_wait_and_reset(TaskPool *pool)
{
  BLI_task_pool_work_and_wait(pool);

  pool->do_work = false;
  pool->is_suspended = pool->start_suspended;
}

void BLI_task_pool_cancel(TaskPool *pool)
{
  pool->do_cancel = true;

  task_scheduler_clear(pool->scheduler, pool);

  /* wait until all entries are cleared */
  BLI_mutex_lock(&pool->num_mutex);
  while (pool->num) {
    BLI_condition_wait(&pool->num_cond, &pool->num_mutex);
  }
  BLI_mutex_unlock(&pool->num_mutex);

  pool->do_cancel = false;
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

void BLI_task_pool_delayed_push_begin(TaskPool *pool, int thread_id)
{
  if (task_can_use_local_queues(pool, thread_id)) {
    ASSERT_THREAD_ID(pool->scheduler, thread_id);
    TaskThreadLocalStorage *tls = get_task_tls(pool, thread_id);
    tls->do_delayed_push = true;
  }
}

void BLI_task_pool_delayed_push_end(TaskPool *pool, int thread_id)
{
  if (task_can_use_local_queues(pool, thread_id)) {
    ASSERT_THREAD_ID(pool->scheduler, thread_id);
    TaskThreadLocalStorage *tls = get_task_tls(pool, thread_id);
    BLI_assert(tls->do_delayed_push);
    task_scheduler_push_all(pool->scheduler, pool, tls->delayed_queue, tls->num_delayed_queue);
    tls->do_delayed_push = false;
    tls->num_delayed_queue = 0;
  }
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
 */

/* Allows to avoid using malloc for userdata_chunk in tasks, when small enough. */
#define MALLOCA(_size) ((_size) <= 8192) ? alloca((_size)) : MEM_mallocN((_size), __func__)
#define MALLOCA_FREE(_mem, _size) \
  if (((_mem) != NULL) && ((_size) > 8192)) \
  MEM_freeN((_mem))

typedef struct ParallelRangeState {
  int start, stop;
  void *userdata;

  TaskParallelRangeFunc func;

  int iter;
  int chunk_size;
} ParallelRangeState;

BLI_INLINE bool parallel_range_next_iter_get(ParallelRangeState *__restrict state,
                                             int *__restrict iter,
                                             int *__restrict count)
{
  int previter = atomic_fetch_and_add_int32(&state->iter, state->chunk_size);

  *iter = previter;
  *count = max_ii(0, min_ii(state->chunk_size, state->stop - previter));

  return (previter < state->stop);
}

static void parallel_range_func(TaskPool *__restrict pool, void *userdata_chunk, int thread_id)
{
  ParallelRangeState *__restrict state = BLI_task_pool_userdata(pool);
  ParallelRangeTLS tls = {
      .thread_id = thread_id,
      .userdata_chunk = userdata_chunk,
  };
  int iter, count;
  while (parallel_range_next_iter_get(state, &iter, &count)) {
    for (int i = 0; i < count; ++i) {
      state->func(state->userdata, iter + i, &tls);
    }
  }
}

static void parallel_range_single_thread(const int start,
                                         int const stop,
                                         void *userdata,
                                         TaskParallelRangeFunc func,
                                         const ParallelRangeSettings *settings)
{
  void *userdata_chunk = settings->userdata_chunk;
  const size_t userdata_chunk_size = settings->userdata_chunk_size;
  void *userdata_chunk_local = NULL;
  const bool use_userdata_chunk = (userdata_chunk_size != 0) && (userdata_chunk != NULL);
  if (use_userdata_chunk) {
    userdata_chunk_local = MALLOCA(userdata_chunk_size);
    memcpy(userdata_chunk_local, userdata_chunk, userdata_chunk_size);
  }
  ParallelRangeTLS tls = {
      .thread_id = 0,
      .userdata_chunk = userdata_chunk_local,
  };
  for (int i = start; i < stop; ++i) {
    func(userdata, i, &tls);
  }
  if (settings->func_finalize != NULL) {
    settings->func_finalize(userdata, userdata_chunk_local);
  }
  MALLOCA_FREE(userdata_chunk_local, userdata_chunk_size);
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
                             const ParallelRangeSettings *settings)
{
  TaskScheduler *task_scheduler;
  TaskPool *task_pool;
  ParallelRangeState state;
  int i, num_threads, num_tasks;

  void *userdata_chunk = settings->userdata_chunk;
  const size_t userdata_chunk_size = settings->userdata_chunk_size;
  void *userdata_chunk_local = NULL;
  void *userdata_chunk_array = NULL;
  const bool use_userdata_chunk = (userdata_chunk_size != 0) && (userdata_chunk != NULL);

  if (start == stop) {
    return;
  }

  BLI_assert(start < stop);
  if (userdata_chunk_size != 0) {
    BLI_assert(userdata_chunk != NULL);
  }

  /* If it's not enough data to be crunched, don't bother with tasks at all,
   * do everything from the main thread.
   */
  if (!settings->use_threading) {
    parallel_range_single_thread(start, stop, userdata, func, settings);
    return;
  }

  task_scheduler = BLI_task_scheduler_get();
  num_threads = BLI_task_scheduler_num_threads(task_scheduler);

  /* The idea here is to prevent creating task for each of the loop iterations
   * and instead have tasks which are evenly distributed across CPU cores and
   * pull next iter to be crunched using the queue.
   */
  num_tasks = num_threads + 2;

  state.start = start;
  state.stop = stop;
  state.userdata = userdata;
  state.func = func;
  state.iter = start;
  switch (settings->scheduling_mode) {
    case TASK_SCHEDULING_STATIC:
      state.chunk_size = max_ii(settings->min_iter_per_thread, (stop - start) / (num_tasks));
      break;
    case TASK_SCHEDULING_DYNAMIC:
      /* TODO(sergey): Make it configurable from min_iter_per_thread. */
      state.chunk_size = 32;
      break;
  }

  num_tasks = min_ii(num_tasks, max_ii(1, (stop - start) / state.chunk_size));

  if (num_tasks == 1) {
    parallel_range_single_thread(start, stop, userdata, func, settings);
    return;
  }

  task_pool = BLI_task_pool_create_suspended(task_scheduler, &state);

  /* NOTE: This way we are adding a memory barrier and ensure all worker
   * threads can read and modify the value, without any locks. */
  atomic_fetch_and_add_int32(&state.iter, 0);

  if (use_userdata_chunk) {
    userdata_chunk_array = MALLOCA(userdata_chunk_size * num_tasks);
  }

  for (i = 0; i < num_tasks; i++) {
    if (use_userdata_chunk) {
      userdata_chunk_local = (char *)userdata_chunk_array + (userdata_chunk_size * i);
      memcpy(userdata_chunk_local, userdata_chunk, userdata_chunk_size);
    }
    /* Use this pool's pre-allocated tasks. */
    BLI_task_pool_push_from_thread(task_pool,
                                   parallel_range_func,
                                   userdata_chunk_local,
                                   false,
                                   TASK_PRIORITY_HIGH,
                                   task_pool->thread_id);
  }

  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);

  if (use_userdata_chunk) {
    if (settings->func_finalize != NULL) {
      for (i = 0; i < num_tasks; i++) {
        userdata_chunk_local = (char *)userdata_chunk_array + (userdata_chunk_size * i);
        settings->func_finalize(userdata, userdata_chunk_local);
      }
    }
    MALLOCA_FREE(userdata_chunk_array, userdata_chunk_size * num_tasks);
  }
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

BLI_INLINE Link *parallel_listbase_next_iter_get(ParallelListState *__restrict state,
                                                 int *__restrict index,
                                                 int *__restrict count)
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

static void parallel_listbase_func(TaskPool *__restrict pool,
                                   void *UNUSED(taskdata),
                                   int UNUSED(threadid))
{
  ParallelListState *__restrict state = BLI_task_pool_userdata(pool);
  Link *link;
  int index, count;

  while ((link = parallel_listbase_next_iter_get(state, &index, &count)) != NULL) {
    for (int i = 0; i < count; ++i) {
      state->func(state->userdata, link, index + i);
      link = link->next;
    }
  }
}

static void task_parallel_listbase_no_threads(struct ListBase *listbase,
                                              void *userdata,
                                              TaskParallelListbaseFunc func)
{
  int i = 0;
  for (Link *link = listbase->first; link != NULL; link = link->next, ++i) {
    func(userdata, link, i);
  }
}

/* NOTE: The idea here is to compensate for rather measurable threading
 * overhead caused by fetching tasks. With too many CPU threads we are starting
 * to spend too much time in those overheads. */
BLI_INLINE int task_parallel_listbasecalc_chunk_size(const int num_threads)
{
  if (num_threads > 32) {
    return 128;
  }
  else if (num_threads > 16) {
    return 64;
  }
  return 32;
}

/**
 * This function allows to parallelize for loops over ListBase items.
 *
 * \param listbase: The double linked list to loop over.
 * \param userdata: Common userdata passed to all instances of \a func.
 * \param func: Callback function.
 * \param use_threading: If \a true, actually split-execute loop in threads,
 * else just do a sequential forloop
 * (allows caller to use any kind of test to switch on parallelization or not).
 *
 * \note There is no static scheduling here,
 * since it would need another full loop over items to count them.
 */
void BLI_task_parallel_listbase(struct ListBase *listbase,
                                void *userdata,
                                TaskParallelListbaseFunc func,
                                const bool use_threading)
{
  if (BLI_listbase_is_empty(listbase)) {
    return;
  }
  if (!use_threading) {
    task_parallel_listbase_no_threads(listbase, userdata, func);
    return;
  }
  TaskScheduler *task_scheduler = BLI_task_scheduler_get();
  const int num_threads = BLI_task_scheduler_num_threads(task_scheduler);
  /* TODO(sergey): Consider making chunk size configurable. */
  const int chunk_size = task_parallel_listbasecalc_chunk_size(num_threads);
  const int num_tasks = min_ii(num_threads, BLI_listbase_count(listbase) / chunk_size);
  if (num_tasks <= 1) {
    task_parallel_listbase_no_threads(listbase, userdata, func);
    return;
  }

  ParallelListState state;
  TaskPool *task_pool = BLI_task_pool_create_suspended(task_scheduler, &state);

  state.index = 0;
  state.link = listbase->first;
  state.userdata = userdata;
  state.func = func;
  state.chunk_size = chunk_size;
  BLI_spin_init(&state.lock);

  BLI_assert(num_tasks > 0);
  for (int i = 0; i < num_tasks; i++) {
    /* Use this pool's pre-allocated tasks. */
    BLI_task_pool_push_from_thread(
        task_pool, parallel_listbase_func, NULL, false, TASK_PRIORITY_HIGH, task_pool->thread_id);
  }

  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);

  BLI_spin_end(&state.lock);
}

typedef struct ParallelMempoolState {
  void *userdata;
  TaskParallelMempoolFunc func;
} ParallelMempoolState;

static void parallel_mempool_func(TaskPool *__restrict pool, void *taskdata, int UNUSED(threadid))
{
  ParallelMempoolState *__restrict state = BLI_task_pool_userdata(pool);
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
  task_pool = BLI_task_pool_create_suspended(task_scheduler, &state);
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

  for (i = 0; i < num_tasks; i++) {
    /* Use this pool's pre-allocated tasks. */
    BLI_task_pool_push_from_thread(task_pool,
                                   parallel_mempool_func,
                                   &mempool_iterators[i],
                                   false,
                                   TASK_PRIORITY_HIGH,
                                   task_pool->thread_id);
  }

  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);

  BLI_mempool_iter_threadsafe_free(mempool_iterators);
}
