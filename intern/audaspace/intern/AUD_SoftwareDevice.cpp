/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/intern/AUD_SoftwareDevice.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_SoftwareDevice.h"
#include "AUD_IReader.h"
#include "AUD_Mixer.h"
#include "AUD_IFactory.h"
#ifdef WITH_SAMPLERATE
#include "AUD_SRCResampleReader.h"
#else
#include "AUD_LinearResampleReader.h"
#endif
#include "AUD_ChannelMapperReader.h"

#include <cstring>
#include <limits>

AUD_SoftwareDevice::AUD_SoftwareHandle::AUD_SoftwareHandle(AUD_SoftwareDevice* device, AUD_Reference<AUD_IReader> reader, AUD_Reference<AUD_PitchReader> pitch, bool keep) :
	m_reader(reader), m_pitch(pitch), m_keep(keep), m_volume(1.0f), m_loopcount(0),
	m_stop(NULL), m_stop_data(NULL), m_status(AUD_STATUS_PLAYING), m_device(device)
{
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::pause()
{
	if(m_status)
	{
		m_device->lock();

		if(m_status == AUD_STATUS_PLAYING)
		{
			m_device->m_playingSounds.remove(this);
			m_device->m_pausedSounds.push_back(this);

			if(m_device->m_playingSounds.empty())
				m_device->playing(m_device->m_playback = false);
			m_status = AUD_STATUS_PAUSED;
			m_device->unlock();

			return true;
		}

		m_device->unlock();
	}

	return false;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::resume()
{
	if(m_status)
	{
		m_device->lock();

		if(m_status == AUD_STATUS_PAUSED)
		{
			m_device->m_pausedSounds.remove(this);
			m_device->m_playingSounds.push_back(this);

			if(!m_device->m_playback)
				m_device->playing(m_device->m_playback = true);
			m_status = AUD_STATUS_PLAYING;
			m_device->unlock();
			return true;
		}

		m_device->unlock();
	}

	return false;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::stop()
{
	if(!m_status)
		return false;

	m_device->lock();

	if(m_status == AUD_STATUS_PLAYING)
	{
		m_device->m_playingSounds.remove(this);

		if(m_device->m_playingSounds.empty())
			m_device->playing(m_device->m_playback = false);
	}
	else
		m_device->m_pausedSounds.remove(this);

	m_device->unlock();
	m_status = AUD_STATUS_INVALID;
	return true;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::getKeep()
{
	if(m_status)
		return m_keep;

	return false;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setKeep(bool keep)
{
	if(!m_status)
		return false;

	m_device->lock();

	m_keep = keep;

	m_device->unlock();

	return true;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::seek(float position)
{
	if(!m_status)
		return false;

	m_device->lock();

	m_reader->seek((int)(position * m_reader->getSpecs().rate));

	m_device->unlock();

	return true;
}

float AUD_SoftwareDevice::AUD_SoftwareHandle::getPosition()
{
	if(!m_status)
		return 0.0f;

	m_device->lock();

	float position = m_reader->getPosition() / (float)m_device->m_specs.rate;

	m_device->unlock();

	return position;
}

AUD_Status AUD_SoftwareDevice::AUD_SoftwareHandle::getStatus()
{
	return m_status;
}

float AUD_SoftwareDevice::AUD_SoftwareHandle::getVolume()
{
	return m_volume;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setVolume(float volume)
{
	if(!m_status)
		return false;
	m_volume = volume;
	return true;
}

float AUD_SoftwareDevice::AUD_SoftwareHandle::getPitch()
{
	return m_pitch->getPitch();
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setPitch(float pitch)
{
	m_pitch->setPitch(pitch);
	return true;
}

int AUD_SoftwareDevice::AUD_SoftwareHandle::getLoopCount()
{
	if(!m_status)
		return 0;
	return m_loopcount;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setLoopCount(int count)
{
	if(!m_status)
		return false;
	m_loopcount = count;
	return true;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setStopCallback(stopCallback callback, void* data)
{
	if(!m_status)
		return false;

	m_device->lock();

	m_stop = callback;
	m_stop_data = data;

	m_device->unlock();

	return true;
}











void AUD_SoftwareDevice::create()
{
	m_playback = false;
	m_volume = 1.0f;
	m_mixer = new AUD_Mixer(m_specs);

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

	while(!m_playingSounds.empty())
		m_playingSounds.front()->stop();

	while(!m_pausedSounds.empty())
		m_pausedSounds.front()->stop();

	pthread_mutex_destroy(&m_mutex);
}

void AUD_SoftwareDevice::mix(data_t* buffer, int length)
{
	m_buffer.assureSize(length * AUD_SAMPLE_SIZE(m_specs));

	lock();

	{
		AUD_Reference<AUD_SoftwareDevice::AUD_SoftwareHandle> sound;
		int len;
		int pos;
		bool eos;
		std::list<AUD_Reference<AUD_SoftwareDevice::AUD_SoftwareHandle> > stopSounds;
		sample_t* buf = m_buffer.getBuffer();

		m_mixer->clear(length);

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

			sound->m_reader->read(len, eos, buf);

			// in case of looping
			while(pos + len < length && sound->m_loopcount && eos)
			{
				m_mixer->mix(buf, pos, len, sound->m_volume);

				pos += len;

				if(sound->m_loopcount > 0)
					sound->m_loopcount--;

				sound->m_reader->seek(0);

				len = length - pos;
				sound->m_reader->read(len, eos, buf);

				// prevent endless loop
				if(!len)
					break;
			}

			m_mixer->mix(buf, pos, len, sound->m_volume);

			// in case the end of the sound is reached
			if(eos && !sound->m_loopcount)
			{
				if(sound->m_stop)
					sound->m_stop(sound->m_stop_data);

				if(sound->m_keep)
					sound->pause();
				else
					stopSounds.push_back(sound);
			}
		}

		// superpose
		m_mixer->read(buffer, m_volume);

		// cleanup
		while(!stopSounds.empty())
		{
			sound = stopSounds.front();
			stopSounds.pop_front();
			sound->stop();
		}
	}

	unlock();
}

AUD_DeviceSpecs AUD_SoftwareDevice::getSpecs() const
{
	return m_specs;
}

AUD_Reference<AUD_IHandle> AUD_SoftwareDevice::play(AUD_Reference<AUD_IReader> reader, bool keep)
{
	// prepare the reader
	// pitch

	AUD_Reference<AUD_PitchReader> pitch = new AUD_PitchReader(reader, 1);
	reader = AUD_Reference<AUD_IReader>(pitch);

	// resample
	#ifdef WITH_SAMPLERATE
		reader = new AUD_SRCResampleReader(reader, m_specs.specs);
	#else
		reader = new AUD_LinearResampleReader(reader, m_specs.specs);
	#endif

	// rechannel
	reader = new AUD_ChannelMapperReader(reader, m_specs.channels);

	if(reader.isNull())
		return NULL;

	// play sound
	AUD_Reference<AUD_SoftwareDevice::AUD_SoftwareHandle> sound = new AUD_SoftwareDevice::AUD_SoftwareHandle(this, reader, pitch, keep);

	lock();
	m_playingSounds.push_back(sound);

	if(!m_playback)
		playing(m_playback = true);
	unlock();

	return AUD_Reference<AUD_IHandle>(sound);
}

AUD_Reference<AUD_IHandle> AUD_SoftwareDevice::play(AUD_Reference<AUD_IFactory> factory, bool keep)
{
	return play(factory->createReader(), keep);
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
