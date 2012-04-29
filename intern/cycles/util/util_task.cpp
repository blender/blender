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

#include "util_debug.h"
#include "util_foreach.h"
#include "util_system.h"
#include "util_task.h"

CCL_NAMESPACE_BEGIN

/* Task Pool */

TaskPool::TaskPool(const TaskRunFunction& run_)
{
	num = 0;
	num_done = 0;

	do_cancel = false;

	run = run_;
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

void TaskPool::wait()
{
	thread_scoped_lock lock(done_mutex);

	while(num_done != num)
		done_cond.wait(lock);
}

void TaskPool::cancel()
{
	TaskScheduler::clear(this);

	do_cancel = true;
	wait();
	do_cancel = false;
}

void TaskPool::stop()
{
	TaskScheduler::clear(this);

	assert(num_done == num);
}

bool TaskPool::cancelled()
{
	return do_cancel;
}

void TaskPool::done_increase(int done)
{
	done_mutex.lock();
	num_done += done;
	done_mutex.unlock();

	assert(num_done <= num);
	done_cond.notify_all();
}

/* Task Scheduler */

thread_mutex TaskScheduler::mutex;
int TaskScheduler::users = 0;
vector<thread*> TaskScheduler::threads;
volatile bool TaskScheduler::do_exit = false;

list<TaskScheduler::Entry> TaskScheduler::queue;
thread_mutex TaskScheduler::queue_mutex;
thread_condition_variable TaskScheduler::queue_cond;

void TaskScheduler::init(int num_threads)
{
	thread_scoped_lock lock(mutex);

	/* multiple cycles instances can use this task scheduler, sharing the same
	   threads, so we keep track of the number of users. */
	if(users == 0) {
		do_exit = false;

		/* launch threads that will be waiting for work */
		if(num_threads == 0)
			num_threads = system_cpu_thread_count();

		threads.resize(num_threads);

		for(size_t i = 0; i < threads.size(); i++)
			threads[i] = new thread(function_bind(&TaskScheduler::thread_run, i));
	}
	
	users++;
}

void TaskScheduler::exit()
{
	thread_scoped_lock lock(mutex);

	users--;

	if(users == 0) {
		/* stop all waiting threads */
		do_exit = true;
		TaskScheduler::queue_cond.notify_all();

		/* delete threads */
		foreach(thread *t, threads) {
			t->join();
			delete t;
		}

		threads.clear();
	}
}

bool TaskScheduler::thread_wait_pop(Entry& entry)
{
	thread_scoped_lock lock(queue_mutex);

	while(queue.empty() && !do_exit)
		queue_cond.wait(lock);

	if(queue.empty()) {
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
	while(thread_wait_pop(entry)) {
		/* run task */
		entry.pool->run(entry.task, thread_id);

		/* delete task */
		delete entry.task;

		/* notify pool task was done */
		entry.pool->done_increase(1);
	}
}

void TaskScheduler::push(Entry& entry, bool front)
{
	/* add entry to queue */
	TaskScheduler::queue_mutex.lock();
	if(front)
		TaskScheduler::queue.push_front(entry);
	else
		TaskScheduler::queue.push_back(entry);
	entry.pool->num++;
	TaskScheduler::queue_mutex.unlock();

	TaskScheduler::queue_cond.notify_one();
}

void TaskScheduler::clear(TaskPool *pool)
{
	thread_scoped_lock lock(TaskScheduler::queue_mutex);

	/* erase all tasks from this pool from the queue */
	list<TaskScheduler::Entry>::iterator it = TaskScheduler::queue.begin();
	int done = 0;

	while(it != TaskScheduler::queue.end()) {
		TaskScheduler::Entry& entry = *it;

		if(entry.pool == pool) {
			done++;
			delete entry.task;

			it = TaskScheduler::queue.erase(it);
		}
		else
			it++;
	}

	/* notify done */
	pool->done_increase(done);
}

CCL_NAMESPACE_END

