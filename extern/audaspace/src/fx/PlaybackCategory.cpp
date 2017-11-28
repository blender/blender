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

#include "fx/PlaybackCategory.h"
#include "fx/VolumeSound.h"

AUD_NAMESPACE_BEGIN

struct HandleData {
	unsigned int id;
	PlaybackCategory* category;
};

PlaybackCategory::PlaybackCategory(std::shared_ptr<IDevice> device) :
	m_currentID(0), m_device(device), m_status(STATUS_PLAYING), m_volumeStorage(std::make_shared<VolumeStorage>(1.0f))
{
}

PlaybackCategory::~PlaybackCategory()
{
	stop();
}

std::shared_ptr<IHandle> PlaybackCategory::play(std::shared_ptr<ISound> sound)
{
	std::shared_ptr<ISound> vs(std::make_shared<VolumeSound>(sound, m_volumeStorage));
	m_device->lock();
	auto handle = m_device->play(vs);
	if(handle == nullptr)
		return nullptr;
	switch (m_status) 
	{
	case STATUS_PAUSED:
		handle->pause();
		break;
	default:
		m_status = STATUS_PLAYING;
	};
	m_handles[m_currentID] = handle;
	HandleData* data = new HandleData;
	data->category = this;
	data->id = m_currentID;
	handle->setStopCallback(cleanHandleCallback, data);
	m_device->unlock();
	
	m_currentID++;
	return handle;
}

void PlaybackCategory::resume()
{
	m_device->lock();
	for(auto i = m_handles.begin(); i != m_handles.end();)
	{
		if(i->second->getStatus() == STATUS_INVALID)
			i = m_handles.erase(i);
		else
		{
			i->second->resume();
			i++;
		}
	}
	m_device->unlock();
	m_status = STATUS_PLAYING;
}

void PlaybackCategory::pause()
{
	m_device->lock();
	for(auto i = m_handles.begin(); i != m_handles.end();)
	{
		if(i->second->getStatus() == STATUS_INVALID)
			i = m_handles.erase(i);
		else
		{
			i->second->pause();
			i++;
		}
	}
	m_device->unlock();
	m_status = STATUS_PAUSED;
}

float PlaybackCategory::getVolume()
{
	return m_volumeStorage->getVolume();
}

void PlaybackCategory::setVolume(float volume)
{
	m_volumeStorage->setVolume(volume);
}

void PlaybackCategory::stop() 
{
	m_device->lock();
	for(auto i = m_handles.begin(); i != m_handles.end();)
	{
		i->second->stop();
		if(i->second->getStatus() == STATUS_INVALID)
			i = m_handles.erase(i);
		else
			i++;			
	}
	m_device->unlock();
	m_status = STATUS_STOPPED;
}

std::shared_ptr<VolumeStorage> PlaybackCategory::getSharedVolume()
{
	return m_volumeStorage;
}

void PlaybackCategory::cleanHandles()
{
	for(auto i = m_handles.begin(); i != m_handles.end();)
	{
		if(i->second->getStatus() == STATUS_INVALID)
			i = m_handles.erase(i);
		else
			i++;
	}
}

void PlaybackCategory::cleanHandleCallback(void* data)
{
	auto dat = reinterpret_cast<HandleData*>(data);
	dat->category->m_handles.erase(dat->id);
	delete dat;
}
AUD_NAMESPACE_END
