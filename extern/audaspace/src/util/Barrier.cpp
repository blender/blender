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

#include "util/Barrier.h"

AUD_NAMESPACE_BEGIN
Barrier::Barrier(unsigned int count) :
	m_threshold(count), m_count(count), m_generation(0)
{
}

Barrier::~Barrier()
{
}

void Barrier::wait() 
{
	std::unique_lock<std::mutex> lck(m_mutex);
	int gen = m_generation;
	if(!--m_count) 
	{
		m_count = m_threshold;
		m_generation++;
		m_condition.notify_all();
	}
	else
		m_condition.wait(lck, [this, gen] { return gen != m_generation; });
}
AUD_NAMESPACE_END