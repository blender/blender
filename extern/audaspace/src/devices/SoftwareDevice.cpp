/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
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

#include "devices/SoftwareDevice.h"
#include "fx/PitchReader.h"
#include "respec/ChannelMapperReader.h"
#include "respec/JOSResampleReader.h"
#include "respec/LinearResampleReader.h"
#include "respec/Mixer.h"
#include "Exception.h"
#include "ISound.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <mutex>

AUD_NAMESPACE_BEGIN

enum RenderFlags
{
	RENDER_DISTANCE = 0x01,
	RENDER_DOPPLER = 0x02,
	RENDER_CONE = 0x04,
	RENDER_VOLUME = 0x08
};

#define PITCH_MAX 10

/******************************************************************************/
/********************** SoftwareHandle Handle Code ************************/
/******************************************************************************/

bool SoftwareDevice::SoftwareHandle::pause(bool keep)
{
	if(m_status)
	{
		std::lock_guard<ILockable> lock(*m_device);

		if(m_status == STATUS_PLAYING)
		{
			for(auto it = m_device->m_playingSounds.begin(); it != m_device->m_playingSounds.end(); it++)
			{
				if(it->get() == this)
				{
					std::shared_ptr<SoftwareHandle> This = *it;

					m_device->m_playingSounds.erase(it);
					m_device->m_pausedSounds.push_back(This);

					if(m_device->m_playingSounds.empty())
						m_device->playing(m_device->m_playback = false);

					m_status = keep ? STATUS_STOPPED : STATUS_PAUSED;

					return true;
				}
			}
		}
	}

	return false;
}

SoftwareDevice::SoftwareHandle::SoftwareHandle(SoftwareDevice* device, std::shared_ptr<IReader> reader, std::shared_ptr<PitchReader> pitch, std::shared_ptr<ResampleReader> resampler, std::shared_ptr<ChannelMapperReader> mapper, bool keep) :
	m_reader(reader), m_pitch(pitch), m_resampler(resampler), m_mapper(mapper), m_keep(keep), m_user_pitch(1.0f), m_user_volume(1.0f), m_user_pan(0.0f), m_volume(1.0f), m_old_volume(0), m_loopcount(0),
	m_relative(true), m_volume_max(1.0f), m_volume_min(0), m_distance_max(std::numeric_limits<float>::max()),
	m_distance_reference(1.0f), m_attenuation(1.0f), m_cone_angle_outer(M_PI), m_cone_angle_inner(M_PI), m_cone_volume_outer(0),
	m_flags(RENDER_CONE), m_stop(nullptr), m_stop_data(nullptr), m_status(STATUS_PLAYING), m_device(device)
{
}

void SoftwareDevice::SoftwareHandle::update()
{
	int flags = 0;

	m_old_volume = m_volume;

	Vector3 SL;
	if(m_relative)
		SL = -m_location;
	else
		SL = m_device->m_location - m_location;
	float distance = SL * SL;

	if(distance > 0)
		distance = sqrt(distance);
	else
		flags |= RENDER_DOPPLER | RENDER_DISTANCE;

	if(m_pitch->getSpecs().channels != CHANNELS_MONO)
	{
		m_volume = m_user_volume;
		m_pitch->setPitch(m_user_pitch);
		return;
	}

	flags = ~(flags | m_flags | m_device->m_flags);

	// Doppler and Pitch

	if(flags & RENDER_DOPPLER)
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
			m_pitch->setPitch(PITCH_MAX);
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

	if(flags & RENDER_VOLUME)
	{
		// Distance

		if(flags & RENDER_DISTANCE)
		{
			if(m_device->m_distance_model == DISTANCE_MODEL_INVERSE_CLAMPED ||
			   m_device->m_distance_model == DISTANCE_MODEL_LINEAR_CLAMPED ||
			   m_device->m_distance_model == DISTANCE_MODEL_EXPONENT_CLAMPED)
			{
				distance = std::max(std::min(m_distance_max, distance), m_distance_reference);
			}

			switch(m_device->m_distance_model)
			{
			case DISTANCE_MODEL_INVERSE:
			case DISTANCE_MODEL_INVERSE_CLAMPED:
				m_volume = m_distance_reference / (m_distance_reference + m_attenuation * (distance - m_distance_reference));
				break;
			case DISTANCE_MODEL_LINEAR:
			case DISTANCE_MODEL_LINEAR_CLAMPED:
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
			case DISTANCE_MODEL_EXPONENT:
			case DISTANCE_MODEL_EXPONENT_CLAMPED:
				if(m_distance_reference == 0)
					m_volume = 0;
				else
					m_volume = std::pow(distance / m_distance_reference, -m_attenuation);
				break;
			default:
				m_volume = 1.0f;
			}
		}
		else
			m_volume = 1.0f;

		// Cone

		if(flags & RENDER_CONE)
		{
			Vector3 SZ = m_orientation.getLookAt();

			float phi = std::acos(float(SZ * SL / (SZ.length() * SL.length())));
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

	Quaternion orientation;

	if(!m_relative)
		orientation = m_device->m_orientation;

	Vector3 Z = orientation.getLookAt();
	Vector3 N = orientation.getUp();
	Vector3 A = N * ((SL * N) / (N * N)) - SL;

	float Asquare = A * A;

	if(Asquare > 0)
	{
		float phi = std::acos(float(Z * A / (Z.length() * std::sqrt(Asquare))));
		if(N.cross(Z) * A > 0)
			phi = -phi;

		m_mapper->setMonoAngle(phi);
	}
	else
		m_mapper->setMonoAngle(m_relative ? m_user_pan * M_PI / 2.0 : 0);
}

void SoftwareDevice::SoftwareHandle::setSpecs(Specs specs)
{
	m_mapper->setChannels(specs.channels);
	m_resampler->setRate(specs.rate);
}

bool SoftwareDevice::SoftwareHandle::pause()
{
	return pause(false);
}

bool SoftwareDevice::SoftwareHandle::resume()
{
	if(m_status)
	{
		std::lock_guard<ILockable> lock(*m_device);

		if(m_status == STATUS_PAUSED)
		{
			for(auto it = m_device->m_pausedSounds.begin(); it != m_device->m_pausedSounds.end(); it++)
			{
				if(it->get() == this)
				{
					std::shared_ptr<SoftwareHandle> This = *it;

					m_device->m_pausedSounds.erase(it);

					m_device->m_playingSounds.push_back(This);

					if(!m_device->m_playback)
						m_device->playing(m_device->m_playback = true);
					m_status = STATUS_PLAYING;

					return true;
				}
			}
		}

	}

	return false;
}

bool SoftwareDevice::SoftwareHandle::stop()
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	m_status = STATUS_INVALID;

	for(auto it = m_device->m_playingSounds.begin(); it != m_device->m_playingSounds.end(); it++)
	{
		if(it->get() == this)
		{
			std::shared_ptr<SoftwareHandle> This = *it;

			m_device->m_playingSounds.erase(it);

			if(m_device->m_playingSounds.empty())
				m_device->playing(m_device->m_playback = false);

			return true;
		}
	}

	for(auto it = m_device->m_pausedSounds.begin(); it != m_device->m_pausedSounds.end(); it++)
	{
		if(it->get() == this)
		{
			std::shared_ptr<SoftwareHandle> This = *it;

			m_device->m_pausedSounds.erase(it);

			return true;
		}
	}

	return false;
}

bool SoftwareDevice::SoftwareHandle::getKeep()
{
	if(m_status)
		return m_keep;

	return false;
}

bool SoftwareDevice::SoftwareHandle::setKeep(bool keep)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	m_keep = keep;

	return true;
}

bool SoftwareDevice::SoftwareHandle::seek(float position)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	m_pitch->setPitch(m_user_pitch);
	m_reader->seek((int)(position * m_reader->getSpecs().rate));

	if(m_status == STATUS_STOPPED)
		m_status = STATUS_PAUSED;

	return true;
}

float SoftwareDevice::SoftwareHandle::getPosition()
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return 0.0f;

	float position = m_reader->getPosition() / (float)m_device->m_specs.rate;

	return position;
}

Status SoftwareDevice::SoftwareHandle::getStatus()
{
	return m_status;
}

float SoftwareDevice::SoftwareHandle::getVolume()
{
	return m_user_volume;
}

bool SoftwareDevice::SoftwareHandle::setVolume(float volume)
{
	if(!m_status)
		return false;
	m_user_volume = volume;

	if(volume == 0)
	{
		m_old_volume = m_volume = volume;
		m_flags |= RENDER_VOLUME;
	}
	else
		m_flags &= ~RENDER_VOLUME;

	return true;
}

float SoftwareDevice::SoftwareHandle::getPitch()
{
	return m_user_pitch;
}

bool SoftwareDevice::SoftwareHandle::setPitch(float pitch)
{
	if(!m_status)
		return false;
	if(pitch > 0.0f)
		m_user_pitch = pitch;
	return true;
}

int SoftwareDevice::SoftwareHandle::getLoopCount()
{
	if(!m_status)
		return 0;
	return m_loopcount;
}

bool SoftwareDevice::SoftwareHandle::setLoopCount(int count)
{
	if(!m_status)
		return false;

	if(m_status == STATUS_STOPPED && (count > m_loopcount || count < 0))
		m_status = STATUS_PAUSED;

	m_loopcount = count;

	return true;
}

bool SoftwareDevice::SoftwareHandle::setStopCallback(stopCallback callback, void* data)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	m_stop = callback;
	m_stop_data = data;

	return true;
}



/******************************************************************************/
/******************** SoftwareHandle 3DHandle Code ************************/
/******************************************************************************/

Vector3 SoftwareDevice::SoftwareHandle::getLocation()
{
	if(!m_status)
		return Vector3();

	return m_location;
}

bool SoftwareDevice::SoftwareHandle::setLocation(const Vector3& location)
{
	if(!m_status)
		return false;

	m_location = location;

	return true;
}

Vector3 SoftwareDevice::SoftwareHandle::getVelocity()
{
	if(!m_status)
		return Vector3();

	return m_velocity;
}

bool SoftwareDevice::SoftwareHandle::setVelocity(const Vector3& velocity)
{
	if(!m_status)
		return false;

	m_velocity = velocity;

	return true;
}

Quaternion SoftwareDevice::SoftwareHandle::getOrientation()
{
	if(!m_status)
		return Quaternion();

	return m_orientation;
}

bool SoftwareDevice::SoftwareHandle::setOrientation(const Quaternion& orientation)
{
	if(!m_status)
		return false;

	m_orientation = orientation;

	return true;
}

bool SoftwareDevice::SoftwareHandle::isRelative()
{
	if(!m_status)
		return false;

	return m_relative;
}

bool SoftwareDevice::SoftwareHandle::setRelative(bool relative)
{
	if(!m_status)
		return false;

	m_relative = relative;

	return true;
}

float SoftwareDevice::SoftwareHandle::getVolumeMaximum()
{
	if(!m_status)
		return std::numeric_limits<float>::quiet_NaN();

	return m_volume_max;
}

bool SoftwareDevice::SoftwareHandle::setVolumeMaximum(float volume)
{
	if(!m_status)
		return false;

	m_volume_max = volume;

	return true;
}

float SoftwareDevice::SoftwareHandle::getVolumeMinimum()
{
	if(!m_status)
		return std::numeric_limits<float>::quiet_NaN();

	return m_volume_min;
}

bool SoftwareDevice::SoftwareHandle::setVolumeMinimum(float volume)
{
	if(!m_status)
		return false;

	m_volume_min = volume;

	return true;
}

float SoftwareDevice::SoftwareHandle::getDistanceMaximum()
{
	if(!m_status)
		return std::numeric_limits<float>::quiet_NaN();

	return m_distance_max;
}

bool SoftwareDevice::SoftwareHandle::setDistanceMaximum(float distance)
{
	if(!m_status)
		return false;

	m_distance_max = distance;

	return true;
}

float SoftwareDevice::SoftwareHandle::getDistanceReference()
{
	if(!m_status)
		return std::numeric_limits<float>::quiet_NaN();

	return m_distance_reference;
}

bool SoftwareDevice::SoftwareHandle::setDistanceReference(float distance)
{
	if(!m_status)
		return false;

	m_distance_reference = distance;

	return true;
}

float SoftwareDevice::SoftwareHandle::getAttenuation()
{
	if(!m_status)
		return std::numeric_limits<float>::quiet_NaN();

	return m_attenuation;
}

bool SoftwareDevice::SoftwareHandle::setAttenuation(float factor)
{
	if(!m_status)
		return false;

	m_attenuation = factor;

	if(factor == 0)
		m_flags |= RENDER_DISTANCE;
	else
		m_flags &= ~RENDER_DISTANCE;

	return true;
}

float SoftwareDevice::SoftwareHandle::getConeAngleOuter()
{
	if(!m_status)
		return std::numeric_limits<float>::quiet_NaN();

	return m_cone_angle_outer * 360.0f / M_PI;
}

bool SoftwareDevice::SoftwareHandle::setConeAngleOuter(float angle)
{
	if(!m_status)
		return false;

	m_cone_angle_outer = angle * M_PI / 360.0f;

	return true;
}

float SoftwareDevice::SoftwareHandle::getConeAngleInner()
{
	if(!m_status)
		return std::numeric_limits<float>::quiet_NaN();

	return m_cone_angle_inner * 360.0f / M_PI;
}

bool SoftwareDevice::SoftwareHandle::setConeAngleInner(float angle)
{
	if(!m_status)
		return false;

	if(angle >= 360)
		m_flags |= RENDER_CONE;
	else
		m_flags &= ~RENDER_CONE;

	m_cone_angle_inner = angle * M_PI / 360.0f;

	return true;
}

float SoftwareDevice::SoftwareHandle::getConeVolumeOuter()
{
	if(!m_status)
		return std::numeric_limits<float>::quiet_NaN();

	return m_cone_volume_outer;
}

bool SoftwareDevice::SoftwareHandle::setConeVolumeOuter(float volume)
{
	if(!m_status)
		return false;

	m_cone_volume_outer = volume;

	return true;
}

/******************************************************************************/
/**************************** IDevice Code ************************************/
/******************************************************************************/

void SoftwareDevice::create()
{
	m_playback = false;
	m_volume = 1.0f;
	m_mixer = std::shared_ptr<Mixer>(new Mixer(m_specs));
	m_speed_of_sound = 343.3f;
	m_doppler_factor = 1.0f;
	m_distance_model = DISTANCE_MODEL_INVERSE_CLAMPED;
	m_flags = 0;
	m_quality = false;
}

void SoftwareDevice::destroy()
{
	if(m_playback)
		playing(m_playback = false);

	while(!m_playingSounds.empty())
		m_playingSounds.front()->stop();

	while(!m_pausedSounds.empty())
		m_pausedSounds.front()->stop();
}

void SoftwareDevice::mix(data_t* buffer, int length)
{
	m_buffer.assureSize(length * AUD_SAMPLE_SIZE(m_specs));

	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	{
		std::shared_ptr<SoftwareDevice::SoftwareHandle> sound;
		int len;
		int pos;
		bool eos;
		std::list<std::shared_ptr<SoftwareDevice::SoftwareHandle> > stopSounds;
		std::list<std::shared_ptr<SoftwareDevice::SoftwareHandle> > pauseSounds;
		sample_t* buf = m_buffer.getBuffer();

		m_mixer->clear(length);

		// for all sounds
		for(auto& sound : m_playingSounds)
		{
			// get the buffer from the source
			pos = 0;
			len = length;

			// update 3D Info
			sound->update();

			try
			{
				sound->m_reader->read(len, eos, buf);

				// in case of looping
				while(pos + len < length && sound->m_loopcount && eos)
				{
					m_mixer->mix(buf, pos, len, sound->m_volume, sound->m_old_volume);

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
			}
			catch(Exception& e)
			{
				len = 0;
				std::cerr << "Caught exception while reading sound data during playback with software mixing: " << e.getMessage() << std::endl;
			}

			m_mixer->mix(buf, pos, len, sound->m_volume, sound->m_old_volume);

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
		for(auto& sound : pauseSounds)
			sound->pause(true);

		for(auto& sound :  stopSounds)
			sound->stop();

		pauseSounds.clear();
		stopSounds.clear();
	}
}

void SoftwareDevice::setPanning(IHandle* handle, float pan)
{
	SoftwareDevice::SoftwareHandle* h = dynamic_cast<SoftwareDevice::SoftwareHandle*>(handle);
	h->m_user_pan = pan;
}

void SoftwareDevice::setQuality(bool quality)
{
	m_quality = quality;
}

void SoftwareDevice::setSpecs(Specs specs)
{
	m_specs.specs = specs;
	m_mixer->setSpecs(specs);

	for(auto& sound : m_playingSounds)
	{
		sound->setSpecs(specs);
	}
}

SoftwareDevice::SoftwareDevice()
{
}

DeviceSpecs SoftwareDevice::getSpecs() const
{
	return m_specs;
}

std::shared_ptr<IHandle> SoftwareDevice::play(std::shared_ptr<IReader> reader, bool keep)
{
	// prepare the reader
	// pitch

	std::shared_ptr<PitchReader> pitch = std::shared_ptr<PitchReader>(new PitchReader(reader, 1));
	reader = std::shared_ptr<IReader>(pitch);

	std::shared_ptr<ResampleReader> resampler;

	// resample
	if(m_quality)
		resampler = std::shared_ptr<ResampleReader>(new JOSResampleReader(reader, m_specs.rate));
	else
		resampler = std::shared_ptr<ResampleReader>(new LinearResampleReader(reader, m_specs.rate));
	reader = std::shared_ptr<IReader>(resampler);

	// rechannel
	std::shared_ptr<ChannelMapperReader> mapper = std::shared_ptr<ChannelMapperReader>(new ChannelMapperReader(reader, m_specs.channels));
	reader = std::shared_ptr<IReader>(mapper);

	if(!reader.get())
		return std::shared_ptr<IHandle>();

	// play sound
	std::shared_ptr<SoftwareDevice::SoftwareHandle> sound = std::shared_ptr<SoftwareDevice::SoftwareHandle>(new SoftwareDevice::SoftwareHandle(this, reader, pitch, resampler, mapper, keep));

	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_playingSounds.push_back(sound);

	if(!m_playback)
		playing(m_playback = true);

	return std::shared_ptr<IHandle>(sound);
}

std::shared_ptr<IHandle> SoftwareDevice::play(std::shared_ptr<ISound> sound, bool keep)
{
	return play(sound->createReader(), keep);
}

void SoftwareDevice::stopAll()
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	while(!m_playingSounds.empty())
		m_playingSounds.front()->stop();

	while(!m_pausedSounds.empty())
		m_pausedSounds.front()->stop();
}

void SoftwareDevice::lock()
{
	m_mutex.lock();
}

void SoftwareDevice::unlock()
{
	m_mutex.unlock();
}

float SoftwareDevice::getVolume() const
{
	return m_volume;
}

void SoftwareDevice::setVolume(float volume)
{
	m_volume = volume;
}

ISynchronizer* SoftwareDevice::getSynchronizer()
{
	return &m_synchronizer;
}

/******************************************************************************/
/**************************** 3D Device Code **********************************/
/******************************************************************************/

Vector3 SoftwareDevice::getListenerLocation() const
{
	return m_location;
}

void SoftwareDevice::setListenerLocation(const Vector3& location)
{
	m_location = location;
}

Vector3 SoftwareDevice::getListenerVelocity() const
{
	return m_velocity;
}

void SoftwareDevice::setListenerVelocity(const Vector3& velocity)
{
	m_velocity = velocity;
}

Quaternion SoftwareDevice::getListenerOrientation() const
{
	return m_orientation;
}

void SoftwareDevice::setListenerOrientation(const Quaternion& orientation)
{
	m_orientation = orientation;
}

float SoftwareDevice::getSpeedOfSound() const
{
	return m_speed_of_sound;
}

void SoftwareDevice::setSpeedOfSound(float speed)
{
	m_speed_of_sound = speed;
}

float SoftwareDevice::getDopplerFactor() const
{
	return m_doppler_factor;
}

void SoftwareDevice::setDopplerFactor(float factor)
{
	m_doppler_factor = factor;
	if(factor == 0)
		m_flags |= RENDER_DOPPLER;
	else
		m_flags &= ~RENDER_DOPPLER;
}

DistanceModel SoftwareDevice::getDistanceModel() const
{
	return m_distance_model;
}

void SoftwareDevice::setDistanceModel(DistanceModel model)
{
	m_distance_model = model;
	if(model == DISTANCE_MODEL_INVALID)
		m_flags |= RENDER_DISTANCE;
	else
		m_flags &= ~RENDER_DISTANCE;
}

AUD_NAMESPACE_END
