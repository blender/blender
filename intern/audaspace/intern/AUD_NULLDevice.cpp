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

#include <limits>

#include "AUD_NULLDevice.h"
#include "AUD_IReader.h"
#include "AUD_IFactory.h"

AUD_NULLDevice::AUD_NULLDevice()
{
}

AUD_DeviceSpecs AUD_NULLDevice::getSpecs() const
{
	AUD_DeviceSpecs specs;
	specs.channels = AUD_CHANNELS_INVALID;
	specs.format = AUD_FORMAT_INVALID;
	specs.rate = AUD_RATE_INVALID;
	return specs;
}

AUD_Handle* AUD_NULLDevice::play(AUD_IFactory* factory, bool keep)
{
	return 0;
}

bool AUD_NULLDevice::pause(AUD_Handle* handle)
{
	return false;
}

bool AUD_NULLDevice::resume(AUD_Handle* handle)
{
	return false;
}

bool AUD_NULLDevice::stop(AUD_Handle* handle)
{
	return false;
}

bool AUD_NULLDevice::getKeep(AUD_Handle* handle)
{
	return false;
}

bool AUD_NULLDevice::setKeep(AUD_Handle* handle, bool keep)
{
	return false;
}

bool AUD_NULLDevice::seek(AUD_Handle* handle, float position)
{
	return false;
}

float AUD_NULLDevice::getPosition(AUD_Handle* handle)
{
	return std::numeric_limits<float>::quiet_NaN();
}

AUD_Status AUD_NULLDevice::getStatus(AUD_Handle* handle)
{
	return AUD_STATUS_INVALID;
}

void AUD_NULLDevice::lock()
{
}

void AUD_NULLDevice::unlock()
{
}

float AUD_NULLDevice::getVolume() const
{
	return 0;
}

void AUD_NULLDevice::setVolume(float volume)
{
}

float AUD_NULLDevice::getVolume(AUD_Handle* handle)
{
	return std::numeric_limits<float>::quiet_NaN();
}

bool AUD_NULLDevice::setVolume(AUD_Handle* handle, float volume)
{
	return false;
}

float AUD_NULLDevice::getPitch(AUD_Handle* handle)
{
	return std::numeric_limits<float>::quiet_NaN();
}

bool AUD_NULLDevice::setPitch(AUD_Handle* handle, float pitch)
{
	return false;
}

int AUD_NULLDevice::getLoopCount(AUD_Handle* handle)
{
	return 0;
}

bool AUD_NULLDevice::setLoopCount(AUD_Handle* handle, int count)
{
	return false;
}

bool AUD_NULLDevice::setStopCallback(AUD_Handle* handle, stopCallback callback, void* data)
{
	return false;
}
