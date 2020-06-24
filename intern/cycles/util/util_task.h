/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_TASK_H__
#define __UTIL_TASK_H__

#include "util/util_list.h"
#include "util/util_string.h"
#include "util/util_thread.h"
#include "util/util_vector.h"

#include "util/util_tbb.h"

CCL_NAMESPACE_BEGIN

class TaskPool;
class TaskScheduler;

typedef function<void(void)> TaskRunFunction;

/* Task Pool
 *
 * Pool of tasks that will be executed by the central TaskScheduler.For each
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

    /* A full multiline description of the state of the pool after
     * all work is done.
     */
    string full_report() const;
  };

  TaskPool();
  ~TaskPool();

  void push(TaskRunFunction &&task);

  void wait_work(Summary *stats = NULL); /* work and wait until all tasks are done */
  void cancel(); /* cancel all tasks and wait until they are no longer executing */

  bool canceled(); /* for worker threads, test if canceled */

 protected:
  tbb::task_group tbb_group;

  /* ** Statistics ** */

  /* Time time stamp of first task pushed. */
  double start_time;

  /* Number of all tasks handled by this pool. */
  int num_tasks_handled;
};

/* Task Scheduler
 *
 * Central scheduler that holds running threads ready to execute tasks. A singe
 * queue holds the task from all pools. */

class TaskScheduler {
 public:
  static void init(int num_threads = 0);
  static void exit();
  static void free_memory();

  /* Approximate number of threads that will work on task, which may be lower
   * or higher than the actual number of threads. Use as little as possible and
   * leave splitting up tasks to the scheduler.. */
  static int num_threads();

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
