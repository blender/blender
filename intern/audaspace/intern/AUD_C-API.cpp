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

#include <cstdlib>
#include <cstring>
#include <cmath>

#include "AUD_NULLDevice.h"
#include "AUD_I3DDevice.h"
#include "AUD_FileFactory.h"
#include "AUD_StreamBufferFactory.h"
#include "AUD_DelayFactory.h"
#include "AUD_LimiterFactory.h"
#include "AUD_PingPongFactory.h"
#include "AUD_LoopFactory.h"
#include "AUD_RectifyFactory.h"
#include "AUD_EnvelopeFactory.h"
#include "AUD_LinearResampleFactory.h"
#include "AUD_LowpassFactory.h"
#include "AUD_HighpassFactory.h"
#include "AUD_AccumulatorFactory.h"
#include "AUD_SumFactory.h"
#include "AUD_SquareFactory.h"
#include "AUD_ChannelMapperFactory.h"
#include "AUD_Buffer.h"
#include "AUD_ReadDevice.h"
#include "AUD_SourceCaps.h"
#include "AUD_IReader.h"
#include "AUD_SequencerFactory.h"

#ifdef WITH_SDL
#include "AUD_SDLDevice.h"
#endif

#ifdef WITH_OPENAL
#include "AUD_OpenALDevice.h"
#endif

#ifdef WITH_JACK
#include "AUD_JackDevice.h"
#endif

#ifdef WITH_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
}
#endif

#include <cassert>

typedef AUD_IFactory AUD_Sound;
typedef AUD_ReadDevice AUD_Device;

#define AUD_CAPI_IMPLEMENTATION
#include "AUD_C-API.h"

#ifndef NULL
#define NULL 0
#endif

static AUD_IDevice* AUD_device = NULL;
static int AUD_available_devices[4];
static AUD_I3DDevice* AUD_3ddevice = NULL;

int AUD_init(AUD_DeviceType device, AUD_DeviceSpecs specs, int buffersize)
{
#ifdef WITH_FFMPEG
	av_register_all();
#endif
	AUD_IDevice* dev = NULL;

	if(AUD_device)
		AUD_exit();

	try
	{
		switch(device)
		{
		case AUD_NULL_DEVICE:
			dev = new AUD_NULLDevice();
			break;
#ifdef WITH_SDL
		case AUD_SDL_DEVICE:
			dev = new AUD_SDLDevice(specs, buffersize);
			break;
#endif
#ifdef WITH_OPENAL
		case AUD_OPENAL_DEVICE:
			dev = new AUD_OpenALDevice(specs, buffersize);
			break;
#endif
#ifdef WITH_JACK
		case AUD_JACK_DEVICE:
			dev = new AUD_JackDevice(specs);
			break;
#endif
		default:
			return false;
		}

		AUD_device = dev;
		if(AUD_device->checkCapability(AUD_CAPS_3D_DEVICE))
			AUD_3ddevice = dynamic_cast<AUD_I3DDevice*>(AUD_device);

		return true;
	}
	catch(AUD_Exception)
	{
		return false;
	}
}

int* AUD_enumDevices()
{
	int i = 0;
#ifdef WITH_SDL
	AUD_available_devices[i++] = AUD_SDL_DEVICE;
#endif
#ifdef WITH_OPENAL
	AUD_available_devices[i++] = AUD_OPENAL_DEVICE;
#endif
#ifdef WITH_JACK
	AUD_available_devices[i++] = AUD_JACK_DEVICE;
#endif
	AUD_available_devices[i++] = AUD_NULL_DEVICE;
	return AUD_available_devices;
}

void AUD_exit()
{
	if(AUD_device)
	{
		delete AUD_device;
		AUD_device = NULL;
		AUD_3ddevice = NULL;
	}
}

void AUD_lock()
{
	assert(AUD_device);
	AUD_device->lock();
}

void AUD_unlock()
{
	assert(AUD_device);
	AUD_device->unlock();
}

AUD_SoundInfo AUD_getInfo(AUD_Sound* sound)
{
	assert(sound);

	AUD_IReader* reader = sound->createReader();

	AUD_SoundInfo info;

	if(reader)
	{
		info.specs = reader->getSpecs();
		info.length = reader->getLength() / (float) info.specs.rate;
	}
	else
	{
		info.specs.channels = AUD_CHANNELS_INVALID;
		info.specs.rate = AUD_RATE_INVALID;
		info.length = 0.0f;
	}

	return info;
}

AUD_Sound* AUD_load(const char* filename)
{
	assert(filename);
	return new AUD_FileFactory(filename);
}

AUD_Sound* AUD_loadBuffer(unsigned char* buffer, int size)
{
	assert(buffer);
	return new AUD_FileFactory(buffer, size);
}

AUD_Sound* AUD_bufferSound(AUD_Sound* sound)
{
	assert(sound);

	try
	{
		return new AUD_StreamBufferFactory(sound);
	}
	catch(AUD_Exception)
	{
		return NULL;
	}
}

AUD_Sound* AUD_delaySound(AUD_Sound* sound, float delay)
{
	assert(sound);

	try
	{
		return new AUD_DelayFactory(sound, delay);
	}
	catch(AUD_Exception)
	{
		return NULL;
	}
}

AUD_Sound* AUD_limitSound(AUD_Sound* sound, float start, float end)
{
	assert(sound);

	try
	{
		return new AUD_LimiterFactory(sound, start, end);
	}
	catch(AUD_Exception)
	{
		return NULL;
	}
}

AUD_Sound* AUD_pingpongSound(AUD_Sound* sound)
{
	assert(sound);

	try
	{
		return new AUD_PingPongFactory(sound);
	}
	catch(AUD_Exception)
	{
		return NULL;
	}
}

AUD_Sound* AUD_loopSound(AUD_Sound* sound)
{
	assert(sound);

	try
	{
		return new AUD_LoopFactory(sound);
	}
	catch(AUD_Exception)
	{
		return NULL;
	}
}

int AUD_setLoop(AUD_Handle* handle, int loops, float time)
{
	if(handle)
	{
		AUD_Message message;
		message.type = AUD_MSG_LOOP;
		message.loopcount = loops;
		message.time = time;

		try
		{
			return AUD_device->sendMessage(handle, message);
		}
		catch(AUD_Exception)
		{
		}
	}
	return false;
}

AUD_Sound* AUD_rectifySound(AUD_Sound* sound)
{
	assert(sound);

	try
	{
		return new AUD_RectifyFactory(sound);
	}
	catch(AUD_Exception)
	{
		return NULL;
	}
}

void AUD_unload(AUD_Sound* sound)
{
	assert(sound);
	delete sound;
}

AUD_Handle* AUD_play(AUD_Sound* sound, int keep)
{
	assert(AUD_device);
	assert(sound);
	try
	{
		return AUD_device->play(sound, keep);
	}
	catch(AUD_Exception)
	{
		return NULL;
	}
}

int AUD_pause(AUD_Handle* handle)
{
	assert(AUD_device);
	return AUD_device->pause(handle);
}

int AUD_resume(AUD_Handle* handle)
{
	assert(AUD_device);
	return AUD_device->resume(handle);
}

int AUD_stop(AUD_Handle* handle)
{
	if(AUD_device)
		return AUD_device->stop(handle);
	return false;
}

int AUD_setKeep(AUD_Handle* handle, int keep)
{
	assert(AUD_device);
	return AUD_device->setKeep(handle, keep);
}

int AUD_seek(AUD_Handle* handle, float seekTo)
{
	assert(AUD_device);
	return AUD_device->seek(handle, seekTo);
}

float AUD_getPosition(AUD_Handle* handle)
{
	assert(AUD_device);
	return AUD_device->getPosition(handle);
}

AUD_Status AUD_getStatus(AUD_Handle* handle)
{
	assert(AUD_device);
	return AUD_device->getStatus(handle);
}

AUD_Handle* AUD_play3D(AUD_Sound* sound, int keep)
{
	assert(AUD_device);
	assert(sound);

	try
	{
		if(AUD_3ddevice)
			return AUD_3ddevice->play3D(sound, keep);
		else
			return AUD_device->play(sound, keep);
	}
	catch(AUD_Exception)
	{
		return NULL;
	}
}

int AUD_updateListener(AUD_3DData* data)
{
	assert(AUD_device);
	assert(data);

	try
	{
		if(AUD_3ddevice)
			return AUD_3ddevice->updateListener(*data);
	}
	catch(AUD_Exception)
	{
	}
	return false;
}

int AUD_set3DSetting(AUD_3DSetting setting, float value)
{
	assert(AUD_device);

	try
	{
		if(AUD_3ddevice)
			return AUD_3ddevice->setSetting(setting, value);
	}
	catch(AUD_Exception)
	{
	}
	return false;
}

float AUD_get3DSetting(AUD_3DSetting setting)
{
	assert(AUD_device);

	try
	{
		if(AUD_3ddevice)
			return AUD_3ddevice->getSetting(setting);
	}
	catch(AUD_Exception)
	{
	}
	return 0.0f;
}

int AUD_update3DSource(AUD_Handle* handle, AUD_3DData* data)
{
	if(handle)
	{
		assert(AUD_device);
		assert(data);

		try
		{
			if(AUD_3ddevice)
				return AUD_3ddevice->updateSource(handle, *data);
		}
		catch(AUD_Exception)
		{
		}
	}
	return false;
}

int AUD_set3DSourceSetting(AUD_Handle* handle,
						   AUD_3DSourceSetting setting, float value)
{
	if(handle)
	{
		assert(AUD_device);

		try
		{
			if(AUD_3ddevice)
				return AUD_3ddevice->setSourceSetting(handle, setting, value);
		}
		catch(AUD_Exception)
		{
		}
	}
	return false;
}

float AUD_get3DSourceSetting(AUD_Handle* handle, AUD_3DSourceSetting setting)
{
	if(handle)
	{
		assert(AUD_device);

		try
		{
			if(AUD_3ddevice)
				return AUD_3ddevice->getSourceSetting(handle, setting);
		}
		catch(AUD_Exception)
		{
		}
	}
	return 0.0f;
}

int AUD_setSoundVolume(AUD_Handle* handle, float volume)
{
	if(handle)
	{
		assert(AUD_device);
		AUD_SourceCaps caps;
		caps.handle = handle;
		caps.value = volume;

		try
		{
			return AUD_device->setCapability(AUD_CAPS_SOURCE_VOLUME, &caps);
		}
		catch(AUD_Exception) {}
	}
	return false;
}

int AUD_setSoundPitch(AUD_Handle* handle, float pitch)
{
	if(handle)
	{
		assert(AUD_device);
		AUD_SourceCaps caps;
		caps.handle = handle;
		caps.value = pitch;

		try
		{
			return AUD_device->setCapability(AUD_CAPS_SOURCE_PITCH, &caps);
		}
		catch(AUD_Exception) {}
	}
	return false;
}

AUD_Device* AUD_openReadDevice(AUD_DeviceSpecs specs)
{
	try
	{
		return new AUD_ReadDevice(specs);
	}
	catch(AUD_Exception)
	{
		return NULL;
	}
}

AUD_Handle* AUD_playDevice(AUD_Device* device, AUD_Sound* sound, float seek)
{
	assert(device);
	assert(sound);

	try
	{
		AUD_Handle* handle = device->play(sound);
		device->seek(handle, seek);
		return handle;
	}
	catch(AUD_Exception)
	{
		return NULL;
	}
}

int AUD_setDeviceVolume(AUD_Device* device, float volume)
{
	assert(device);

	try
	{
		return device->setCapability(AUD_CAPS_VOLUME, &volume);
	}
	catch(AUD_Exception) {}
	
	return false;
}

int AUD_setDeviceSoundVolume(AUD_Device* device, AUD_Handle* handle,
							 float volume)
{
	if(handle)
	{
		assert(device);
		AUD_SourceCaps caps;
		caps.handle = handle;
		caps.value = volume;

		try
		{
			return device->setCapability(AUD_CAPS_SOURCE_VOLUME, &caps);
		}
		catch(AUD_Exception) {}
	}
	return false;
}

int AUD_readDevice(AUD_Device* device, data_t* buffer, int length)
{
	assert(device);
	assert(buffer);

	try
	{
		return device->read(buffer, length);
	}
	catch(AUD_Exception)
	{
		return false;
	}
}

void AUD_closeReadDevice(AUD_Device* device)
{
	assert(device);

	try
	{
		delete device;
	}
	catch(AUD_Exception)
	{
	}
}

float* AUD_readSoundBuffer(const char* filename, float low, float high,
						   float attack, float release, float threshold,
						   int accumulate, int additive, int square,
						   float sthreshold, int samplerate, int* length)
{
	AUD_Buffer buffer;
	AUD_DeviceSpecs specs;
	specs.channels = AUD_CHANNELS_MONO;
	specs.rate = (AUD_SampleRate)samplerate;
	AUD_Sound* sound;

	AUD_FileFactory file(filename);
	AUD_ChannelMapperFactory mapper(&file, specs);
	AUD_LowpassFactory lowpass(&mapper, high);
	AUD_HighpassFactory highpass(&lowpass, low);
	AUD_EnvelopeFactory envelope(&highpass, attack, release, threshold, 0.1f);
	AUD_LinearResampleFactory resampler(&envelope, specs);
	sound = &resampler;
	AUD_SquareFactory squaref(sound, sthreshold);
	if(square)
		sound = &squaref;
	AUD_AccumulatorFactory accumulator(sound, additive);
	AUD_SumFactory sum(sound);
	if(accumulate)
		sound = &accumulator;
	else if(additive)
		sound = &sum;

	AUD_IReader* reader = sound->createReader();

	if(reader == NULL)
		return NULL;

	int len;
	int position = 0;
	sample_t* readbuffer;
	do
	{
		len = samplerate;
		buffer.resize((position + len) * sizeof(float), true);
		reader->read(len, readbuffer);
		memcpy(buffer.getBuffer() + position, readbuffer, len * sizeof(float));
		position += len;
	} while(len != 0);
	delete reader;

	float* result = (float*)malloc(position * sizeof(float));
	memcpy(result, buffer.getBuffer(), position * sizeof(float));
	*length = position;
	return result;
}

AUD_Sound* AUD_createSequencer(void* data, AUD_volumeFunction volume)
{
	if(AUD_device)
	{
		return new AUD_SequencerFactory(AUD_device->getSpecs().specs, data, volume);
	}
	else
	{
		AUD_Specs specs;
		specs.channels = AUD_CHANNELS_STEREO;
		specs.rate = AUD_RATE_44100;
		return new AUD_SequencerFactory(specs, data, volume);
	}
}

void AUD_destroySequencer(AUD_Sound* sequencer)
{
	delete ((AUD_SequencerFactory*)sequencer);
}

AUD_SequencerEntry* AUD_addSequencer(AUD_Sound** sequencer, AUD_Sound* sound,
								 float begin, float end, float skip, void* data)
{
	return ((AUD_SequencerFactory*)sequencer)->add((AUD_IFactory**) sound, begin, end, skip, data);
}

void AUD_removeSequencer(AUD_Sound* sequencer, AUD_SequencerEntry* entry)
{
	((AUD_SequencerFactory*)sequencer)->remove(entry);
}

void AUD_moveSequencer(AUD_Sound* sequencer, AUD_SequencerEntry* entry,
				   float begin, float end, float skip)
{
	((AUD_SequencerFactory*)sequencer)->move(entry, begin, end, skip);
}

void AUD_muteSequencer(AUD_Sound* sequencer, AUD_SequencerEntry* entry, char mute)
{
	((AUD_SequencerFactory*)sequencer)->mute(entry, mute);
}

int AUD_readSound(AUD_Sound* sound, sample_t* buffer, int length)
{
	AUD_IReader* reader = sound->createReader();
	AUD_DeviceSpecs specs;
	sample_t* buf;

	specs.specs = reader->getSpecs();
	specs.channels = AUD_CHANNELS_MONO;
	specs.format = AUD_FORMAT_FLOAT32;

	AUD_ChannelMapperFactory mapper(reader, specs);

	if(!reader || reader->getType() != AUD_TYPE_BUFFER)
		return -1;

	reader = mapper.createReader();

	if(!reader)
		return -1;

	int len = reader->getLength();
	float samplejump = (float)len / (float)length;
	float min, max;

	for(int i = 0; i < length; i++)
	{
		len = floor(samplejump * (i+1)) - floor(samplejump * i);
		reader->read(len, buf);

		if(len < 1)
		{
			length = i;
			break;
		}

		max = min = *buf;
		for(int j = 1; j < len; j++)
		{
			if(buf[j] < min)
				min = buf[j];
			if(buf[j] > max)
				max = buf[j];
			buffer[i * 2] = min;
			buffer[i * 2 + 1] = max;
		}
	}

	delete reader; AUD_DELETE("reader")

	return length;
}
