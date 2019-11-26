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
  for (int i = 0; i < task_mempool->num_tasks; i++) {
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
    for (int i = 0; i < scheduler->num_threads + 1; i++) {
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
 * Create a normal task pool. Tasks will be executed as soon as they are added.
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
  for (int i = 0; i < pool->scheduler->num_threads + 1; i++) {
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

  /* Function called from calling thread once whole range have been processed. */
  TaskParallelFinalizeFunc func_finalize;

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
  TaskParallelRangePool *__restrict range_pool = BLI_task_pool_userdata(pool);
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
    void *flatten_tls_storage = NULL;
    const bool use_tls_data = (tls_data_size != 0) && (initial_tls_memory != NULL);
    if (use_tls_data) {
      flatten_tls_storage = MALLOCA(tls_data_size);
      memcpy(flatten_tls_storage, initial_tls_memory, tls_data_size);
    }
    TaskParallelTLS tls = {
        .thread_id = 0,
        .userdata_chunk = flatten_tls_storage,
    };
    for (int i = start; i < stop; i++) {
      func(userdata, i, &tls);
    }
    if (state->func_finalize != NULL) {
      state->func_finalize(userdata, flatten_tls_storage);
    }
    MALLOCA_FREE(flatten_tls_storage, tls_data_size);
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
      .func_finalize = settings->func_finalize,
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

  TaskPool *task_pool = range_pool.pool = BLI_task_pool_create_suspended(task_scheduler,
                                                                         &range_pool);

  range_pool.current_state = &state;

  if (use_tls_data) {
    state.flatten_tls_storage = flatten_tls_storage = MALLOCA(tls_data_size * (size_t)num_tasks);
    state.tls_data_size = tls_data_size;
  }

  for (i = 0; i < num_tasks; i++) {
    if (use_tls_data) {
      void *userdata_chunk_local = (char *)flatten_tls_storage + (tls_data_size * (size_t)i);
      memcpy(userdata_chunk_local, tls_data, tls_data_size);
    }
    /* Use this pool's pre-allocated tasks. */
    BLI_task_pool_push_from_thread(task_pool,
                                   parallel_range_func,
                                   POINTER_FROM_INT(i),
                                   false,
                                   TASK_PRIORITY_HIGH,
                                   task_pool->thread_id);
  }

  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);

  if (use_tls_data) {
    if (settings->func_finalize != NULL) {
      for (i = 0; i < num_tasks; i++) {
        void *userdata_chunk_local = (char *)flatten_tls_storage + (tls_data_size * (size_t)i);
        settings->func_finalize(userdata, userdata_chunk_local);
      }
    }
    MALLOCA_FREE(flatten_tls_storage, tls_data_size * (size_t)num_tasks);
  }
}

/**
 * Initialize a task pool to parallelize several for loops at the same time.
 *
 * See public API doc of ParallelRangeSettings for description of all settings.
 * Note that loop-specific settings (like 'tls' data or finalize function) must be left NULL here.
 * Only settings controlling how iteration is parallelized must be defined, as those will affect
 * all loops added to that pool.
 */
TaskParallelRangePool *BLI_task_parallel_range_pool_init(const TaskParallelSettings *settings)
{
  TaskParallelRangePool *range_pool = MEM_callocN(sizeof(*range_pool), __func__);

  BLI_assert(settings->userdata_chunk == NULL);
  BLI_assert(settings->func_finalize == NULL);
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
  state->func_finalize = settings->func_finalize;

  state->next = range_pool->parallel_range_states;
  range_pool->parallel_range_states = state;
}

static void parallel_range_func_finalize(TaskPool *__restrict pool,
                                         void *v_state,
                                         int UNUSED(thread_id))
{
  TaskParallelRangePool *__restrict range_pool = BLI_task_pool_userdata(pool);
  TaskParallelRangeState *state = v_state;

  for (int i = 0; i < range_pool->num_tasks; i++) {
    void *tls_data = (char *)state->flatten_tls_storage + (state->tls_data_size * (size_t)i);
    state->func_finalize(state->userdata_shared, tls_data);
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

  TaskPool *task_pool = range_pool->pool = BLI_task_pool_create_suspended(task_scheduler,
                                                                          range_pool);

  range_pool->current_state = range_pool->parallel_range_states;

  for (int i = 0; i < num_tasks; i++) {
    BLI_task_pool_push_from_thread(task_pool,
                                   parallel_range_func,
                                   POINTER_FROM_INT(i),
                                   false,
                                   TASK_PRIORITY_HIGH,
                                   task_pool->thread_id);
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

    if (state->func_finalize != NULL) {
      BLI_task_pool_push_from_thread(task_pool,
                                     parallel_range_func_finalize,
                                     state,
                                     false,
                                     TASK_PRIORITY_HIGH,
                                     task_pool->thread_id);
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
  TaskParallelIteratorState *__restrict state = BLI_task_pool_userdata(pool);

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

  if (use_userdata_chunk) {
    if (settings->func_finalize != NULL) {
      settings->func_finalize(state->userdata, userdata_chunk_local);
    }
    MALLOCA_FREE(userdata_chunk_local, userdata_chunk_size);
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

  TaskPool *task_pool = BLI_task_pool_create_suspended(task_scheduler, state);

  if (use_userdata_chunk) {
    userdata_chunk_array = MALLOCA(userdata_chunk_size * num_tasks);
  }

  for (size_t i = 0; i < num_tasks; i++) {
    if (use_userdata_chunk) {
      userdata_chunk_local = (char *)userdata_chunk_array + (userdata_chunk_size * i);
      memcpy(userdata_chunk_local, userdata_chunk, userdata_chunk_size);
    }
    /* Use this pool's pre-allocated tasks. */
    BLI_task_pool_push_from_thread(task_pool,
                                   parallel_iterator_func,
                                   userdata_chunk_local,
                                   false,
                                   TASK_PRIORITY_HIGH,
                                   task_pool->thread_id);
  }

  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);

  if (use_userdata_chunk) {
    if (settings->func_finalize != NULL) {
      for (size_t i = 0; i < num_tasks; i++) {
        userdata_chunk_local = (char *)userdata_chunk_array + (userdata_chunk_size * i);
        settings->func_finalize(state->userdata, userdata_chunk_local);
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
