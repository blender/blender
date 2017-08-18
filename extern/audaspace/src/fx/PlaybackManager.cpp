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

#include "fx/PlaybackManager.h"
#include "fx/VolumeSound.h"

#include <stdexcept> 

AUD_NAMESPACE_BEGIN
PlaybackManager::PlaybackManager(std::shared_ptr<IDevice> device) :
	m_device(device), m_currentKey(0)
{
}

unsigned int PlaybackManager::addCategory(std::shared_ptr<PlaybackCategory> category)
{
	bool flag = true;
	unsigned int k = -1;
	do {
		auto iter = m_categories.find(m_currentKey);
		if(iter == m_categories.end())
		{
			m_categories[m_currentKey] = category;
			k = m_currentKey;
			m_currentKey++;
			flag = false;
		}
		else
			m_currentKey++;
	} while(flag);

	return k;
}

unsigned int PlaybackManager::addCategory(float volume)
{
	std::shared_ptr<PlaybackCategory> category = std::make_shared<PlaybackCategory>(m_device);
	category->setVolume(volume);
	bool flag = true;
	unsigned int k = -1;
	do {
		auto iter = m_categories.find(m_currentKey);
		if(iter == m_categories.end())
		{
			m_categories[m_currentKey] = category;
			k = m_currentKey;
			m_currentKey++;
			flag = false;
		}
		else
			m_currentKey++;
	} while(flag);

	return k;
}

std::shared_ptr<IHandle> PlaybackManager::play(std::shared_ptr<ISound> sound, unsigned int catKey)
{
	auto iter = m_categories.find(catKey);
	std::shared_ptr<PlaybackCategory> category;

	if(iter != m_categories.end())
	{
		category = iter->second;
	}
	else
	{
		category = std::make_shared<PlaybackCategory>(m_device);
		m_categories[catKey] = category;
	}
	return category->play(sound);
}

bool PlaybackManager::resume(unsigned int catKey)
{
	auto iter = m_categories.find(catKey);

	if(iter != m_categories.end())
	{
		iter->second->resume();
		return true;
	}
	else
	{
		return false;
	}
}

bool PlaybackManager::pause(unsigned int catKey)
{
	auto iter = m_categories.find(catKey);

	if(iter != m_categories.end())
	{
		iter->second->pause();
		return true;
	}
	else
	{
		return false;
	}
}

float PlaybackManager::getVolume(unsigned int catKey)
{
	auto iter = m_categories.find(catKey);

	if(iter != m_categories.end())
	{
		return iter->second->getVolume();
	}
	else
	{
		return -1.0;
	}
}

bool PlaybackManager::setVolume(float volume, unsigned int catKey)
{
	auto iter = m_categories.find(catKey);

	if(iter != m_categories.end())
	{
		iter->second->setVolume(volume);
		return true;
	}
	else
	{
		return false;
	}
}

bool PlaybackManager::stop(unsigned int catKey)
{
	auto iter = m_categories.find(catKey);

	if(iter != m_categories.end())
	{
		iter->second->stop();
		return true;
	}
	else
	{
		return false;
	}
}

void PlaybackManager::clean()
{
	for(auto cat : m_categories)
		cat.second->cleanHandles();
}

bool PlaybackManager::clean(unsigned int catKey)
{
	auto iter = m_categories.find(catKey);

	if(iter != m_categories.end())
	{
		iter->second->cleanHandles();
		return true;
	}
	else
	{
		return false;
	}
}

std::shared_ptr<IDevice> PlaybackManager::getDevice()
{
	return m_device;
}
AUD_NAMESPACE_END
