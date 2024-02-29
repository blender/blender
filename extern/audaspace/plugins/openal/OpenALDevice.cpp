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

#include "OpenALDevice.h"
#include "devices/DeviceManager.h"
#include "devices/IDeviceFactory.h"
#include "respec/ConverterReader.h"
#include "Exception.h"
#include "ISound.h"

#include <chrono>
#include <cstring>
#include <iostream>

AUD_NAMESPACE_BEGIN

/******************************************************************************/
/*********************** OpenALHandle Handle Code *************************/
/******************************************************************************/

bool OpenALDevice::OpenALHandle::pause(bool keep)
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
					std::shared_ptr<OpenALHandle> This = *it;

					m_device->m_playingSounds.erase(it);
					m_device->m_pausedSounds.push_back(This);

					alSourcePause(m_source);

					m_status = keep ? STATUS_STOPPED : STATUS_PAUSED;

					return true;
				}
			}
		}
	}

	return false;
}

bool OpenALDevice::OpenALHandle::reinitialize()
{
	DeviceSpecs specs = m_device->m_specs;
	specs.specs = m_reader->getSpecs();

	ALenum format;

	if(!m_device->getFormat(format, specs.specs))
		return true;

	m_format = format;

	// OpenAL playback code
	alGenBuffers(CYCLE_BUFFERS, m_buffers);
	if(alGetError() != AL_NO_ERROR)
		return true;

	m_device->m_buffer.assureSize(m_device->m_buffersize * AUD_DEVICE_SAMPLE_SIZE(specs));
	int length;
	bool eos;

	for(m_current = 0; m_current < CYCLE_BUFFERS; m_current++)
	{
		length = m_device->m_buffersize;
		m_reader->read(length, eos, m_device->m_buffer.getBuffer());

		if(length == 0)
			break;

		alBufferData(m_buffers[m_current], m_format, m_device->m_buffer.getBuffer(), length * AUD_DEVICE_SAMPLE_SIZE(specs), specs.rate);

		if(alGetError() != AL_NO_ERROR)
			return true;
	}

	alGenSources(1, &m_source);
	if(alGetError() != AL_NO_ERROR)
		return true;

	alSourceQueueBuffers(m_source, m_current, m_buffers);
	if(alGetError() != AL_NO_ERROR)
		return true;

	alSourcei(m_source, AL_SOURCE_RELATIVE, m_relative);

	return false;
}

OpenALDevice::OpenALHandle::OpenALHandle(OpenALDevice* device, ALenum format, std::shared_ptr<IReader> reader, bool keep) :
	m_isBuffered(false), m_reader(reader), m_keep(keep), m_format(format),
	m_eos(false), m_loopcount(0), m_stop(nullptr), m_stop_data(nullptr), m_status(STATUS_PLAYING),
	m_relative(1), m_device(device)
{
	DeviceSpecs specs = m_device->m_specs;
	specs.specs = m_reader->getSpecs();

	// OpenAL playback code
	alGenBuffers(CYCLE_BUFFERS, m_buffers);
	if(alGetError() != AL_NO_ERROR)
		AUD_THROW(DeviceException, "Buffer generation failed while staring playback with OpenAL.");

	try
	{
		m_device->m_buffer.assureSize(m_device->m_buffersize * AUD_DEVICE_SAMPLE_SIZE(specs));
		int length;
		bool eos;

		for(m_current = 0; m_current < CYCLE_BUFFERS; m_current++)
		{
			length = m_device->m_buffersize;
			reader->read(length, eos, m_device->m_buffer.getBuffer());

			if(length == 0)
				break;

			alBufferData(m_buffers[m_current], m_format, m_device->m_buffer.getBuffer(), length * AUD_DEVICE_SAMPLE_SIZE(specs), specs.rate);

			if(alGetError() != AL_NO_ERROR)
				AUD_THROW(DeviceException, "Filling the buffer with data failed while starting playback with OpenAL.");
		}

		alGenSources(1, &m_source);
		if(alGetError() != AL_NO_ERROR)
			AUD_THROW(DeviceException, "Source generation failed while starting playback with OpenAL.");

		try
		{
			alSourceQueueBuffers(m_source, m_current, m_buffers);
			if(alGetError() != AL_NO_ERROR)
				AUD_THROW(DeviceException, "Buffer queuing failed while starting playback with OpenAL.");
		}
		catch(Exception&)
		{
			alDeleteSources(1, &m_source);
			throw;
		}
	}
	catch(Exception&)
	{
		alDeleteBuffers(CYCLE_BUFFERS, m_buffers);
		throw;
	}
	alSourcei(m_source, AL_SOURCE_RELATIVE, 1);
}

bool OpenALDevice::OpenALHandle::pause()
{
	return pause(false);
}

bool OpenALDevice::OpenALHandle::resume()
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
					std::shared_ptr<OpenALHandle> This = *it;

					m_device->m_pausedSounds.erase(it);
					m_device->m_playingSounds.push_back(This);

					m_device->start();
					m_status = STATUS_PLAYING;

					return true;
				}
			}
		}
	}

	return false;
}

bool OpenALDevice::OpenALHandle::stop()
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	if(m_stop)
		m_stop(m_stop_data);

	m_status = STATUS_INVALID;

	alDeleteSources(1, &m_source);
	if(!m_isBuffered)
		alDeleteBuffers(CYCLE_BUFFERS, m_buffers);

	for(auto it = m_device->m_playingSounds.begin(); it != m_device->m_playingSounds.end(); it++)
	{
		if(it->get() == this)
		{
			std::shared_ptr<OpenALHandle> This = *it;

			m_device->m_playingSounds.erase(it);

			return true;
		}
	}

	for(auto it = m_device->m_pausedSounds.begin(); it != m_device->m_pausedSounds.end(); it++)
	{
		if(it->get() == this)
		{
			std::shared_ptr<OpenALHandle> This = *it;

			m_device->m_pausedSounds.erase(it);

			return true;
		}
	}

	return false;
}

bool OpenALDevice::OpenALHandle::getKeep()
{
	if(m_status)
		return m_keep;

	return false;
}

bool OpenALDevice::OpenALHandle::setKeep(bool keep)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	m_keep = keep;

	return true;
}

bool OpenALDevice::OpenALHandle::seek(double position)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	if(m_isBuffered)
		alSourcef(m_source, AL_SEC_OFFSET, position);
	else
	{
		m_reader->seek((int)(position * m_reader->getSpecs().rate));
		m_eos = false;

		ALint info;

		alGetSourcei(m_source, AL_SOURCE_STATE, &info);

		// we need to stop playing sounds as well to clear the buffers
		// this might cause clicks, but fixes a bug regarding position determination
		if(info == AL_PAUSED || info == AL_PLAYING)
			alSourceStop(m_source);

		alSourcei(m_source, AL_BUFFER, 0);

		ALenum err;
		if((err = alGetError()) == AL_NO_ERROR)
		{
			int length;
			DeviceSpecs specs = m_device->m_specs;
			specs.specs = m_reader->getSpecs();
			m_device->m_buffer.assureSize(m_device->m_buffersize * AUD_DEVICE_SAMPLE_SIZE(specs));

			for(m_current = 0; m_current < CYCLE_BUFFERS; m_current++)
			{
				length = m_device->m_buffersize;

				m_reader->read(length, m_eos, m_device->m_buffer.getBuffer());

				if(length == 0)
					break;

				alBufferData(m_buffers[m_current], m_format, m_device->m_buffer.getBuffer(), length * AUD_DEVICE_SAMPLE_SIZE(specs), specs.rate);

				if(alGetError() != AL_NO_ERROR)
					break;
			}

			if(m_loopcount != 0)
				m_eos = false;

			alSourceQueueBuffers(m_source, m_current, m_buffers);
		}

		alSourceRewind(m_source);
	}

	if(m_status == STATUS_STOPPED)
		m_status = STATUS_PAUSED;

	return true;
}

double OpenALDevice::OpenALHandle::getPosition()
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return 0.0f;

	float position = 0.0f;

	alGetSourcef(m_source, AL_SEC_OFFSET, &position);

	if(!m_isBuffered)
	{
		int queued;

		// this usually always returns CYCLE_BUFFERS
		alGetSourcei(m_source, AL_BUFFERS_QUEUED, &queued);

		Specs specs = m_reader->getSpecs();
		position += (m_reader->getPosition() - m_device->m_buffersize * queued) / (float)specs.rate;
	}

	return position;
}

Status OpenALDevice::OpenALHandle::getStatus()
{
	return m_status;
}

float OpenALDevice::OpenALHandle::getVolume()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_GAIN, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setVolume(float volume)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	if(volume >= 0.0f)
		alSourcef(m_source, AL_GAIN, volume);

	return true;
}

float OpenALDevice::OpenALHandle::getPitch()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_PITCH, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setPitch(float pitch)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	if(pitch > 0.0f)
		alSourcef(m_source, AL_PITCH, pitch);

	return true;
}

int OpenALDevice::OpenALHandle::getLoopCount()
{
	if(!m_status)
		return 0;
	return m_loopcount;
}

bool OpenALDevice::OpenALHandle::setLoopCount(int count)
{
	if(!m_status)
		return false;

	if(m_status == STATUS_STOPPED && (count > m_loopcount || count < 0))
		m_status = STATUS_PAUSED;

	m_loopcount = count;

	return true;
}

bool OpenALDevice::OpenALHandle::setStopCallback(stopCallback callback, void* data)
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
/********************* OpenALHandle 3DHandle Code *************************/
/******************************************************************************/

Vector3 OpenALDevice::OpenALHandle::getLocation()
{
	Vector3 result = Vector3(0, 0, 0);

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	ALfloat p[3];
	alGetSourcefv(m_source, AL_POSITION, p);

	result = Vector3(p[0], p[1], p[2]);

	return result;
}

bool OpenALDevice::OpenALHandle::setLocation(const Vector3& location)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcefv(m_source, AL_POSITION, (ALfloat*)location.get());

	return true;
}

Vector3 OpenALDevice::OpenALHandle::getVelocity()
{
	Vector3 result = Vector3(0, 0, 0);

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	ALfloat v[3];
	alGetSourcefv(m_source, AL_VELOCITY, v);

	result = Vector3(v[0], v[1], v[2]);

	return result;
}

bool OpenALDevice::OpenALHandle::setVelocity(const Vector3& velocity)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcefv(m_source, AL_VELOCITY, (ALfloat*)velocity.get());

	return true;
}

Quaternion OpenALDevice::OpenALHandle::getOrientation()
{
	return m_orientation;
}

bool OpenALDevice::OpenALHandle::setOrientation(const Quaternion& orientation)
{
	ALfloat direction[3];
	direction[0] = -2 * (orientation.w() * orientation.y() +
						 orientation.x() * orientation.z());
	direction[1] = 2 * (orientation.x() * orientation.w() -
						orientation.z() * orientation.y());
	direction[2] = 2 * (orientation.x() * orientation.x() +
						orientation.y() * orientation.y()) - 1;

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcefv(m_source, AL_DIRECTION, direction);

	m_orientation = orientation;

	return true;
}

bool OpenALDevice::OpenALHandle::isRelative()
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alGetSourcei(m_source, AL_SOURCE_RELATIVE, &m_relative);

	return m_relative;
}

bool OpenALDevice::OpenALHandle::setRelative(bool relative)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	m_relative = relative;

	alSourcei(m_source, AL_SOURCE_RELATIVE, m_relative);

	return true;
}

float OpenALDevice::OpenALHandle::getVolumeMaximum()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_MAX_GAIN, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setVolumeMaximum(float volume)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	if(volume >= 0.0f && volume <= 1.0f)
		alSourcef(m_source, AL_MAX_GAIN, volume);

	return true;
}

float OpenALDevice::OpenALHandle::getVolumeMinimum()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_MIN_GAIN, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setVolumeMinimum(float volume)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	if(volume >= 0.0f && volume <= 1.0f)
		alSourcef(m_source, AL_MIN_GAIN, volume);

	return true;
}

float OpenALDevice::OpenALHandle::getDistanceMaximum()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_MAX_DISTANCE, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setDistanceMaximum(float distance)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	if(distance >= 0.0f)
		alSourcef(m_source, AL_MAX_DISTANCE, distance);

	return true;
}

float OpenALDevice::OpenALHandle::getDistanceReference()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_REFERENCE_DISTANCE, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setDistanceReference(float distance)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	if(distance >= 0.0f)
		alSourcef(m_source, AL_REFERENCE_DISTANCE, distance);

	return true;
}

float OpenALDevice::OpenALHandle::getAttenuation()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_ROLLOFF_FACTOR, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setAttenuation(float factor)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	if(factor >= 0.0f)
		alSourcef(m_source, AL_ROLLOFF_FACTOR, factor);

	return true;
}

float OpenALDevice::OpenALHandle::getConeAngleOuter()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_CONE_OUTER_ANGLE, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setConeAngleOuter(float angle)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcef(m_source, AL_CONE_OUTER_ANGLE, angle);

	return true;
}

float OpenALDevice::OpenALHandle::getConeAngleInner()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_CONE_INNER_ANGLE, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setConeAngleInner(float angle)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcef(m_source, AL_CONE_INNER_ANGLE, angle);

	return true;
}

float OpenALDevice::OpenALHandle::getConeVolumeOuter()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_CONE_OUTER_GAIN, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setConeVolumeOuter(float volume)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	if(volume >= 0.0f && volume <= 1.0f)
	alSourcef(m_source, AL_CONE_OUTER_GAIN, volume);

	return true;
}

/******************************************************************************/
/**************************** Threading Code **********************************/
/******************************************************************************/

void OpenALDevice::start()
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if(!m_playing)
	{
		if(m_thread.joinable())
			m_thread.join();

		m_thread = std::thread(&OpenALDevice::updateStreams, this);

		m_playing = true;
	}
}

void OpenALDevice::updateStreams()
{
	int length;

	ALint info;
	DeviceSpecs specs = m_specs;
	ALCenum cerr;
	std::list<std::shared_ptr<OpenALHandle> > stopSounds;
	std::list<std::shared_ptr<OpenALHandle> > pauseSounds;

	auto sleepDuration = std::chrono::milliseconds(20);

	for(;;)
	{
		lock();

		if(m_checkDisconnect)
		{
			ALCint connected;
			alcGetIntegerv(m_device, alcGetEnumValue(m_device, "ALC_CONNECTED"), 1, &connected);

			if(!connected)
			{
				// quit OpenAL
				alcMakeContextCurrent(nullptr);
				alcDestroyContext(m_context);
				alcCloseDevice(m_device);

				// restart
				if(m_name.empty())
					m_device = alcOpenDevice(nullptr);
				else
					m_device = alcOpenDevice(m_name.c_str());

				// if device opening failed, there's really nothing we can do
				if(m_device)
				{
					// at least try to set the frequency

					ALCint attribs[] = { ALC_FREQUENCY, (ALCint)specs.rate, 0 };
					ALCint* attributes = attribs;
					if(specs.rate == RATE_INVALID)
						attributes = nullptr;

					m_context = alcCreateContext(m_device, attributes);
					alcMakeContextCurrent(m_context);

					m_checkDisconnect = alcIsExtensionPresent(m_device, "ALC_EXT_disconnect");

					alcGetIntegerv(m_device, ALC_FREQUENCY, 1, (ALCint*)&specs.rate);

					// check for specific formats and channel counts to be played back
					if(alIsExtensionPresent("AL_EXT_FLOAT32") == AL_TRUE)
						specs.format = FORMAT_FLOAT32;
					else
						specs.format = FORMAT_S16;

					// if the format of the device changed, all handles are invalidated
					// this is unlikely to happen though
					if(specs.format != m_specs.format)
						stopAll();

					m_useMC = alIsExtensionPresent("AL_EXT_MCFORMATS") == AL_TRUE;

					if((!m_useMC && specs.channels > CHANNELS_STEREO) ||
							specs.channels == CHANNELS_STEREO_LFE ||
							specs.channels == CHANNELS_SURROUND5)
						specs.channels = CHANNELS_STEREO;

					alGetError();
					alcGetError(m_device);

					m_specs = specs;

					std::list<std::shared_ptr<OpenALHandle> > stopSounds;

					for(auto& handle : m_playingSounds)
						if(handle->reinitialize())
							stopSounds.push_back(handle);

					for(auto& handle : m_pausedSounds)
						if(handle->reinitialize())
							stopSounds.push_back(handle);

					for(auto& sound : stopSounds)
						sound->stop();
				}
			}
		}

		alcSuspendContext(m_context);
		cerr = alcGetError(m_device);
		if(cerr == ALC_NO_ERROR)
		{
			// for all sounds
			for(auto& sound : m_playingSounds)
			{
				// is it a streamed sound?
				if(!sound->m_isBuffered)
				{
					// check for buffer refilling
					alGetSourcei(sound->m_source, AL_BUFFERS_PROCESSED, &info);

					info += (OpenALHandle::CYCLE_BUFFERS - sound->m_current);

					if(info)
					{
						specs.specs = sound->m_reader->getSpecs();
						m_buffer.assureSize(m_buffersize * AUD_DEVICE_SAMPLE_SIZE(specs));

						// for all empty buffers
						while(info--)
						{
							// if there's still data to play back
							if(!sound->m_eos)
							{
								// read data
								length = m_buffersize;

								try
								{
									sound->m_reader->read(length, sound->m_eos, m_buffer.getBuffer());

									// looping necessary?
									if(length == 0 && sound->m_loopcount)
									{
										if(sound->m_loopcount > 0)
											sound->m_loopcount--;

										sound->m_reader->seek(0);

										length = m_buffersize;
										sound->m_reader->read(length, sound->m_eos, m_buffer.getBuffer());
									}
								}
								catch(Exception& e)
								{
									length = 0;
									std::cerr << "Caught exception while reading sound data during playback with OpenAL: " << e.getMessage() << std::endl;
								}

								if(sound->m_loopcount != 0)
									sound->m_eos = false;

								// read nothing?
								if(length == 0)
								{
									break;
								}

								ALuint buffer;

								if(sound->m_current < OpenALHandle::CYCLE_BUFFERS)
									buffer = sound->m_buffers[sound->m_current++];
								else
									alSourceUnqueueBuffers(sound->m_source, 1, &buffer);

								ALenum err;
								if((err = alGetError()) != AL_NO_ERROR)
								{
									sound->m_eos = true;
									break;
								}

								// fill with new data
								alBufferData(buffer, sound->m_format, m_buffer.getBuffer(), length * AUD_DEVICE_SAMPLE_SIZE(specs), specs.rate);

								if((err = alGetError()) != AL_NO_ERROR)
								{
									sound->m_eos = true;
									break;
								}

								// and queue again
								alSourceQueueBuffers(sound->m_source, 1,&buffer);
								if(alGetError() != AL_NO_ERROR)
								{
									sound->m_eos = true;
									break;
								}
							}
							else
								break;
						}
					}
				}

				// check if the sound has been stopped
				alGetSourcei(sound->m_source, AL_SOURCE_STATE, &info);

				if(info != AL_PLAYING)
				{
					// if it really stopped
					if(sound->m_eos && info != AL_INITIAL)
					{
						// pause or
						if(sound->m_keep)
						{
							if(sound->m_stop)
								sound->m_stop(sound->m_stop_data);

							pauseSounds.push_back(sound);
						}
						// stop
						else
							stopSounds.push_back(sound);
					}
					// continue playing
					else
						alSourcePlay(sound->m_source);
				}
			}

			for(auto& sound : pauseSounds)
				sound->pause(true);

			for(auto& sound : stopSounds)
				sound->stop();

			pauseSounds.clear();
			stopSounds.clear();

			alcProcessContext(m_context);
		}

		// stop thread
		if(m_playingSounds.empty() || (cerr != ALC_NO_ERROR))
		{
			m_playing = false;
			unlock();

			return;
		}

		unlock();

		std::this_thread::sleep_for(sleepDuration);
	}
}

/******************************************************************************/
/**************************** IDevice Code ************************************/
/******************************************************************************/

OpenALDevice::OpenALDevice(DeviceSpecs specs, int buffersize, const std::string &name) :
	m_name(name), m_playing(false), m_buffersize(buffersize)
{
	// cannot determine how many channels or which format OpenAL uses, but
	// it at least is able to play 16 bit stereo audio
	specs.format = FORMAT_S16;

	if(m_name.empty())
		m_device = alcOpenDevice(nullptr);
	else
		m_device = alcOpenDevice(m_name.c_str());

	if(!m_device)
		AUD_THROW(DeviceException, "The audio device couldn't be opened with OpenAL.");

	// at least try to set the frequency
	ALCint attribs[] = { ALC_FREQUENCY, (ALCint)specs.rate, 0 };
	ALCint* attributes = attribs;
	if(specs.rate == RATE_INVALID)
		attributes = nullptr;

	m_context = alcCreateContext(m_device, attributes);
	alcMakeContextCurrent(m_context);

	m_checkDisconnect = alcIsExtensionPresent(m_device, "ALC_EXT_disconnect");

	alcGetIntegerv(m_device, ALC_FREQUENCY, 1, (ALCint*)&specs.rate);

	// check for specific formats and channel counts to be played back
	if(alIsExtensionPresent("AL_EXT_FLOAT32") == AL_TRUE)
		specs.format = FORMAT_FLOAT32;

	m_useMC = alIsExtensionPresent("AL_EXT_MCFORMATS") == AL_TRUE;

	if((!m_useMC && specs.channels > CHANNELS_STEREO) ||
			specs.channels == CHANNELS_STEREO_LFE ||
			specs.channels == CHANNELS_SURROUND5 ||
			specs.channels > CHANNELS_SURROUND71)
		specs.channels = CHANNELS_STEREO;

	alGetError();
	alcGetError(m_device);

	m_specs = specs;
}

OpenALDevice::~OpenALDevice()
{
	lock();
	alcSuspendContext(m_context);

	while(!m_playingSounds.empty())
		m_playingSounds.front()->stop();

	while(!m_pausedSounds.empty())
		m_pausedSounds.front()->stop();

	alcProcessContext(m_context);

	// wait for the thread to stop
	unlock();
	if(m_thread.joinable())
		m_thread.join();

	// quit OpenAL
	alcMakeContextCurrent(nullptr);
	alcDestroyContext(m_context);
	alcCloseDevice(m_device);
}

DeviceSpecs OpenALDevice::getSpecs() const
{
	return m_specs;
}

bool OpenALDevice::getFormat(ALenum &format, Specs specs)
{
	bool valid = true;
	format = 0;

	switch(m_specs.format)
	{
	case FORMAT_S16:
		switch(specs.channels)
		{
		case CHANNELS_MONO:
			format = AL_FORMAT_MONO16;
			break;
		case CHANNELS_STEREO:
			format = AL_FORMAT_STEREO16;
			break;
		case CHANNELS_SURROUND4:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_QUAD16");
				break;
			}
		case CHANNELS_SURROUND51:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_51CHN16");
				break;
			}
		case CHANNELS_SURROUND61:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_61CHN16");
				break;
			}
		case CHANNELS_SURROUND71:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_71CHN16");
				break;
			}
		default:
			valid = false;
		}
		break;
	case FORMAT_FLOAT32:
		switch(specs.channels)
		{
		case CHANNELS_MONO:
			format = alGetEnumValue("AL_FORMAT_MONO_FLOAT32");
			break;
		case CHANNELS_STEREO:
			format = alGetEnumValue("AL_FORMAT_STEREO_FLOAT32");
			break;
		case CHANNELS_SURROUND4:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_QUAD32");
				break;
			}
		case CHANNELS_SURROUND51:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_51CHN32");
				break;
			}
		case CHANNELS_SURROUND61:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_61CHN32");
				break;
			}
		case CHANNELS_SURROUND71:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_71CHN32");
				break;
			}
		default:
			valid = false;
		}
		break;
	default:
		valid = false;
	}

	if(!format)
		valid = false;

	return valid;
}

std::shared_ptr<IHandle> OpenALDevice::play(std::shared_ptr<IReader> reader, bool keep)
{
	Specs specs = reader->getSpecs();

	// check format
	if(specs.channels == CHANNELS_INVALID)
		return std::shared_ptr<IHandle>();

	if(m_specs.format != FORMAT_FLOAT32)
		reader = std::shared_ptr<IReader>(new ConverterReader(reader, m_specs));

	ALenum format;

	if(!getFormat(format, specs))
		return std::shared_ptr<IHandle>();

	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	alcSuspendContext(m_context);

	std::shared_ptr<OpenALDevice::OpenALHandle> sound;

	try
	{
		// create the handle
		sound = std::shared_ptr<OpenALDevice::OpenALHandle>(new OpenALDevice::OpenALHandle(this, format, reader, keep));
	}
	catch(Exception&)
	{
		alcProcessContext(m_context);
		throw;
	}

	alcProcessContext(m_context);

	// play sound
	m_playingSounds.push_back(sound);

	start();

	return std::shared_ptr<IHandle>(sound);
}

std::shared_ptr<IHandle> OpenALDevice::play(std::shared_ptr<ISound> sound, bool keep)
{
	return play(sound->createReader(), keep);
}

void OpenALDevice::stopAll()
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	alcSuspendContext(m_context);

	while(!m_playingSounds.empty())
		m_playingSounds.front()->stop();

	while(!m_pausedSounds.empty())
		m_pausedSounds.front()->stop();

	alcProcessContext(m_context);
}

void OpenALDevice::lock()
{
	m_mutex.lock();
}

void OpenALDevice::unlock()
{
	m_mutex.unlock();
}

float OpenALDevice::getVolume() const
{
	float result;

	alGetListenerf(AL_GAIN, &result);
	return result;
}

void OpenALDevice::setVolume(float volume)
{
	if(volume < 0.0f)
		return;

	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	alListenerf(AL_GAIN, volume);
}

ISynchronizer* OpenALDevice::getSynchronizer()
{
	return &m_synchronizer;
}

/******************************************************************************/
/**************************** 3D Device Code **********************************/
/******************************************************************************/

Vector3 OpenALDevice::getListenerLocation() const
{
	ALfloat p[3];

	alGetListenerfv(AL_POSITION, p);
	return Vector3(p[0], p[1], p[2]);
}

void OpenALDevice::setListenerLocation(const Vector3& location)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	alListenerfv(AL_POSITION, (ALfloat*)location.get());
}

Vector3 OpenALDevice::getListenerVelocity() const
{
	ALfloat v[3];

	alGetListenerfv(AL_VELOCITY, v);
	return Vector3(v[0], v[1], v[2]);
}

void OpenALDevice::setListenerVelocity(const Vector3& velocity)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	alListenerfv(AL_VELOCITY, (ALfloat*)velocity.get());
}

Quaternion OpenALDevice::getListenerOrientation() const
{
	return m_orientation;
}

void OpenALDevice::setListenerOrientation(const Quaternion& orientation)
{
	ALfloat direction[6];

	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	direction[0] = -2 * (orientation.w() * orientation.y() +
						 orientation.x() * orientation.z());
	direction[1] = 2 * (orientation.x() * orientation.w() -
						orientation.z() * orientation.y());
	direction[2] = 2 * (orientation.x() * orientation.x() +
						orientation.y() * orientation.y()) - 1;
	direction[3] = 2 * (orientation.x() * orientation.y() -
						orientation.w() * orientation.z());
	direction[4] = 1 - 2 * (orientation.x() * orientation.x() +
							orientation.z() * orientation.z());
	direction[5] = 2 * (orientation.w() * orientation.x() +
						orientation.y() * orientation.z());
	alListenerfv(AL_ORIENTATION, direction);
	m_orientation = orientation;
}

float OpenALDevice::getSpeedOfSound() const
{
	return alGetFloat(AL_SPEED_OF_SOUND);
}

void OpenALDevice::setSpeedOfSound(float speed)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	alSpeedOfSound(speed);
}

float OpenALDevice::getDopplerFactor() const
{
	return alGetFloat(AL_DOPPLER_FACTOR);
}

void OpenALDevice::setDopplerFactor(float factor)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	alDopplerFactor(factor);
}

DistanceModel OpenALDevice::getDistanceModel() const
{
	switch(alGetInteger(AL_DISTANCE_MODEL))
	{
	case AL_INVERSE_DISTANCE:
		return DISTANCE_MODEL_INVERSE;
	case AL_INVERSE_DISTANCE_CLAMPED:
		return DISTANCE_MODEL_INVERSE_CLAMPED;
	case AL_LINEAR_DISTANCE:
		return DISTANCE_MODEL_LINEAR;
	case AL_LINEAR_DISTANCE_CLAMPED:
		return DISTANCE_MODEL_LINEAR_CLAMPED;
	case AL_EXPONENT_DISTANCE:
		return DISTANCE_MODEL_EXPONENT;
	case AL_EXPONENT_DISTANCE_CLAMPED:
		return DISTANCE_MODEL_EXPONENT_CLAMPED;
	default:
		return DISTANCE_MODEL_INVALID;
	}
}

void OpenALDevice::setDistanceModel(DistanceModel model)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	switch(model)
	{
	case DISTANCE_MODEL_INVERSE:
		alDistanceModel(AL_INVERSE_DISTANCE);
		break;
	case DISTANCE_MODEL_INVERSE_CLAMPED:
		alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
		break;
	case DISTANCE_MODEL_LINEAR:
		alDistanceModel(AL_LINEAR_DISTANCE);
		break;
	case DISTANCE_MODEL_LINEAR_CLAMPED:
		alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
		break;
	case DISTANCE_MODEL_EXPONENT:
		alDistanceModel(AL_EXPONENT_DISTANCE);
		break;
	case DISTANCE_MODEL_EXPONENT_CLAMPED:
		alDistanceModel(AL_EXPONENT_DISTANCE_CLAMPED);
		break;
	default:
		alDistanceModel(AL_NONE);
	}
}

std::list<std::string> OpenALDevice::getDeviceNames()
{
	std::list<std::string> names;

	if(alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT") == AL_TRUE)
	{
		ALCchar* devices = const_cast<ALCchar*>(alcGetString(nullptr, ALC_DEVICE_SPECIFIER));
		std::string default_device = alcGetString(nullptr, ALC_DEFAULT_DEVICE_SPECIFIER);

		while(*devices)
		{
			std::string device = devices;

			if(device == default_device)
				names.push_front(device);
			else
				names.push_back(device);

			devices += strlen(devices) + 1;
		}
	}

	return names;
}

class OpenALDeviceFactory : public IDeviceFactory
{
private:
	DeviceSpecs m_specs;
	int m_buffersize;
	std::string m_name;

public:
	OpenALDeviceFactory(const std::string &name = "") :
		m_buffersize(AUD_DEFAULT_BUFFER_SIZE),
		m_name(name)
	{
		m_specs.format = FORMAT_FLOAT32;
		m_specs.channels = CHANNELS_SURROUND51;
		m_specs.rate = RATE_48000;
	}

	virtual std::shared_ptr<IDevice> openDevice()
	{
		return std::shared_ptr<IDevice>(new OpenALDevice(m_specs, m_buffersize, m_name));
	}

	virtual int getPriority()
	{
		return 1 << 10;
	}

	virtual void setSpecs(DeviceSpecs specs)
	{
		m_specs = specs;
	}

	virtual void setBufferSize(int buffersize)
	{
		m_buffersize = buffersize;
	}

	virtual void setName(const std::string &name)
	{
	}
};

void OpenALDevice::registerPlugin()
{
	auto names = OpenALDevice::getDeviceNames();
	DeviceManager::registerDevice("OpenAL", std::shared_ptr<IDeviceFactory>(new OpenALDeviceFactory));
	for(const std::string &name : names)
	{
		DeviceManager::registerDevice("OpenAL - " + name, std::shared_ptr<IDeviceFactory>(new OpenALDeviceFactory(name)));
	}
}

#ifdef OPENAL_PLUGIN
extern "C" AUD_PLUGIN_API void registerPlugin()
{
	OpenALDevice::registerPlugin();
}

extern "C" AUD_PLUGIN_API const char* getName()
{
	return "OpenAL";
}
#endif

AUD_NAMESPACE_END
