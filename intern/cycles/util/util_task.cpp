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

//#define THREADING_DEBUG_ENABLED

#ifdef THREADING_DEBUG_ENABLED
#  include <stdio.h>
#  define THREADING_DEBUG(...) \
    do { \
      printf(__VA_ARGS__); \
      fflush(stdout); \
    } while (0)
#else
#  define THREADING_DEBUG(...)
#endif

CCL_NAMESPACE_BEGIN

/* Task Pool */

TaskPool::TaskPool()
{
  num_tasks_handled = 0;
  num = 0;
  do_cancel = false;
}

TaskPool::~TaskPool()
{
  stop();
}

void TaskPool::push(Task *task, bool front)
{
  TaskScheduler::Entry entry;

  entry.task = task;
  entry.pool = this;

  TaskScheduler::push(entry, front);
}

void TaskPool::push(TaskRunFunction &&run, bool front)
{
  push(new Task(std::move(run)), front);
}

void TaskPool::wait_work(Summary *stats)
{
  thread_scoped_lock num_lock(num_mutex);

  while (num != 0) {
    num_lock.unlock();

    thread_scoped_lock queue_lock(TaskScheduler::queue_mutex);

    /* find task from this pool. if we get a task from another pool,
     * we can get into deadlock */
    TaskScheduler::Entry work_entry;
    bool found_entry = false;
    list<TaskScheduler::Entry>::iterator it;

    for (it = TaskScheduler::queue.begin(); it != TaskScheduler::queue.end(); it++) {
      TaskScheduler::Entry &entry = *it;

      if (entry.pool == this) {
        work_entry = entry;
        found_entry = true;
        TaskScheduler::queue.erase(it);
        break;
      }
    }

    queue_lock.unlock();

    /* if found task, do it, otherwise wait until other tasks are done */
    if (found_entry) {
      /* run task */
      work_entry.task->run(0);

      /* delete task */
      delete work_entry.task;

      /* notify pool task was done */
      num_decrease(1);
    }

    num_lock.lock();
    if (num == 0)
      break;

    if (!found_entry) {
      THREADING_DEBUG("num==%d, Waiting for condition in TaskPool::wait_work !found_entry\n", num);
      num_cond.wait(num_lock);
      THREADING_DEBUG("num==%d, condition wait done in TaskPool::wait_work !found_entry\n", num);
    }
  }

  if (stats != NULL) {
    stats->time_total = time_dt() - start_time;
    stats->num_tasks_handled = num_tasks_handled;
  }
}

void TaskPool::cancel()
{
  do_cancel = true;

  TaskScheduler::clear(this);

  {
    thread_scoped_lock num_lock(num_mutex);

    while (num) {
      THREADING_DEBUG("num==%d, Waiting for condition in TaskPool::cancel\n", num);
      num_cond.wait(num_lock);
      THREADING_DEBUG("num==%d condition wait done in TaskPool::cancel\n", num);
    }
  }

  do_cancel = false;
}

void TaskPool::stop()
{
  TaskScheduler::clear(this);

  assert(num == 0);
}

bool TaskPool::canceled()
{
  return do_cancel;
}

bool TaskPool::finished()
{
  thread_scoped_lock num_lock(num_mutex);
  return num == 0;
}

void TaskPool::num_decrease(int done)
{
  num_mutex.lock();
  num -= done;

  assert(num >= 0);
  if (num == 0) {
    THREADING_DEBUG("num==%d, notifying all in TaskPool::num_decrease\n", num);
    num_cond.notify_all();
  }

  num_mutex.unlock();
}

void TaskPool::num_increase()
{
  thread_scoped_lock num_lock(num_mutex);
  if (num_tasks_handled == 0) {
    start_time = time_dt();
  }
  num++;
  num_tasks_handled++;
  THREADING_DEBUG("num==%d, notifying all in TaskPool::num_increase\n", num);
  num_cond.notify_all();
}

/* Task Scheduler */

thread_mutex TaskScheduler::mutex;
int TaskScheduler::users = 0;
vector<thread *> TaskScheduler::threads;
bool TaskScheduler::do_exit = false;

list<TaskScheduler::Entry> TaskScheduler::queue;
thread_mutex TaskScheduler::queue_mutex;
thread_condition_variable TaskScheduler::queue_cond;

namespace {

/* Get number of processors on each of the available nodes. The result is sized
 * by the highest node index, and element corresponds to number of processors on
 * that node.
 * If node is not available, then the corresponding number of processors is
 * zero. */
void get_per_node_num_processors(vector<int> *num_per_node_processors)
{
  const int num_nodes = system_cpu_num_numa_nodes();
  if (num_nodes == 0) {
    LOG(ERROR) << "Zero available NUMA nodes, is not supposed to happen.";
    return;
  }
  num_per_node_processors->resize(num_nodes);
  for (int node = 0; node < num_nodes; ++node) {
    if (!system_cpu_is_numa_node_available(node)) {
      (*num_per_node_processors)[node] = 0;
      continue;
    }
    (*num_per_node_processors)[node] = system_cpu_num_numa_node_processors(node);
  }
}

/* Calculate total number of processors on all available nodes.
 * This is similar to system_cpu_thread_count(), but uses pre-calculated number
 * of processors on each of the node, avoiding extra system calls and checks for
 * the node availability. */
int get_num_total_processors(const vector<int> &num_per_node_processors)
{
  int num_total_processors = 0;
  foreach (int num_node_processors, num_per_node_processors) {
    num_total_processors += num_node_processors;
  }
  return num_total_processors;
}

/* Compute NUMA node for every thread to run on, for the best performance. */
vector<int> distribute_threads_on_nodes(const int num_threads)
{
  /* Start with all threads unassigned to any specific NUMA node. */
  vector<int> thread_nodes(num_threads, -1);
  const int num_active_group_processors = system_cpu_num_active_group_processors();
  VLOG(1) << "Detected " << num_active_group_processors << " processors "
          << "in active group.";
  if (num_active_group_processors >= num_threads) {
    /* If the current thread is set up in a way that its affinity allows to
     * use at least requested number of threads we do not explicitly set
     * affinity to the worker threads.
     * This way we allow users to manually edit affinity of the parent
     * thread, and here we follow that affinity. This way it's possible to
     * have two Cycles/Blender instances running manually set to a different
     * dies on a CPU. */
    VLOG(1) << "Not setting thread group affinity.";
    return thread_nodes;
  }
  vector<int> num_per_node_processors;
  get_per_node_num_processors(&num_per_node_processors);
  if (num_per_node_processors.size() == 0) {
    /* Error was already reported, here we can't do anything, so we simply
     * leave default affinity to all the worker threads. */
    return thread_nodes;
  }
  const int num_nodes = num_per_node_processors.size();
  int thread_index = 0;
  /* First pass: fill in all the nodes to their maximum.
   *
   * If there is less threads than the overall nodes capacity, some of the
   * nodes or parts of them will idle.
   *
   * TODO(sergey): Consider picking up fastest nodes if number of threads
   * fits on them. For example, on Threadripper2 we might consider using nodes
   * 0 and 2 if user requested 32 render threads. */
  const int num_total_node_processors = get_num_total_processors(num_per_node_processors);
  int current_node_index = 0;
  while (thread_index < num_total_node_processors && thread_index < num_threads) {
    const int num_node_processors = num_per_node_processors[current_node_index];
    for (int processor_index = 0; processor_index < num_node_processors; ++processor_index) {
      VLOG(1) << "Scheduling thread " << thread_index << " to node " << current_node_index << ".";
      thread_nodes[thread_index] = current_node_index;
      ++thread_index;
      if (thread_index == num_threads) {
        /* All threads are scheduled on their nodes. */
        return thread_nodes;
      }
    }
    ++current_node_index;
  }
  /* Second pass: keep scheduling threads to each node one by one,
   * uniformly filling them in.
   * This is where things becomes tricky to predict for the maximum
   * performance: on the one hand this avoids too much threading overhead on
   * few nodes, but for the final performance having all the overhead on one
   * node might be better idea (since other nodes will have better chance of
   * rendering faster).
   * But more tricky is that nodes might have difference capacity, so we might
   * want to do some weighted scheduling. For example, if node 0 has 16
   * processors and node 1 has 32 processors, we'd better schedule 1 extra
   * thread on node 0 and 2 extra threads on node 1. */
  current_node_index = 0;
  while (thread_index < num_threads) {
    /* Skip unavailable nodes. */
    /* TODO(sergey): Add sanity check against deadlock. */
    while (num_per_node_processors[current_node_index] == 0) {
      current_node_index = (current_node_index + 1) % num_nodes;
    }
    VLOG(1) << "Scheduling thread " << thread_index << " to node " << current_node_index << ".";
    ++thread_index;
    current_node_index = (current_node_index + 1) % num_nodes;
  }

  return thread_nodes;
}

}  // namespace

void TaskScheduler::init(int num_threads)
{
  thread_scoped_lock lock(mutex);
  /* Multiple cycles instances can use this task scheduler, sharing the same
   * threads, so we keep track of the number of users. */
  ++users;
  if (users != 1) {
    return;
  }
  do_exit = false;
  const bool use_auto_threads = (num_threads == 0);
  if (use_auto_threads) {
    /* Automatic number of threads. */
    num_threads = system_cpu_thread_count();
  }
  VLOG(1) << "Creating pool of " << num_threads << " threads.";

  /* Compute distribution on NUMA nodes. */
  vector<int> thread_nodes = distribute_threads_on_nodes(num_threads);

  /* Launch threads that will be waiting for work. */
  threads.resize(num_threads);
  for (int thread_index = 0; thread_index < num_threads; ++thread_index) {
    threads[thread_index] = new thread(function_bind(&TaskScheduler::thread_run, thread_index + 1),
                                       thread_nodes[thread_index]);
  }
}

void TaskScheduler::exit()
{
  thread_scoped_lock lock(mutex);
  users--;
  if (users == 0) {
    VLOG(1) << "De-initializing thread pool of task scheduler.";
    /* stop all waiting threads */
    TaskScheduler::queue_mutex.lock();
    do_exit = true;
    TaskScheduler::queue_cond.notify_all();
    TaskScheduler::queue_mutex.unlock();

    /* delete threads */
    foreach (thread *t, threads) {
      t->join();
      delete t;
    }
    threads.clear();
  }
}

void TaskScheduler::free_memory()
{
  assert(users == 0);
  threads.free_memory();
}

bool TaskScheduler::thread_wait_pop(Entry &entry)
{
  thread_scoped_lock queue_lock(queue_mutex);

  while (queue.empty() && !do_exit)
    queue_cond.wait(queue_lock);

  if (queue.empty()) {
    assert(do_exit);
    return false;
  }

  entry = queue.front();
  queue.pop_front();

  return true;
}

void TaskScheduler::thread_run(int thread_id)
{
  Entry entry;

  /* todo: test affinity/denormal mask */

  /* keep popping off tasks */
  while (thread_wait_pop(entry)) {
    /* run task */
    entry.task->run(thread_id);

    /* delete task */
    delete entry.task;

    /* notify pool task was done */
    entry.pool->num_decrease(1);
  }
}

void TaskScheduler::push(Entry &entry, bool front)
{
  entry.pool->num_increase();

  /* add entry to queue */
  TaskScheduler::queue_mutex.lock();
  if (front)
    TaskScheduler::queue.push_front(entry);
  else
    TaskScheduler::queue.push_back(entry);

  TaskScheduler::queue_cond.notify_one();
  TaskScheduler::queue_mutex.unlock();
}

void TaskScheduler::clear(TaskPool *pool)
{
  thread_scoped_lock queue_lock(TaskScheduler::queue_mutex);

  /* erase all tasks from this pool from the queue */
  list<Entry>::iterator it = queue.begin();
  int done = 0;

  while (it != queue.end()) {
    Entry &entry = *it;

    if (entry.pool == pool) {
      done++;
      delete entry.task;

      it = queue.erase(it);
    }
    else
      it++;
  }

  queue_lock.unlock();

  /* notify done */
  pool->num_decrease(done);
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
  stop();
  worker_thread->join();
  delete worker_thread;
}

void DedicatedTaskPool::push(Task *task, bool front)
{
  num_increase();

  /* add task to queue */
  queue_mutex.lock();
  if (front)
    queue.push_front(task);
  else
    queue.push_back(task);

  queue_cond.notify_one();
  queue_mutex.unlock();
}

void DedicatedTaskPool::push(TaskRunFunction &&run, bool front)
{
  push(new Task(std::move(run)), front);
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

void DedicatedTaskPool::stop()
{
  clear();

  do_exit = true;
  queue_cond.notify_all();

  wait();

  assert(num == 0);
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

bool DedicatedTaskPool::thread_wait_pop(Task *&task)
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
  Task *task;

  /* keep popping off tasks */
  while (thread_wait_pop(task)) {
    /* run task */
    task->run(0);

    /* delete task */
    delete task;

    /* notify task was done */
    num_decrease(1);
  }
}

void DedicatedTaskPool::clear()
{
  thread_scoped_lock queue_lock(queue_mutex);

  /* erase all tasks from the queue */
  list<Task *>::iterator it = queue.begin();
  int done = 0;

  while (it != queue.end()) {
    done++;
    delete *it;

    it = queue.erase(it);
  }

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
