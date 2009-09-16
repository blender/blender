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

#include "AUD_NULLDevice.h"
#include "AUD_I3DDevice.h"
#include "AUD_FileFactory.h"
#include "AUD_StreamBufferFactory.h"
#include "AUD_DelayFactory.h"
#include "AUD_LimiterFactory.h"
#include "AUD_PingPongFactory.h"
#include "AUD_LoopFactory.h"
#include "AUD_ReadDevice.h"
#include "AUD_SourceCaps.h"
#include "AUD_IReader.h"

#ifdef WITH_SDL
#include "AUD_SDLDevice.h"
#include "AUD_FloatMixer.h"
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

#include <assert.h>

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

int AUD_init(AUD_DeviceType device, AUD_Specs specs, int buffersize)
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
			{
				dev = new AUD_SDLDevice(specs, buffersize);
				AUD_FloatMixer* mixer = new AUD_FloatMixer();
				((AUD_SDLDevice*)dev)->setMixer(mixer);
				break;
			}
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
		info.specs.format = AUD_FORMAT_INVALID;
		info.specs.rate = AUD_RATE_INVALID;
		info.length = 0.0;
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

extern AUD_Sound* AUD_limitSound(AUD_Sound* sound, float start, float end)
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

int AUD_stopLoop(AUD_Handle* handle)
{
	if(handle)
	{
		AUD_Message message;
		message.type = AUD_MSG_LOOP;
		message.loopcount = 0;

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
	return 0.0;
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
	return 0.0;
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

AUD_Device* AUD_openReadDevice(AUD_Specs specs)
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

int AUD_playDevice(AUD_Device* device, AUD_Sound* sound)
{
	assert(device);
	assert(sound);

	try
	{
		return device->play(sound) != NULL;
	}
	catch(AUD_Exception)
	{
		return false;
	}
}

int AUD_readDevice(AUD_Device* device, sample_t* buffer, int length)
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
