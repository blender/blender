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

/** \file audaspace/OpenAL/AUD_OpenALDevice.cpp
 *  \ingroup audopenal
 */


#include "AUD_OpenALDevice.h"
#include "AUD_IFactory.h"
#include "AUD_IReader.h"
#include "AUD_ConverterReader.h"

#include <cstring>
#include <limits>

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define AUD_OPENAL_CYCLE_BUFFERS 3

/// Saves the data for playback.
struct AUD_OpenALHandle : AUD_Handle
{
	/// Whether it's a buffered or a streamed source.
	bool isBuffered;

	/// The reader source.
	AUD_IReader* reader;

	/// Whether to keep the source if end of it is reached.
	bool keep;

	/// OpenAL sample format.
	ALenum format;

	/// OpenAL source.
	ALuint source;

	/// OpenAL buffers.
	ALuint buffers[AUD_OPENAL_CYCLE_BUFFERS];

	/// The first buffer to be read next.
	int current;

	/// Whether the stream doesn't return any more data.
	bool data_end;

	/// The loop count of the source.
	int loopcount;

	/// The stop callback.
	stopCallback stop;

	/// Stop callback data.
	void* stop_data;
};

struct AUD_OpenALBufferedFactory
{
	/// The factory.
	AUD_IFactory* factory;

	/// The OpenAL buffer.
	ALuint buffer;
};

typedef std::list<AUD_OpenALHandle*>::iterator AUD_HandleIterator;
typedef std::list<AUD_OpenALBufferedFactory*>::iterator AUD_BFIterator;

/******************************************************************************/
/**************************** Threading Code **********************************/
/******************************************************************************/

void* AUD_openalRunThread(void* device)
{
	AUD_OpenALDevice* dev = (AUD_OpenALDevice*)device;
	dev->updateStreams();
	return NULL;
}

void AUD_OpenALDevice::start(bool join)
{
	lock();

	if(!m_playing)
	{
		if(join)
			pthread_join(m_thread, NULL);

		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

		pthread_create(&m_thread, &attr, AUD_openalRunThread, this);

		pthread_attr_destroy(&attr);

		m_playing = true;
	}

	unlock();
}

void AUD_OpenALDevice::updateStreams()
{
	AUD_OpenALHandle* sound;

	int length;
	sample_t* buffer;

	ALint info;
	AUD_DeviceSpecs specs = m_specs;
	ALCenum cerr;
	std::list<AUD_OpenALHandle*> stopSounds;
	std::list<AUD_OpenALHandle*> pauseSounds;
	AUD_HandleIterator it;

	while(1)
	{
		lock();

		alcSuspendContext(m_context);
		cerr = alcGetError(m_device);
		if(cerr == ALC_NO_ERROR)
		{
			// for all sounds
			for(it = m_playingSounds->begin(); it != m_playingSounds->end(); it++)
			{
				sound = *it;

				// is it a streamed sound?
				if(!sound->isBuffered)
				{
					// check for buffer refilling
					alGetSourcei(sound->source, AL_BUFFERS_PROCESSED, &info);

					if(info)
					{
						specs.specs = sound->reader->getSpecs();

						// for all empty buffers
						while(info--)
						{
							// if there's still data to play back
							if(!sound->data_end)
							{
								// read data
								length = m_buffersize;
								sound->reader->read(length, buffer);

								// looping necessary?
								if(length == 0 && sound->loopcount)
								{
									if(sound->loopcount > 0)
										sound->loopcount--;

									sound->reader->seek(0);

									length = m_buffersize;
									sound->reader->read(length, buffer);
								}

								// read nothing?
								if(length == 0)
								{
									sound->data_end = true;
									break;
								}

								// unqueue buffer
								alSourceUnqueueBuffers(sound->source, 1,
												&sound->buffers[sound->current]);
								ALenum err;
								if((err = alGetError()) != AL_NO_ERROR)
								{
									sound->data_end = true;
									break;
								}

								// fill with new data
								alBufferData(sound->buffers[sound->current],
											 sound->format,
											 buffer, length *
											 AUD_DEVICE_SAMPLE_SIZE(specs),
											 specs.rate);

								if((err = alGetError()) != AL_NO_ERROR)
								{
									sound->data_end = true;
									break;
								}

								// and queue again
								alSourceQueueBuffers(sound->source, 1,
												&sound->buffers[sound->current]);
								if(alGetError() != AL_NO_ERROR)
								{
									sound->data_end = true;
									break;
								}

								sound->current = (sound->current+1) %
												 AUD_OPENAL_CYCLE_BUFFERS;
							}
							else
								break;
						}
					}
				}

				// check if the sound has been stopped
				alGetSourcei(sound->source, AL_SOURCE_STATE, &info);

				if(info != AL_PLAYING)
				{
					// if it really stopped
					if(sound->data_end)
					{
						if(sound->stop)
							sound->stop(sound->stop_data);

						// pause or
						if(sound->keep)
							pauseSounds.push_back(sound);
						// stop
						else
							stopSounds.push_back(sound);
					}
					// continue playing
					else
						alSourcePlay(sound->source);
				}
			}

			for(it = pauseSounds.begin(); it != pauseSounds.end(); it++)
				pause(*it);

			for(it = stopSounds.begin(); it != stopSounds.end(); it++)
				stop(*it);

			pauseSounds.clear();
			stopSounds.clear();

			alcProcessContext(m_context);
		}

		// stop thread
		if(m_playingSounds->empty() || (cerr != ALC_NO_ERROR))
		{
			m_playing = false;
			unlock();
			pthread_exit(NULL);
		}

		unlock();

#ifdef WIN32
		Sleep(20);
#else
		usleep(20000);
#endif
	}
}

/******************************************************************************/
/**************************** IDevice Code ************************************/
/******************************************************************************/

bool AUD_OpenALDevice::isValid(AUD_Handle* handle)
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

static const char* open_error = "AUD_OpenALDevice: Device couldn't be opened.";

AUD_OpenALDevice::AUD_OpenALDevice(AUD_DeviceSpecs specs, int buffersize)
{
	// cannot determine how many channels or which format OpenAL uses, but
	// it at least is able to play 16 bit stereo audio
	specs.channels = AUD_CHANNELS_STEREO;
	specs.format = AUD_FORMAT_S16;

#if 0
	if(alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT") == AL_TRUE)
	{
		ALCchar* devices = const_cast<ALCchar*>(alcGetString(NULL, ALC_DEVICE_SPECIFIER));
		printf("OpenAL devices (standard is: %s):\n", alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER));

		while(*devices)
		{
			printf("%s\n", devices);
			devices += strlen(devices) + 1;
		}
	}
#endif

	m_device = alcOpenDevice(NULL);

	if(!m_device)
		AUD_THROW(AUD_ERROR_OPENAL, open_error);

	// at least try to set the frequency
	ALCint attribs[] = { ALC_FREQUENCY, specs.rate, 0 };
	ALCint* attributes = attribs;
	if(specs.rate == AUD_RATE_INVALID)
		attributes = NULL;

	m_context = alcCreateContext(m_device, attributes);
	alcMakeContextCurrent(m_context);

	alcGetIntegerv(m_device, ALC_FREQUENCY, 1, (ALCint*)&specs.rate);

	// check for specific formats and channel counts to be played back
	if(alIsExtensionPresent("AL_EXT_FLOAT32") == AL_TRUE)
		specs.format = AUD_FORMAT_FLOAT32;

	m_useMC = alIsExtensionPresent("AL_EXT_MCFORMATS") == AL_TRUE;

	alGetError();
	alcGetError(m_device);

	m_specs = specs;
	m_buffersize = buffersize;
	m_playing = false;

	m_playingSounds = new std::list<AUD_OpenALHandle*>();
	m_pausedSounds = new std::list<AUD_OpenALHandle*>();
	m_bufferedFactories = new std::list<AUD_OpenALBufferedFactory*>();

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&m_mutex, &attr);

	pthread_mutexattr_destroy(&attr);

	start(false);
}

AUD_OpenALDevice::~AUD_OpenALDevice()
{
	AUD_OpenALHandle* sound;

	lock();
	alcSuspendContext(m_context);

	// delete all playing sounds
	while(!m_playingSounds->empty())
	{
		sound = *(m_playingSounds->begin());
		alDeleteSources(1, &sound->source);
		if(!sound->isBuffered)
		{
			delete sound->reader;
			alDeleteBuffers(AUD_OPENAL_CYCLE_BUFFERS, sound->buffers);
		}
		delete sound;
		m_playingSounds->erase(m_playingSounds->begin());
	}

	// delete all paused sounds
	while(!m_pausedSounds->empty())
	{
		sound = *(m_pausedSounds->begin());
		alDeleteSources(1, &sound->source);
		if(!sound->isBuffered)
		{
			delete sound->reader;
			alDeleteBuffers(AUD_OPENAL_CYCLE_BUFFERS, sound->buffers);
		}
		delete sound;
		m_pausedSounds->erase(m_pausedSounds->begin());
	}

	// delete all buffered factories
	while(!m_bufferedFactories->empty())
	{
		alDeleteBuffers(1, &(*(m_bufferedFactories->begin()))->buffer);
		delete *m_bufferedFactories->begin();
		m_bufferedFactories->erase(m_bufferedFactories->begin());
	}

	alcProcessContext(m_context);

	// wait for the thread to stop
	unlock();
	pthread_join(m_thread, NULL);

	delete m_playingSounds;
	delete m_pausedSounds;
	delete m_bufferedFactories;

	// quit OpenAL
	alcMakeContextCurrent(NULL);
	alcDestroyContext(m_context);
	alcCloseDevice(m_device);

	pthread_mutex_destroy(&m_mutex);
}

AUD_DeviceSpecs AUD_OpenALDevice::getSpecs() const
{
	return m_specs;
}

bool AUD_OpenALDevice::getFormat(ALenum &format, AUD_Specs specs)
{
	bool valid = true;
	format = 0;

	switch(m_specs.format)
	{
	case AUD_FORMAT_S16:
		switch(specs.channels)
		{
		case AUD_CHANNELS_MONO:
			format = AL_FORMAT_MONO16;
			break;
		case AUD_CHANNELS_STEREO:
			format = AL_FORMAT_STEREO16;
			break;
		case AUD_CHANNELS_SURROUND4:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_QUAD16");
				break;
			}
		case AUD_CHANNELS_SURROUND51:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_51CHN16");
				break;
			}
		case AUD_CHANNELS_SURROUND61:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_61CHN16");
				break;
			}
		case AUD_CHANNELS_SURROUND71:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_71CHN16");
				break;
			}
		default:
			valid = false;
		}
		break;
	case AUD_FORMAT_FLOAT32:
		switch(specs.channels)
		{
		case AUD_CHANNELS_MONO:
			format = alGetEnumValue("AL_FORMAT_MONO_FLOAT32");
			break;
		case AUD_CHANNELS_STEREO:
			format = alGetEnumValue("AL_FORMAT_STEREO_FLOAT32");
			break;
		case AUD_CHANNELS_SURROUND4:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_QUAD32");
				break;
			}
		case AUD_CHANNELS_SURROUND51:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_51CHN32");
				break;
			}
		case AUD_CHANNELS_SURROUND61:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_61CHN32");
				break;
			}
		case AUD_CHANNELS_SURROUND71:
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

static const char* genbuffer_error = "AUD_OpenALDevice: Buffer couldn't be "
									 "generated.";
static const char* gensource_error = "AUD_OpenALDevice: Source couldn't be "
									 "generated.";
static const char* queue_error = "AUD_OpenALDevice: Buffer couldn't be "
								 "queued to the source.";
static const char* bufferdata_error = "AUD_OpenALDevice: Buffer couldn't be "
									  "filled with data.";

AUD_Handle* AUD_OpenALDevice::play(AUD_IReader* reader, bool keep)
{
	AUD_OpenALHandle* sound = NULL;

	AUD_DeviceSpecs specs = m_specs;
	specs.specs = reader->getSpecs();

	// check format
	bool valid = specs.channels != AUD_CHANNELS_INVALID;

	if(m_specs.format != AUD_FORMAT_FLOAT32)
		reader = new AUD_ConverterReader(reader, m_specs);

	// create the handle
	sound = new AUD_OpenALHandle;
	sound->keep = keep;
	sound->reader = reader;
	sound->current = 0;
	sound->isBuffered = false;
	sound->data_end = false;
	sound->loopcount = 0;
	sound->stop = NULL;
	sound->stop_data = NULL;

	valid &= getFormat(sound->format, specs.specs);

	if(!valid)
	{
		delete sound;
		delete reader;
		return NULL;
	}

	lock();
	alcSuspendContext(m_context);

	// OpenAL playback code
	try
	{
		alGenBuffers(AUD_OPENAL_CYCLE_BUFFERS, sound->buffers);
		if(alGetError() != AL_NO_ERROR)
			AUD_THROW(AUD_ERROR_OPENAL, genbuffer_error);

		try
		{
			sample_t* buf;
			int length;

			for(int i = 0; i < AUD_OPENAL_CYCLE_BUFFERS; i++)
			{
				length = m_buffersize;
				reader->read(length, buf);
				alBufferData(sound->buffers[i], sound->format, buf,
							 length * AUD_DEVICE_SAMPLE_SIZE(specs),
							 specs.rate);
				if(alGetError() != AL_NO_ERROR)
					AUD_THROW(AUD_ERROR_OPENAL, bufferdata_error);
			}

			alGenSources(1, &sound->source);
			if(alGetError() != AL_NO_ERROR)
				AUD_THROW(AUD_ERROR_OPENAL, gensource_error);

			try
			{
				alSourceQueueBuffers(sound->source, AUD_OPENAL_CYCLE_BUFFERS,
									 sound->buffers);
				if(alGetError() != AL_NO_ERROR)
					AUD_THROW(AUD_ERROR_OPENAL, queue_error);
			}
			catch(AUD_Exception&)
			{
				alDeleteSources(1, &sound->source);
				throw;
			}
		}
		catch(AUD_Exception&)
		{
			alDeleteBuffers(AUD_OPENAL_CYCLE_BUFFERS, sound->buffers);
			throw;
		}
	}
	catch(AUD_Exception&)
	{
		delete sound;
		delete reader;
		alcProcessContext(m_context);
		unlock();
		throw;
	}

	// play sound
	m_playingSounds->push_back(sound);
	alSourcei(sound->source, AL_SOURCE_RELATIVE, 1);

	start();

	alcProcessContext(m_context);
	unlock();

	return sound;
}

AUD_Handle* AUD_OpenALDevice::play(AUD_IFactory* factory, bool keep)
{
	AUD_OpenALHandle* sound = NULL;

	lock();

	try
	{
		// check if it is a buffered factory
		for(AUD_BFIterator i = m_bufferedFactories->begin();
			i != m_bufferedFactories->end(); i++)
		{
			if((*i)->factory == factory)
			{
				// create the handle
				sound = new AUD_OpenALHandle;
				sound->keep = keep;
				sound->current = -1;
				sound->isBuffered = true;
				sound->data_end = true;
				sound->loopcount = 0;
				sound->stop = NULL;
				sound->stop_data = NULL;

				alcSuspendContext(m_context);

				// OpenAL playback code
				try
				{
					alGenSources(1, &sound->source);
					if(alGetError() != AL_NO_ERROR)
						AUD_THROW(AUD_ERROR_OPENAL, gensource_error);

					try
					{
						alSourcei(sound->source, AL_BUFFER, (*i)->buffer);
						if(alGetError() != AL_NO_ERROR)
							AUD_THROW(AUD_ERROR_OPENAL, queue_error);
					}
					catch(AUD_Exception&)
					{
						alDeleteSources(1, &sound->source);
						throw;
					}
				}
				catch(AUD_Exception&)
				{
					delete sound;
					alcProcessContext(m_context);
					throw;
				}

				// play sound
				m_playingSounds->push_back(sound);

				alSourcei(sound->source, AL_SOURCE_RELATIVE, 1);
				start();

				alcProcessContext(m_context);
			}
		}
	}
	catch(AUD_Exception&)
	{
		unlock();
		throw;
	}

	unlock();

	if(sound)
		return sound;

	return play(factory->createReader(), keep);
}

bool AUD_OpenALDevice::pause(AUD_Handle* handle)
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
			alSourcePause((*i)->source);
			m_playingSounds->erase(i);
			result = true;
			break;
		}
	}

	unlock();

	return result;
}

bool AUD_OpenALDevice::resume(AUD_Handle* handle)
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
			start();
			m_pausedSounds->erase(i);
			result = true;
			break;
		}
	}

	unlock();

	return result;
}

bool AUD_OpenALDevice::stop(AUD_Handle* handle)
{
	AUD_OpenALHandle* sound;

	bool result = false;

	lock();

	for(AUD_HandleIterator i = m_playingSounds->begin();
		i != m_playingSounds->end(); i++)
	{
		if(*i == handle)
		{
			sound = *i;
			alDeleteSources(1, &sound->source);
			if(!sound->isBuffered)
			{
				delete sound->reader;
				alDeleteBuffers(AUD_OPENAL_CYCLE_BUFFERS, sound->buffers);
			}
			delete *i;
			m_playingSounds->erase(i);
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
				sound = *i;
				alDeleteSources(1, &sound->source);
				if(!sound->isBuffered)
				{
					delete sound->reader;
					alDeleteBuffers(AUD_OPENAL_CYCLE_BUFFERS, sound->buffers);
				}
				delete *i;
				m_pausedSounds->erase(i);
				result = true;
				break;
			}
		}
	}

	unlock();

	return result;
}

bool AUD_OpenALDevice::getKeep(AUD_Handle* handle)
{
	bool result = false;

	lock();

	if(isValid(handle))
		result = ((AUD_OpenALHandle*)handle)->keep;

	unlock();

	return result;
}

bool AUD_OpenALDevice::setKeep(AUD_Handle* handle, bool keep)
{
	bool result = false;

	lock();

	if(isValid(handle))
	{
		((AUD_OpenALHandle*)handle)->keep = keep;
		result = true;
	}

	unlock();

	return result;
}

bool AUD_OpenALDevice::seek(AUD_Handle* handle, float position)
{
	bool result = false;

	lock();

	if(isValid(handle))
	{
		AUD_OpenALHandle* alhandle = (AUD_OpenALHandle*)handle;
		if(alhandle->isBuffered)
			alSourcef(alhandle->source, AL_SEC_OFFSET, position);
		else
		{
			alhandle->reader->seek((int)(position *
										 alhandle->reader->getSpecs().rate));
			alhandle->data_end = false;

			ALint info;

			alGetSourcei(alhandle->source, AL_SOURCE_STATE, &info);

			if(info != AL_PLAYING)
			{
				if(info == AL_PAUSED)
					alSourceStop(alhandle->source);

				alSourcei(alhandle->source, AL_BUFFER, 0);
				alhandle->current = 0;

				ALenum err;
				if((err = alGetError()) == AL_NO_ERROR)
				{
					sample_t* buf;
					int length;
					AUD_DeviceSpecs specs = m_specs;
					specs.specs = alhandle->reader->getSpecs();

					for(int i = 0; i < AUD_OPENAL_CYCLE_BUFFERS; i++)
					{
						length = m_buffersize;
						alhandle->reader->read(length, buf);
						alBufferData(alhandle->buffers[i], alhandle->format,
									 buf,
									 length * AUD_DEVICE_SAMPLE_SIZE(specs),
									 specs.rate);

						if(alGetError() != AL_NO_ERROR)
							break;
					}

					alSourceQueueBuffers(alhandle->source,
										 AUD_OPENAL_CYCLE_BUFFERS,
										 alhandle->buffers);
				}

				alSourceRewind(alhandle->source);
			}
		}
		result = true;
	}

	unlock();
	return result;
}

float AUD_OpenALDevice::getPosition(AUD_Handle* handle)
{
	float position = 0.0f;

	lock();

	if(isValid(handle))
	{
		AUD_OpenALHandle* h = (AUD_OpenALHandle*)handle;
		alGetSourcef(h->source, AL_SEC_OFFSET, &position);
		if(!h->isBuffered)
		{
			AUD_Specs specs = h->reader->getSpecs();
			position += (h->reader->getPosition() - m_buffersize *
									AUD_OPENAL_CYCLE_BUFFERS) /
					   (float)specs.rate;
		}
	}

	unlock();
	return position;
}

AUD_Status AUD_OpenALDevice::getStatus(AUD_Handle* handle)
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

void AUD_OpenALDevice::lock()
{
	pthread_mutex_lock(&m_mutex);
}

void AUD_OpenALDevice::unlock()
{
	pthread_mutex_unlock(&m_mutex);
}

float AUD_OpenALDevice::getVolume() const
{
	float result;
	alGetListenerf(AL_GAIN, &result);
	return result;
}

void AUD_OpenALDevice::setVolume(float volume)
{
	alListenerf(AL_GAIN, volume);
}

float AUD_OpenALDevice::getVolume(AUD_Handle* handle)
{
	lock();
	float result = std::numeric_limits<float>::quiet_NaN();
	if(isValid(handle))
		alGetSourcef(((AUD_OpenALHandle*)handle)->source,AL_GAIN, &result);
	unlock();
	return result;
}

bool AUD_OpenALDevice::setVolume(AUD_Handle* handle, float volume)
{
	lock();
	bool result = isValid(handle);
	if(result)
		alSourcef(((AUD_OpenALHandle*)handle)->source, AL_GAIN, volume);
	unlock();
	return result;
}

float AUD_OpenALDevice::getPitch(AUD_Handle* handle)
{
	lock();
	float result = std::numeric_limits<float>::quiet_NaN();
	if(isValid(handle))
		alGetSourcef(((AUD_OpenALHandle*)handle)->source,AL_PITCH, &result);
	unlock();
	return result;
}

bool AUD_OpenALDevice::setPitch(AUD_Handle* handle, float pitch)
{
	lock();
	bool result = isValid(handle);
	if(result)
		alSourcef(((AUD_OpenALHandle*)handle)->source, AL_PITCH, pitch);
	unlock();
	return result;
}

int AUD_OpenALDevice::getLoopCount(AUD_Handle* handle)
{
	lock();
	int result = 0;
	if(isValid(handle))
		result = ((AUD_OpenALHandle*)handle)->loopcount;
	unlock();
	return result;
}

bool AUD_OpenALDevice::setLoopCount(AUD_Handle* handle, int count)
{
	lock();
	bool result = isValid(handle);
	if(result)
		((AUD_OpenALHandle*)handle)->loopcount = count;
	unlock();
	return result;
}

bool AUD_OpenALDevice::setStopCallback(AUD_Handle* handle, stopCallback callback, void* data)
{
	lock();
	bool result = isValid(handle);
	if(result)
	{
		AUD_OpenALHandle* h = (AUD_OpenALHandle*)handle;
		h->stop = callback;
		h->stop_data = data;
	}
	unlock();
	return result;
}

/* AUD_XXX Temorary disabled

bool AUD_OpenALDevice::bufferFactory(void *value)
{
	bool result = false;
	AUD_IFactory* factory = (AUD_IFactory*) value;

	// load the factory into an OpenAL buffer
	if(factory)
	{
		// check if the factory is already buffered
		lock();
		for(AUD_BFIterator i = m_bufferedFactories->begin();
			i != m_bufferedFactories->end(); i++)
		{
			if((*i)->factory == factory)
			{
				result = true;
				break;
			}
		}
		unlock();
		if(result)
			return result;

		AUD_IReader* reader = factory->createReader();

		if(reader == NULL)
			return false;

		AUD_DeviceSpecs specs = m_specs;
		specs.specs = reader->getSpecs();

		if(m_specs.format != AUD_FORMAT_FLOAT32)
			reader = new AUD_ConverterReader(reader, m_specs);

		ALenum format;

		if(!getFormat(format, specs.specs))
		{
			delete reader;
			return false;
		}

		// load into a buffer
		lock();
		alcSuspendContext(m_context);

		AUD_OpenALBufferedFactory* bf = new AUD_OpenALBufferedFactory;
		bf->factory = factory;

		try
		{
			alGenBuffers(1, &bf->buffer);
			if(alGetError() != AL_NO_ERROR)
				AUD_THROW(AUD_ERROR_OPENAL);

			try
			{
				sample_t* buf;
				int length = reader->getLength();

				reader->read(length, buf);
				alBufferData(bf->buffer, format, buf,
							 length * AUD_DEVICE_SAMPLE_SIZE(specs),
							 specs.rate);
				if(alGetError() != AL_NO_ERROR)
					AUD_THROW(AUD_ERROR_OPENAL);
			}
			catch(AUD_Exception&)
			{
				alDeleteBuffers(1, &bf->buffer);
				throw;
			}
		}
		catch(AUD_Exception&)
		{
			delete bf;
			delete reader;
			alcProcessContext(m_context);
			unlock();
			return false;
		}

		m_bufferedFactories->push_back(bf);

		alcProcessContext(m_context);
		unlock();
	}
	else
	{
		// stop all playing and paused buffered sources
		lock();
		alcSuspendContext(m_context);

		AUD_OpenALHandle* sound;
		AUD_HandleIterator it = m_playingSounds->begin();
		while(it != m_playingSounds->end())
		{
			sound = *it;
			++it;

			if(sound->isBuffered)
				stop(sound);
		}
		alcProcessContext(m_context);

		while(!m_bufferedFactories->empty())
		{
			alDeleteBuffers(1,
							&(*(m_bufferedFactories->begin()))->buffer);
			delete *m_bufferedFactories->begin();
			m_bufferedFactories->erase(m_bufferedFactories->begin());
		}
		unlock();
	}

	return true;
}*/

/******************************************************************************/
/**************************** 3D Device Code **********************************/
/******************************************************************************/

AUD_Vector3 AUD_OpenALDevice::getListenerLocation() const
{
	ALfloat p[3];
	alGetListenerfv(AL_POSITION, p);
	return AUD_Vector3(p[0], p[1], p[2]);
}

void AUD_OpenALDevice::setListenerLocation(const AUD_Vector3& location)
{
	alListenerfv(AL_POSITION, (ALfloat*)location.get());
}

AUD_Vector3 AUD_OpenALDevice::getListenerVelocity() const
{
	ALfloat v[3];
	alGetListenerfv(AL_VELOCITY, v);
	return AUD_Vector3(v[0], v[1], v[2]);
}

void AUD_OpenALDevice::setListenerVelocity(const AUD_Vector3& velocity)
{
	alListenerfv(AL_VELOCITY, (ALfloat*)velocity.get());
}

AUD_Quaternion AUD_OpenALDevice::getListenerOrientation() const
{
	// AUD_XXX not implemented yet
	return AUD_Quaternion(0, 0, 0, 0);
}

void AUD_OpenALDevice::setListenerOrientation(const AUD_Quaternion& orientation)
{
	ALfloat direction[6];
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
}

float AUD_OpenALDevice::getSpeedOfSound() const
{
	return alGetFloat(AL_SPEED_OF_SOUND);
}

void AUD_OpenALDevice::setSpeedOfSound(float speed)
{
	alSpeedOfSound(speed);
}

float AUD_OpenALDevice::getDopplerFactor() const
{
	return alGetFloat(AL_DOPPLER_FACTOR);
}

void AUD_OpenALDevice::setDopplerFactor(float factor)
{
	alDopplerFactor(factor);
}

AUD_DistanceModel AUD_OpenALDevice::getDistanceModel() const
{
	switch(alGetInteger(AL_DISTANCE_MODEL))
	{
	case AL_INVERSE_DISTANCE:
		return AUD_DISTANCE_MODEL_INVERSE;
	case AL_INVERSE_DISTANCE_CLAMPED:
		return AUD_DISTANCE_MODEL_INVERSE_CLAMPED;
	case AL_LINEAR_DISTANCE:
		return AUD_DISTANCE_MODEL_LINEAR;
	case AL_LINEAR_DISTANCE_CLAMPED:
		return AUD_DISTANCE_MODEL_LINEAR_CLAMPED;
	case AL_EXPONENT_DISTANCE:
		return AUD_DISTANCE_MODEL_EXPONENT;
	case AL_EXPONENT_DISTANCE_CLAMPED:
		return AUD_DISTANCE_MODEL_EXPONENT_CLAMPED;
	default:
		return AUD_DISTANCE_MODEL_INVALID;
	}
}

void AUD_OpenALDevice::setDistanceModel(AUD_DistanceModel model)
{
	switch(model)
	{
	case AUD_DISTANCE_MODEL_INVERSE:
		alDistanceModel(AL_INVERSE_DISTANCE);
		break;
	case AUD_DISTANCE_MODEL_INVERSE_CLAMPED:
		alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
		break;
	case AUD_DISTANCE_MODEL_LINEAR:
		alDistanceModel(AL_LINEAR_DISTANCE);
		break;
	case AUD_DISTANCE_MODEL_LINEAR_CLAMPED:
		alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
		break;
	case AUD_DISTANCE_MODEL_EXPONENT:
		alDistanceModel(AL_EXPONENT_DISTANCE);
		break;
	case AUD_DISTANCE_MODEL_EXPONENT_CLAMPED:
		alDistanceModel(AL_EXPONENT_DISTANCE_CLAMPED);
		break;
	default:
		alDistanceModel(AL_NONE);
	}
}

AUD_Vector3 AUD_OpenALDevice::getSourceLocation(AUD_Handle* handle)
{
	AUD_Vector3 result = AUD_Vector3(0, 0, 0);
	ALfloat p[3];
	lock();

	if(isValid(handle))
	{
		alGetSourcefv(((AUD_OpenALHandle*)handle)->source, AL_POSITION, p);
		result = AUD_Vector3(p[0], p[1], p[2]);
	}

	unlock();
	return result;
}

bool AUD_OpenALDevice::setSourceLocation(AUD_Handle* handle, const AUD_Vector3& location)
{
	lock();
	bool result = isValid(handle);

	if(result)
		alSourcefv(((AUD_OpenALHandle*)handle)->source, AL_POSITION,
				   (ALfloat*)location.get());

	unlock();
	return result;
}

AUD_Vector3 AUD_OpenALDevice::getSourceVelocity(AUD_Handle* handle)
{
	AUD_Vector3 result = AUD_Vector3(0, 0, 0);
	ALfloat v[3];
	lock();

	if(isValid(handle))
	{
		alGetSourcefv(((AUD_OpenALHandle*)handle)->source, AL_VELOCITY, v);
		result = AUD_Vector3(v[0], v[1], v[2]);
	}

	unlock();
	return result;
}

bool AUD_OpenALDevice::setSourceVelocity(AUD_Handle* handle, const AUD_Vector3& velocity)
{
	lock();
	bool result = isValid(handle);

	if(result)
		alSourcefv(((AUD_OpenALHandle*)handle)->source, AL_VELOCITY,
				   (ALfloat*)velocity.get());

	unlock();
	return result;
}

AUD_Quaternion AUD_OpenALDevice::getSourceOrientation(AUD_Handle* handle)
{
	// AUD_XXX not implemented yet
	return AUD_Quaternion(0, 0, 0, 0);
}

bool AUD_OpenALDevice::setSourceOrientation(AUD_Handle* handle, const AUD_Quaternion& orientation)
{
	lock();
	bool result = isValid(handle);

	if(result)
	{
		ALfloat direction[3];
		direction[0] = -2 * (orientation.w() * orientation.y() +
							 orientation.x() * orientation.z());
		direction[1] = 2 * (orientation.x() * orientation.w() -
							orientation.z() * orientation.y());
		direction[2] = 2 * (orientation.x() * orientation.x() +
							orientation.y() * orientation.y()) - 1;
		alSourcefv(((AUD_OpenALHandle*)handle)->source, AL_DIRECTION,
				   direction);
	}

	unlock();
	return result;
}

bool AUD_OpenALDevice::isRelative(AUD_Handle* handle)
{
	int result = std::numeric_limits<float>::quiet_NaN();;

	lock();

	if(isValid(handle))
		alGetSourcei(((AUD_OpenALHandle*)handle)->source, AL_SOURCE_RELATIVE,
					 &result);

	unlock();
	return result;
}

bool AUD_OpenALDevice::setRelative(AUD_Handle* handle, bool relative)
{
	lock();
	bool result = isValid(handle);

	if(result)
		alSourcei(((AUD_OpenALHandle*)handle)->source, AL_SOURCE_RELATIVE,
				  relative);

	unlock();
	return result;
}

float AUD_OpenALDevice::getVolumeMaximum(AUD_Handle* handle)
{
	float result = std::numeric_limits<float>::quiet_NaN();;

	lock();

	if(isValid(handle))
		alGetSourcef(((AUD_OpenALHandle*)handle)->source, AL_MAX_GAIN,
					 &result);

	unlock();
	return result;
}

bool AUD_OpenALDevice::setVolumeMaximum(AUD_Handle* handle, float volume)
{
	lock();
	bool result = isValid(handle);

	if(result)

		alSourcef(((AUD_OpenALHandle*)handle)->source, AL_MAX_GAIN,
				  volume);

	unlock();
	return result;
}

float AUD_OpenALDevice::getVolumeMinimum(AUD_Handle* handle)
{
	float result = std::numeric_limits<float>::quiet_NaN();;

	lock();

	if(isValid(handle))
		alGetSourcef(((AUD_OpenALHandle*)handle)->source, AL_MIN_GAIN,
					 &result);

	unlock();
	return result;
}

bool AUD_OpenALDevice::setVolumeMinimum(AUD_Handle* handle, float volume)
{
	lock();
	bool result = isValid(handle);

	if(result)
		alSourcef(((AUD_OpenALHandle*)handle)->source, AL_MIN_GAIN,
				  volume);

	unlock();
	return result;
}

float AUD_OpenALDevice::getDistanceMaximum(AUD_Handle* handle)
{
	float result = std::numeric_limits<float>::quiet_NaN();;

	lock();

	if(isValid(handle))
		alGetSourcef(((AUD_OpenALHandle*)handle)->source, AL_MAX_DISTANCE,
					 &result);

	unlock();
	return result;
}

bool AUD_OpenALDevice::setDistanceMaximum(AUD_Handle* handle, float distance)
{
	lock();
	bool result = isValid(handle);

	if(result)
		alSourcef(((AUD_OpenALHandle*)handle)->source, AL_MAX_DISTANCE,
				  distance);

	unlock();
	return result;
}

float AUD_OpenALDevice::getDistanceReference(AUD_Handle* handle)
{
	float result = std::numeric_limits<float>::quiet_NaN();;

	lock();

	if(isValid(handle))
		alGetSourcef(((AUD_OpenALHandle*)handle)->source, AL_REFERENCE_DISTANCE,
					 &result);

	unlock();
	return result;
}

bool AUD_OpenALDevice::setDistanceReference(AUD_Handle* handle, float distance)
{
	lock();
	bool result = isValid(handle);

	if(result)
		alSourcef(((AUD_OpenALHandle*)handle)->source, AL_REFERENCE_DISTANCE,
				  distance);

	unlock();
	return result;
}

float AUD_OpenALDevice::getAttenuation(AUD_Handle* handle)
{
	float result = std::numeric_limits<float>::quiet_NaN();;

	lock();

	if(isValid(handle))
		alGetSourcef(((AUD_OpenALHandle*)handle)->source, AL_ROLLOFF_FACTOR,
					 &result);

	unlock();
	return result;
}

bool AUD_OpenALDevice::setAttenuation(AUD_Handle* handle, float factor)
{
	lock();
	bool result = isValid(handle);

	if(result)
		alSourcef(((AUD_OpenALHandle*)handle)->source, AL_ROLLOFF_FACTOR,
				  factor);

	unlock();
	return result;
}

float AUD_OpenALDevice::getConeAngleOuter(AUD_Handle* handle)
{
	float result = std::numeric_limits<float>::quiet_NaN();;

	lock();

	if(isValid(handle))
		alGetSourcef(((AUD_OpenALHandle*)handle)->source, AL_CONE_OUTER_ANGLE,
					 &result);

	unlock();
	return result;
}

bool AUD_OpenALDevice::setConeAngleOuter(AUD_Handle* handle, float angle)
{
	lock();
	bool result = isValid(handle);

	if(result)
		alSourcef(((AUD_OpenALHandle*)handle)->source, AL_CONE_OUTER_ANGLE,
				  angle);

	unlock();
	return result;
}

float AUD_OpenALDevice::getConeAngleInner(AUD_Handle* handle)
{
	float result = std::numeric_limits<float>::quiet_NaN();;

	lock();

	if(isValid(handle))
		alGetSourcef(((AUD_OpenALHandle*)handle)->source, AL_CONE_INNER_ANGLE,
					 &result);

	unlock();
	return result;
}

bool AUD_OpenALDevice::setConeAngleInner(AUD_Handle* handle, float angle)
{
	lock();
	bool result = isValid(handle);

	if(result)
		alSourcef(((AUD_OpenALHandle*)handle)->source, AL_CONE_INNER_ANGLE,
				  angle);

	unlock();
	return result;
}

float AUD_OpenALDevice::getConeVolumeOuter(AUD_Handle* handle)
{
	float result = std::numeric_limits<float>::quiet_NaN();;

	lock();

	if(isValid(handle))
		alGetSourcef(((AUD_OpenALHandle*)handle)->source, AL_CONE_OUTER_GAIN,
					 &result);

	unlock();
	return result;
}

bool AUD_OpenALDevice::setConeVolumeOuter(AUD_Handle* handle, float volume)
{
	lock();
	bool result = isValid(handle);

	if(result)
		alSourcef(((AUD_OpenALHandle*)handle)->source, AL_CONE_OUTER_GAIN,
				  volume);

	unlock();
	return result;
}
