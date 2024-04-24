/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * Task pool to run tasks in parallel.
 */

#include <cstdlib>
#include <memory>
#include <utility>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_mempool.h"
#include "BLI_task.h"
#include "BLI_threads.h"

#ifdef WITH_TBB
#  include <tbb/blocked_range.h>
#  include <tbb/task_arena.h>
#  include <tbb/task_group.h>
#endif

/**
 * Task
 *
 * Unit of work to execute. This is a C++ class to work with TBB.
 */
class Task {
 public:
  TaskPool *pool;
  TaskRunFunction run;
  void *taskdata;
  bool free_taskdata;
  TaskFreeFunction freedata;

  Task(TaskPool *pool,
       TaskRunFunction run,
       void *taskdata,
       bool free_taskdata,
       TaskFreeFunction freedata)
      : pool(pool), run(run), taskdata(taskdata), free_taskdata(free_taskdata), freedata(freedata)
  {
  }

  ~Task()
  {
    if (free_taskdata) {
      if (freedata) {
        freedata(pool, taskdata);
      }
      else {
        MEM_freeN(taskdata);
      }
    }
  }

  /* Move constructor.
   * For performance, ensure we never copy the task and only move it.
   * For TBB version 2017 and earlier we apply a workaround to make up for
   * the lack of move constructor support. */
  Task(Task &&other)
      : pool(other.pool),
        run(other.run),
        taskdata(other.taskdata),
        free_taskdata(other.free_taskdata),
        freedata(other.freedata)
  {
    other.pool = nullptr;
    other.run = nullptr;
    other.taskdata = nullptr;
    other.free_taskdata = false;
    other.freedata = nullptr;
  }

// TBB has a check in tbb/include/task_group.h where __TBB_CPP11_RVALUE_REF_PRESENT should evaluate to true as with
// the other MSVC build. However, because of the clang compiler it does not and we attempt to call a deleted constructor
// in the tbb_task_pool_run function. This check fixes this issue and keeps our Task constructor valid
#if (defined(WITH_TBB) && TBB_INTERFACE_VERSION_MAJOR < 10) || (defined(_MSC_VER) && defined(__clang__) && TBB_INTERFACE_VERSION_MAJOR < 12)
  Task(const Task &other)
      : pool(other.pool),
        run(other.run),
        taskdata(other.taskdata),
        free_taskdata(other.free_taskdata),
        freedata(other.freedata)
  {
    ((Task &)other).pool = nullptr;
    ((Task &)other).run = nullptr;
    ((Task &)other).taskdata = nullptr;
    ((Task &)other).free_taskdata = false;
    ((Task &)other).freedata = nullptr;
  }
#else
  Task(const Task &other) = delete;
#endif

  Task &operator=(const Task &other) = delete;
  Task &operator=(Task &&other) = delete;

  void operator()() const;
};

/* TBB Task Group.
 *
 * Subclass since there seems to be no other way to set priority. */

#ifdef WITH_TBB
class TBBTaskGroup : public tbb::task_group {
 public:
  TBBTaskGroup(eTaskPriority priority)
  {
#  if TBB_INTERFACE_VERSION_MAJOR >= 12
    /* TODO: support priorities in TBB 2021, where they are only available as
     * part of task arenas, no longer for task groups. Or remove support for
     * task priorities if they are no longer useful. */
    UNUSED_VARS(priority);
#  else
    switch (priority) {
      case TASK_PRIORITY_LOW:
        my_context.set_priority(tbb::priority_low);
        break;
      case TASK_PRIORITY_HIGH:
        my_context.set_priority(tbb::priority_normal);
        break;
    }
#  endif
  }
};
#endif

/* Task Pool */

enum TaskPoolType {
  TASK_POOL_TBB,
  TASK_POOL_TBB_SUSPENDED,
  TASK_POOL_NO_THREADS,
  TASK_POOL_BACKGROUND,
  TASK_POOL_BACKGROUND_SERIAL,
};

struct TaskPool {
  TaskPoolType type;
  bool use_threads;

  ThreadMutex user_mutex;
  void *userdata;

#ifdef WITH_TBB
  /* TBB task pool. */
  TBBTaskGroup tbb_group;
#endif
  volatile bool is_suspended;
  BLI_mempool *suspended_mempool;

  /* Background task pool. */
  ListBase background_threads;
  ThreadQueue *background_queue;
  volatile bool background_is_canceling;
};

/* Execute task. */
void Task::operator()() const
{
  run(pool, taskdata);
}

/* TBB Task Pool.
 *
 * Task pool using the TBB scheduler for tasks. When building without TBB
 * support or running Blender with -t 1, this reverts to single threaded.
 *
 * Tasks may be suspended until in all are created, to make it possible to
 * initialize data structures and create tasks in a single pass. */

static void tbb_task_pool_create(TaskPool *pool, eTaskPriority priority)
{
  if (pool->type == TASK_POOL_TBB_SUSPENDED) {
    pool->is_suspended = true;
    pool->suspended_mempool = BLI_mempool_create(sizeof(Task), 512, 512, BLI_MEMPOOL_ALLOW_ITER);
  }

#ifdef WITH_TBB
  if (pool->use_threads) {
    new (&pool->tbb_group) TBBTaskGroup(priority);
  }
#else
  UNUSED_VARS(priority);
#endif
}

static void tbb_task_pool_run(TaskPool *pool, Task &&task)
{
  if (pool->is_suspended) {
    /* Suspended task that will be executed in work_and_wait(). */
    Task *task_mem = (Task *)BLI_mempool_alloc(pool->suspended_mempool);
    new (task_mem) Task(std::move(task));
#ifdef __GNUC__
    /* Work around apparent compiler bug where task is not properly copied
     * to task_mem. This appears unrelated to the use of placement new or
     * move semantics, happens even writing to a plain C struct. Rather the
     * call into TBB seems to have some indirect effect. */
    std::atomic_thread_fence(std::memory_order_release);
#endif
  }
#ifdef WITH_TBB
  else if (pool->use_threads) {
    /* Execute in TBB task group. */
    pool->tbb_group.run(std::move(task));
  }
#endif
  else {
    /* Execute immediately. */
    task();
  }
}

static void tbb_task_pool_work_and_wait(TaskPool *pool)
{
  /* Start any suspended task now. */
  if (pool->suspended_mempool) {
    pool->is_suspended = false;

    BLI_mempool_iter iter;
    BLI_mempool_iternew(pool->suspended_mempool, &iter);
    while (Task *task = (Task *)BLI_mempool_iterstep(&iter)) {
      tbb_task_pool_run(pool, std::move(*task));
    }

    BLI_mempool_clear(pool->suspended_mempool);
  }

#ifdef WITH_TBB
  if (pool->use_threads) {
    /* This is called wait(), but internally it can actually do work. This
     * matters because we don't want recursive usage of task pools to run
     * out of threads and get stuck. */
    pool->tbb_group.wait();
  }
#endif
}

static void tbb_task_pool_cancel(TaskPool *pool)
{
#ifdef WITH_TBB
  if (pool->use_threads) {
    pool->tbb_group.cancel();
    pool->tbb_group.wait();
  }
#else
  UNUSED_VARS(pool);
#endif
}

static bool tbb_task_pool_canceled(TaskPool *pool)
{
#ifdef WITH_TBB
  if (pool->use_threads) {
    return tbb::is_current_task_group_canceling();
  }
#else
  UNUSED_VARS(pool);
#endif

  return false;
}

static void tbb_task_pool_free(TaskPool *pool)
{
#ifdef WITH_TBB
  if (pool->use_threads) {
    pool->tbb_group.~TBBTaskGroup();
  }
#endif

  if (pool->suspended_mempool) {
    BLI_mempool_destroy(pool->suspended_mempool);
  }
}

/* Background Task Pool.
 *
 * Fallback for running background tasks when building without TBB. */

static void *background_task_run(void *userdata)
{
  TaskPool *pool = (TaskPool *)userdata;
  while (Task *task = (Task *)BLI_thread_queue_pop(pool->background_queue)) {
    (*task)();
    task->~Task();
    MEM_freeN(task);
  }
  return nullptr;
}

static void background_task_pool_create(TaskPool *pool)
{
  pool->background_queue = BLI_thread_queue_init();
  BLI_threadpool_init(&pool->background_threads, background_task_run, 1);
}

static void background_task_pool_run(TaskPool *pool, Task &&task)
{
  Task *task_mem = (Task *)MEM_mallocN(sizeof(Task), __func__);
  new (task_mem) Task(std::move(task));
  BLI_thread_queue_push(pool->background_queue, task_mem);

  if (BLI_available_threads(&pool->background_threads)) {
    BLI_threadpool_insert(&pool->background_threads, pool);
  }
}

static void background_task_pool_work_and_wait(TaskPool *pool)
{
  /* Signal background thread to stop waiting for new tasks if none are
   * left, and wait for tasks and thread to finish. */
  BLI_thread_queue_nowait(pool->background_queue);
  BLI_thread_queue_wait_finish(pool->background_queue);
  BLI_threadpool_clear(&pool->background_threads);
}

static void background_task_pool_cancel(TaskPool *pool)
{
  pool->background_is_canceling = true;

  /* Remove tasks not yet started by background thread. */
  BLI_thread_queue_nowait(pool->background_queue);
  while (Task *task = (Task *)BLI_thread_queue_pop(pool->background_queue)) {
    task->~Task();
    MEM_freeN(task);
  }

  /* Let background thread finish or cancel task it is working on. */
  BLI_threadpool_remove(&pool->background_threads, pool);
  pool->background_is_canceling = false;
}

static bool background_task_pool_canceled(TaskPool *pool)
{
  return pool->background_is_canceling;
}

static void background_task_pool_free(TaskPool *pool)
{
  background_task_pool_work_and_wait(pool);

  BLI_threadpool_end(&pool->background_threads);
  BLI_thread_queue_free(pool->background_queue);
}

/* Task Pool */

static TaskPool *task_pool_create_ex(void *userdata, TaskPoolType type, eTaskPriority priority)
{
  const bool use_threads = BLI_task_scheduler_num_threads() > 1 && type != TASK_POOL_NO_THREADS;

  /* Background task pool uses regular TBB scheduling if available. Only when
   * building without TBB or running with -t 1 do we need to ensure these tasks
   * do not block the main thread. */
  if (type == TASK_POOL_BACKGROUND && use_threads) {
    type = TASK_POOL_TBB;
  }

  /* Allocate task pool. */
  TaskPool *pool = (TaskPool *)MEM_callocN(sizeof(TaskPool), "TaskPool");

  pool->type = type;
  pool->use_threads = use_threads;

  pool->userdata = userdata;
  BLI_mutex_init(&pool->user_mutex);

  switch (type) {
    case TASK_POOL_TBB:
    case TASK_POOL_TBB_SUSPENDED:
    case TASK_POOL_NO_THREADS:
      tbb_task_pool_create(pool, priority);
      break;
    case TASK_POOL_BACKGROUND:
    case TASK_POOL_BACKGROUND_SERIAL:
      background_task_pool_create(pool);
      break;
  }

  return pool;
}

TaskPool *BLI_task_pool_create(void *userdata, eTaskPriority priority)
{
  return task_pool_create_ex(userdata, TASK_POOL_TBB, priority);
}

TaskPool *BLI_task_pool_create_background(void *userdata, eTaskPriority priority)
{
  /* NOTE: In multi-threaded context, there is no differences with #BLI_task_pool_create(),
   * but in single-threaded case it is ensured to have at least one worker thread to run on
   * (i.e. you don't have to call #BLI_task_pool_work_and_wait
   * on it to be sure it will be processed).
   *
   * NOTE: Background pools are non-recursive
   * (that is, you should not create other background pools in tasks assigned to a background pool,
   * they could end never being executed, since the 'fallback' background thread is already
   * busy with parent task in single-threaded context). */
  return task_pool_create_ex(userdata, TASK_POOL_BACKGROUND, priority);
}

TaskPool *BLI_task_pool_create_suspended(void *userdata, eTaskPriority priority)
{
  /* NOTE: Similar to #BLI_task_pool_create() but does not schedule any tasks for execution
   * for until BLI_task_pool_work_and_wait() is called. This helps reducing threading
   * overhead when pushing huge amount of small initial tasks from the main thread. */
  return task_pool_create_ex(userdata, TASK_POOL_TBB_SUSPENDED, priority);
}

TaskPool *BLI_task_pool_create_no_threads(void *userdata)
{
  return task_pool_create_ex(userdata, TASK_POOL_NO_THREADS, TASK_PRIORITY_HIGH);
}

TaskPool *BLI_task_pool_create_background_serial(void *userdata, eTaskPriority priority)
{
  return task_pool_create_ex(userdata, TASK_POOL_BACKGROUND_SERIAL, priority);
}

void BLI_task_pool_free(TaskPool *pool)
{
  switch (pool->type) {
    case TASK_POOL_TBB:
    case TASK_POOL_TBB_SUSPENDED:
    case TASK_POOL_NO_THREADS:
      tbb_task_pool_free(pool);
      break;
    case TASK_POOL_BACKGROUND:
    case TASK_POOL_BACKGROUND_SERIAL:
      background_task_pool_free(pool);
      break;
  }

  BLI_mutex_end(&pool->user_mutex);

  MEM_freeN(pool);
}

void BLI_task_pool_push(TaskPool *pool,
                        TaskRunFunction run,
                        void *taskdata,
                        bool free_taskdata,
                        TaskFreeFunction freedata)
{
  Task task(pool, run, taskdata, free_taskdata, freedata);

  switch (pool->type) {
    case TASK_POOL_TBB:
    case TASK_POOL_TBB_SUSPENDED:
    case TASK_POOL_NO_THREADS:
      tbb_task_pool_run(pool, std::move(task));
      break;
    case TASK_POOL_BACKGROUND:
    case TASK_POOL_BACKGROUND_SERIAL:
      background_task_pool_run(pool, std::move(task));
      break;
  }
}

void BLI_task_pool_work_and_wait(TaskPool *pool)
{
  switch (pool->type) {
    case TASK_POOL_TBB:
    case TASK_POOL_TBB_SUSPENDED:
    case TASK_POOL_NO_THREADS:
      tbb_task_pool_work_and_wait(pool);
      break;
    case TASK_POOL_BACKGROUND:
    case TASK_POOL_BACKGROUND_SERIAL:
      background_task_pool_work_and_wait(pool);
      break;
  }
}

void BLI_task_pool_cancel(TaskPool *pool)
{
  switch (pool->type) {
    case TASK_POOL_TBB:
    case TASK_POOL_TBB_SUSPENDED:
    case TASK_POOL_NO_THREADS:
      tbb_task_pool_cancel(pool);
      break;
    case TASK_POOL_BACKGROUND:
    case TASK_POOL_BACKGROUND_SERIAL:
      background_task_pool_cancel(pool);
      break;
  }
}

bool BLI_task_pool_current_canceled(TaskPool *pool)
{
  switch (pool->type) {
    case TASK_POOL_TBB:
    case TASK_POOL_TBB_SUSPENDED:
    case TASK_POOL_NO_THREADS:
      return tbb_task_pool_canceled(pool);
    case TASK_POOL_BACKGROUND:
    case TASK_POOL_BACKGROUND_SERIAL:
      return background_task_pool_canceled(pool);
  }
  BLI_assert_msg(0, "BLI_task_pool_canceled: Control flow should not come here!");
  return false;
}

void *BLI_task_pool_user_data(TaskPool *pool)
{
  return pool->userdata;
}

ThreadMutex *BLI_task_pool_user_mutex(TaskPool *pool)
{
  return &pool->user_mutex;
}
