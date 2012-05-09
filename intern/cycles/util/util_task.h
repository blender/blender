/*
 * Copyright 2011, Blender Foundation.
 *
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
 * The run callback that actually executes the task may be create like this:
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

	bool cancelled();	/* for worker threads, test if cancelled */

protected:
	friend class TaskScheduler;

	void done_increase(int done);

	thread_mutex done_mutex;
	thread_condition_variable done_cond;

	volatile int num, num_done;
	volatile bool do_cancel;
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

	static int num_threads() { return threads.size(); }

protected:
	friend class TaskPool;

	struct Entry {
		Task *task;
		TaskPool *pool;
	};

	static thread_mutex mutex;
	static int users;
	static vector<thread*> threads;
	static vector<int> thread_level;
	static volatile bool do_exit;

	static list<Entry> queue;
	static thread_mutex queue_mutex;
	static thread_condition_variable queue_cond;

	static void thread_run(int thread_id);
	static bool thread_wait_pop(Entry& entry);

	static void push(Entry& entry, bool front);
	static void clear(TaskPool *pool);
};

CCL_NAMESPACE_END

#endif

