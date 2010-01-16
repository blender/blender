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

#include "AUD_OpenALDevice.h"
#include "AUD_IReader.h"
#include "AUD_ConverterFactory.h"
#include "AUD_SourceCaps.h"

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

void AUD_OpenALDevice::start()
{
	lock();

	if(!m_playing)
	{
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

	while(1)
	{
		lock();

		alcSuspendContext(m_context);

		{
			// for all sounds
			AUD_HandleIterator it = m_playingSounds->begin();
			while(it != m_playingSounds->end())
			{
				sound = *it;
				// increment the iterator to make sure it's valid,
				// in case the sound gets deleted after stopping
				++it;

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

								if(alGetError() != AL_NO_ERROR)
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
						// pause or
						if(sound->keep)
							pause(sound);
						// stop
						else
							stop(sound);
					}
					// continue playing
					else
						alSourcePlay(sound->source);
				}
			}
		}

		alcProcessContext(m_context);

		// stop thread
		if(m_playingSounds->empty())
		{
			unlock();
			m_playing = false;
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

	m_device = alcOpenDevice("ALSA Software");
	if(m_device == NULL)
		m_device = alcOpenDevice(NULL);

	if(!m_device)
		AUD_THROW(AUD_ERROR_OPENAL);

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
	{
		specs.format = AUD_FORMAT_FLOAT32;
		m_converter = NULL;
	}
	else
		m_converter = new AUD_ConverterFactory(specs); AUD_NEW("factory")

	m_useMC = alIsExtensionPresent("AL_EXT_MCFORMATS") == AL_TRUE;

	alGetError();

	m_specs = specs;
	m_buffersize = buffersize;
	m_playing = false;

	m_playingSounds = new std::list<AUD_OpenALHandle*>(); AUD_NEW("list")
	m_pausedSounds = new std::list<AUD_OpenALHandle*>(); AUD_NEW("list")
	m_bufferedFactories = new std::list<AUD_OpenALBufferedFactory*>();
	AUD_NEW("list")

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&m_mutex, &attr);

	pthread_mutexattr_destroy(&attr);
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
			delete sound->reader; AUD_DELETE("reader")
			alDeleteBuffers(AUD_OPENAL_CYCLE_BUFFERS, sound->buffers);
		}
		delete sound; AUD_DELETE("handle")
		m_playingSounds->erase(m_playingSounds->begin());
	}

	// delete all paused sounds
	while(!m_pausedSounds->empty())
	{
		sound = *(m_pausedSounds->begin());
		alDeleteSources(1, &sound->source);
		if(!sound->isBuffered)
		{
			delete sound->reader; AUD_DELETE("reader")
			alDeleteBuffers(AUD_OPENAL_CYCLE_BUFFERS, sound->buffers);
		}
		delete sound; AUD_DELETE("handle")
		m_pausedSounds->erase(m_pausedSounds->begin());
	}

	// delete all buffered factories
	while(!m_bufferedFactories->empty())
	{
		alDeleteBuffers(1, &(*(m_bufferedFactories->begin()))->buffer);
		delete *m_bufferedFactories->begin(); AUD_DELETE("bufferedfactory");
		m_bufferedFactories->erase(m_bufferedFactories->begin());
	}

	alcProcessContext(m_context);

	// wait for the thread to stop
	if(m_playing)
	{
		unlock();
		pthread_join(m_thread, NULL);
	}
	else
		unlock();

	delete m_playingSounds; AUD_DELETE("list")
	delete m_pausedSounds; AUD_DELETE("list")
	delete m_bufferedFactories; AUD_DELETE("list")

	// quit OpenAL
	alcMakeContextCurrent(NULL);
	alcDestroyContext(m_context);
	alcCloseDevice(m_device);

	if(m_converter)
		delete m_converter; AUD_DELETE("factory")

	pthread_mutex_destroy(&m_mutex);
}

AUD_DeviceSpecs AUD_OpenALDevice::getSpecs()
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

AUD_Handle* AUD_OpenALDevice::play(AUD_IFactory* factory, bool keep)
{
	lock();

	AUD_OpenALHandle* sound = NULL;

	try
	{
		// check if it is a buffered factory
		for(AUD_BFIterator i = m_bufferedFactories->begin();
			i != m_bufferedFactories->end(); i++)
		{
			if((*i)->factory == factory)
			{
				// create the handle
				sound = new AUD_OpenALHandle; AUD_NEW("handle")
				sound->keep = keep;
				sound->current = -1;
				sound->isBuffered = true;
				sound->data_end = true;

				alcSuspendContext(m_context);

				// OpenAL playback code
				try
				{
					alGenSources(1, &sound->source);
					if(alGetError() != AL_NO_ERROR)
						AUD_THROW(AUD_ERROR_OPENAL);

					try
					{
						alSourcei(sound->source, AL_BUFFER, (*i)->buffer);
						if(alGetError() != AL_NO_ERROR)
							AUD_THROW(AUD_ERROR_OPENAL);
					}
					catch(AUD_Exception)
					{
						alDeleteSources(1, &sound->source);
						throw;
					}
				}
				catch(AUD_Exception)
				{
					delete sound; AUD_DELETE("handle")
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
	catch(AUD_Exception)
	{
		unlock();
		throw;
	}

	unlock();

	if(sound)
		return sound;

	AUD_IReader* reader = factory->createReader();

	if(reader == NULL)
		AUD_THROW(AUD_ERROR_READER);

	AUD_DeviceSpecs specs = m_specs;
	specs.specs = reader->getSpecs();

	// check format
	bool valid = specs.channels != AUD_CHANNELS_INVALID;

	if(m_converter)
	{
		m_converter->setReader(reader);
		reader = m_converter->createReader();
	}

	// create the handle
	sound = new AUD_OpenALHandle; AUD_NEW("handle")
	sound->keep = keep;
	sound->reader = reader;
	sound->current = 0;
	sound->isBuffered = false;
	sound->data_end = false;

	valid &= getFormat(sound->format, specs.specs);

	if(!valid)
	{
		delete sound; AUD_DELETE("handle")
		delete reader; AUD_DELETE("reader")
		return NULL;
	}

	lock();
	alcSuspendContext(m_context);

	// OpenAL playback code
	try
	{
		alGenBuffers(AUD_OPENAL_CYCLE_BUFFERS, sound->buffers);
		if(alGetError() != AL_NO_ERROR)
			AUD_THROW(AUD_ERROR_OPENAL);

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
					AUD_THROW(AUD_ERROR_OPENAL);
			}

			alGenSources(1, &sound->source);
			if(alGetError() != AL_NO_ERROR)
				AUD_THROW(AUD_ERROR_OPENAL);

			try
			{
				alSourceQueueBuffers(sound->source, AUD_OPENAL_CYCLE_BUFFERS,
									 sound->buffers);
				if(alGetError() != AL_NO_ERROR)
					AUD_THROW(AUD_ERROR_OPENAL);
			}
			catch(AUD_Exception)
			{
				alDeleteSources(1, &sound->source);
				throw;
			}
		}
		catch(AUD_Exception)
		{
			alDeleteBuffers(AUD_OPENAL_CYCLE_BUFFERS, sound->buffers);
			throw;
		}
	}
	catch(AUD_Exception)
	{
		delete sound; AUD_DELETE("handle")
		delete reader; AUD_DELETE("reader")
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
				delete sound->reader; AUD_DELETE("reader")
				alDeleteBuffers(AUD_OPENAL_CYCLE_BUFFERS, sound->buffers);
			}
			delete *i; AUD_DELETE("handle")
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
					delete sound->reader; AUD_DELETE("reader")
					alDeleteBuffers(AUD_OPENAL_CYCLE_BUFFERS, sound->buffers);
				}
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

bool AUD_OpenALDevice::sendMessage(AUD_Handle* handle, AUD_Message &message)
{
	bool result = false;

	lock();

	if(handle == 0)
	{
		for(AUD_HandleIterator i = m_playingSounds->begin();
			i != m_playingSounds->end(); i++)
			if(!(*i)->isBuffered)
				result |= (*i)->reader->notify(message);
		for(AUD_HandleIterator i = m_pausedSounds->begin();
			i != m_pausedSounds->end(); i++)
			if(!(*i)->isBuffered)
				result |= (*i)->reader->notify(message);
	}
	else if(isValid(handle))
		if(!((AUD_OpenALHandle*)handle)->isBuffered)
			result = ((AUD_OpenALHandle*)handle)->reader->notify(message);

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
				if(info != AL_STOPPED)
					alSourceStop(alhandle->source);

				alSourceUnqueueBuffers(alhandle->source,
									   AUD_OPENAL_CYCLE_BUFFERS,
									   alhandle->buffers);
				if(alGetError() == AL_NO_ERROR)
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
		if(h->isBuffered)
			alGetSourcef(h->source, AL_SEC_OFFSET, &position);
		else
			position = h->reader->getPosition() /
					   (float)h->reader->getSpecs().rate;
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

/******************************************************************************/
/**************************** Capabilities Code *******************************/
/******************************************************************************/

bool AUD_OpenALDevice::checkCapability(int capability)
{
	return capability == AUD_CAPS_3D_DEVICE ||
		   capability == AUD_CAPS_VOLUME ||
		   capability == AUD_CAPS_SOURCE_VOLUME ||
		   capability == AUD_CAPS_SOURCE_PITCH ||
		   capability == AUD_CAPS_BUFFERED_FACTORY;
}

bool AUD_OpenALDevice::setCapability(int capability, void *value)
{
	bool result = false;
	switch(capability)
	{
	case AUD_CAPS_VOLUME:
		alListenerf(AL_GAIN, *((float*)value));
		return true;
	case AUD_CAPS_SOURCE_VOLUME:
		{
			AUD_SourceCaps* caps = (AUD_SourceCaps*) value;
			lock();
			if(isValid(caps->handle))
			{
				alSourcef(((AUD_OpenALHandle*)caps->handle)->source,
						  AL_GAIN, caps->value);
				result = true;
			}
			unlock();
		}
		break;
	case AUD_CAPS_SOURCE_PITCH:
		{
			AUD_SourceCaps* caps = (AUD_SourceCaps*) value;
			lock();
			if(isValid(caps->handle))
			{
				alSourcef(((AUD_OpenALHandle*)caps->handle)->source,
						  AL_PITCH, caps->value);
				result = true;
			}
			unlock();
		}
		break;
	case AUD_CAPS_BUFFERED_FACTORY:
		{
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

				// determine format
				bool valid = reader->getType() == AUD_TYPE_BUFFER;

				if(valid)
				{
					if(m_converter)
					{
						m_converter->setReader(reader);
						reader = m_converter->createReader();
					}
				}

				ALenum format;

				if(valid)
					valid = getFormat(format, specs.specs);

				if(!valid)
				{
					delete reader; AUD_DELETE("reader")
					return false;
				}

				// load into a buffer
				lock();
				alcSuspendContext(m_context);

				AUD_OpenALBufferedFactory* bf = new AUD_OpenALBufferedFactory;
				AUD_NEW("bufferedfactory");
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
					catch(AUD_Exception)
					{
						alDeleteBuffers(1, &bf->buffer);
						throw;
					}
				}
				catch(AUD_Exception)
				{
					delete bf; AUD_DELETE("bufferedfactory")
					delete reader; AUD_DELETE("reader")
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
					AUD_DELETE("bufferedfactory");
					m_bufferedFactories->erase(m_bufferedFactories->begin());
				}
				unlock();
			}

			return true;
		}
		break;
	}
	return result;
}

bool AUD_OpenALDevice::getCapability(int capability, void *value)
{
	bool result = false;

	switch(capability)
	{
	case AUD_CAPS_VOLUME:
		alGetListenerf(AL_GAIN, (float*)value);
		return true;
	case AUD_CAPS_SOURCE_VOLUME:
		{
			AUD_SourceCaps* caps = (AUD_SourceCaps*) value;
			lock();
			if(isValid(caps->handle))
			{
				alGetSourcef(((AUD_OpenALHandle*)caps->handle)->source,
						  AL_GAIN, &caps->value);
				result = true;
			}
			unlock();
		}
		break;
	case AUD_CAPS_SOURCE_PITCH:
		{
			AUD_SourceCaps* caps = (AUD_SourceCaps*) value;
			lock();
			if(isValid(caps->handle))
			{
				alGetSourcef(((AUD_OpenALHandle*)caps->handle)->source,
						  AL_PITCH, &caps->value);
				result = true;
			}
			unlock();
		}
		break;
	}

	return result;
}

/******************************************************************************/
/**************************** 3D Device Code **********************************/
/******************************************************************************/

AUD_Handle* AUD_OpenALDevice::play3D(AUD_IFactory* factory, bool keep)
{
	AUD_OpenALHandle* handle = (AUD_OpenALHandle*)play(factory, keep);
	if(handle)
		alSourcei(handle->source, AL_SOURCE_RELATIVE, 0);
	return handle;
}

bool AUD_OpenALDevice::updateListener(AUD_3DData &data)
{
	alListenerfv(AL_POSITION, (ALfloat*)data.position);
	alListenerfv(AL_VELOCITY, (ALfloat*)data.velocity);
	alListenerfv(AL_ORIENTATION, (ALfloat*)&(data.orientation[3]));

	return true;
}

bool AUD_OpenALDevice::setSetting(AUD_3DSetting setting, float value)
{
	switch(setting)
	{
	case AUD_3DS_DISTANCE_MODEL:
		if(value == AUD_DISTANCE_MODEL_NONE)
			alDistanceModel(AL_NONE);
		else if(value == AUD_DISTANCE_MODEL_INVERSE)
			alDistanceModel(AL_INVERSE_DISTANCE);
		else if(value == AUD_DISTANCE_MODEL_INVERSE_CLAMPED)
			alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
		else if(value == AUD_DISTANCE_MODEL_LINEAR)
			alDistanceModel(AL_LINEAR_DISTANCE);
		else if(value == AUD_DISTANCE_MODEL_LINEAR_CLAMPED)
			alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
		else if(value == AUD_DISTANCE_MODEL_EXPONENT)
			alDistanceModel(AL_EXPONENT_DISTANCE);
		else if(value == AUD_DISTANCE_MODEL_EXPONENT_CLAMPED)
			alDistanceModel(AL_EXPONENT_DISTANCE_CLAMPED);
		else
			return false;
		return true;
	case AUD_3DS_DOPPLER_FACTOR:
		alDopplerFactor(value);
		return true;
	case AUD_3DS_SPEED_OF_SOUND:
		alSpeedOfSound(value);
		return true;
	default:
		return false;
	}
}

float AUD_OpenALDevice::getSetting(AUD_3DSetting setting)
{
	switch(setting)
	{
	case AUD_3DS_DISTANCE_MODEL:
		switch(alGetInteger(AL_DISTANCE_MODEL))
		{
			case AL_NONE:
				return AUD_DISTANCE_MODEL_NONE;
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
		}
	case AUD_3DS_DOPPLER_FACTOR:
		return alGetFloat(AL_DOPPLER_FACTOR);
	case AUD_3DS_SPEED_OF_SOUND:
		return alGetFloat(AL_SPEED_OF_SOUND);
	default:
		return std::numeric_limits<float>::quiet_NaN();
	}
}

bool AUD_OpenALDevice::updateSource(AUD_Handle* handle, AUD_3DData &data)
{
	bool result = false;

	lock();

	if(isValid(handle))
	{
		int source = ((AUD_OpenALHandle*)handle)->source;
		alSourcefv(source, AL_POSITION, (ALfloat*)data.position);
		alSourcefv(source, AL_VELOCITY, (ALfloat*)data.velocity);
		alSourcefv(source, AL_DIRECTION, (ALfloat*)&(data.orientation[3]));
		result = true;
	}

	unlock();

	return result;
}

bool AUD_OpenALDevice::setSourceSetting(AUD_Handle* handle,
										AUD_3DSourceSetting setting,
										float value)
{
	lock();

	bool result = false;

	if(isValid(handle))
	{
		int source = ((AUD_OpenALHandle*)handle)->source;

		switch(setting)
		{
		case AUD_3DSS_CONE_INNER_ANGLE:
			alSourcef(source, AL_CONE_INNER_ANGLE, value);
			result = true;
			break;
		case AUD_3DSS_CONE_OUTER_ANGLE:
			alSourcef(source, AL_CONE_OUTER_ANGLE, value);
			result = true;
			break;
		case AUD_3DSS_CONE_OUTER_GAIN:
			alSourcef(source, AL_CONE_OUTER_GAIN, value);
			result = true;
			break;
		case AUD_3DSS_IS_RELATIVE:
			alSourcei(source, AL_SOURCE_RELATIVE, value > 0.0f);
			result = true;
			break;
		case AUD_3DSS_MAX_DISTANCE:
			alSourcef(source, AL_MAX_DISTANCE, value);
			result = true;
			break;
		case AUD_3DSS_MAX_GAIN:
			alSourcef(source, AL_MAX_GAIN, value);
			result = true;
			break;
		case AUD_3DSS_MIN_GAIN:
			alSourcef(source, AL_MIN_GAIN, value);
			result = true;
			break;
		case AUD_3DSS_REFERENCE_DISTANCE:
			alSourcef(source, AL_REFERENCE_DISTANCE, value);
			result = true;
			break;
		case AUD_3DSS_ROLLOFF_FACTOR:
			alSourcef(source, AL_ROLLOFF_FACTOR, value);
			result = true;
			break;
		default:
			break;
		}
	}

	unlock();
	return result;
}

float AUD_OpenALDevice::getSourceSetting(AUD_Handle* handle,
										 AUD_3DSourceSetting setting)
{
	float result = std::numeric_limits<float>::quiet_NaN();;

	lock();

	if(isValid(handle))
	{
		int source = ((AUD_OpenALHandle*)handle)->source;

		switch(setting)
		{
		case AUD_3DSS_CONE_INNER_ANGLE:
			alGetSourcef(source, AL_CONE_INNER_ANGLE, &result);
			break;
		case AUD_3DSS_CONE_OUTER_ANGLE:
			alGetSourcef(source, AL_CONE_OUTER_ANGLE, &result);
			break;
		case AUD_3DSS_CONE_OUTER_GAIN:
			alGetSourcef(source, AL_CONE_OUTER_GAIN, &result);
			break;
		case AUD_3DSS_IS_RELATIVE:
			{
				ALint i;
				alGetSourcei(source, AL_SOURCE_RELATIVE, &i);
				result = i ? 1.0f : 0.0f;
				break;
			}
		case AUD_3DSS_MAX_DISTANCE:
			alGetSourcef(source, AL_MAX_DISTANCE, &result);
			break;
		case AUD_3DSS_MAX_GAIN:
			alGetSourcef(source, AL_MAX_GAIN, &result);
			break;
		case AUD_3DSS_MIN_GAIN:
			alGetSourcef(source, AL_MIN_GAIN, &result);
			break;
		case AUD_3DSS_REFERENCE_DISTANCE:
			alGetSourcef(source, AL_REFERENCE_DISTANCE, &result);
			break;
		case AUD_3DSS_ROLLOFF_FACTOR:
			alGetSourcef(source, AL_ROLLOFF_FACTOR, &result);
			break;
		default:
			break;
		}
	}

	unlock();
	return result;
}
