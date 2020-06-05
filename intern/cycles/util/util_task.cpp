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

#include "util/util_task.h"
#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_system.h"
#include "util/util_time.h"

CCL_NAMESPACE_BEGIN

/* Task Pool */

TaskPool::TaskPool() : start_time(time_dt()), num_tasks_handled(0)
{
}

TaskPool::~TaskPool()
{
  cancel();
}

void TaskPool::push(TaskRunFunction &&task)
{
  tbb_group.run(std::move(task));
  num_tasks_handled++;
}

void TaskPool::wait_work(Summary *stats)
{
  tbb_group.wait();

  if (stats != NULL) {
    stats->time_total = time_dt() - start_time;
    stats->num_tasks_handled = num_tasks_handled;
  }
}

void TaskPool::cancel()
{
  tbb_group.cancel();
  tbb_group.wait();
}

bool TaskPool::canceled()
{
  return tbb_group.is_canceling();
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
    VLOG(1) << "Overriding number of TBB threads to " << num_threads << ".";
    global_control = new tbb::global_control(tbb::global_control::max_allowed_parallelism,
                                             num_threads);
    active_num_threads = num_threads;
  }
  else {
    active_num_threads = system_cpu_thread_count();
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

int TaskScheduler::num_threads()
{
  return active_num_threads;
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
