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

/** \file audaspace/intern/AUD_C-API.cpp
 *  \ingroup audaspaceintern
 */


// needed for INT64_C
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifdef WITH_PYTHON
#include "AUD_PyInit.h"
#include "AUD_PyAPI.h"

Device* g_device;
bool g_pyinitialized = false;
#endif

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
#include "AUD_IReader.h"
#include "AUD_SequencerFactory.h"
#include "AUD_SilenceFactory.h"

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
typedef AUD_Handle AUD_Channel;

#define AUD_CAPI_IMPLEMENTATION
#include "AUD_C-API.h"

#ifndef NULL
#define NULL 0
#endif

static AUD_IDevice* AUD_device = NULL;
static AUD_I3DDevice* AUD_3ddevice = NULL;

void AUD_initOnce()
{
#ifdef WITH_FFMPEG
	av_register_all();
#endif
}

int AUD_init(AUD_DeviceType device, AUD_DeviceSpecs specs, int buffersize)
{
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
			dev = new AUD_JackDevice("Blender", specs, buffersize);
			break;
#endif
		default:
			return false;
		}

		AUD_device = dev;
		AUD_3ddevice = dynamic_cast<AUD_I3DDevice*>(AUD_device);

#ifdef WITH_PYTHON
		if(g_pyinitialized)
		{
			g_device = (Device*)Device_empty();
			if(g_device != NULL)
			{
				g_device->device = dev;
			}
		}
#endif

		return true;
	}
	catch(AUD_Exception&)
	{
		return false;
	}
}

void AUD_exit()
{
#ifdef WITH_PYTHON
	if(g_device)
	{
		Py_XDECREF(g_device);
		g_device = NULL;
	}
	else
#endif
	if(AUD_device)
		delete AUD_device;
	AUD_device = NULL;
	AUD_3ddevice = NULL;
}

#ifdef WITH_PYTHON
static PyObject* AUD_getCDevice(PyObject* self)
{
	if(g_device)
	{
		Py_INCREF(g_device);
		return (PyObject*)g_device;
	}
	Py_RETURN_NONE;
}

static PyMethodDef meth_getcdevice[] = {{ "device", (PyCFunction)AUD_getCDevice, METH_NOARGS,
										  "device()\n\n"
										  "Returns the application's :class:`Device`.\n\n"
										  ":return: The application's :class:`Device`.\n"
										  ":rtype: :class:`Device`"}};

PyObject* AUD_initPython()
{
	PyObject* module = PyInit_aud();
	PyModule_AddObject(module, "device", (PyObject *)PyCFunction_New(meth_getcdevice, NULL));
	PyDict_SetItemString(PyImport_GetModuleDict(), "aud", module);
	if(AUD_device)
	{
		g_device = (Device*)Device_empty();
		if(g_device != NULL)
		{
			g_device->device = AUD_device;
		}
	}
	g_pyinitialized = true;

	return module;
}
#endif

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

	AUD_SoundInfo info;
	info.specs.channels = AUD_CHANNELS_INVALID;
	info.specs.rate = AUD_RATE_INVALID;
	info.length = 0.0f;

	try
	{
		AUD_IReader* reader = sound->createReader();

		if(reader)
		{
			info.specs = reader->getSpecs();
			info.length = reader->getLength() / (float) info.specs.rate;
			delete reader;
		}
	}
	catch(AUD_Exception&)
	{
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
	catch(AUD_Exception&)
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
	catch(AUD_Exception&)
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
	catch(AUD_Exception&)
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
	catch(AUD_Exception&)
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
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

int AUD_setLoop(AUD_Channel* handle, int loops)
{
	if(handle)
	{
		try
		{
			return AUD_device->setLoopCount(handle, loops);
		}
		catch(AUD_Exception&)
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
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

void AUD_unload(AUD_Sound* sound)
{
	assert(sound);
	delete sound;
}

AUD_Channel* AUD_play(AUD_Sound* sound, int keep)
{
	assert(AUD_device);
	assert(sound);
	try
	{
		return AUD_device->play(sound, keep);
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

int AUD_pause(AUD_Channel* handle)
{
	assert(AUD_device);
	return AUD_device->pause(handle);
}

int AUD_resume(AUD_Channel* handle)
{
	assert(AUD_device);
	return AUD_device->resume(handle);
}

int AUD_stop(AUD_Channel* handle)
{
	if(AUD_device)
		return AUD_device->stop(handle);
	return false;
}

int AUD_setKeep(AUD_Channel* handle, int keep)
{
	assert(AUD_device);
	return AUD_device->setKeep(handle, keep);
}

int AUD_seek(AUD_Channel* handle, float seekTo)
{
	assert(AUD_device);
	return AUD_device->seek(handle, seekTo);
}

float AUD_getPosition(AUD_Channel* handle)
{
	assert(AUD_device);
	return AUD_device->getPosition(handle);
}

AUD_Status AUD_getStatus(AUD_Channel* handle)
{
	assert(AUD_device);
	return AUD_device->getStatus(handle);
}

int AUD_setListenerLocation(const float* location)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		AUD_Vector3 v(location[0], location[1], location[2]);
		AUD_3ddevice->setListenerLocation(v);
		return true;
	}

	return false;
}

int AUD_setListenerVelocity(const float* velocity)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		AUD_Vector3 v(velocity[0], velocity[1], velocity[2]);
		AUD_3ddevice->setListenerVelocity(v);
		return true;
	}

	return false;
}

int AUD_setListenerOrientation(const float* orientation)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		AUD_Quaternion q(orientation[3], orientation[0], orientation[1], orientation[2]);
		AUD_3ddevice->setListenerOrientation(q);
		return true;
	}

	return false;
}

int AUD_setSpeedOfSound(float speed)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		AUD_3ddevice->setSpeedOfSound(speed);
		return true;
	}

	return false;
}

int AUD_setDopplerFactor(float factor)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		AUD_3ddevice->setDopplerFactor(factor);
		return true;
	}

	return false;
}

int AUD_setDistanceModel(AUD_DistanceModel model)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		AUD_3ddevice->setDistanceModel(model);
		return true;
	}

	return false;
}

int AUD_setSourceLocation(AUD_Channel* handle, const float* location)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		AUD_Vector3 v(location[0], location[1], location[2]);
		return AUD_3ddevice->setSourceLocation(handle, v);
	}

	return false;
}

int AUD_setSourceVelocity(AUD_Channel* handle, const float* velocity)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		AUD_Vector3 v(velocity[0], velocity[1], velocity[2]);
		return AUD_3ddevice->setSourceVelocity(handle, v);
	}

	return false;
}

int AUD_setSourceOrientation(AUD_Channel* handle, const float* orientation)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		AUD_Quaternion q(orientation[3], orientation[0], orientation[1], orientation[2]);
		return AUD_3ddevice->setSourceOrientation(handle, q);
	}

	return false;
}

int AUD_setRelative(AUD_Channel* handle, int relative)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		return AUD_3ddevice->setRelative(handle, relative);
	}

	return false;
}

int AUD_setVolumeMaximum(AUD_Channel* handle, float volume)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		return AUD_3ddevice->setVolumeMaximum(handle, volume);
	}

	return false;
}

int AUD_setVolumeMinimum(AUD_Channel* handle, float volume)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		return AUD_3ddevice->setVolumeMinimum(handle, volume);
	}

	return false;
}

int AUD_setDistanceMaximum(AUD_Channel* handle, float distance)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		return AUD_3ddevice->setDistanceMaximum(handle, distance);
	}

	return false;
}

int AUD_setDistanceReference(AUD_Channel* handle, float distance)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		return AUD_3ddevice->setDistanceReference(handle, distance);
	}

	return false;
}

int AUD_setAttenuation(AUD_Channel* handle, float factor)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		return AUD_3ddevice->setAttenuation(handle, factor);
	}

	return false;
}

int AUD_setConeAngleOuter(AUD_Channel* handle, float angle)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		return AUD_3ddevice->setConeAngleOuter(handle, angle);
	}

	return false;
}

int AUD_setConeAngleInner(AUD_Channel* handle, float angle)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		return AUD_3ddevice->setConeAngleInner(handle, angle);
	}

	return false;
}

int AUD_setConeVolumeOuter(AUD_Channel* handle, float volume)
{
	assert(AUD_device);

	if(AUD_3ddevice)
	{
		return AUD_3ddevice->setConeVolumeOuter(handle, volume);
	}

	return false;
}

int AUD_setSoundVolume(AUD_Channel* handle, float volume)
{
	if(handle)
	{
		assert(AUD_device);

		try
		{
			return AUD_device->setVolume(handle, volume);
		}
		catch(AUD_Exception&) {}
	}
	return false;
}

int AUD_setSoundPitch(AUD_Channel* handle, float pitch)
{
	if(handle)
	{
		assert(AUD_device);

		try
		{
			return AUD_device->setPitch(handle, pitch);
		}
		catch(AUD_Exception&) {}
	}
	return false;
}

AUD_Device* AUD_openReadDevice(AUD_DeviceSpecs specs)
{
	try
	{
		return new AUD_ReadDevice(specs);
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

AUD_Channel* AUD_playDevice(AUD_Device* device, AUD_Sound* sound, float seek)
{
	assert(device);
	assert(sound);

	try
	{
		AUD_Channel* handle = device->play(sound);
		device->seek(handle, seek);
		return handle;
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

int AUD_setDeviceVolume(AUD_Device* device, float volume)
{
	assert(device);

	try
	{
		device->setVolume(volume);
		return true;
	}
	catch(AUD_Exception&) {}

	return false;
}

int AUD_setDeviceSoundVolume(AUD_Device* device, AUD_Channel* handle,
							 float volume)
{
	if(handle)
	{
		assert(device);

		try
		{
			return device->setVolume(handle, volume);
		}
		catch(AUD_Exception&) {}
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
	catch(AUD_Exception&)
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
	catch(AUD_Exception&)
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

	AUD_IReader* reader = file.createReader();
	AUD_SampleRate rate = reader->getSpecs().rate;
	delete reader;

	AUD_ChannelMapperFactory mapper(&file, specs);
	sound = &mapper;
	AUD_LowpassFactory lowpass(sound, high);
	if(high < rate)
		sound = &lowpass;
	AUD_HighpassFactory highpass(sound, low);
	if(low > 0)
		sound = &highpass;
	AUD_EnvelopeFactory envelope(sound, attack, release, threshold, 0.1f);
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

	reader = sound->createReader();

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

static void pauseSound(AUD_Channel* handle)
{
	assert(AUD_device);

	AUD_device->pause(handle);
}

AUD_Channel* AUD_pauseAfter(AUD_Channel* handle, float seconds)
{
	assert(AUD_device);

	AUD_SilenceFactory silence;
	AUD_LimiterFactory limiter(&silence, 0, seconds);

	try
	{
		AUD_Channel* channel = AUD_device->play(&limiter);
		AUD_device->setStopCallback(channel, (stopCallback)pauseSound, handle);
		return channel;
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

AUD_Sound* AUD_createSequencer(int muted, void* data, AUD_volumeFunction volume)
{
/* AUD_XXX should be this: but AUD_createSequencer is called before the device
 * is initialized.

	return new AUD_SequencerFactory(AUD_device->getSpecs().specs, data, volume);
*/
	AUD_Specs specs;
	specs.channels = AUD_CHANNELS_STEREO;
	specs.rate = AUD_RATE_44100;
	return new AUD_SequencerFactory(specs, muted, data, volume);
}

void AUD_destroySequencer(AUD_Sound* sequencer)
{
	delete ((AUD_SequencerFactory*)sequencer);
}

void AUD_setSequencerMuted(AUD_Sound* sequencer, int muted)
{
	((AUD_SequencerFactory*)sequencer)->mute(muted);
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
	AUD_DeviceSpecs specs;
	sample_t* buf;

	specs.rate = AUD_RATE_INVALID;
	specs.channels = AUD_CHANNELS_MONO;
	specs.format = AUD_FORMAT_INVALID;

	AUD_ChannelMapperFactory mapper(sound, specs);

	AUD_IReader* reader = mapper.createReader();

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

	delete reader;

	return length;
}

void AUD_startPlayback()
{
#ifdef WITH_JACK
	AUD_JackDevice* device = dynamic_cast<AUD_JackDevice*>(AUD_device);
	if(device)
		device->startPlayback();
#endif
}

void AUD_stopPlayback()
{
#ifdef WITH_JACK
	AUD_JackDevice* device = dynamic_cast<AUD_JackDevice*>(AUD_device);
	if(device)
		device->stopPlayback();
#endif
}

void AUD_seekSequencer(AUD_Channel* handle, float time)
{
#ifdef WITH_JACK
	AUD_JackDevice* device = dynamic_cast<AUD_JackDevice*>(AUD_device);
	if(device)
		device->seekPlayback(time);
	else
#endif
	{
		AUD_device->seek(handle, time);
	}
}

float AUD_getSequencerPosition(AUD_Channel* handle)
{
#ifdef WITH_JACK
	AUD_JackDevice* device = dynamic_cast<AUD_JackDevice*>(AUD_device);
	if(device)
		return device->getPlaybackPosition();
	else
#endif
	{
		return AUD_device->getPosition(handle);
	}
}

#ifdef WITH_JACK
void AUD_setSyncCallback(AUD_syncFunction function, void* data)
{
	AUD_JackDevice* device = dynamic_cast<AUD_JackDevice*>(AUD_device);
	if(device)
		device->setSyncCallback(function, data);
}
#endif

int AUD_doesPlayback()
{
#ifdef WITH_JACK
	AUD_JackDevice* device = dynamic_cast<AUD_JackDevice*>(AUD_device);
	if(device)
		return device->doesPlayback();
#endif
	return -1;
}
