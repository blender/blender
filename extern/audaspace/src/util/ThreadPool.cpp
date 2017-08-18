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

#include "util/ThreadPool.h"

AUD_NAMESPACE_BEGIN

ThreadPool::ThreadPool(unsigned int count) :
	m_stopFlag(false), m_numThreads(count)
{
	for(unsigned int i = 0; i < count; i++)
		m_threads.emplace_back(&ThreadPool::threadFunction, this);
}

ThreadPool::~ThreadPool()
{
	m_mutex.lock();
	m_stopFlag = true;
	m_mutex.unlock();
	m_condition.notify_all();
	for(unsigned int i = 0; i < m_threads.size(); i++)
		m_threads[i].join();
}

unsigned int ThreadPool::getNumOfThreads()
{
	return m_numThreads;
}

void ThreadPool::threadFunction()
{
	while(true)
	{
		std::function<void()> task;
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_condition.wait(lock, [this] { return m_stopFlag || !m_queue.empty(); });
			if(m_stopFlag && m_queue.empty())
				return;
			task = std::move(m_queue.front());
			this->m_queue.pop();
		}
		task();
	}
}

AUD_NAMESPACE_END
