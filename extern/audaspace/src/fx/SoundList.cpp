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

#include "fx/SoundList.h"
#include "Exception.h"

#include <cstring>
#include <cstdlib>
#include <chrono>

AUD_NAMESPACE_BEGIN

SoundList::SoundList(bool random) :
m_random(random)
{
	std::srand(time(NULL));
}

SoundList::SoundList(std::vector<std::shared_ptr<ISound>>& list, bool random) :
m_list(list), m_random(random)
{
	std::srand(time(NULL));
}

std::shared_ptr<IReader> SoundList::createReader()
{
	if(m_list.size() > 0)
	{
		m_mutex.lock();

		if(!m_random){
			m_index++;
			if(m_index >= m_list.size())
				m_index = 0;
		}
		else
		{
			int temp;
			do{
				temp = std::rand() % m_list.size();
			} while(temp == m_index && m_list.size()>1);
			m_index = temp;
		}
		auto reader = m_list[m_index]->createReader();
		m_mutex.unlock();
		return reader;
	}
	else
		AUD_THROW(FileException, "The sound list is empty");
}

void SoundList::addSound(std::shared_ptr<ISound> sound)
{
	m_list.push_back(sound);
}

void SoundList::setRandomMode(bool random)
{
	m_random = random;
}

bool SoundList::getRandomMode()
{
	return m_random;
}

int SoundList::getSize()
{
	return m_list.size();
}
AUD_NAMESPACE_END