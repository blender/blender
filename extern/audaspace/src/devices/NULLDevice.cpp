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

#include "devices/NULLDevice.h"
#include "devices/DeviceManager.h"
#include "devices/IDeviceFactory.h"

#include <limits>
#include <string>

AUD_NAMESPACE_BEGIN

NULLDevice::NULLHandle::NULLHandle()
{
}

bool NULLDevice::NULLHandle::pause()
{
	return false;
}

bool NULLDevice::NULLHandle::resume()
{
	return false;
}

bool NULLDevice::NULLHandle::stop()
{
	return false;
}

bool NULLDevice::NULLHandle::getKeep()
{
	return false;
}

bool NULLDevice::NULLHandle::setKeep(bool keep)
{
	return false;
}

bool NULLDevice::NULLHandle::seek(double position)
{
	return false;
}

double NULLDevice::NULLHandle::getPosition()
{
	return std::numeric_limits<float>::quiet_NaN();
}

Status NULLDevice::NULLHandle::getStatus()
{
	return STATUS_INVALID;
}

float NULLDevice::NULLHandle::getVolume()
{
	return std::numeric_limits<float>::quiet_NaN();
}

bool NULLDevice::NULLHandle::setVolume(float volume)
{
	return false;
}

float NULLDevice::NULLHandle::getPitch()
{
	return std::numeric_limits<float>::quiet_NaN();
}

bool NULLDevice::NULLHandle::setPitch(float pitch)
{
	return false;
}

int NULLDevice::NULLHandle::getLoopCount()
{
	return 0;
}

bool NULLDevice::NULLHandle::setLoopCount(int count)
{
	return false;
}

bool NULLDevice::NULLHandle::setStopCallback(stopCallback callback, void* data)
{
	return false;
}

NULLDevice::NULLDevice()
{
}

NULLDevice::~NULLDevice()
{
}

DeviceSpecs NULLDevice::getSpecs() const
{
	DeviceSpecs specs;
	specs.channels = CHANNELS_INVALID;
	specs.format = FORMAT_INVALID;
	specs.rate = RATE_INVALID;
	return specs;
}

std::shared_ptr<IHandle> NULLDevice::play(std::shared_ptr<IReader> reader, bool keep)
{
	return std::shared_ptr<IHandle>(new NULLHandle());
}

std::shared_ptr<IHandle> NULLDevice::play(std::shared_ptr<ISound> sound, bool keep)
{
	return std::shared_ptr<IHandle>(new NULLHandle());
}

void NULLDevice::stopAll()
{
}

void NULLDevice::lock()
{
}

void NULLDevice::unlock()
{
}

float NULLDevice::getVolume() const
{
	return std::numeric_limits<float>::quiet_NaN();
}

void NULLDevice::setVolume(float volume)
{
}

ISynchronizer* NULLDevice::getSynchronizer()
{
	return nullptr;
}

class NULLDeviceFactory : public IDeviceFactory
{
public:
	NULLDeviceFactory()
	{
	}

	virtual std::shared_ptr<IDevice> openDevice()
	{
		return std::shared_ptr<IDevice>(new NULLDevice());
	}

	virtual int getPriority()
	{
		return std::numeric_limits<int>::min();
	}

	virtual void setSpecs(DeviceSpecs specs)
	{
	}

	virtual void setBufferSize(int buffersize)
	{
	}

	virtual void setName(std::string name)
	{
	}
};

void NULLDevice::registerPlugin()
{
	DeviceManager::registerDevice("None", std::shared_ptr<IDeviceFactory>(new NULLDeviceFactory));
}

AUD_NAMESPACE_END
