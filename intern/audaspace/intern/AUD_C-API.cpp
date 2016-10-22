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

// quiet unudef define warning
#ifdef __STDC_CONSTANT_MACROS
// pass
#endif

#ifdef WITH_PYTHON
#  include "AUD_PyInit.h"
#  include "AUD_PyAPI.h"
#endif

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <sstream>
#include <iostream>

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
#include "AUD_MutexLock.h"

#ifdef WITH_SDL
#include "AUD_SDLDevice.h"
#endif

#ifdef WITH_OPENAL
#include "AUD_OpenALDevice.h"
#endif

#ifdef WITH_JACK
#include "AUD_JackDevice.h"
#include "AUD_JackLibrary.h"
#endif


#ifdef WITH_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
}
#endif

#include <cassert>

typedef boost::shared_ptr<AUD_IFactory> AUD_Sound;
typedef boost::shared_ptr<AUD_IDevice> AUD_Device;
typedef boost::shared_ptr<AUD_IHandle> AUD_Handle;
typedef boost::shared_ptr<AUD_SequencerEntry> AUD_SEntry;

#define AUD_CAPI_IMPLEMENTATION
#include "AUD_C-API.h"

#ifndef NULL
#  define NULL (void *)0
#endif

static boost::shared_ptr<AUD_IDevice> AUD_device;
static AUD_I3DDevice *AUD_3ddevice;

void AUD_initOnce()
{
#ifdef WITH_FFMPEG
	av_register_all();
#endif
#ifdef WITH_JACK
	AUD_jack_init();
#endif
}

void AUD_exitOnce()
{
#ifdef WITH_JACK
	AUD_jack_exit();
#endif
}

AUD_Device* AUD_init(const char* device, AUD_DeviceSpecs specs, int buffersize, const char* name)
{
	boost::shared_ptr<AUD_IDevice> dev;

	if (AUD_device.get()) {
		AUD_exit(NULL);
	}

	std::string dname = device;

	try {
		if(dname == "Null") {
			dev = boost::shared_ptr<AUD_IDevice>(new AUD_NULLDevice());
		}
#ifdef WITH_SDL
		else if(dname == "SDL")
		{
			dev = boost::shared_ptr<AUD_IDevice>(new AUD_SDLDevice(specs, buffersize));
		}
#endif
#ifdef WITH_OPENAL
		else if(dname == "OpenAL")
		{
			dev = boost::shared_ptr<AUD_IDevice>(new AUD_OpenALDevice(specs, buffersize));
		}
#endif
#ifdef WITH_JACK
		else if(dname == "JACK")
		{
#ifdef __APPLE__
			struct stat st;
			if (stat("/Library/Frameworks/Jackmp.framework", &st) != 0) {
				printf("Warning: JACK Framework not installed\n");
				return NULL;
			}
			else
#endif
			if (!AUD_jack_supported()) {
				printf("Warning: JACK cllient not installed\n");
				return NULL;
			}
			else {
				dev = boost::shared_ptr<AUD_IDevice>(new AUD_JackDevice(name, specs, buffersize));
			}
		}
#endif
		else
		{
			return NULL;
		}

		AUD_device = dev;
		AUD_3ddevice = dynamic_cast<AUD_I3DDevice *>(AUD_device.get());

		return (AUD_Device*)1;
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

void AUD_exit(AUD_Device* device)
{
	AUD_device = boost::shared_ptr<AUD_IDevice>();
	AUD_3ddevice = NULL;
}

#ifdef WITH_PYTHON
static PyObject *AUD_getCDevice(PyObject *self)
{
	if (AUD_device.get()) {
		Device *device = (Device *)Device_empty();
		if (device != NULL) {
			device->device = new boost::shared_ptr<AUD_IDevice>(AUD_device);
			return (PyObject *)device;
		}
	}

	Py_RETURN_NONE;
}

static PyMethodDef meth_getcdevice[] = {
    {"device", (PyCFunction)AUD_getCDevice, METH_NOARGS,
     "device()\n\n"
     "Returns the application's :class:`Device`.\n\n"
     ":return: The application's :class:`Device`.\n"
     ":rtype: :class:`Device`"}
};

extern "C" {
extern void *BKE_sound_get_factory(void *sound);
}

static PyObject *AUD_getSoundFromPointer(PyObject *self, PyObject *args)
{
	long int lptr;

	if (PyArg_Parse(args, "l:_sound_from_pointer", &lptr)) {
		if (lptr) {
			boost::shared_ptr<AUD_IFactory>* factory = (boost::shared_ptr<AUD_IFactory>*) BKE_sound_get_factory((void *) lptr);

			if (factory) {
				Factory *obj = (Factory *)Factory_empty();
				if (obj) {
					obj->factory = new boost::shared_ptr<AUD_IFactory>(*factory);
					return (PyObject *) obj;
				}
			}
		}
	}

	Py_RETURN_NONE;
}

static PyMethodDef meth_sound_from_pointer[] = {
    {"_sound_from_pointer", (PyCFunction)AUD_getSoundFromPointer, METH_O,
     "_sound_from_pointer(pointer)\n\n"
     "Returns the corresponding :class:`Factory` object.\n\n"
     ":arg pointer: The pointer to the bSound object as long.\n"
     ":type pointer: long\n"
     ":return: The corresponding :class:`Factory` object.\n"
     ":rtype: :class:`Factory`"}
};

PyObject *AUD_initPython()
{
	PyObject *module = PyInit_aud();
	PyModule_AddObject(module, "device", (PyObject *)PyCFunction_New(meth_getcdevice, NULL));
	PyModule_AddObject(module, "_sound_from_pointer", (PyObject *)PyCFunction_New(meth_sound_from_pointer, NULL));
	PyDict_SetItemString(PyImport_GetModuleDict(), "aud", module);

	return module;
}

void *AUD_getPythonSound(AUD_Sound *sound)
{
	if (sound) {
		Factory *obj = (Factory *) Factory_empty();
		if (obj) {
			obj->factory = new boost::shared_ptr<AUD_IFactory>(*sound);
			return (PyObject *) obj;
		}
	}

	return NULL;
}

AUD_Sound *AUD_getSoundFromPython(void *sound)
{
	Factory *factory = checkFactory((PyObject *)sound);

	if (!factory)
		return NULL;

	return new boost::shared_ptr<AUD_IFactory>(*reinterpret_cast<boost::shared_ptr<AUD_IFactory>*>(factory->factory));
}

#endif

void AUD_Device_lock(AUD_Device* device)
{
	AUD_device->lock();
}

void AUD_Device_unlock(AUD_Device* device)
{
	AUD_device->unlock();
}

AUD_Channels AUD_Device_getChannels(AUD_Device* device)
{
	return AUD_device->getSpecs().channels;
}

AUD_SampleRate AUD_Device_getRate(AUD_Device* device)
{
	return AUD_device->getSpecs().rate;
}

AUD_SoundInfo AUD_getInfo(AUD_Sound *sound)
{
	assert(sound);

	AUD_SoundInfo info;
	info.specs.channels = AUD_CHANNELS_INVALID;
	info.specs.rate = AUD_RATE_INVALID;
	info.length = 0.0f;

	try {
		boost::shared_ptr<AUD_IReader> reader = (*sound)->createReader();

		if (reader.get()) {
			info.specs = reader->getSpecs();
			info.length = reader->getLength() / (float) info.specs.rate;
		}
	}
	catch(AUD_Exception &ae)
	{
		std::cout << ae.str << std::endl;
	}

	return info;
}

AUD_Sound *AUD_Sound_file(const char *filename)
{
	assert(filename);
	return new AUD_Sound(new AUD_FileFactory(filename));
}

AUD_Sound *AUD_Sound_bufferFile(unsigned char *buffer, int size)
{
	assert(buffer);
	return new AUD_Sound(new AUD_FileFactory(buffer, size));
}

AUD_Sound *AUD_Sound_cache(AUD_Sound *sound)
{
	assert(sound);

	try {
		return new AUD_Sound(new AUD_StreamBufferFactory(*sound));
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

AUD_Sound *AUD_Sound_rechannel(AUD_Sound *sound, AUD_Channels channels)
{
	assert(sound);

	try {
		AUD_DeviceSpecs specs;
		specs.channels = channels;
		specs.rate = AUD_RATE_INVALID;
		specs.format = AUD_FORMAT_INVALID;
		return new AUD_Sound(new AUD_ChannelMapperFactory(*sound, specs));
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

AUD_Sound *AUD_Sound_delay(AUD_Sound *sound, float delay)
{
	assert(sound);

	try {
		return new AUD_Sound(new AUD_DelayFactory(*sound, delay));
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

AUD_Sound *AUD_Sound_limit(AUD_Sound *sound, float start, float end)
{
	assert(sound);

	try {
		return new AUD_Sound(new AUD_LimiterFactory(*sound, start, end));
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

AUD_Sound *AUD_Sound_pingpong(AUD_Sound *sound)
{
	assert(sound);

	try {
		return new AUD_Sound(new AUD_PingPongFactory(*sound));
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

AUD_Sound *AUD_Sound_loop(AUD_Sound *sound)
{
	assert(sound);

	try {
		return new AUD_Sound(new AUD_LoopFactory(*sound));
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

int AUD_Handle_setLoopCount(AUD_Handle *handle, int loops)
{
	assert(handle);

	try {
		return (*handle)->setLoopCount(loops);
	}
	catch(AUD_Exception&)
	{
	}

	return false;
}

AUD_Sound *AUD_rectifySound(AUD_Sound *sound)
{
	assert(sound);

	try {
		return new AUD_Sound(new AUD_RectifyFactory(*sound));
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

void AUD_Sound_free(AUD_Sound *sound)
{
	assert(sound);
	delete sound;
}

AUD_Handle *AUD_Device_play(AUD_Device* device, AUD_Sound *sound, int keep)
{
	assert(sound);
	try {
		AUD_Handle handle = AUD_device->play(*sound, keep);
		if (handle.get()) {
			return new AUD_Handle(handle);
		}
	}
	catch(AUD_Exception&)
	{
	}
	return NULL;
}

int AUD_Handle_pause(AUD_Handle *handle)
{
	assert(handle);
	return (*handle)->pause();
}

int AUD_Handle_resume(AUD_Handle *handle)
{
	assert(handle);
	return (*handle)->resume();
}

int AUD_Handle_stop(AUD_Handle *handle)
{
	assert(handle);
	int result = (*handle)->stop();
	delete handle;
	return result;
}

void AUD_Device_stopAll(void* device)
{
	AUD_device->stopAll();
}

int AUD_Handle_setKeep(AUD_Handle *handle, int keep)
{
	assert(handle);
	return (*handle)->setKeep(keep);
}

int AUD_Handle_setPosition(AUD_Handle *handle, float seekTo)
{
	assert(handle);
	return (*handle)->seek(seekTo);
}

float AUD_Handle_getPosition(AUD_Handle *handle)
{
	assert(handle);
	return (*handle)->getPosition();
}

AUD_Status AUD_Handle_getStatus(AUD_Handle *handle)
{
	assert(handle);
	return (*handle)->getStatus();
}

int AUD_Device_setListenerLocation(const float location[3])
{
	if (AUD_3ddevice) {
		AUD_Vector3 v(location[0], location[1], location[2]);
		AUD_3ddevice->setListenerLocation(v);
		return true;
	}

	return false;
}

int AUD_Device_setListenerVelocity(const float velocity[3])
{
	if (AUD_3ddevice) {
		AUD_Vector3 v(velocity[0], velocity[1], velocity[2]);
		AUD_3ddevice->setListenerVelocity(v);
		return true;
	}

	return false;
}

int AUD_Device_setListenerOrientation(const float orientation[4])
{
	if (AUD_3ddevice) {
		AUD_Quaternion q(orientation[3], orientation[0], orientation[1], orientation[2]);
		AUD_3ddevice->setListenerOrientation(q);
		return true;
	}

	return false;
}

int AUD_Device_setSpeedOfSound(void* device, float speed)
{
	if (AUD_3ddevice) {
		AUD_3ddevice->setSpeedOfSound(speed);
		return true;
	}

	return false;
}

int AUD_Device_setDopplerFactor(void* device, float factor)
{
	if (AUD_3ddevice) {
		AUD_3ddevice->setDopplerFactor(factor);
		return true;
	}

	return false;
}

int AUD_Device_setDistanceModel(void* device, AUD_DistanceModel model)
{
	if (AUD_3ddevice) {
		AUD_3ddevice->setDistanceModel(model);
		return true;
	}

	return false;
}

int AUD_Handle_setLocation(AUD_Handle *handle, const float location[3])
{
	assert(handle);
	boost::shared_ptr<AUD_I3DHandle> h = boost::dynamic_pointer_cast<AUD_I3DHandle>(*handle);

	if (h.get()) {
		AUD_Vector3 v(location[0], location[1], location[2]);
		return h->setSourceLocation(v);
	}

	return false;
}

int AUD_Handle_setVelocity(AUD_Handle *handle, const float velocity[3])
{
	assert(handle);
	boost::shared_ptr<AUD_I3DHandle> h = boost::dynamic_pointer_cast<AUD_I3DHandle>(*handle);

	if (h.get()) {
		AUD_Vector3 v(velocity[0], velocity[1], velocity[2]);
		return h->setSourceVelocity(v);
	}

	return false;
}

int AUD_Handle_setOrientation(AUD_Handle *handle, const float orientation[4])
{
	assert(handle);
	boost::shared_ptr<AUD_I3DHandle> h = boost::dynamic_pointer_cast<AUD_I3DHandle>(*handle);

	if (h.get()) {
		AUD_Quaternion q(orientation[3], orientation[0], orientation[1], orientation[2]);
		return h->setSourceOrientation(q);
	}

	return false;
}

int AUD_Handle_setRelative(AUD_Handle *handle, int relative)
{
	assert(handle);
	boost::shared_ptr<AUD_I3DHandle> h = boost::dynamic_pointer_cast<AUD_I3DHandle>(*handle);

	if (h.get()) {
		return h->setRelative(relative);
	}

	return false;
}

int AUD_Handle_setVolumeMaximum(AUD_Handle *handle, float volume)
{
	assert(handle);
	boost::shared_ptr<AUD_I3DHandle> h = boost::dynamic_pointer_cast<AUD_I3DHandle>(*handle);

	if (h.get()) {
		return h->setVolumeMaximum(volume);
	}

	return false;
}

int AUD_Handle_setVolumeMinimum(AUD_Handle *handle, float volume)
{
	assert(handle);
	boost::shared_ptr<AUD_I3DHandle> h = boost::dynamic_pointer_cast<AUD_I3DHandle>(*handle);

	if (h.get()) {
		return h->setVolumeMinimum(volume);
	}

	return false;
}

int AUD_Handle_setDistanceMaximum(AUD_Handle *handle, float distance)
{
	assert(handle);
	boost::shared_ptr<AUD_I3DHandle> h = boost::dynamic_pointer_cast<AUD_I3DHandle>(*handle);

	if (h.get()) {
		return h->setDistanceMaximum(distance);
	}

	return false;
}

int AUD_Handle_setDistanceReference(AUD_Handle *handle, float distance)
{
	assert(handle);
	boost::shared_ptr<AUD_I3DHandle> h = boost::dynamic_pointer_cast<AUD_I3DHandle>(*handle);

	if (h.get()) {
		return h->setDistanceReference(distance);
	}

	return false;
}

int AUD_Handle_setAttenuation(AUD_Handle *handle, float factor)
{
	assert(handle);
	boost::shared_ptr<AUD_I3DHandle> h = boost::dynamic_pointer_cast<AUD_I3DHandle>(*handle);

	if (h.get()) {
		return h->setAttenuation(factor);
	}

	return false;
}

int AUD_Handle_setConeAngleOuter(AUD_Handle *handle, float angle)
{
	assert(handle);
	boost::shared_ptr<AUD_I3DHandle> h = boost::dynamic_pointer_cast<AUD_I3DHandle>(*handle);

	if (h.get()) {
		return h->setConeAngleOuter(angle);
	}

	return false;
}

int AUD_Handle_setConeAngleInner(AUD_Handle *handle, float angle)
{
	assert(handle);
	boost::shared_ptr<AUD_I3DHandle> h = boost::dynamic_pointer_cast<AUD_I3DHandle>(*handle);

	if (h.get()) {
		return h->setConeAngleInner(angle);
	}

	return false;
}

int AUD_Handle_setConeVolumeOuter(AUD_Handle *handle, float volume)
{
	assert(handle);
	boost::shared_ptr<AUD_I3DHandle> h = boost::dynamic_pointer_cast<AUD_I3DHandle>(*handle);

	if (h.get()) {
		return h->setConeVolumeOuter(volume);
	}

	return false;
}

int AUD_Handle_setVolume(AUD_Handle *handle, float volume)
{
	assert(handle);
	try {
		return (*handle)->setVolume(volume);
	}
	catch(AUD_Exception&) {}
	return false;
}

int AUD_Handle_setPitch(AUD_Handle *handle, float pitch)
{
	assert(handle);
	try {
		return (*handle)->setPitch(pitch);
	}
	catch(AUD_Exception&) {}
	return false;
}

AUD_Device *AUD_openReadDevice(AUD_DeviceSpecs specs)
{
	try {
		return new AUD_Device(new AUD_ReadDevice(specs));
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

AUD_Handle *AUD_playDevice(AUD_Device *device, AUD_Sound *sound, float seek)
{
	assert(device);
	assert(sound);

	try {
		AUD_Handle handle = (*device)->play(*sound);
		if (handle.get()) {
			handle->seek(seek);
			return new AUD_Handle(handle);
		}
	}
	catch(AUD_Exception&)
	{
	}
	return NULL;
}

int AUD_setDeviceVolume(AUD_Device *device, float volume)
{
	assert(device);

	try {
		(*device)->setVolume(volume);
		return true;
	}
	catch(AUD_Exception&) {}

	return false;
}

int AUD_Device_read(AUD_Device *device, data_t *buffer, int length)
{
	assert(device);
	assert(buffer);

	try {
		return boost::dynamic_pointer_cast<AUD_ReadDevice>(*device)->read(buffer, length);
	}
	catch(AUD_Exception&)
	{
		return false;
	}
}

void AUD_Device_free(AUD_Device *device)
{
	try {
		if(device != &AUD_device)
			delete device;
	}
	catch(AUD_Exception&)
	{
	}
}

float *AUD_readSoundBuffer(const char *filename, float low, float high,
                           float attack, float release, float threshold,
                           int accumulate, int additive, int square,
                           float sthreshold, double samplerate, int *length)
{
	AUD_Buffer buffer;
	AUD_DeviceSpecs specs;
	specs.channels = AUD_CHANNELS_MONO;
	specs.rate = (AUD_SampleRate)samplerate;
	boost::shared_ptr<AUD_IFactory> sound;

	boost::shared_ptr<AUD_IFactory> file = boost::shared_ptr<AUD_IFactory>(new AUD_FileFactory(filename));

	int position = 0;

	try {
		boost::shared_ptr<AUD_IReader> reader = file->createReader();

		AUD_SampleRate rate = reader->getSpecs().rate;

		sound = boost::shared_ptr<AUD_IFactory>(new AUD_ChannelMapperFactory(file, specs));

		if (high < rate)
			sound = boost::shared_ptr<AUD_IFactory>(new AUD_LowpassFactory(sound, high));
		if (low > 0)
			sound = boost::shared_ptr<AUD_IFactory>(new AUD_HighpassFactory(sound, low));

		sound = boost::shared_ptr<AUD_IFactory>(new AUD_EnvelopeFactory(sound, attack, release, threshold, 0.1f));
		sound = boost::shared_ptr<AUD_IFactory>(new AUD_LinearResampleFactory(sound, specs));

		if (square)
			sound = boost::shared_ptr<AUD_IFactory>(new AUD_SquareFactory(sound, sthreshold));

		if (accumulate)
			sound = boost::shared_ptr<AUD_IFactory>(new AUD_AccumulatorFactory(sound, additive));
		else if (additive)
			sound = boost::shared_ptr<AUD_IFactory>(new AUD_SumFactory(sound));

		reader = sound->createReader();

		if (!reader.get())
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

	float * result = (float *)malloc(position * sizeof(float));
	memcpy(result, buffer.getBuffer(), position * sizeof(float));
	*length = position;
	return result;
}

static void pauseSound(AUD_Handle *handle)
{
	assert(handle);
	(*handle)->pause();
}

AUD_Handle *AUD_pauseAfter(AUD_Handle *handle, float seconds)
{
	boost::shared_ptr<AUD_IFactory> silence = boost::shared_ptr<AUD_IFactory>(new AUD_SilenceFactory);
	boost::shared_ptr<AUD_IFactory> limiter = boost::shared_ptr<AUD_IFactory>(new AUD_LimiterFactory(silence, 0, seconds));

	AUD_MutexLock lock(*AUD_device);

	try {
		AUD_Handle handle2 = AUD_device->play(limiter);
		if (handle2.get()) {
			handle2->setStopCallback((stopCallback)pauseSound, handle);
			return new AUD_Handle(handle2);
		}
	}
	catch(AUD_Exception&)
	{
	}

	return NULL;
}

AUD_Sound *AUD_Sequence_create(float fps, int muted)
{
	// specs are changed at a later point!
	AUD_Specs specs;
	specs.channels = AUD_CHANNELS_STEREO;
	specs.rate = AUD_RATE_48000;
	AUD_Sound *sequencer = new AUD_Sound(boost::shared_ptr<AUD_SequencerFactory>(new AUD_SequencerFactory(specs, fps, muted)));
	return sequencer;
}

void AUD_Sequence_free(AUD_Sound *sequencer)
{
	delete sequencer;
}

void AUD_Sequence_setMuted(AUD_Sound *sequencer, int muted)
{
	dynamic_cast<AUD_SequencerFactory *>(sequencer->get())->mute(muted);
}

void AUD_Sequence_setFPS(AUD_Sound *sequencer, float fps)
{
	dynamic_cast<AUD_SequencerFactory *>(sequencer->get())->setFPS(fps);
}

AUD_SEntry *AUD_Sequence_add(AUD_Sound *sequencer, AUD_Sound *sound,
                            float begin, float end, float skip)
{
	if (!sound)
		return new AUD_SEntry(((AUD_SequencerFactory *)sequencer->get())->add(AUD_Sound(), begin, end, skip));
	return new AUD_SEntry(((AUD_SequencerFactory *)sequencer->get())->add(*sound, begin, end, skip));
}

void AUD_Sequence_remove(AUD_Sound *sequencer, AUD_SEntry *entry)
{
	dynamic_cast<AUD_SequencerFactory *>(sequencer->get())->remove(*entry);
	delete entry;
}

void AUD_SequenceEntry_move(AUD_SEntry *entry, float begin, float end, float skip)
{
	(*entry)->move(begin, end, skip);
}

void AUD_SequenceEntry_setMuted(AUD_SEntry *entry, char mute)
{
	(*entry)->mute(mute);
}

void AUD_SequenceEntry_setSound(AUD_SEntry *entry, AUD_Sound *sound)
{
	if (sound)
		(*entry)->setSound(*sound);
	else
		(*entry)->setSound(AUD_Sound());
}

void AUD_SequenceEntry_setAnimationData(AUD_SEntry *entry, AUD_AnimateablePropertyType type, int frame, float *data, char animated)
{
	AUD_AnimateableProperty *prop = (*entry)->getAnimProperty(type);
	if (animated) {
		if (frame >= 0)
			prop->write(data, frame, 1);
	}
	else {
		prop->write(data);
	}
}

void AUD_Sequence_setAnimationData(AUD_Sound *sequencer, AUD_AnimateablePropertyType type, int frame, float *data, char animated)
{
	AUD_AnimateableProperty *prop = dynamic_cast<AUD_SequencerFactory *>(sequencer->get())->getAnimProperty(type);
	if (animated) {
		if (frame >= 0) {
			prop->write(data, frame, 1);
		}
	}
	else {
		prop->write(data);
	}
}

void AUD_Sequence_setDistanceModel(AUD_Sound* sequence, AUD_DistanceModel value)
{
	assert(sequence);
	dynamic_cast<AUD_SequencerFactory *>(sequence->get())->setDistanceModel(static_cast<AUD_DistanceModel>(value));
}

void AUD_Sequence_setDopplerFactor(AUD_Sound* sequence, float value)
{
	assert(sequence);
	dynamic_cast<AUD_SequencerFactory *>(sequence->get())->setDopplerFactor(value);
}

void AUD_Sequence_setSpeedOfSound(AUD_Sound* sequence, float value)
{
	assert(sequence);
	dynamic_cast<AUD_SequencerFactory *>(sequence->get())->setSpeedOfSound(value);
}

void AUD_SequenceEntry_setAttenuation(AUD_SEntry* sequence_entry, float value)
{
	assert(sequence_entry);
	(*sequence_entry)->setAttenuation(value);
}

void AUD_SequenceEntry_setConeAngleInner(AUD_SEntry* sequence_entry, float value)
{
	assert(sequence_entry);
	(*sequence_entry)->setConeAngleInner(value);
}

void AUD_SequenceEntry_setConeAngleOuter(AUD_SEntry* sequence_entry, float value)
{
	assert(sequence_entry);
	(*sequence_entry)->setConeAngleOuter(value);
}

void AUD_SequenceEntry_setConeVolumeOuter(AUD_SEntry* sequence_entry, float value)
{
	assert(sequence_entry);
	(*sequence_entry)->setConeVolumeOuter(value);
}

void AUD_SequenceEntry_setDistanceMaximum(AUD_SEntry* sequence_entry, float value)
{
	assert(sequence_entry);
	(*sequence_entry)->setDistanceMaximum(value);
}

void AUD_SequenceEntry_setDistanceReference(AUD_SEntry* sequence_entry, float value)
{
	assert(sequence_entry);
	(*sequence_entry)->setDistanceReference(value);
}

void AUD_SequenceEntry_setRelative(AUD_SEntry* sequence_entry, int value)
{
	assert(sequence_entry);
	(*sequence_entry)->setRelative(value);
}

void AUD_SequenceEntry_setVolumeMaximum(AUD_SEntry* sequence_entry, float value)
{
	assert(sequence_entry);
	(*sequence_entry)->setVolumeMaximum(value);
}

void AUD_SequenceEntry_setVolumeMinimum(AUD_SEntry* sequence_entry, float value)
{
	assert(sequence_entry);
	(*sequence_entry)->setVolumeMinimum(value);
}

void AUD_setSequencerDeviceSpecs(AUD_Sound *sequencer)
{
	dynamic_cast<AUD_SequencerFactory *>(sequencer->get())->setSpecs(AUD_device->getSpecs().specs);
}

void AUD_Sequence_setSpecs(AUD_Sound *sequencer, AUD_Specs specs)
{
	dynamic_cast<AUD_SequencerFactory *>(sequencer->get())->setSpecs(specs);
}

void AUD_seekSynchronizer(AUD_Handle *handle, float time)
{
#ifdef WITH_JACK
	AUD_JackDevice *device = dynamic_cast<AUD_JackDevice *>(AUD_device.get());
	if (device) {
		device->seekPlayback(time);
	}
	else
#endif
	{
		assert(handle);
		(*handle)->seek(time);
	}
}

float AUD_getSynchronizerPosition(AUD_Handle *handle)
{
#ifdef WITH_JACK
	AUD_JackDevice *device = dynamic_cast<AUD_JackDevice *>(AUD_device.get());
	if (device) {
		return device->getPlaybackPosition();
	}
	else
#endif
	{
		assert(handle);
		return (*handle)->getPosition();
	}
}

void AUD_playSynchronizer()
{
#ifdef WITH_JACK
	AUD_JackDevice *device = dynamic_cast<AUD_JackDevice *>(AUD_device.get());
	if (device) {
		device->startPlayback();
	}
#endif
}

void AUD_stopSynchronizer()
{
#ifdef WITH_JACK
	AUD_JackDevice *device = dynamic_cast<AUD_JackDevice *>(AUD_device.get());
	if (device) {
		device->stopPlayback();
	}
#endif
}

#ifdef WITH_JACK
void AUD_setSynchronizerCallback(AUD_syncFunction function, void *data)
{
	AUD_JackDevice *device = dynamic_cast<AUD_JackDevice *>(AUD_device.get());
	if (device) {
		device->setSyncCallback(function, data);
	}
}
#endif

int AUD_isSynchronizerPlaying()
{
#ifdef WITH_JACK
	AUD_JackDevice *device = dynamic_cast<AUD_JackDevice *>(AUD_device.get());
	if (device) {
		return device->doesPlayback();
	}
#endif
	return -1;
}

int AUD_readSound(AUD_Sound *sound, sample_t *buffer, int length, int samples_per_second, short *interrupt)
{
	AUD_DeviceSpecs specs;
	sample_t *buf;
	AUD_Buffer aBuffer;

	specs.rate = AUD_RATE_INVALID;
	specs.channels = AUD_CHANNELS_MONO;
	specs.format = AUD_FORMAT_INVALID;

	boost::shared_ptr<AUD_IReader> reader = AUD_ChannelMapperFactory(*sound, specs).createReader();

	specs.specs = reader->getSpecs();
	int len;
	float samplejump = specs.rate / samples_per_second;
	float min, max, power, overallmax;
	bool eos;

	overallmax = 0;

	for (int i = 0; i < length; i++) {
		len = floor(samplejump * (i+1)) - floor(samplejump * i);

		if (*interrupt) {
			return 0;
		}
		aBuffer.assureSize(len * AUD_SAMPLE_SIZE(specs));
		buf = aBuffer.getBuffer();

		reader->read(len, eos, buf);

		max = min = *buf;
		power = *buf * *buf;
		for (int j = 1; j < len; j++) {
			if (buf[j] < min)
				min = buf[j];
			if (buf[j] > max)
				max = buf[j];
			power += buf[j] * buf[j];
		}

		buffer[i * 3] = min;
		buffer[i * 3 + 1] = max;
		buffer[i * 3 + 2] = sqrt(power) / len;

		if (overallmax < max)
			overallmax = max;
		if (overallmax < -min)
			overallmax = -min;

		if (eos) {
			length = i;
			break;
		}
	}

	if (overallmax > 1.0f) {
		for (int i = 0; i < length * 3; i++) {
			buffer[i] /= overallmax;
		}
	}

	return length;
}

AUD_Sound *AUD_Sound_copy(AUD_Sound *sound)
{
	return new boost::shared_ptr<AUD_IFactory>(*sound);
}

void AUD_Handle_free(AUD_Handle *handle)
{
	delete handle;
}

const char *AUD_mixdown(AUD_Sound *sound, unsigned int start, unsigned int length, unsigned int buffersize, const char *filename, AUD_DeviceSpecs specs, AUD_Container format, AUD_Codec codec, unsigned int bitrate)
{
	try {
		AUD_SequencerFactory *f = dynamic_cast<AUD_SequencerFactory *>(sound->get());

		f->setSpecs(specs.specs);
		boost::shared_ptr<AUD_IReader> reader = f->createQualityReader();
		reader->seek(start);
		boost::shared_ptr<AUD_IWriter> writer = AUD_FileWriter::createWriter(filename, specs, format, codec, bitrate);
		AUD_FileWriter::writeReader(reader, writer, length, buffersize);

		return NULL;
	}
	catch(AUD_Exception& e)
	{
		return e.str;
	}
}

const char *AUD_mixdown_per_channel(AUD_Sound *sound, unsigned int start, unsigned int length, unsigned int buffersize, const char *filename, AUD_DeviceSpecs specs, AUD_Container format, AUD_Codec codec, unsigned int bitrate)
{
	try {
		AUD_SequencerFactory *f = dynamic_cast<AUD_SequencerFactory *>(sound->get());

		f->setSpecs(specs.specs);

		std::vector<boost::shared_ptr<AUD_IWriter> > writers;

		int channels = specs.channels;
		specs.channels = AUD_CHANNELS_MONO;

		for (int i = 0; i < channels; i++) {
			std::stringstream stream;
			std::string fn = filename;
			size_t index = fn.find_last_of('.');
			size_t index_slash = fn.find_last_of('/');
			size_t index_backslash = fn.find_last_of('\\');

			if ((index == std::string::npos) ||
			    ((index < index_slash) && (index_slash != std::string::npos)) ||
			    ((index < index_backslash) && (index_backslash != std::string::npos)))
			{
				stream << filename << "_" << (i + 1);
			}
			else {
				stream << fn.substr(0, index) << "_" << (i + 1) << fn.substr(index);
			}
			writers.push_back(AUD_FileWriter::createWriter(stream.str(), specs, format, codec, bitrate));
		}

		boost::shared_ptr<AUD_IReader> reader = f->createQualityReader();
		reader->seek(start);
		AUD_FileWriter::writeReader(reader, writers, length, buffersize);

		return NULL;
	}
	catch(AUD_Exception& e)
	{
		return e.str;
	}
}

AUD_Device *AUD_openMixdownDevice(AUD_DeviceSpecs specs, AUD_Sound *sequencer, float volume, float start)
{
	try {
		AUD_ReadDevice *device = new AUD_ReadDevice(specs);
		device->setQuality(true);
		device->setVolume(volume);

		AUD_SequencerFactory *f = dynamic_cast<AUD_SequencerFactory *>(sequencer->get());

		f->setSpecs(specs.specs);

		AUD_Handle handle = device->play(f->createQualityReader());
		if (handle.get()) {
			handle->seek(start);
		}

		return new AUD_Device(device);
	}
	catch(AUD_Exception&)
	{
		return NULL;
	}
}

AUD_Device *AUD_Device_getCurrent(void)
{
	return &AUD_device;
}

int AUD_isJackSupported(void)
{
#ifdef WITH_JACK
	return AUD_jack_supported();
#else
	return 0;
#endif
}
