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

TaskPool::TaskPool()
{
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

void TaskPool::push(const TaskRunFunction& run, bool front)
{
	push(new Task(run), front);
}

void TaskPool::wait_work()
{
	thread_scoped_lock num_lock(num_mutex);

	while(num != 0) {
		num_lock.unlock();

		thread_scoped_lock queue_lock(TaskScheduler::queue_mutex);

		/* find task from this pool. if we get a task from another pool,
		 * we can get into deadlock */
		TaskScheduler::Entry work_entry;
		bool found_entry = false;
		list<TaskScheduler::Entry>::iterator it;

		for(it = TaskScheduler::queue.begin(); it != TaskScheduler::queue.end(); it++) {
			TaskScheduler::Entry& entry = *it;

			if(entry.pool == this) {
				work_entry = entry;
				found_entry = true;
				TaskScheduler::queue.erase(it);
				break;
			}
		}

		queue_lock.unlock();

		/* if found task, do it, otherwise wait until other tasks are done */
		if(found_entry) {
			/* run task */
			work_entry.task->run();

			/* delete task */
			delete work_entry.task;

			/* notify pool task was done */
			num_decrease(1);
		}

		num_lock.lock();
		if(num == 0)
			break;

		if(!found_entry)
			num_cond.wait(num_lock);
	}
}

void TaskPool::cancel()
{
	do_cancel = true;

	TaskScheduler::clear(this);
	
	{
		thread_scoped_lock num_lock(num_mutex);

		while(num)
			num_cond.wait(num_lock);
	}

	do_cancel = false;
}

void TaskPool::stop()
{
	TaskScheduler::clear(this);

	assert(num == 0);
}

bool TaskPool::cancelled()
{
	return do_cancel;
}

void TaskPool::num_decrease(int done)
{
	num_mutex.lock();
	num -= done;

	assert(num >= 0);
	if(num == 0)
		num_cond.notify_all();

	num_mutex.unlock();
}

void TaskPool::num_increase()
{
	thread_scoped_lock num_lock(num_mutex);
	num++;
	num_cond.notify_all();
}

/* Task Scheduler */

thread_mutex TaskScheduler::mutex;
int TaskScheduler::users = 0;
vector<thread*> TaskScheduler::threads;
vector<int> TaskScheduler::thread_level;
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
		thread_level.resize(num_threads);

		for(size_t i = 0; i < threads.size(); i++) {
			threads[i] = new thread(function_bind(&TaskScheduler::thread_run, i));
			thread_level[i] = 0;
		}
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
		thread_level.clear();
	}
}

bool TaskScheduler::thread_wait_pop(Entry& entry)
{
	thread_scoped_lock queue_lock(queue_mutex);

	while(queue.empty() && !do_exit)
		queue_cond.wait(queue_lock);

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
		entry.task->run();

		/* delete task */
		delete entry.task;

		/* notify pool task was done */
		entry.pool->num_decrease(1);
	}
}

void TaskScheduler::push(Entry& entry, bool front)
{
	entry.pool->num_increase();

	/* add entry to queue */
	TaskScheduler::queue_mutex.lock();
	if(front)
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

	while(it != queue.end()) {
		Entry& entry = *it;

		if(entry.pool == pool) {
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

CCL_NAMESPACE_END

