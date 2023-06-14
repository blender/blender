/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_TASK_H__
#define __UTIL_TASK_H__

#include "util/list.h"
#include "util/string.h"
#include "util/tbb.h"
#include "util/thread.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class TaskPool;
class TaskScheduler;

typedef function<void(void)> TaskRunFunction;

/* Task Pool
 *
 * Pool of tasks that will be executed by the central TaskScheduler. For each
 * pool, we can wait for all tasks to be done, or cancel them before they are
 * done.
 *
 * TaskRunFunction may be created with std::bind or lambda expressions. */

class TaskPool {
 public:
  struct Summary {
    /* Time spent to handle all tasks. */
    double time_total;

    /* Number of all tasks handled by this pool. */
    int num_tasks_handled;

    /* A full multi-line description of the state of the pool after
     * all work is done.
     */
    string full_report() const;
  };

  TaskPool();
  ~TaskPool();

  void push(TaskRunFunction &&task);

  void wait_work(Summary *stats = NULL); /* work and wait until all tasks are done */
  void cancel(); /* cancel all tasks and wait until they are no longer executing */

  static bool canceled(); /* For worker threads, test if current task pool canceled. */

 protected:
  tbb::task_group tbb_group;

  /* ** Statistics ** */

  /* Time stamp of first task pushed. */
  double start_time;

  /* Number of all tasks pushed to the pool. Cleared after wait_work() and cancel(). */
  int num_tasks_pushed;
};

/* Task Scheduler
 *
 * Central scheduler that holds running threads ready to execute tasks. A single
 * queue holds the task from all pools. */

class TaskScheduler {
 public:
  static void init(int num_threads = 0);
  static void exit();
  static void free_memory();

  /* Maximum number of threads that will work on task. Use as little as
   * possible and leave scheduling and splitting up tasks to the scheduler. */
  static int max_concurrency();

 protected:
  static thread_mutex mutex;
  static int users;
  static int active_num_threads;

#ifdef WITH_TBB_GLOBAL_CONTROL
  static tbb::global_control *global_control;
#endif
};

/* Dedicated Task Pool
 *
 * Like a TaskPool, but will launch one dedicated thread to execute all tasks.
 *
 * The run callback that actually executes the task may be created like this:
 * function_bind(&MyClass::task_execute, this, _1, _2) */

class DedicatedTaskPool {
 public:
  DedicatedTaskPool();
  ~DedicatedTaskPool();

  void push(TaskRunFunction &&run, bool front = false);

  void wait();   /* wait until all tasks are done */
  void cancel(); /* cancel all tasks, keep worker thread running */

  bool canceled(); /* for worker thread, test if canceled */

 protected:
  void num_decrease(int done);
  void num_increase();

  void thread_run();
  bool thread_wait_pop(TaskRunFunction &task);

  void clear();

  thread_mutex num_mutex;
  thread_condition_variable num_cond;

  list<TaskRunFunction> queue;
  thread_mutex queue_mutex;
  thread_condition_variable queue_cond;

  int num;
  bool do_cancel;
  bool do_exit;

  thread *worker_thread;
};

CCL_NAMESPACE_END

#endif
