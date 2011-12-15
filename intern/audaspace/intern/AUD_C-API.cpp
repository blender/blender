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
#endif

#include <set>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include "AUD_NULLDevice.h"
#include "AUD_I3DDevice.h"
#include "AUD_I3DHandle.h"
#include "AUD_FileFactory.h"
#include "AUD_FileWriter.h"
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
#include "AUD_SequencerEntry.h"
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

typedef AUD_Reference<AUD_IFactory> AUD_Sound;
typedef AUD_Reference<AUD_ReadDevice> AUD_Device;
typedef AUD_Reference<AUD_IHandle> AUD_Handle;
typedef AUD_Reference<AUD_SequencerEntry> AUD_SEntry;

#define AUD_CAPI_IMPLEMENTATION
#include "AUD_C-API.h"

#ifndef NULL
#define NULL 0
#endif

static AUD_Reference<AUD_IDevice> AUD_device;
static AUD_I3DDevice* AUD_3ddevice;

void AUD_initOnce()
{
#ifdef WITH_FFMPEG
	av_register_all();
#endif
}

int AUD_init(AUD_DeviceType device, AUD_DeviceSpecs specs, int buffersize)
{
	AUD_Reference<AUD_IDevice> dev;

	if(!AUD_device.isNull())
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
#ifdef __APPLE__
			struct stat st;
			if(stat("/Library/Frameworks/Jackmp.framework", &st) != 0)
			{
				printf("Warning: Jack Framework not installed\n");
				// No break, fall through to default, to return false
			}
			else
			{
#endif
				dev = new AUD_JackDevice("Blender", specs, buffersize);
				break;
#ifdef __APPLE__
			}
#endif
#endif
		default:
			return false;
		}

		AUD_device = dev;
		AUD_3ddevice = dynamic_cast<AUD_I3DDevice*>(AUD_device.get());

		return true;
	}
	catch(AUD_Exception&)
	{
		return false;
	}
}

void AUD_exit()
{
	AUD_device = AUD_Reference<AUD_IDevice>();
	AUD_3ddevice = NULL;
}

#ifdef WITH_PYTHON
static PyObject* AUD_getCDevice(PyObject* self)
{
	if(!AUD_device.isNull())
	{
		Device* device = (Device*)Device_empty();
		if(device != NULL)
		{
			device->device = new AUD_Reference<AUD_IDevice>(AUD_device);
			return (PyObject*)device;
		}
	}

	Py_RETURN_NONE;
}

static PyMethodDef meth_getcdevice[] = {{ "device", (PyCFunction)AUD_getCDevice, METH_NOARGS,
										  "device()\n\n"
										  "Returns the application's :class:`Device`.\n\n"
										  ":return: The application's :class:`Device`.\n"
										  ":rtype: :class:`Device`"}};

extern "C" {
extern void* sound_get_factory(void* sound);
}

static PyObject* AUD_getSoundFromPointer(PyObject* self, PyObject* args)
{
	long int lptr;

	if(PyArg_Parse(args, "l:_sound_from_pointer", &lptr))
	{
		if(lptr)
		{
			AUD_Reference<AUD_IFactory>* factory = (AUD_Reference<AUD_IFactory>*) sound_get_factory((void*) lptr);

			if(factory)
			{
				Factory* obj = (Factory*) Factory_empty();
				if(obj)
				{
					obj->factory = new AUD_Reference<AUD_IFactory>(*factory);
					return (PyObject*) obj;
				}
			}
		}
	}

	Py_RETURN_NONE;
}

static PyMethodDef meth_sound_from_pointer[] = {{ "_sound_from_pointer", (PyCFunction)AUD_getSoundFromPointer, METH_O,
										  "_sound_from_pointer(pointer)\n\n"
										  "Returns the corresponding :class:`Factory` object.\n\n"
										  ":arg pointer: The pointer to the bSound object as long.\n"
										  ":type pointer: long\n"
										  ":return: The corresponding :class:`Factory` object.\n"
										  ":rtype: :class:`Factory`"}};

PyObject* AUD_initPython()
{
	PyObject* module = PyInit_aud();
	PyModule_AddObject(module, "device", (PyObject*)PyCFunction_New(meth_getcdevice, NULL));
	PyModule_AddObject(module, "_sound_from_pointer", (PyObject*)PyCFunction_New(meth_sound_from_pointer, NULL));
	PyDict_SetItemString(PyImport_GetModuleDict(), "aud", module);

	return module;
}

PyObject* AUD_getPythonFactory(AUD_Sound* sound)
{
	if(sound)
	{
		Factory* obj = (Factory*) Factory_empty();
		if(obj)
		{
			obj->factory = new AUD_Reference<AUD_IFactory>(*sound);
			return (PyObject*) obj;
		}
	}

	return NULL;
}

AUD_Sound* AUD_getPythonSound(PyObject* sound)
{
	Factory* factory = checkFactory(sound);

	if(!factory)
		return NULL;

	return new AUD_Reference<AUD_IFactory>(*reinterpret_cast<AUD_Reference<AUD_IFactory>*>(factory->factory));
}

#endif

void AUD_lock()
{
	AUD_device->lock();
}

void AUD_unlock()
{
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
		AUD_Reference<AUD_IReader> reader = (*sound)->createReader();

		if(!reader.isNull())
		{
			info.specs = reader->getSpecs();
			info.length = reader->getLength() / (float) info.specs.rate;
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
	return new AUD_Sound(new AUD_FileFactory(filename));
}

AUD_Sound* AUD_loadBuffer(unsigned char* buffer, int size)
{
	assert(buffer);
	return new AUD_Sound(new AUD_FileFactory(buffer, size));
}

AUD_Sound* AUD_bufferSound(AUD_Sound* sound)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new AUD_StreamBufferFactory(*sound));
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

AUD_Sound* AUD_monoSound(AUD_Sound* sound)
{
	assert(sound);

	try
	{
		AUD_DeviceSpecs specs;
		specs.channels = AUD_CHANNELS_MONO;
		specs.rate = AUD_RATE_INVALID;
		specs.format = AUD_FORMAT_INVALID;
		return new AUD_Sound(new AUD_ChannelMapperFactory(*sound, specs));
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
		return new AUD_Sound(new AUD_DelayFactory(*sound, delay));
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
		return new AUD_Sound(new AUD_LimiterFactory(*sound, start, end));
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
		return new AUD_Sound(new AUD_PingPongFactory(*sound));
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
		return new AUD_Sound(new AUD_LoopFactory(*sound));
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

int AUD_setLoop(AUD_Handle* handle, int loops)
{
	assert(handle);

	try
	{
		return (*handle)->setLoopCount(loops);
	}
	catch(AUD_Exception&)
	{
	}

	return false;
}

AUD_Sound* AUD_rectifySound(AUD_Sound* sound)
{
	assert(sound);

	try
	{
		return new AUD_Sound(new AUD_RectifyFactory(*sound));
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

AUD_Handle* AUD_play(AUD_Sound* sound, int keep)
{
	assert(sound);
	try
	{
		AUD_Handle handle = AUD_device->play(*sound, keep);
		if(!handle.isNull())
			return new AUD_Handle(handle);
	}
	catch(AUD_Exception&)
	{
	}
	return NULL;
}

int AUD_pause(AUD_Handle* handle)
{
	assert(handle);
	return (*handle)->pause();
}

int AUD_resume(AUD_Handle* handle)
{
	assert(handle);
	return (*handle)->resume();
}

int AUD_stop(AUD_Handle* handle)
{
	assert(handle);
	int result = (*handle)->stop();
	delete handle;
	return result;
}

int AUD_setKeep(AUD_Handle* handle, int keep)
{
	assert(handle);
	return (*handle)->setKeep(keep);
}

int AUD_seek(AUD_Handle* handle, float seekTo)
{
	assert(handle);
	return (*handle)->seek(seekTo);
}

float AUD_getPosition(AUD_Handle* handle)
{
	assert(handle);
	return (*handle)->getPosition();
}

AUD_Status AUD_getStatus(AUD_Handle* handle)
{
	assert(handle);
	return (*handle)->getStatus();
}

int AUD_setListenerLocation(const float* location)
{
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
	if(AUD_3ddevice)
	{
		AUD_3ddevice->setSpeedOfSound(speed);
		return true;
	}

	return false;
}

int AUD_setDopplerFactor(float factor)
{
	if(AUD_3ddevice)
	{
		AUD_3ddevice->setDopplerFactor(factor);
		return true;
	}

	return false;
}

int AUD_setDistanceModel(AUD_DistanceModel model)
{
	if(AUD_3ddevice)
	{
		AUD_3ddevice->setDistanceModel(model);
		return true;
	}

	return false;
}

int AUD_setSourceLocation(AUD_Handle* handle, const float* location)
{
	assert(handle);
	AUD_Reference<AUD_I3DHandle> h(*handle);

	if(!h.isNull())
	{
		AUD_Vector3 v(location[0], location[1], location[2]);
		return h->setSourceLocation(v);
	}

	return false;
}

int AUD_setSourceVelocity(AUD_Handle* handle, const float* velocity)
{
	assert(handle);
	AUD_Reference<AUD_I3DHandle> h(*handle);

	if(!h.isNull())
	{
		AUD_Vector3 v(velocity[0], velocity[1], velocity[2]);
		return h->setSourceVelocity(v);
	}

	return false;
}

int AUD_setSourceOrientation(AUD_Handle* handle, const float* orientation)
{
	assert(handle);
	AUD_Reference<AUD_I3DHandle> h(*handle);

	if(!h.isNull())
	{
		AUD_Quaternion q(orientation[3], orientation[0], orientation[1], orientation[2]);
		return h->setSourceOrientation(q);
	}

	return false;
}

int AUD_setRelative(AUD_Handle* handle, int relative)
{
	assert(handle);
	AUD_Reference<AUD_I3DHandle> h(*handle);

	if(!h.isNull())
	{
		return h->setRelative(relative);
	}

	return false;
}

int AUD_setVolumeMaximum(AUD_Handle* handle, float volume)
{
	assert(handle);
	AUD_Reference<AUD_I3DHandle> h(*handle);

	if(!h.isNull())
	{
		return h->setVolumeMaximum(volume);
	}

	return false;
}

int AUD_setVolumeMinimum(AUD_Handle* handle, float volume)
{
	assert(handle);
	AUD_Reference<AUD_I3DHandle> h(*handle);

	if(!h.isNull())
	{
		return h->setVolumeMinimum(volume);
	}

	return false;
}

int AUD_setDistanceMaximum(AUD_Handle* handle, float distance)
{
	assert(handle);
	AUD_Reference<AUD_I3DHandle> h(*handle);

	if(!h.isNull())
	{
		return h->setDistanceMaximum(distance);
	}

	return false;
}

int AUD_setDistanceReference(AUD_Handle* handle, float distance)
{
	assert(handle);
	AUD_Reference<AUD_I3DHandle> h(*handle);

	if(!h.isNull())
	{
		return h->setDistanceReference(distance);
	}

	return false;
}

int AUD_setAttenuation(AUD_Handle* handle, float factor)
{
	assert(handle);
	AUD_Reference<AUD_I3DHandle> h(*handle);

	if(!h.isNull())
	{
		return h->setAttenuation(factor);
	}

	return false;
}

int AUD_setConeAngleOuter(AUD_Handle* handle, float angle)
{
	assert(handle);
	AUD_Reference<AUD_I3DHandle> h(*handle);

	if(!h.isNull())
	{
		return h->setConeAngleOuter(angle);
	}

	return false;
}

int AUD_setConeAngleInner(AUD_Handle* handle, float angle)
{
	assert(handle);
	AUD_Reference<AUD_I3DHandle> h(*handle);

	if(!h.isNull())
	{
		return h->setConeAngleInner(angle);
	}

	return false;
}

int AUD_setConeVolumeOuter(AUD_Handle* handle, float volume)
{
	assert(handle);
	AUD_Reference<AUD_I3DHandle> h(*handle);

	if(!h.isNull())
	{
		return h->setConeVolumeOuter(volume);
	}

	return false;
}

int AUD_setSoundVolume(AUD_Handle* handle, float volume)
{
	assert(handle);
	try
	{
		return (*handle)->setVolume(volume);
	}
	catch(AUD_Exception&) {}
	return false;
}

int AUD_setSoundPitch(AUD_Handle* handle, float pitch)
{
	assert(handle);
	try
	{
		return (*handle)->setPitch(pitch);
	}
	catch(AUD_Exception&) {}
	return false;
}

AUD_Device* AUD_openReadDevice(AUD_DeviceSpecs specs)
{
	try
	{
		return new AUD_Device(new AUD_ReadDevice(specs));
	}
	catch(AUD_Exception&)
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
		AUD_Handle handle = (*device)->play(*sound);
		if(!handle.isNull())
		{
			handle->seek(seek);
			return new AUD_Handle(handle);
		}
	}
	catch(AUD_Exception&)
	{
	}
	return NULL;
}

int AUD_setDeviceVolume(AUD_Device* device, float volume)
{
	assert(device);

	try
	{
		(*device)->setVolume(volume);
		return true;
	}
	catch(AUD_Exception&) {}

	return false;
}

int AUD_readDevice(AUD_Device* device, data_t* buffer, int length)
{
	assert(device);
	assert(buffer);

	try
	{
		return (*device)->read(buffer, length);
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
						   float sthreshold, double samplerate, int* length)
{
	AUD_Buffer buffer;
	AUD_DeviceSpecs specs;
	specs.channels = AUD_CHANNELS_MONO;
	specs.rate = (AUD_SampleRate)samplerate;
	AUD_Reference<AUD_IFactory> sound;

	AUD_Reference<AUD_IFactory> file = new AUD_FileFactory(filename);

	int position = 0;

	try
	{
		AUD_Reference<AUD_IReader> reader = file->createReader();

		AUD_SampleRate rate = reader->getSpecs().rate;

		sound = new AUD_ChannelMapperFactory(file, specs);

		if(high < rate)
			sound = new AUD_LowpassFactory(sound, high);
		if(low > 0)
			sound = new AUD_HighpassFactory(sound, low);

		sound = new AUD_EnvelopeFactory(sound, attack, release, threshold, 0.1f);
		sound = new AUD_LinearResampleFactory(sound, specs);

		if(square)
			sound = new AUD_SquareFactory(sound, sthreshold);

		if(accumulate)
			sound = new AUD_AccumulatorFactory(sound, additive);
		else if(additive)
			sound = new AUD_SumFactory(sound);

		reader = sound->createReader();

		if(reader.isNull())
			return NULL;

		int len;
		bool eos;
		do
		{
			len = samplerate;
			buffer.resize((position + len) * sizeof(float), true);
			reader->read(len, eos, buffer.getBuffer() + position);
			position += len;
		} while(!eos);
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}

	float* result = (float*)malloc(position * sizeof(float));
	memcpy(result, buffer.getBuffer(), position * sizeof(float));
	*length = position;
	return result;
}

static void pauseSound(AUD_Handle* handle)
{
	assert(handle);
	(*handle)->pause();
}

AUD_Handle* AUD_pauseAfter(AUD_Handle* handle, float seconds)
{
	AUD_Reference<AUD_IFactory> silence = new AUD_SilenceFactory;
	AUD_Reference<AUD_IFactory> limiter = new AUD_LimiterFactory(silence, 0, seconds);

	AUD_device->lock();

	try
	{
		AUD_Handle handle2 = AUD_device->play(limiter);
		if(!handle2.isNull())
		{
			handle2->setStopCallback((stopCallback)pauseSound, handle);
			AUD_device->unlock();
			return new AUD_Handle(handle2);
		}
	}
	catch(AUD_Exception&)
	{
	}

	AUD_device->unlock();

	return NULL;
}

AUD_Sound* AUD_createSequencer(float fps, int muted)
{
	// specs are changed at a later point!
	AUD_Specs specs;
	specs.channels = AUD_CHANNELS_STEREO;
	specs.rate = AUD_RATE_44100;
	AUD_Sound* sequencer = new AUD_Sound(AUD_Reference<AUD_SequencerFactory>(new AUD_SequencerFactory(specs, fps, muted)));
	return sequencer;
}

void AUD_destroySequencer(AUD_Sound* sequencer)
{
	delete sequencer;
}

void AUD_setSequencerMuted(AUD_Sound* sequencer, int muted)
{
	dynamic_cast<AUD_SequencerFactory*>(sequencer->get())->mute(muted);
}

void AUD_setSequencerFPS(AUD_Sound* sequencer, float fps)
{
	dynamic_cast<AUD_SequencerFactory*>(sequencer->get())->setFPS(fps);
}

AUD_SEntry* AUD_addSequence(AUD_Sound* sequencer, AUD_Sound* sound,
							 float begin, float end, float skip)
{
	if(!sound)
		return new AUD_SEntry(((AUD_SequencerFactory*)sequencer->get())->add(AUD_Sound(), begin, end, skip));
	return new AUD_SEntry(((AUD_SequencerFactory*)sequencer->get())->add(*sound, begin, end, skip));
}

void AUD_removeSequence(AUD_Sound* sequencer, AUD_SEntry* entry)
{
	dynamic_cast<AUD_SequencerFactory*>(sequencer->get())->remove(*entry);
	delete entry;
}

void AUD_moveSequence(AUD_SEntry* entry, float begin, float end, float skip)
{
	(*entry)->move(begin, end, skip);
}

void AUD_muteSequence(AUD_SEntry* entry, char mute)
{
	(*entry)->mute(mute);
}

void AUD_setRelativeSequence(AUD_SEntry* entry, char relative)
{
	(*entry)->setRelative(relative);
}

void AUD_updateSequenceSound(AUD_SEntry* entry, AUD_Sound* sound)
{
	if(sound)
		(*entry)->setSound(*sound);
	else
		(*entry)->setSound(AUD_Sound());
}

void AUD_setSequenceAnimData(AUD_SEntry* entry, AUD_AnimateablePropertyType type, int frame, float* data, char animated)
{
	AUD_AnimateableProperty* prop = (*entry)->getAnimProperty(type);
	if(animated)
	{
		if(frame >= 0)
			prop->write(data, frame, 1);
	}
	else
		prop->write(data);
}

void AUD_setSequencerAnimData(AUD_Sound* sequencer, AUD_AnimateablePropertyType type, int frame, float* data, char animated)
{
	AUD_AnimateableProperty* prop = dynamic_cast<AUD_SequencerFactory*>(sequencer->get())->getAnimProperty(type);
	if(animated)
	{
		if(frame >= 0)
			prop->write(data, frame, 1);
	}
	else
		prop->write(data);
}

void AUD_updateSequenceData(AUD_SEntry* entry, float volume_max, float volume_min,
							float distance_max, float distance_reference, float attenuation,
							float cone_angle_outer, float cone_angle_inner, float cone_volume_outer)
{
	(*entry)->updateAll(volume_max, volume_min, distance_max, distance_reference, attenuation,
						cone_angle_outer, cone_angle_inner, cone_volume_outer);
}

void AUD_updateSequencerData(AUD_Sound* sequencer, float speed_of_sound,
							 float factor, AUD_DistanceModel model)
{
	AUD_SequencerFactory* f = dynamic_cast<AUD_SequencerFactory*>(sequencer->get());
	f->setSpeedOfSound(speed_of_sound);
	f->setDopplerFactor(factor);
	f->setDistanceModel(model);
}

void AUD_setSequencerDeviceSpecs(AUD_Sound* sequencer)
{
	dynamic_cast<AUD_SequencerFactory*>(sequencer->get())->setSpecs(AUD_device->getSpecs().specs);
}

void AUD_setSequencerSpecs(AUD_Sound* sequencer, AUD_Specs specs)
{
	dynamic_cast<AUD_SequencerFactory*>(sequencer->get())->setSpecs(specs);
}

void AUD_seekSequencer(AUD_Handle* handle, float time)
{
#ifdef WITH_JACK
	AUD_JackDevice* device = dynamic_cast<AUD_JackDevice*>(AUD_device.get());
	if(device)
		device->seekPlayback(time);
	else
#endif
	{
		assert(handle);
		(*handle)->seek(time);
	}
}

float AUD_getSequencerPosition(AUD_Handle* handle)
{
#ifdef WITH_JACK
	AUD_JackDevice* device = dynamic_cast<AUD_JackDevice*>(AUD_device.get());
	if(device)
		return device->getPlaybackPosition();
	else
#endif
	{
		assert(handle);
		return (*handle)->getPosition();
	}
}

void AUD_startPlayback()
{
#ifdef WITH_JACK
	AUD_JackDevice* device = dynamic_cast<AUD_JackDevice*>(AUD_device.get());
	if(device)
		device->startPlayback();
#endif
}

void AUD_stopPlayback()
{
#ifdef WITH_JACK
	AUD_JackDevice* device = dynamic_cast<AUD_JackDevice*>(AUD_device.get());
	if(device)
		device->stopPlayback();
#endif
}

#ifdef WITH_JACK
void AUD_setSyncCallback(AUD_syncFunction function, void* data)
{
	AUD_JackDevice* device = dynamic_cast<AUD_JackDevice*>(AUD_device.get());
	if(device)
		device->setSyncCallback(function, data);
}
#endif

int AUD_doesPlayback()
{
#ifdef WITH_JACK
	AUD_JackDevice* device = dynamic_cast<AUD_JackDevice*>(AUD_device.get());
	if(device)
		return device->doesPlayback();
#endif
	return -1;
}

int AUD_readSound(AUD_Sound* sound, sample_t* buffer, int length, int samples_per_second)
{
	AUD_DeviceSpecs specs;
	sample_t* buf;
	AUD_Buffer aBuffer;

	specs.rate = AUD_RATE_INVALID;
	specs.channels = AUD_CHANNELS_MONO;
	specs.format = AUD_FORMAT_INVALID;

	AUD_Reference<AUD_IReader> reader = AUD_ChannelMapperFactory(*sound, specs).createReader();

	specs.specs = reader->getSpecs();
	int len;
	float samplejump = specs.rate / samples_per_second;
	float min, max, power, overallmax;
	bool eos;

	overallmax = 0;

	for(int i = 0; i < length; i++)
	{
		len = floor(samplejump * (i+1)) - floor(samplejump * i);

		aBuffer.assureSize(len * AUD_SAMPLE_SIZE(specs));
		buf = aBuffer.getBuffer();

		reader->read(len, eos, buf);

		max = min = *buf;
		power = *buf * *buf;
		for(int j = 1; j < len; j++)
		{
			if(buf[j] < min)
				min = buf[j];
			if(buf[j] > max)
				max = buf[j];
			power += buf[j] * buf[j];
		}

		buffer[i * 3] = min;
		buffer[i * 3 + 1] = max;
		buffer[i * 3 + 2] = sqrt(power) / len;

		if(overallmax < max)
			overallmax = max;
		if(overallmax < -min)
			overallmax = -min;

		if(eos)
		{
			length = i;
			break;
		}
	}

	if(overallmax > 1.0f)
	{
		for(int i = 0; i < length * 3; i++)
		{
			buffer[i] /= overallmax;
		}
	}

	return length;
}

AUD_Sound* AUD_copy(AUD_Sound* sound)
{
	return new AUD_Reference<AUD_IFactory>(*sound);
}

void AUD_freeHandle(AUD_Handle* handle)
{
	delete handle;
}

void* AUD_createSet()
{
	return new std::set<void*>();
}

void AUD_destroySet(void* set)
{
	delete reinterpret_cast<std::set<void*>*>(set);
}

char AUD_removeSet(void* set, void* entry)
{
	if(set)
		return reinterpret_cast<std::set<void*>*>(set)->erase(entry);
	return 0;
}

void AUD_addSet(void* set, void* entry)
{
	if(entry)
		reinterpret_cast<std::set<void*>*>(set)->insert(entry);
}

void* AUD_getSet(void* set)
{
	if(set)
	{
		std::set<void*>* rset = reinterpret_cast<std::set<void*>*>(set);
		if(!rset->empty())
		{
			std::set<void*>::iterator it = rset->begin();
			void* result = *it;
			rset->erase(it);
			return result;
		}
	}

	return NULL;
}

const char* AUD_mixdown(AUD_Sound* sound, unsigned int start, unsigned int length, unsigned int buffersize, const char* filename, AUD_DeviceSpecs specs, AUD_Container format, AUD_Codec codec, unsigned int bitrate)
{
	try
	{
		AUD_SequencerFactory* f = dynamic_cast<AUD_SequencerFactory*>(sound->get());

		f->setSpecs(specs.specs);
		AUD_Reference<AUD_IReader> reader = f->createQualityReader();
		reader->seek(start);
		AUD_Reference<AUD_IWriter> writer = AUD_FileWriter::createWriter(filename, specs, format, codec, bitrate);
		AUD_FileWriter::writeReader(reader, writer, length, buffersize);

		return NULL;
	}
	catch(AUD_Exception& e)
	{
		return e.str;
	}
}

AUD_Device* AUD_openMixdownDevice(AUD_DeviceSpecs specs, AUD_Sound* sequencer, float volume, float start)
{
	try
	{
		AUD_ReadDevice* device = new AUD_ReadDevice(specs);
		device->setQuality(true);
		device->setVolume(volume);

		dynamic_cast<AUD_SequencerFactory*>(sequencer->get())->setSpecs(specs.specs);

		AUD_Handle handle = device->play(*sequencer);
		if(!handle.isNull())
			handle->seek(start);

		return new AUD_Device(device);
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

AUD_Reference<AUD_IDevice> AUD_getDevice()
{
	return AUD_device;
}

AUD_I3DDevice* AUD_get3DDevice()
{
	return AUD_3ddevice;
}
