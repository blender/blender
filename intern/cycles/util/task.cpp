/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "util/task.h"
#include "util/foreach.h"
#include "util/log.h"
#include "util/system.h"
#include "util/time.h"

CCL_NAMESPACE_BEGIN

/* Task Pool */

TaskPool::TaskPool() : start_time(time_dt()), num_tasks_pushed(0) {}

TaskPool::~TaskPool()
{
  cancel();
}

void TaskPool::push(TaskRunFunction &&task)
{
  tbb_group.run(std::move(task));
  num_tasks_pushed++;
}

void TaskPool::wait_work(Summary *stats)
{
  tbb_group.wait();

  if (stats != NULL) {
    stats->time_total = time_dt() - start_time;
    stats->num_tasks_handled = num_tasks_pushed;
  }

  num_tasks_pushed = 0;
}

void TaskPool::cancel()
{
  if (num_tasks_pushed > 0) {
    tbb_group.cancel();
    tbb_group.wait();
    num_tasks_pushed = 0;
  }
}

bool TaskPool::canceled()
{
  return tbb::is_current_task_group_canceling();
}

/* Task Scheduler */

thread_mutex TaskScheduler::mutex;
int TaskScheduler::users = 0;
int TaskScheduler::active_num_threads = 0;
tbb::global_control *TaskScheduler::global_control = nullptr;

void TaskScheduler::init(int num_threads)
{
  thread_scoped_lock lock(mutex);
  /* Multiple cycles instances can use this task scheduler, sharing the same
   * threads, so we keep track of the number of users. */
  ++users;
  if (users != 1) {
    return;
  }
  if (num_threads > 0) {
    /* Automatic number of threads. */
    VLOG_INFO << "Overriding number of TBB threads to " << num_threads << ".";
    global_control = new tbb::global_control(tbb::global_control::max_allowed_parallelism,
                                             num_threads);
    active_num_threads = num_threads;
  }
  else {
    active_num_threads = tbb::this_task_arena::max_concurrency();
  }
}

void TaskScheduler::exit()
{
  thread_scoped_lock lock(mutex);
  users--;
  if (users == 0) {
    delete global_control;
    global_control = nullptr;
    active_num_threads = 0;
  }
}

void TaskScheduler::free_memory()
{
  assert(users == 0);
}

int TaskScheduler::max_concurrency()
{
  thread_scoped_lock lock(mutex);
  return (users > 0) ? active_num_threads : tbb::this_task_arena::max_concurrency();
}

/* Dedicated Task Pool */

DedicatedTaskPool::DedicatedTaskPool()
{
  do_cancel = false;
  do_exit = false;
  num = 0;

  worker_thread = new thread(function_bind(&DedicatedTaskPool::thread_run, this));
}

DedicatedTaskPool::~DedicatedTaskPool()
{
  wait();

  do_exit = true;
  queue_cond.notify_all();

  worker_thread->join();
  delete worker_thread;
}

void DedicatedTaskPool::push(TaskRunFunction &&task, bool front)
{
  num_increase();

  /* add task to queue */
  queue_mutex.lock();
  if (front)
    queue.emplace_front(std::move(task));
  else
    queue.emplace_back(std::move(task));

  queue_cond.notify_one();
  queue_mutex.unlock();
}

void DedicatedTaskPool::wait()
{
  thread_scoped_lock num_lock(num_mutex);

  while (num)
    num_cond.wait(num_lock);
}

void DedicatedTaskPool::cancel()
{
  do_cancel = true;

  clear();
  wait();

  do_cancel = false;
}

bool DedicatedTaskPool::canceled()
{
  return do_cancel;
}

void DedicatedTaskPool::num_decrease(int done)
{
  thread_scoped_lock num_lock(num_mutex);
  num -= done;

  assert(num >= 0);
  if (num == 0)
    num_cond.notify_all();
}

void DedicatedTaskPool::num_increase()
{
  thread_scoped_lock num_lock(num_mutex);
  num++;
  num_cond.notify_all();
}

bool DedicatedTaskPool::thread_wait_pop(TaskRunFunction &task)
{
  thread_scoped_lock queue_lock(queue_mutex);

  while (queue.empty() && !do_exit)
    queue_cond.wait(queue_lock);

  if (queue.empty()) {
    assert(do_exit);
    return false;
  }

  task = queue.front();
  queue.pop_front();

  return true;
}

void DedicatedTaskPool::thread_run()
{
  TaskRunFunction task;

  /* keep popping off tasks */
  while (thread_wait_pop(task)) {
    /* run task */
    task();

    /* delete task */
    task = nullptr;

    /* notify task was done */
    num_decrease(1);
  }
}

void DedicatedTaskPool::clear()
{
  thread_scoped_lock queue_lock(queue_mutex);

  /* erase all tasks from the queue */
  int done = queue.size();
  queue.clear();

  queue_lock.unlock();

  /* notify done */
  num_decrease(done);
}

string TaskPool::Summary::full_report() const
{
  string report = "";
  report += string_printf("Total time:    %f\n", time_total);
  report += string_printf("Tasks handled: %d\n", num_tasks_handled);
  return report;
}

CCL_NAMESPACE_END
