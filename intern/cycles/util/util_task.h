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

#include "util_list.h"
#include "util_thread.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class Task;
class TaskPool;
class TaskScheduler;

typedef boost::function<void(void)> TaskRunFunction;

/* Task
 *
 * Base class for tasks to be executed in threads. */

class Task
{
public:
	Task() {};
	Task(const TaskRunFunction& run_) : run(run_) {}

	virtual ~Task() {}

	TaskRunFunction run;
};

/* Task Pool
 *
 * Pool of tasks that will be executed by the central TaskScheduler.For each
 * pool, we can wait for all tasks to be done, or cancel them before they are
 * done.
 *
 * The run callback that actually executes the task may be created like this:
 * function_bind(&MyClass::task_execute, this, _1, _2) */

class TaskPool
{
public:
	TaskPool();
	~TaskPool();

	void push(Task *task, bool front = false);
	void push(const TaskRunFunction& run, bool front = false);

	void wait_work();	/* work and wait until all tasks are done */
	void cancel();		/* cancel all tasks, keep worker threads running */
	void stop();		/* stop all worker threads */

	bool canceled();	/* for worker threads, test if canceled */

protected:
	friend class TaskScheduler;

	void num_decrease(int done);
	void num_increase();

	thread_mutex num_mutex;
	thread_condition_variable num_cond;

	int num;
	bool do_cancel;
};

/* Task Scheduler
 * 
 * Central scheduler that holds running threads ready to execute tasks. A singe
 * queue holds the task from all pools. */

class TaskScheduler
{
public:
	static void init(int num_threads = 0);
	static void exit();

	/* number of threads that can work on task */
	static int num_threads() { return threads.size(); }

	/* test if any session is using the scheduler */
	static bool active() { return users != 0; }

protected:
	friend class TaskPool;

	struct Entry {
		Task *task;
		TaskPool *pool;
	};

	static thread_mutex mutex;
	static int users;
	static vector<thread*> threads;
	static bool do_exit;

	static list<Entry> queue;
	static thread_mutex queue_mutex;
	static thread_condition_variable queue_cond;

	static void thread_run(int thread_id);
	static bool thread_wait_pop(Entry& entry);

	static void push(Entry& entry, bool front);
	static void clear(TaskPool *pool);
};

/* Dedicated Task Pool
 *
 * Like a TaskPool, but will launch one dedicated thread to execute all tasks.
 *
 * The run callback that actually executes the task may be created like this:
 * function_bind(&MyClass::task_execute, this, _1, _2) */

class DedicatedTaskPool
{
public:
	DedicatedTaskPool();
	~DedicatedTaskPool();

	void push(Task *task, bool front = false);
	void push(const TaskRunFunction& run, bool front = false);

	void wait();        /* wait until all tasks are done */
	void cancel();		/* cancel all tasks, keep worker thread running */
	void stop();		/* stop worker thread */

	bool canceled();	/* for worker thread, test if canceled */

protected:
	void num_decrease(int done);
	void num_increase();

	void thread_run();
	bool thread_wait_pop(Task*& entry);

	void clear();

	thread_mutex num_mutex;
	thread_condition_variable num_cond;

	list<Task*> queue;
	thread_mutex queue_mutex;
	thread_condition_variable queue_cond;

	int num;
	bool do_cancel;
	bool do_exit;

	thread *worker_thread;
};

CCL_NAMESPACE_END

#endif

