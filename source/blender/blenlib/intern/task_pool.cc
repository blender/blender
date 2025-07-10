/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * Task pool to run tasks in parallel.
 */

#include <atomic>
#include <cstdlib>
#include <memory>
#include <utility>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_assert.h"
#include "BLI_mempool.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_vector.hh"

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

/* TBB has a check in `tbb/include/task_group.h` where `__TBB_CPP11_RVALUE_REF_PRESENT` should
 * evaluate to true as with the other MSVC build. However, because of the clang compiler
 * it does not and we attempt to call a deleted constructor in the tbb_task_pool_run function.
 * This check fixes this issue and keeps our Task constructor valid. */
#if (defined(WITH_TBB) && TBB_INTERFACE_VERSION_MAJOR < 10) || \
    (defined(_MSC_VER) && defined(__clang__) && TBB_INTERFACE_VERSION_MAJOR < 12)
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

/* Execute task. */
void Task::operator()() const
{
  run(pool, taskdata);
}

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

  void *userdata;

#ifdef WITH_TBB
  /* TBB task pool. */
  std::unique_ptr<TBBTaskGroup> tbb_group;
#endif
  volatile bool is_suspended = false;
  blender::Vector<Task> suspended_tasks;

  /* Background task pool. */
  ListBase background_threads;
  ThreadQueue *background_queue;
  volatile bool background_is_canceling = false;

  eTaskPriority priority;

  TaskPool(const TaskPoolType type, const eTaskPriority priority, void *userdata)
      : type(type), userdata(userdata), priority(priority)
  {
    this->use_threads = BLI_task_scheduler_num_threads() > 1 && type != TASK_POOL_NO_THREADS;

    /* Background task pool uses regular TBB scheduling if available. Only when
     * building without TBB or running with -t 1 do we need to ensure these tasks
     * do not block the main thread. */
    if (this->type == TASK_POOL_BACKGROUND && this->use_threads) {
      this->type = TASK_POOL_TBB;
    }

    switch (this->type) {
      case TASK_POOL_TBB:
      case TASK_POOL_TBB_SUSPENDED:
      case TASK_POOL_NO_THREADS: {
        if (type == TASK_POOL_TBB_SUSPENDED) {
          this->is_suspended = true;
        }

#ifdef WITH_TBB
        if (use_threads) {
          this->tbb_group = std::make_unique<TBBTaskGroup>(priority);
        }
#endif
        break;
      }
      case TASK_POOL_BACKGROUND:
      case TASK_POOL_BACKGROUND_SERIAL: {
        this->background_queue = BLI_thread_queue_init();
        BLI_threadpool_init(&this->background_threads, this->background_task_run, 1);
        break;
      }
    }
  }

  ~TaskPool()
  {
    switch (type) {
      case TASK_POOL_TBB:
      case TASK_POOL_TBB_SUSPENDED:
      case TASK_POOL_NO_THREADS:
        break;
      case TASK_POOL_BACKGROUND:
      case TASK_POOL_BACKGROUND_SERIAL: {
        this->background_task_pool_work_and_wait();

        BLI_threadpool_end(&this->background_threads);
        BLI_thread_queue_free(this->background_queue);
        break;
      }
    }
  }

  TaskPool(TaskPool &&other) = delete;
#if 0
        : type(other.type), use_threads(other.use_threads), userdata(other.userdata)
    {
      other.pool = nullptr;
      other.run = nullptr;
      other.taskdata = nullptr;
      other.free_taskdata = false;
      other.freedata = nullptr;
    }
#endif

  TaskPool(const TaskPool &other) = delete;

  TaskPool &operator=(const TaskPool &other) = delete;
  TaskPool &operator=(TaskPool &&other) = delete;

  /**
   * Create and add a new task to the pool.
   */
  void task_push(TaskRunFunction run,
                 void *taskdata,
                 bool free_taskdata,
                 TaskFreeFunction freedata)
  {
    switch (this->type) {
      case TASK_POOL_TBB:
      case TASK_POOL_TBB_SUSPENDED:
      case TASK_POOL_NO_THREADS:
        this->tbb_task_pool_run({this, run, taskdata, free_taskdata, freedata});
        break;
      case TASK_POOL_BACKGROUND:
      case TASK_POOL_BACKGROUND_SERIAL:
        this->background_task_pool_run({this, run, taskdata, free_taskdata, freedata});
        break;
    }
  }

  /**
   * Work and wait until all tasks are done.
   */
  void work_and_wait()
  {
    switch (this->type) {
      case TASK_POOL_TBB:
      case TASK_POOL_TBB_SUSPENDED:
      case TASK_POOL_NO_THREADS:
        this->tbb_task_pool_work_and_wait();
        break;
      case TASK_POOL_BACKGROUND:
      case TASK_POOL_BACKGROUND_SERIAL:
        this->background_task_pool_work_and_wait();
        break;
    }
  }

  /**
   * Cancel all tasks, keep worker threads running.
   */
  void cancel()
  {
    switch (this->type) {
      case TASK_POOL_TBB:
      case TASK_POOL_TBB_SUSPENDED:
      case TASK_POOL_NO_THREADS:
        this->tbb_task_pool_cancel();
        break;
      case TASK_POOL_BACKGROUND:
      case TASK_POOL_BACKGROUND_SERIAL:
        this->background_task_pool_cancel();
        break;
    }
  }

  bool current_canceled()
  {
    switch (this->type) {
      case TASK_POOL_TBB:
      case TASK_POOL_TBB_SUSPENDED:
      case TASK_POOL_NO_THREADS:
        return this->tbb_task_pool_canceled();
      case TASK_POOL_BACKGROUND:
      case TASK_POOL_BACKGROUND_SERIAL:
        return this->background_task_pool_canceled();
    }
    BLI_assert_msg(0, "TaskPool::current_canceled: Control flow should not come here!");
    return false;
  }

 private:
  /* TBB Task Pool.
   *
   * Task pool using the TBB scheduler for tasks. When building without TBB
   * support or running Blender with -t 1, this reverts to single threaded.
   *
   * Tasks may be suspended until in all are created, to make it possible to
   * initialize data structures and create tasks in a single pass. */
  void tbb_task_pool_run(Task &&task);
  void tbb_task_pool_work_and_wait();
  void tbb_task_pool_cancel();
  bool tbb_task_pool_canceled();

  /* Background Task Pool.
   *
   * Fallback for running background tasks when building without TBB. */
  void background_task_pool_run(Task &&task);
  void background_task_pool_work_and_wait();
  void background_task_pool_cancel();
  bool background_task_pool_canceled();
  static void *background_task_run(void *userdata);
};

void TaskPool::tbb_task_pool_run(Task &&task)
{
  BLI_assert(ELEM(this->type, TASK_POOL_TBB, TASK_POOL_TBB_SUSPENDED, TASK_POOL_NO_THREADS));
  if (this->is_suspended) {
    /* Suspended task that will be executed in work_and_wait(). */
    this->suspended_tasks.append(std::move(task));

    /* Added as part of original 'use TBB' commit (d8a3f3595af0fb). Unclear whether this is still
     * needed, tests are passing on linux buildbot, but not sure if any would trigger the issue
     * addressed by this code. So keeping around for now. */
#if 0
#  ifdef __GNUC__
    /* Work around apparent compiler bug where task is not properly copied
     * to task_mem. This appears unrelated to the use of placement new or
     * move semantics, happens even writing to a plain C struct. Rather the
     * call into TBB seems to have some indirect effect. */
    std::atomic_thread_fence(std::memory_order_release);
#  endif
#endif
  }
#ifdef WITH_TBB
  else if (this->use_threads) {
    /* Execute in TBB task group. */
    this->tbb_group->run(std::move(task));
  }
#endif
  else {
    /* Execute immediately. */
    task();
  }
}

void TaskPool::tbb_task_pool_work_and_wait()
{
  BLI_assert(ELEM(this->type, TASK_POOL_TBB, TASK_POOL_TBB_SUSPENDED, TASK_POOL_NO_THREADS));
  /* Start any suspended task now. */
  if (!this->suspended_tasks.is_empty()) {
    BLI_assert(this->is_suspended);
    this->is_suspended = false;

    for (Task &task : this->suspended_tasks) {
      this->tbb_task_pool_run(std::move(task));
    }
    this->suspended_tasks.clear();
  }

#ifdef WITH_TBB
  if (this->use_threads) {
    /* This is called wait(), but internally it can actually do work. This
     * matters because we don't want recursive usage of task pools to run
     * out of threads and get stuck. */
    this->tbb_group->wait();
  }
#endif
}

void TaskPool::tbb_task_pool_cancel()
{
  BLI_assert(ELEM(this->type, TASK_POOL_TBB, TASK_POOL_TBB_SUSPENDED, TASK_POOL_NO_THREADS));
#ifdef WITH_TBB
  if (this->use_threads) {
    this->tbb_group->cancel();
    this->tbb_group->wait();
  }
#endif
}

bool TaskPool::tbb_task_pool_canceled()
{
  BLI_assert(ELEM(this->type, TASK_POOL_TBB, TASK_POOL_TBB_SUSPENDED, TASK_POOL_NO_THREADS));
#ifdef WITH_TBB
  if (this->use_threads) {
    return tbb::is_current_task_group_canceling();
  }
#endif
  return false;
}

void TaskPool::background_task_pool_run(Task &&task)
{
  BLI_assert(ELEM(this->type, TASK_POOL_BACKGROUND, TASK_POOL_BACKGROUND_SERIAL));

  Task *task_mem = MEM_new<Task>(__func__, std::move(task));
  BLI_thread_queue_push(this->background_queue,
                        task_mem,
                        this->priority == TASK_PRIORITY_HIGH ?
                            BLI_THREAD_QUEUE_WORK_PRIORITY_HIGH :
                            BLI_THREAD_QUEUE_WORK_PRIORITY_NORMAL);

  if (BLI_available_threads(&this->background_threads)) {
    BLI_threadpool_insert(&this->background_threads, this);
  }
}

void TaskPool::background_task_pool_work_and_wait()
{
  BLI_assert(ELEM(this->type, TASK_POOL_BACKGROUND, TASK_POOL_BACKGROUND_SERIAL));

  /* Signal background thread to stop waiting for new tasks if none are
   * left, and wait for tasks and thread to finish. */
  BLI_thread_queue_nowait(this->background_queue);
  BLI_thread_queue_wait_finish(this->background_queue);
  BLI_threadpool_clear(&this->background_threads);
}

void TaskPool::background_task_pool_cancel()
{
  BLI_assert(ELEM(this->type, TASK_POOL_BACKGROUND, TASK_POOL_BACKGROUND_SERIAL));

  this->background_is_canceling = true;

  /* Remove tasks not yet started by background thread. */
  BLI_thread_queue_nowait(this->background_queue);
  while (Task *task = static_cast<Task *>(BLI_thread_queue_pop(this->background_queue))) {
    MEM_delete(task);
  }

  /* Let background thread finish or cancel task it is working on. */
  BLI_threadpool_remove(&this->background_threads, this);
  this->background_is_canceling = false;
}

bool TaskPool::background_task_pool_canceled()
{
  BLI_assert(ELEM(this->type, TASK_POOL_BACKGROUND, TASK_POOL_BACKGROUND_SERIAL));

  return this->background_is_canceling;
}

void *TaskPool::background_task_run(void *userdata)
{
  TaskPool *pool = static_cast<TaskPool *>(userdata);
  while (Task *task = static_cast<Task *>(BLI_thread_queue_pop(pool->background_queue))) {
    (*task)();
    MEM_delete(task);
  }
  return nullptr;
}

/* Task Pool public API. */

TaskPool *BLI_task_pool_create(void *userdata, eTaskPriority priority)
{
  return MEM_new<TaskPool>(__func__, TASK_POOL_TBB, priority, userdata);
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
  return MEM_new<TaskPool>(__func__, TASK_POOL_BACKGROUND, priority, userdata);
}

TaskPool *BLI_task_pool_create_suspended(void *userdata, eTaskPriority priority)
{
  /* NOTE: Similar to #BLI_task_pool_create() but does not schedule any tasks for execution
   * for until BLI_task_pool_work_and_wait() is called. This helps reducing threading
   * overhead when pushing huge amount of small initial tasks from the main thread. */
  return MEM_new<TaskPool>(__func__, TASK_POOL_TBB_SUSPENDED, priority, userdata);
}

TaskPool *BLI_task_pool_create_no_threads(void *userdata)
{
  return MEM_new<TaskPool>(__func__, TASK_POOL_NO_THREADS, TASK_PRIORITY_HIGH, userdata);
}

TaskPool *BLI_task_pool_create_background_serial(void *userdata, eTaskPriority priority)
{
  return MEM_new<TaskPool>(__func__, TASK_POOL_BACKGROUND_SERIAL, priority, userdata);
}

void BLI_task_pool_free(TaskPool *pool)
{
  MEM_delete(pool);
}

void BLI_task_pool_push(TaskPool *pool,
                        TaskRunFunction run,
                        void *taskdata,
                        bool free_taskdata,
                        TaskFreeFunction freedata)
{
  pool->task_push(run, taskdata, free_taskdata, freedata);
}

void BLI_task_pool_work_and_wait(TaskPool *pool)
{
  pool->work_and_wait();
}

void BLI_task_pool_cancel(TaskPool *pool)
{
  pool->cancel();
}

bool BLI_task_pool_current_canceled(TaskPool *pool)
{
  return pool->current_canceled();
}

void *BLI_task_pool_user_data(TaskPool *pool)
{
  return pool->userdata;
}
