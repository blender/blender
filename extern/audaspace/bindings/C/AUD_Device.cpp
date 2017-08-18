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

#include "devices/DeviceManager.h"
#include "devices/I3DDevice.h"
#include "devices/IDeviceFactory.h"
#include "devices/ReadDevice.h"
#include "Exception.h"

#include <cassert>

using namespace aud;

#define AUD_CAPI_IMPLEMENTATION
#include "AUD_Device.h"

static inline aud::Specs convCToSpec(AUD_Specs specs)
{
	aud::Specs s;
	s.channels = static_cast<Channels>(specs.channels);
	s.rate = static_cast<SampleRate>(specs.rate);
	return s;
}

static inline aud::DeviceSpecs convCToDSpec(AUD_DeviceSpecs specs)
{
	aud::DeviceSpecs s;
	s.specs = convCToSpec(specs.specs);
	s.format = static_cast<SampleFormat>(specs.format);
	return s;
}

AUD_API AUD_Device* AUD_Device_open(const char* type, AUD_DeviceSpecs specs, int buffersize, const char* name)
{
	DeviceSpecs dspecs = convCToDSpec(specs);

	if(dspecs.channels == CHANNELS_INVALID)
		dspecs.channels = CHANNELS_STEREO;
	if(dspecs.format == FORMAT_INVALID)
		dspecs.format = FORMAT_FLOAT32;
	if(dspecs.rate == RATE_INVALID)
		dspecs.rate = RATE_48000;
	if(buffersize < 128)
		buffersize = AUD_DEFAULT_BUFFER_SIZE;
	if(name == nullptr)
		name = "";

	try
	{
		if(!type)
		{
			auto device = DeviceManager::getDevice();
			if(!device)
			{
				DeviceManager::openDefaultDevice();
				device = DeviceManager::getDevice();
			}
			return new AUD_Device(device);
		}

		if(type == std::string("read"))
		{
			return new AUD_Device(new ReadDevice(dspecs));
		}

		std::shared_ptr<IDeviceFactory> factory;
		if(!*type)
			factory = DeviceManager::getDefaultDeviceFactory();
		else
			factory = DeviceManager::getDeviceFactory(type);

		if(factory)
		{
			factory->setName(name);
			factory->setSpecs(dspecs);
			factory->setBufferSize(buffersize);
			return new AUD_Device(factory->openDevice());
		}
	}
	catch(Exception&)
	{
	}
	return nullptr;
}

AUD_API void AUD_Device_lock(AUD_Device* device)
{
	auto dev = device ? *device : DeviceManager::getDevice();
	dev->lock();
}

AUD_API AUD_Handle* AUD_Device_play(AUD_Device* device, AUD_Sound* sound, int keep)
{
	assert(sound);
	auto dev = device ? *device : DeviceManager::getDevice();

	try
	{
		AUD_Handle handle = dev->play(*sound, keep);
		if(handle.get())
		{
			return new AUD_Handle(handle);
		}
	}
	catch(Exception&)
	{
	}
	return nullptr;
}

AUD_API void AUD_Device_stopAll(AUD_Device* device)
{
	auto dev = device ? *device : DeviceManager::getDevice();
	dev->stopAll();
}

AUD_API void AUD_Device_unlock(AUD_Device* device)
{
	auto dev = device ? *device : DeviceManager::getDevice();
	dev->unlock();
}

AUD_API AUD_Channels AUD_Device_getChannels(AUD_Device* device)
{
	auto dev = device ? *device : DeviceManager::getDevice();
	return static_cast<AUD_Channels>(dev->getSpecs().channels);
}

AUD_API AUD_DistanceModel AUD_Device_getDistanceModel(AUD_Device* device)
{
	auto dev = device ? std::dynamic_pointer_cast<I3DDevice>(*device) : DeviceManager::get3DDevice();
	return static_cast<AUD_DistanceModel>(dev->getDistanceModel());
}

AUD_API void AUD_Device_setDistanceModel(AUD_Device* device, AUD_DistanceModel value)
{
	auto dev = device ? std::dynamic_pointer_cast<I3DDevice>(*device) : DeviceManager::get3DDevice();
	dev->setDistanceModel(static_cast<DistanceModel>(value));
}

AUD_API float AUD_Device_getDopplerFactor(AUD_Device* device)
{
	auto dev = device ? std::dynamic_pointer_cast<I3DDevice>(*device) : DeviceManager::get3DDevice();
	return dev->getDopplerFactor();
}

AUD_API void AUD_Device_setDopplerFactor(AUD_Device* device, float value)
{
	auto dev = device ? std::dynamic_pointer_cast<I3DDevice>(*device) : DeviceManager::get3DDevice();
	dev->setDopplerFactor(value);
}

AUD_API AUD_SampleFormat AUD_Device_getFormat(AUD_Device* device)
{
	auto dev = device ? *device : DeviceManager::getDevice();
	return static_cast<AUD_SampleFormat>(dev->getSpecs().format);
}

AUD_API void AUD_Device_getListenerLocation(AUD_Device* device, float value[3])
{
	auto dev = device ? std::dynamic_pointer_cast<I3DDevice>(*device) : DeviceManager::get3DDevice();
	Vector3 v = dev->getListenerLocation();
	value[0] = v.x();
	value[1] = v.y();
	value[2] = v.z();
}

AUD_API void AUD_Device_setListenerLocation(AUD_Device* device, const float value[3])
{
	auto dev = device ? std::dynamic_pointer_cast<I3DDevice>(*device) : DeviceManager::get3DDevice();
	Vector3 v(value[0], value[1], value[2]);
	dev->setListenerLocation(v);
}

AUD_API void AUD_Device_getListenerOrientation(AUD_Device* device, float value[4])
{
	auto dev = device ? std::dynamic_pointer_cast<I3DDevice>(*device) : DeviceManager::get3DDevice();
	Quaternion v = dev->getListenerOrientation();
	value[0] = v.x();
	value[1] = v.y();
	value[2] = v.z();
	value[3] = v.w();
}

AUD_API void AUD_Device_setListenerOrientation(AUD_Device* device, const float value[4])
{
	auto dev = device ? std::dynamic_pointer_cast<I3DDevice>(*device) : DeviceManager::get3DDevice();
	Quaternion v(value[3], value[0], value[1], value[2]);
	dev->setListenerOrientation(v);
}

AUD_API void AUD_Device_getListenerVelocity(AUD_Device* device, float value[3])
{
	auto dev = device ? std::dynamic_pointer_cast<I3DDevice>(*device) : DeviceManager::get3DDevice();
	Vector3 v = dev->getListenerVelocity();
	value[0] = v.x();
	value[1] = v.y();
	value[2] = v.z();
}

AUD_API void AUD_Device_setListenerVelocity(AUD_Device* device, const float value[3])
{
	auto dev = device ? std::dynamic_pointer_cast<I3DDevice>(*device) : DeviceManager::get3DDevice();
	Vector3 v(value[0], value[1], value[2]);
	dev->setListenerVelocity(v);
}

AUD_API double AUD_Device_getRate(AUD_Device* device)
{
	auto dev = device ? *device : DeviceManager::getDevice();
	return dev->getSpecs().rate;
}

AUD_API float AUD_Device_getSpeedOfSound(AUD_Device* device)
{
	auto dev = device ? std::dynamic_pointer_cast<I3DDevice>(*device) : DeviceManager::get3DDevice();
	return dev->getSpeedOfSound();
}

AUD_API void AUD_Device_setSpeedOfSound(AUD_Device* device, float value)
{
	auto dev = device ? std::dynamic_pointer_cast<I3DDevice>(*device) : DeviceManager::get3DDevice();
	dev->setSpeedOfSound(value);
}

AUD_API float AUD_Device_getVolume(AUD_Device* device)
{
	auto dev = device ? *device : DeviceManager::getDevice();
	return dev->getVolume();
}

AUD_API void AUD_Device_setVolume(AUD_Device* device, float value)
{
	auto dev = device ? *device : DeviceManager::getDevice();
	dev->setVolume(value);
}

AUD_API int AUD_Device_read(AUD_Device* device, unsigned char* buffer, int length)
{
	assert(device);
	assert(buffer);

	auto readDevice = std::dynamic_pointer_cast<ReadDevice>(*device);
	if(!readDevice)
		return false;

	try
	{
		return readDevice->read(buffer, length);
	}
	catch(Exception&)
	{
		return false;
	}
}

AUD_API void AUD_Device_free(AUD_Device* device)
{
	assert(device);

	try
	{
		delete device;
	}
	catch(Exception&)
	{
	}
}

AUD_API AUD_Device* AUD_Device_getCurrent()
{
	auto device = DeviceManager::getDevice();

	if(!device)
		return nullptr;

	return new AUD_Device(device);
}

AUD_API void AUD_seekSynchronizer(AUD_Handle* handle, float time)
{
	auto synchronizer = DeviceManager::getDevice()->getSynchronizer();
	if(synchronizer)
		synchronizer->seek(*reinterpret_cast<std::shared_ptr<IHandle>*>(handle), time);
}

AUD_API float AUD_getSynchronizerPosition(AUD_Handle* handle)
{
	auto synchronizer = DeviceManager::getDevice()->getSynchronizer();
	if(synchronizer)
		return synchronizer->getPosition(*reinterpret_cast<std::shared_ptr<IHandle>*>(handle));
	return (*reinterpret_cast<std::shared_ptr<IHandle>*>(handle))->getPosition();
}

AUD_API void AUD_playSynchronizer()
{
	auto synchronizer = DeviceManager::getDevice()->getSynchronizer();
	if(synchronizer)
		synchronizer->play();
}

AUD_API void AUD_stopSynchronizer()
{
	auto synchronizer = DeviceManager::getDevice()->getSynchronizer();
	if(synchronizer)
		synchronizer->stop();
}

AUD_API void AUD_setSynchronizerCallback(AUD_syncFunction function, void* data)
{
	auto synchronizer = DeviceManager::getDevice()->getSynchronizer();
	if(synchronizer)
		synchronizer->setSyncCallback(function, data);
}

AUD_API int AUD_isSynchronizerPlaying()
{
	auto synchronizer = DeviceManager::getDevice()->getSynchronizer();
	if(synchronizer)
		return synchronizer->isPlaying();
	return false;
}

