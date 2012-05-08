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

#ifndef __UTIL_THREAD_H__
#define __UTIL_THREAD_H__

#include <boost/thread.hpp>
#include <pthread.h>
#include <queue>

#include "util_function.h"

CCL_NAMESPACE_BEGIN

/* use boost for mutexes */

typedef boost::mutex thread_mutex;
typedef boost::mutex::scoped_lock thread_scoped_lock;
typedef boost::condition_variable thread_condition_variable;

/* own pthread based implementation, to avoid boost version conflicts with
   dynamically loaded blender plugins */

class thread {
public:
	thread(boost::function<void(void)> run_cb_)
	{
		joined = false;
		run_cb = run_cb_;

		pthread_create(&pthread_id, NULL, run, (void*)this);
	}

	~thread()
	{
		if(!joined)
			join();
	}

	static void *run(void *arg)
	{
		((thread*)arg)->run_cb();;
		return NULL;
	}

	bool join()
	{
		return pthread_join(pthread_id, NULL) == 0;
	}

protected:
	boost::function<void(void)> run_cb;
	pthread_t pthread_id;
	bool joined;
};

/* Thread Safe Queue to pass tasks from one thread to another. Tasks should be
 * pushed into the queue, while the worker thread waits to pop the next task
 * off the queue. Once all tasks are into the queue, calling stop() will stop
 * the worker threads from waiting for more tasks once all tasks are done. */

template<typename T> class ThreadQueue
{
public:
	ThreadQueue()
	{
		tot = 0;
		tot_done = 0;
		do_stop = false;
		do_cancel = false;
	}

	/* Main thread functions */

	/* push a task to be executed */
	void push(const T& value)
	{
		thread_scoped_lock lock(queue_mutex);
		queue.push(value);
		tot++;
		lock.unlock();

		queue_cond.notify_one();
	}

	/* wait until all tasks are done */
	void wait_done()
	{
		thread_scoped_lock lock(done_mutex);

		while(tot_done != tot)
			done_cond.wait(lock);
	}

	/* stop all worker threads */
	void stop()
	{
		clear();
		do_stop = true;
		queue_cond.notify_all();
	}

	/* cancel all tasks, but keep worker threads running */
	void cancel()
	{
		clear();
		do_cancel = true;
		wait_done();
		do_cancel = false;
	}

	/* Worker thread functions
     *
	 * while(queue.worker_wait_pop(task)) {
	 *		for(..) {
	 *			... do work ...
	 *
	 *			if(queue.worker_cancel())
	 *				break;
	 *      }
	 *		
	 *		queue.worker_done();
	 * }
	 */

	bool worker_wait_pop(T& value)
	{
		thread_scoped_lock lock(queue_mutex);

		while(queue.empty() && !do_stop)
			queue_cond.wait(lock);

		if(queue.empty())
			return false;
		
		value = queue.front();
		queue.pop();

		return true;
	}

	void worker_done()
	{
		thread_scoped_lock lock(done_mutex);
		tot_done++;
		lock.unlock();

		assert(tot_done <= tot);

		done_cond.notify_all();
	}

	bool worker_cancel()
	{
		return do_cancel;
	}

protected:
	void clear()
	{
		thread_scoped_lock lock(queue_mutex);

		while(!queue.empty()) {
			thread_scoped_lock done_lock(done_mutex);
			tot_done++;
			done_lock.unlock();

			queue.pop();
		}

		done_cond.notify_all();
	}

	std::queue<T> queue;
	thread_mutex queue_mutex;
	thread_mutex done_mutex;
	thread_condition_variable queue_cond;
	thread_condition_variable done_cond;
	volatile bool do_stop;
	volatile bool do_cancel;
	volatile int tot, tot_done;
};

/* Thread Local Storage
 *
 * Boost implementation is a bit slow, and Mac OS X __thread is not supported
 * but the pthreads implementation is optimized, so we use these macros. */

#ifdef __APPLE__

#define tls_ptr(type, name) \
	pthread_key_t name
#define tls_set(name, value) \
	pthread_setspecific(name, value)
#define tls_get(type, name) \
	((type*)pthread_getspecific(name))
#define tls_create(type, name) \
	pthread_key_create(&name, NULL)
#define tls_delete(type, name) \
	pthread_key_delete(name);

#else

#ifdef __WIN32
#define __thread __declspec(thread)
#endif

#define tls_ptr(type, name) \
	__thread type *name
#define tls_set(name, value) \
	name = value
#define tls_get(type, name) \
	name
#define tls_create(type, name)
#define tls_delete(type, name)

#endif

CCL_NAMESPACE_END

#endif /* __UTIL_THREAD_H__ */

