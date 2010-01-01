/*
 * $Id$
 *
 * ***** BEGIN LGPL LICENSE BLOCK *****
 *
 * Copyright 2009 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * AudaSpace is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with AudaSpace.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ***** END LGPL LICENSE BLOCK *****
 */

#include "AUD_SoftwareDevice.h"
#include "AUD_IReader.h"
#include "AUD_Mixer.h"
#include "AUD_IFactory.h"
#include "AUD_SourceCaps.h"

#include <cstring>

/// Saves the data for playback.
struct AUD_SoftwareHandle : AUD_Handle
{
	/// The reader source.
	AUD_IReader* reader;

	/// Whether to keep the source if end of it is reached.
	bool keep;

	/// The volume of the source.
	float volume;
};

typedef std::list<AUD_SoftwareHandle*>::iterator AUD_HandleIterator;

void AUD_SoftwareDevice::create()
{
	m_playingSounds = new std::list<AUD_SoftwareHandle*>(); AUD_NEW("list")
	m_pausedSounds = new std::list<AUD_SoftwareHandle*>(); AUD_NEW("list")
	m_playback = false;
	m_volume = 1.0;
	m_mixer = new AUD_Mixer(); AUD_NEW("mixer")
	m_mixer->setSpecs(m_specs);

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&m_mutex, &attr);

	pthread_mutexattr_destroy(&attr);
}

void AUD_SoftwareDevice::destroy()
{
	if(m_playback)
		playing(m_playback = false);

	delete m_mixer; AUD_DELETE("mixer")

	// delete all playing sounds
	while(m_playingSounds->begin() != m_playingSounds->end())
	{
		delete (*(m_playingSounds->begin()))->reader; AUD_DELETE("reader")
		delete *(m_playingSounds->begin()); AUD_DELETE("handle")
		m_playingSounds->erase(m_playingSounds->begin());
	}
	delete m_playingSounds; AUD_DELETE("list")

	// delete all paused sounds
	while(m_pausedSounds->begin() != m_pausedSounds->end())
	{
		delete (*(m_pausedSounds->begin()))->reader; AUD_DELETE("reader")
		delete *(m_pausedSounds->begin()); AUD_DELETE("handle")
		m_pausedSounds->erase(m_pausedSounds->begin());
	}
	delete m_pausedSounds; AUD_DELETE("list")

	pthread_mutex_destroy(&m_mutex);
}

void AUD_SoftwareDevice::mix(data_t* buffer, int length)
{
	lock();

	{
		AUD_SoftwareHandle* sound;
		int len;
		sample_t* buf;
		int sample_size = AUD_DEVICE_SAMPLE_SIZE(m_specs);
		std::list<AUD_SoftwareHandle*> stopSounds;

		// for all sounds
		AUD_HandleIterator it = m_playingSounds->begin();
		while(it != m_playingSounds->end())
		{
			sound = *it;
			// increment the iterator to make sure it's valid,
			// in case the sound gets deleted after stopping
			++it;

			// get the buffer from the source
			len = length;
			sound->reader->read(len, buf);

			m_mixer->add(buf, len, sound->volume);

			// in case the end of the sound is reached
			if(len < length)
			{
				if(sound->keep)
					pause(sound);
				else
					stopSounds.push_back(sound);
			}
		}

		// fill with silence
		if(m_specs.format == AUD_FORMAT_U8)
			memset(buffer, 0x80, length * sample_size);
		else
			memset(buffer, 0, length * sample_size);

		// superpose
		m_mixer->superpose(buffer, length, m_volume);

		while(!stopSounds.empty())
		{
			sound = stopSounds.front();
			stopSounds.pop_front();
			stop(sound);
		}
	}

	unlock();
}

bool AUD_SoftwareDevice::isValid(AUD_Handle* handle)
{
	for(AUD_HandleIterator i = m_playingSounds->begin();
		i != m_playingSounds->end(); i++)
		if(*i == handle)
			return true;
	for(AUD_HandleIterator i = m_pausedSounds->begin();
		i != m_pausedSounds->end(); i++)
		if(*i == handle)
			return true;
	return false;
}

AUD_DeviceSpecs AUD_SoftwareDevice::getSpecs()
{
	return m_specs;
}

AUD_Handle* AUD_SoftwareDevice::play(AUD_IFactory* factory, bool keep)
{
	AUD_IReader* reader = factory->createReader();

	if(reader == NULL)
		AUD_THROW(AUD_ERROR_READER);

	// prepare the reader
	reader = m_mixer->prepare(reader);
	if(reader == NULL)
		return NULL;

	AUD_Specs rs = reader->getSpecs();

	// play sound
	AUD_SoftwareHandle* sound = new AUD_SoftwareHandle; AUD_NEW("handle")
	sound->keep = keep;
	sound->reader = reader;
	sound->volume = 1.0;

	lock();
	m_playingSounds->push_back(sound);

	if(!m_playback)
		playing(m_playback = true);
	unlock();

	return sound;
}

bool AUD_SoftwareDevice::pause(AUD_Handle* handle)
{
	bool result = false;

	lock();

	// only songs that are played can be paused
	for(AUD_HandleIterator i = m_playingSounds->begin();
		i != m_playingSounds->end(); i++)
	{
		if(*i == handle)
		{
			m_pausedSounds->push_back(*i);
			m_playingSounds->erase(i);
			if(m_playingSounds->empty())
				playing(m_playback = false);
			result = true;
			break;
		}
	}

	unlock();

	return result;
}

bool AUD_SoftwareDevice::resume(AUD_Handle* handle)
{
	bool result = false;

	lock();

	// only songs that are paused can be resumed
	for(AUD_HandleIterator i = m_pausedSounds->begin();
		i != m_pausedSounds->end(); i++)
	{
		if(*i == handle)
		{
			m_playingSounds->push_back(*i);
			m_pausedSounds->erase(i);
			if(!m_playback)
				playing(m_playback = true);
			result = true;
			break;
		}
	}

	unlock();

	return result;
}

bool AUD_SoftwareDevice::stop(AUD_Handle* handle)
{
	bool result = false;

	lock();

	for(AUD_HandleIterator i = m_playingSounds->begin();
		i != m_playingSounds->end(); i++)
	{
		if(*i == handle)
		{
			delete (*i)->reader; AUD_DELETE("reader")
			delete *i; AUD_DELETE("handle")
			m_playingSounds->erase(i);
			if(m_playingSounds->empty())
				playing(m_playback = false);
			result = true;
			break;
		}
	}
	if(!result)
	{
		for(AUD_HandleIterator i = m_pausedSounds->begin();
			i != m_pausedSounds->end(); i++)
		{
			if(*i == handle)
			{
				delete (*i)->reader; AUD_DELETE("reader")
				delete *i; AUD_DELETE("handle")
				m_pausedSounds->erase(i);
				result = true;
				break;
			}
		}
	}

	unlock();

	return result;
}

bool AUD_SoftwareDevice::setKeep(AUD_Handle* handle, bool keep)
{
	bool result = false;

	lock();

	if(isValid(handle))
	{
		((AUD_SoftwareHandle*)handle)->keep = keep;
		result = true;
	}

	unlock();

	return result;
}

bool AUD_SoftwareDevice::sendMessage(AUD_Handle* handle, AUD_Message &message)
{
	lock();

	bool result = false;

	if(handle == 0)
	{
		for(AUD_HandleIterator i = m_playingSounds->begin();
			i != m_playingSounds->end(); i++)
			result |= (*i)->reader->notify(message);
		for(AUD_HandleIterator i = m_pausedSounds->begin();
			i != m_pausedSounds->end(); i++)
			result |= (*i)->reader->notify(message);
	}
	else if(isValid(handle))
		result = ((AUD_SoftwareHandle*)handle)->reader->notify(message);
	unlock();
	return result;
}

bool AUD_SoftwareDevice::seek(AUD_Handle* handle, float position)
{
	lock();

	bool result = false;

	if(isValid(handle))
	{
		AUD_IReader* reader = ((AUD_SoftwareHandle*)handle)->reader;
		reader->seek((int)(position * reader->getSpecs().rate));
		result = true;
	}

	unlock();

	return result;
}

float AUD_SoftwareDevice::getPosition(AUD_Handle* handle)
{
	lock();

	float position = 0.0f;

	if(isValid(handle))
	{
		AUD_SoftwareHandle* h = (AUD_SoftwareHandle*)handle;
		position = h->reader->getPosition() / (float)m_specs.rate;
	}

	unlock();

	return position;
}

AUD_Status AUD_SoftwareDevice::getStatus(AUD_Handle* handle)
{
	AUD_Status status = AUD_STATUS_INVALID;

	lock();

	for(AUD_HandleIterator i = m_playingSounds->begin();
		i != m_playingSounds->end(); i++)
	{
		if(*i == handle)
		{
			status = AUD_STATUS_PLAYING;
			break;
		}
	}
	if(status == AUD_STATUS_INVALID)
	{
		for(AUD_HandleIterator i = m_pausedSounds->begin();
			i != m_pausedSounds->end(); i++)
		{
			if(*i == handle)
			{
				status = AUD_STATUS_PAUSED;
				break;
			}
		}
	}

	unlock();

	return status;
}

void AUD_SoftwareDevice::lock()
{
	pthread_mutex_lock(&m_mutex);
}

void AUD_SoftwareDevice::unlock()
{
	pthread_mutex_unlock(&m_mutex);
}

bool AUD_SoftwareDevice::checkCapability(int capability)
{
	return capability == AUD_CAPS_SOFTWARE_DEVICE ||
		   capability == AUD_CAPS_VOLUME ||
		   capability == AUD_CAPS_SOURCE_VOLUME;
}

bool AUD_SoftwareDevice::setCapability(int capability, void *value)
{
	bool result = false;

	switch(capability)
	{
	case AUD_CAPS_VOLUME:
		lock();
		m_volume = *((float*)value);
		if(m_volume > 1.0)
			m_volume = 1.0;
		else if(m_volume < 0.0)
			m_volume = 0.0;
		unlock();
		return true;
	case AUD_CAPS_SOURCE_VOLUME:
		{
			AUD_SourceCaps* caps = (AUD_SourceCaps*) value;
			lock();
			if(isValid(caps->handle))
			{
				AUD_SoftwareHandle* handle = (AUD_SoftwareHandle*)caps->handle;
				handle->volume = caps->value;
				if(handle->volume > 1.0)
					handle->volume = 1.0;
				else if(handle->volume < 0.0)
					handle->volume = 0.0;
				result = true;
			}
			unlock();
		}
		break;
	}

	return result;;
}

bool AUD_SoftwareDevice::getCapability(int capability, void *value)
{
	bool result = false;

	switch(capability)
	{
	case AUD_CAPS_VOLUME:
		lock();
		*((float*)value) = m_volume;
		unlock();
		return true;
	case AUD_CAPS_SOURCE_VOLUME:
		{
			AUD_SourceCaps* caps = (AUD_SourceCaps*) value;

			lock();

			if(isValid(caps->handle))
			{
				caps->value = ((AUD_SoftwareHandle*)caps->handle)->volume;
				result = true;
			}

			unlock();
		}
		break;
	}

	return result;
}
