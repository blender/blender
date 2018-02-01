/*******************************************************************************
* Copyright 2015-2016 Juan Francisco Crespo Galán
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
******************************************************************************/

#pragma once

/**
* @file ThreadPool.h
* @ingroup util
* The ThreadPool class.
*/

#include "Audaspace.h"

#include <mutex>
#include <condition_variable>
#include <vector>
#include <thread>
#include <queue>
#include <future>
#include <functional>

AUD_NAMESPACE_BEGIN
/**
* This represents pool of threads.
*/
class AUD_API ThreadPool
{
private:
	/** 
	* A queue of tasks.
	*/
	std::queue<std::function<void()>> m_queue;

	/**
	* A vector of thread objects.
	*/
	std::vector<std::thread> m_threads;

	/**
	* A mutex for synchronization.
	*/
	std::mutex m_mutex;

	/**
	* A condition variable used to stop the threads when there are no tasks.
	*/
	std::condition_variable m_condition;

	/**
	* Stop flag.
	*/
	bool m_stopFlag;

	/**
	* The number fo threads.
	*/
	unsigned int m_numThreads;

	// delete copy constructor and operator=
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
public:
	/**
	* Creates a new ThreadPool object.
	* \param count The number of threads of the pool. It must not be 0.
	*/
	ThreadPool(unsigned int count);

	virtual ~ThreadPool();

	/**
	* Enqueues a new task for the threads to realize.
	* \param t A function that realices a task.
	* \param args The arguments of the task.
	* \return A future of the same type as the return type of the task.
	*/
	template<class T, class... Args>
	std::future<typename std::result_of<T(Args...)>::type> enqueue(T&& t, Args&&... args)
	{
		using pkgdTask = std::packaged_task<typename std::result_of<T(Args...)>::type()>;

		std::shared_ptr<pkgdTask> task = std::make_shared<pkgdTask>(std::bind(std::forward<T>(t), std::forward<Args>(args)...));
		auto result = task->get_future();

		m_mutex.lock();
		m_queue.emplace([task]() { (*task)(); });
		m_mutex.unlock();

		m_condition.notify_one();
		return result;
	}

	/**
	* Retrieves the number of threads of the pool.
	* \return The number of threads.
	*/
	unsigned int getNumOfThreads();

private:

	/**
	* Worker thread function.
	*/
	void threadFunction();
};
AUD_NAMESPACE_END
