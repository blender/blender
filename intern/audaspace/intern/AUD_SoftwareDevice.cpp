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
#include "AUD_DefaultMixer.h"
#include "AUD_IFactory.h"

#include <cstring>
#include <limits>

/// Saves the data for playback.
struct AUD_SoftwareHandle : AUD_Handle
{
	/// The reader source.
	AUD_IReader* reader;

	/// Whether to keep the source if end of it is reached.
	bool keep;

	/// The volume of the source.
	float volume;

	/// The loop count of the source.
	int loopcount;

	/// The stop callback.
	stopCallback stop;

	/// Stop callback data.
	void* stop_data;
};

typedef std::list<AUD_SoftwareHandle*>::iterator AUD_HandleIterator;

void AUD_SoftwareDevice::create()
{
	m_playback = false;
	m_volume = 1.0f;
	m_mixer = new AUD_DefaultMixer(m_specs);

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

	delete m_mixer;

	AUD_SoftwareHandle* handle;

	// delete all playing sounds
	while(!m_playingSounds.empty())
	{
		handle = m_playingSounds.front();
		m_playingSounds.pop_front();
		delete handle->reader;
		delete handle;
	}

	// delete all paused sounds
	while(!m_pausedSounds.empty())
	{
		handle = m_pausedSounds.front();
		m_pausedSounds.pop_front();
		delete handle->reader;
		delete handle;
	}

	pthread_mutex_destroy(&m_mutex);
}

void AUD_SoftwareDevice::mix(data_t* buffer, int length)
{
	lock();

	{
		AUD_SoftwareHandle* sound;
		int len;
		int pos;
		sample_t* buf;
		std::list<AUD_SoftwareHandle*> stopSounds;
		std::list<AUD_Buffer*> tempBufs;
		AUD_Buffer* tempbuf;
		int samplesize = AUD_SAMPLE_SIZE(m_specs);

		// for all sounds
		AUD_HandleIterator it = m_playingSounds.begin();
		while(it != m_playingSounds.end())
		{
			sound = *it;
			// increment the iterator to make sure it's valid,
			// in case the sound gets deleted after stopping
			++it;

			// get the buffer from the source
			pos = 0;
			len = length;
			sound->reader->read(len, buf);

			// in case of looping
			while(pos + len < length && sound->loopcount)
			{
				tempbuf = new AUD_Buffer(len * samplesize);
				memcpy(tempbuf->getBuffer(), buf, len * samplesize);
				tempBufs.push_back(tempbuf);
				m_mixer->add(tempbuf->getBuffer(), pos, len, sound->volume);

				pos += len;

				if(sound->loopcount > 0)
					sound->loopcount--;

				sound->reader->seek(0);

				len = length - pos;
				sound->reader->read(len, buf);

				// prevent endless loop
				if(!len)
					break;
			}

			m_mixer->add(buf, pos, len, sound->volume);
			pos += len;

			// in case the end of the sound is reached
			if(pos < length)
			{
				if(sound->stop)
					sound->stop(sound->stop_data);

				if(sound->keep)
					pause(sound);
				else
					stopSounds.push_back(sound);
			}
		}

		// superpose
		m_mixer->superpose(buffer, length, m_volume);

		// cleanup
		while(!stopSounds.empty())
		{
			sound = stopSounds.front();
			stopSounds.pop_front();
			stop(sound);
		}

		while(!tempBufs.empty())
		{
			tempbuf = tempBufs.front();
			tempBufs.pop_front();
			delete tempbuf;
		}
	}

	unlock();
}

bool AUD_SoftwareDevice::isValid(AUD_Handle* handle)
{
	for(AUD_HandleIterator i = m_playingSounds.begin();
		i != m_playingSounds.end(); i++)
		if(*i == handle)
			return true;
	for(AUD_HandleIterator i = m_pausedSounds.begin();
		i != m_pausedSounds.end(); i++)
		if(*i == handle)
			return true;
	return false;
}

AUD_DeviceSpecs AUD_SoftwareDevice::getSpecs() const
{
	return m_specs;
}

AUD_Handle* AUD_SoftwareDevice::play(AUD_IFactory* factory, bool keep)
{
	AUD_IReader* reader = factory->createReader();

	// prepare the reader
	reader = m_mixer->prepare(reader);
	if(reader == NULL)
		return NULL;

	// play sound
	AUD_SoftwareHandle* sound = new AUD_SoftwareHandle;
	sound->keep = keep;
	sound->reader = reader;
	sound->volume = 1.0f;
	sound->loopcount = 0;
	sound->stop = NULL;
	sound->stop_data = NULL;

	lock();
	m_playingSounds.push_back(sound);

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
	for(AUD_HandleIterator i = m_playingSounds.begin();
		i != m_playingSounds.end(); i++)
	{
		if(*i == handle)
		{
			m_pausedSounds.push_back(*i);
			m_playingSounds.erase(i);
			if(m_playingSounds.empty())
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
	for(AUD_HandleIterator i = m_pausedSounds.begin();
		i != m_pausedSounds.end(); i++)
	{
		if(*i == handle)
		{
			m_playingSounds.push_back(*i);
			m_pausedSounds.erase(i);
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

	for(AUD_HandleIterator i = m_playingSounds.begin();
		i != m_playingSounds.end(); i++)
	{
		if(*i == handle)
		{
			delete (*i)->reader;
			delete *i;
			m_playingSounds.erase(i);
			if(m_playingSounds.empty())
				playing(m_playback = false);
			result = true;
			break;
		}
	}
	if(!result)
	{
		for(AUD_HandleIterator i = m_pausedSounds.begin();
			i != m_pausedSounds.end(); i++)
		{
			if(*i == handle)
			{
				delete (*i)->reader;
				delete *i;
				m_pausedSounds.erase(i);
				result = true;
				break;
			}
		}
	}

	unlock();

	return result;
}

bool AUD_SoftwareDevice::getKeep(AUD_Handle* handle)
{
	bool result = false;

	lock();

	if(isValid(handle))
		result = ((AUD_SoftwareHandle*)handle)->keep;

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

	for(AUD_HandleIterator i = m_playingSounds.begin();
		i != m_playingSounds.end(); i++)
	{
		if(*i == handle)
		{
			status = AUD_STATUS_PLAYING;
			break;
		}
	}
	if(status == AUD_STATUS_INVALID)
	{
		for(AUD_HandleIterator i = m_pausedSounds.begin();
			i != m_pausedSounds.end(); i++)
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

float AUD_SoftwareDevice::getVolume() const
{
	return m_volume;
}

void AUD_SoftwareDevice::setVolume(float volume)
{
	m_volume = volume;
}

float AUD_SoftwareDevice::getVolume(AUD_Handle* handle)
{
	lock();
	float result = std::numeric_limits<float>::quiet_NaN();
	if(isValid(handle))
		result = ((AUD_SoftwareHandle*)handle)->volume;
	unlock();
	return result;
}

bool AUD_SoftwareDevice::setVolume(AUD_Handle* handle, float volume)
{
	lock();
	bool result = isValid(handle);
	if(result)
		((AUD_SoftwareHandle*)handle)->volume = volume;
	unlock();
	return result;
}

float AUD_SoftwareDevice::getPitch(AUD_Handle* handle)
{
	return std::numeric_limits<float>::quiet_NaN();
}

bool AUD_SoftwareDevice::setPitch(AUD_Handle* handle, float pitch)
{
	return false;
}

int AUD_SoftwareDevice::getLoopCount(AUD_Handle* handle)
{
	lock();
	int result = 0;
	if(isValid(handle))
		result = ((AUD_SoftwareHandle*)handle)->loopcount;
	unlock();
	return result;
}

bool AUD_SoftwareDevice::setLoopCount(AUD_Handle* handle, int count)
{
	lock();
	bool result = isValid(handle);
	if(result)
		((AUD_SoftwareHandle*)handle)->loopcount = count;
	unlock();
	return result;
}

bool AUD_SoftwareDevice::setStopCallback(AUD_Handle* handle, stopCallback callback, void* data)
{
	lock();
	bool result = isValid(handle);
	if(result)
	{
		AUD_SoftwareHandle* h = (AUD_SoftwareHandle*)handle;
		h->stop = callback;
		h->stop_data = data;
	}
	unlock();
	return result;
}
