/*
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
#include "AUD_JOSResampleReader.h"
#include "AUD_LinearResampleReader.h"

#include <cstring>
#include <cmath>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum
{
	AUD_RENDER_DISTANCE = 0x01,
	AUD_RENDER_DOPPLER = 0x02,
	AUD_RENDER_CONE = 0x04,
	AUD_RENDER_VOLUME = 0x08
} AUD_RenderFlags;

#define AUD_PITCH_MAX 10

/******************************************************************************/
/********************** AUD_SoftwareHandle Handle Code ************************/
/******************************************************************************/

AUD_SoftwareDevice::AUD_SoftwareHandle::AUD_SoftwareHandle(AUD_SoftwareDevice* device, AUD_Reference<AUD_IReader> reader, AUD_Reference<AUD_PitchReader> pitch, AUD_Reference<AUD_ResampleReader> resampler, AUD_Reference<AUD_ChannelMapperReader> mapper, bool keep) :
	m_reader(reader), m_pitch(pitch), m_resampler(resampler), m_mapper(mapper), m_keep(keep), m_user_pitch(1.0f), m_user_volume(1.0f), m_user_pan(0.0f), m_volume(1.0f), m_loopcount(0),
	m_relative(true), m_volume_max(1.0f), m_volume_min(0), m_distance_max(std::numeric_limits<float>::max()),
	m_distance_reference(1.0f), m_attenuation(1.0f), m_cone_angle_outer(M_PI), m_cone_angle_inner(M_PI), m_cone_volume_outer(0),
	m_flags(AUD_RENDER_CONE), m_stop(NULL), m_stop_data(NULL), m_status(AUD_STATUS_PLAYING), m_device(device)
{
}

void AUD_SoftwareDevice::AUD_SoftwareHandle::update()
{
	int flags = 0;

	AUD_Vector3 SL;
	if(m_relative)
		SL = -m_location;
	else
		SL = m_device->m_location - m_location;
	float distance = SL * SL;

	if(distance > 0)
		distance = sqrt(distance);
	else
		flags |= AUD_RENDER_DOPPLER | AUD_RENDER_DISTANCE;

	if(m_pitch->getSpecs().channels != AUD_CHANNELS_MONO)
	{
		m_volume = m_user_volume;
		m_pitch->setPitch(m_user_pitch);
		return;
	}

	flags = ~(flags | m_flags | m_device->m_flags);

	// Doppler and Pitch

	if(flags & AUD_RENDER_DOPPLER)
	{
		float vls;
		if(m_relative)
			vls = 0;
		else
			vls = SL * m_device->m_velocity / distance;
		float vss = SL * m_velocity / distance;
		float max = m_device->m_speed_of_sound / m_device->m_doppler_factor;
		if(vss >= max)
		{
			m_pitch->setPitch(AUD_PITCH_MAX);
		}
		else
		{
			if(vls > max)
				vls = max;

			m_pitch->setPitch((m_device->m_speed_of_sound - m_device->m_doppler_factor * vls) / (m_device->m_speed_of_sound - m_device->m_doppler_factor * vss) * m_user_pitch);
		}
	}
	else
		m_pitch->setPitch(m_user_pitch);

	if(flags & AUD_RENDER_VOLUME)
	{
		// Distance

		if(flags & AUD_RENDER_DISTANCE)
		{
			if(m_device->m_distance_model == AUD_DISTANCE_MODEL_INVERSE_CLAMPED ||
			   m_device->m_distance_model == AUD_DISTANCE_MODEL_LINEAR_CLAMPED ||
			   m_device->m_distance_model == AUD_DISTANCE_MODEL_EXPONENT_CLAMPED)
			{
				distance = AUD_MAX(AUD_MIN(m_distance_max, distance), m_distance_reference);
			}

			switch(m_device->m_distance_model)
			{
			case AUD_DISTANCE_MODEL_INVERSE:
			case AUD_DISTANCE_MODEL_INVERSE_CLAMPED:
				m_volume = m_distance_reference / (m_distance_reference + m_attenuation * (distance - m_distance_reference));
				break;
			case AUD_DISTANCE_MODEL_LINEAR:
			case AUD_DISTANCE_MODEL_LINEAR_CLAMPED:
			{
				float temp = m_distance_max - m_distance_reference;
				if(temp == 0)
				{
					if(distance > m_distance_reference)
						m_volume = 0.0f;
					else
						m_volume = 1.0f;
				}
				else
					m_volume = 1.0f - m_attenuation * (distance - m_distance_reference) / (m_distance_max - m_distance_reference);
				break;
			}
			case AUD_DISTANCE_MODEL_EXPONENT:
			case AUD_DISTANCE_MODEL_EXPONENT_CLAMPED:
				if(m_distance_reference == 0)
					m_volume = 0;
				else
					m_volume = pow(distance / m_distance_reference, -m_attenuation);
				break;
			default:
				m_volume = 1.0f;
			}
		}
		else
			m_volume = 1.0f;

		// Cone

		if(flags & AUD_RENDER_CONE)
		{
			AUD_Vector3 SZ = m_orientation.getLookAt();

			float phi = acos(float(SZ * SL / (SZ.length() * SL.length())));
			float t = (phi - m_cone_angle_inner)/(m_cone_angle_outer - m_cone_angle_inner);

			if(t > 0)
			{
				if(t > 1)
					m_volume *= m_cone_volume_outer;
				else
					m_volume *= 1 + t * (m_cone_volume_outer - 1);
			}
		}

		if(m_volume > m_volume_max)
			m_volume = m_volume_max;
		else if(m_volume < m_volume_min)
			m_volume = m_volume_min;

		// Volume

		m_volume *= m_user_volume;
	}

	// 3D Cue

	AUD_Quaternion orientation;

	if(!m_relative)
		orientation = m_device->m_orientation;

	AUD_Vector3 Z = orientation.getLookAt();
	AUD_Vector3 N = orientation.getUp();
	AUD_Vector3 A = N * ((SL * N) / (N * N)) - SL;

	float Asquare = A * A;

	if(Asquare > 0)
	{
		float phi = acos(float(Z * A / (Z.length() * sqrt(Asquare))));
		if(N.cross(Z) * A > 0)
			phi = -phi;

		m_mapper->setMonoAngle(phi);
	}
	else
		m_mapper->setMonoAngle(m_relative ? m_user_pan * M_PI / 2.0 : 0);
}

void AUD_SoftwareDevice::AUD_SoftwareHandle::setSpecs(AUD_Specs specs)
{
	m_mapper->setChannels(specs.channels);
	m_resampler->setRate(specs.rate);
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

	// AUD_XXX Create a reference of our own object so that it doesn't get
	// deleted before the end of this function
	AUD_Reference<AUD_SoftwareHandle> This = this;

	if(m_status == AUD_STATUS_PLAYING)
	{
		m_device->m_playingSounds.remove(This);

		if(m_device->m_playingSounds.empty())
			m_device->playing(m_device->m_playback = false);
	}
	else
		m_device->m_pausedSounds.remove(This);

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
	return m_user_volume;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setVolume(float volume)
{
	if(!m_status)
		return false;
	m_user_volume = volume;

	if(volume == 0)
	{
		m_volume = volume;
		m_flags |= AUD_RENDER_VOLUME;
	}
	else
		m_flags &= ~AUD_RENDER_VOLUME;

	return true;
}

float AUD_SoftwareDevice::AUD_SoftwareHandle::getPitch()
{
	return m_user_pitch;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setPitch(float pitch)
{
	if(!m_status)
		return false;
	m_user_pitch = pitch;
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



/******************************************************************************/
/******************** AUD_SoftwareHandle 3DHandle Code ************************/
/******************************************************************************/

AUD_Vector3 AUD_SoftwareDevice::AUD_SoftwareHandle::getSourceLocation()
{
	if(!m_status)
		return AUD_Vector3();

	return m_location;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setSourceLocation(const AUD_Vector3& location)
{
	if(!m_status)
		return false;

	m_location = location;

	return true;
}

AUD_Vector3 AUD_SoftwareDevice::AUD_SoftwareHandle::getSourceVelocity()
{
	if(!m_status)
		return AUD_Vector3();

	return m_velocity;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setSourceVelocity(const AUD_Vector3& velocity)
{
	if(!m_status)
		return false;

	m_velocity = velocity;

	return true;
}

AUD_Quaternion AUD_SoftwareDevice::AUD_SoftwareHandle::getSourceOrientation()
{
	if(!m_status)
		return AUD_Quaternion();

	return m_orientation;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setSourceOrientation(const AUD_Quaternion& orientation)
{
	if(!m_status)
		return false;

	m_orientation = orientation;

	return true;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::isRelative()
{
	if(!m_status)
		return false;

	return m_relative;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setRelative(bool relative)
{
	if(!m_status)
		return false;

	m_relative = relative;

	return true;
}

float AUD_SoftwareDevice::AUD_SoftwareHandle::getVolumeMaximum()
{
	if(!m_status)
		return std::numeric_limits<float>::quiet_NaN();

	return m_volume_max;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setVolumeMaximum(float volume)
{
	if(!m_status)
		return false;

	m_volume_max = volume;

	return true;
}

float AUD_SoftwareDevice::AUD_SoftwareHandle::getVolumeMinimum()
{
	if(!m_status)
		return std::numeric_limits<float>::quiet_NaN();

	return m_volume_min;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setVolumeMinimum(float volume)
{
	if(!m_status)
		return false;

	m_volume_min = volume;

	return true;
}

float AUD_SoftwareDevice::AUD_SoftwareHandle::getDistanceMaximum()
{
	if(!m_status)
		return std::numeric_limits<float>::quiet_NaN();

	return m_distance_max;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setDistanceMaximum(float distance)
{
	if(!m_status)
		return false;

	m_distance_max = distance;

	return true;
}

float AUD_SoftwareDevice::AUD_SoftwareHandle::getDistanceReference()
{
	if(!m_status)
		return std::numeric_limits<float>::quiet_NaN();

	return m_distance_reference;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setDistanceReference(float distance)
{
	if(!m_status)
		return false;

	m_distance_reference = distance;

	return true;
}

float AUD_SoftwareDevice::AUD_SoftwareHandle::getAttenuation()
{
	if(!m_status)
		return std::numeric_limits<float>::quiet_NaN();

	return m_attenuation;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setAttenuation(float factor)
{
	if(!m_status)
		return false;

	m_attenuation = factor;

	if(factor == 0)
		m_flags |= AUD_RENDER_DISTANCE;
	else
		m_flags &= ~AUD_RENDER_DISTANCE;

	return true;
}

float AUD_SoftwareDevice::AUD_SoftwareHandle::getConeAngleOuter()
{
	if(!m_status)
		return std::numeric_limits<float>::quiet_NaN();

	return m_cone_angle_outer * 360.0f / M_PI;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setConeAngleOuter(float angle)
{
	if(!m_status)
		return false;

	m_cone_angle_outer = angle * M_PI / 360.0f;

	return true;
}

float AUD_SoftwareDevice::AUD_SoftwareHandle::getConeAngleInner()
{
	if(!m_status)
		return std::numeric_limits<float>::quiet_NaN();

	return m_cone_angle_inner * 360.0f / M_PI;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setConeAngleInner(float angle)
{
	if(!m_status)
		return false;

	if(angle >= 360)
		m_flags |= AUD_RENDER_CONE;
	else
		m_flags &= ~AUD_RENDER_CONE;

	m_cone_angle_inner = angle * M_PI / 360.0f;

	return true;
}

float AUD_SoftwareDevice::AUD_SoftwareHandle::getConeVolumeOuter()
{
	if(!m_status)
		return std::numeric_limits<float>::quiet_NaN();

	return m_cone_volume_outer;
}

bool AUD_SoftwareDevice::AUD_SoftwareHandle::setConeVolumeOuter(float volume)
{
	if(!m_status)
		return false;

	m_cone_volume_outer = volume;

	return true;
}

/******************************************************************************/
/**************************** IDevice Code ************************************/
/******************************************************************************/

void AUD_SoftwareDevice::create()
{
	m_playback = false;
	m_volume = 1.0f;
	m_mixer = new AUD_Mixer(m_specs);
	m_speed_of_sound = 343.0f;
	m_doppler_factor = 1.0f;
	m_distance_model = AUD_DISTANCE_MODEL_INVERSE_CLAMPED;
	m_flags = 0;
	m_quality = false;

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
		std::list<AUD_Reference<AUD_SoftwareDevice::AUD_SoftwareHandle> > pauseSounds;
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

			// update 3D Info
			sound->update();

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
					pauseSounds.push_back(sound);
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

		while(!pauseSounds.empty())
		{
			sound = pauseSounds.front();
			pauseSounds.pop_front();
			sound->pause();
		}
	}

	unlock();
}

void AUD_SoftwareDevice::setPanning(AUD_IHandle* handle, float pan)
{
	AUD_SoftwareDevice::AUD_SoftwareHandle* h = dynamic_cast<AUD_SoftwareDevice::AUD_SoftwareHandle*>(handle);
	h->m_user_pan = pan;
}

void AUD_SoftwareDevice::setQuality(bool quality)
{
	m_quality = quality;
}

void AUD_SoftwareDevice::setSpecs(AUD_Specs specs)
{
	m_specs.specs = specs;
	m_mixer->setSpecs(specs);

	for(AUD_HandleIterator it = m_playingSounds.begin(); it != m_playingSounds.end(); it++)
	{
		(*it)->setSpecs(specs);
	}
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

	AUD_Reference<AUD_ResampleReader> resampler;

	// resample
	if(m_quality)
		resampler = new AUD_JOSResampleReader(reader, m_specs.specs);
	else
		resampler = new AUD_LinearResampleReader(reader, m_specs.specs);
	reader = AUD_Reference<AUD_IReader>(resampler);

	// rechannel
	AUD_Reference<AUD_ChannelMapperReader> mapper = new AUD_ChannelMapperReader(reader, m_specs.channels);
	reader = AUD_Reference<AUD_IReader>(mapper);

	if(reader.isNull())
		return AUD_Reference<AUD_IHandle>();

	// play sound
	AUD_Reference<AUD_SoftwareDevice::AUD_SoftwareHandle> sound = new AUD_SoftwareDevice::AUD_SoftwareHandle(this, reader, pitch, resampler, mapper, keep);

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

void AUD_SoftwareDevice::stopAll()
{
	lock();

	while(!m_playingSounds.empty())
		m_playingSounds.front()->stop();

	while(!m_pausedSounds.empty())
		m_pausedSounds.front()->stop();

	unlock();
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

/******************************************************************************/
/**************************** 3D Device Code **********************************/
/******************************************************************************/

AUD_Vector3 AUD_SoftwareDevice::getListenerLocation() const
{
	return m_location;
}

void AUD_SoftwareDevice::setListenerLocation(const AUD_Vector3& location)
{
	m_location = location;
}

AUD_Vector3 AUD_SoftwareDevice::getListenerVelocity() const
{
	return m_velocity;
}

void AUD_SoftwareDevice::setListenerVelocity(const AUD_Vector3& velocity)
{
	m_velocity = velocity;
}

AUD_Quaternion AUD_SoftwareDevice::getListenerOrientation() const
{
	return m_orientation;
}

void AUD_SoftwareDevice::setListenerOrientation(const AUD_Quaternion& orientation)
{
	m_orientation = orientation;
}

float AUD_SoftwareDevice::getSpeedOfSound() const
{
	return m_speed_of_sound;
}

void AUD_SoftwareDevice::setSpeedOfSound(float speed)
{
	m_speed_of_sound = speed;
}

float AUD_SoftwareDevice::getDopplerFactor() const
{
	return m_doppler_factor;
}

void AUD_SoftwareDevice::setDopplerFactor(float factor)
{
	m_doppler_factor = factor;
	if(factor == 0)
		m_flags |= AUD_RENDER_DOPPLER;
	else
		m_flags &= ~AUD_RENDER_DOPPLER;
}

AUD_DistanceModel AUD_SoftwareDevice::getDistanceModel() const
{
	return m_distance_model;
}

void AUD_SoftwareDevice::setDistanceModel(AUD_DistanceModel model)
{
	m_distance_model = model;
	if(model == AUD_DISTANCE_MODEL_INVALID)
		m_flags |= AUD_RENDER_DISTANCE;
	else
		m_flags &= ~AUD_RENDER_DISTANCE;
}
