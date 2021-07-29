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

/** \file audaspace/intern/AUD_NULLDevice.cpp
 *  \ingroup audaspaceintern
 */


#include <limits>

#include "AUD_NULLDevice.h"

AUD_NULLDevice::AUD_NULLHandle::AUD_NULLHandle()
{
}

bool AUD_NULLDevice::AUD_NULLHandle::pause()
{
	return false;
}

bool AUD_NULLDevice::AUD_NULLHandle::resume()
{
	return false;
}

bool AUD_NULLDevice::AUD_NULLHandle::stop()
{
	return false;
}

bool AUD_NULLDevice::AUD_NULLHandle::getKeep()
{
	return false;
}

bool AUD_NULLDevice::AUD_NULLHandle::setKeep(bool keep)
{
	return false;
}

bool AUD_NULLDevice::AUD_NULLHandle::seek(float position)
{
	return false;
}

float AUD_NULLDevice::AUD_NULLHandle::getPosition()
{
	return std::numeric_limits<float>::quiet_NaN();
}

AUD_Status AUD_NULLDevice::AUD_NULLHandle::getStatus()
{
	return AUD_STATUS_INVALID;
}

float AUD_NULLDevice::AUD_NULLHandle::getVolume()
{
	return std::numeric_limits<float>::quiet_NaN();
}

bool AUD_NULLDevice::AUD_NULLHandle::setVolume(float volume)
{
	return false;
}

float AUD_NULLDevice::AUD_NULLHandle::getPitch()
{
	return std::numeric_limits<float>::quiet_NaN();
}

bool AUD_NULLDevice::AUD_NULLHandle::setPitch(float pitch)
{
	return false;
}

int AUD_NULLDevice::AUD_NULLHandle::getLoopCount()
{
	return 0;
}

bool AUD_NULLDevice::AUD_NULLHandle::setLoopCount(int count)
{
	return false;
}

bool AUD_NULLDevice::AUD_NULLHandle::setStopCallback(stopCallback callback, void* data)
{
	return false;
}

AUD_NULLDevice::AUD_NULLDevice()
{
}

AUD_NULLDevice::~AUD_NULLDevice()
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

boost::shared_ptr<AUD_IHandle> AUD_NULLDevice::play(boost::shared_ptr<AUD_IReader> reader, bool keep)
{
	return boost::shared_ptr<AUD_IHandle>(new AUD_NULLHandle());
}

boost::shared_ptr<AUD_IHandle> AUD_NULLDevice::play(boost::shared_ptr<AUD_IFactory> factory, bool keep)
{
	return boost::shared_ptr<AUD_IHandle>(new AUD_NULLHandle());
}

void AUD_NULLDevice::stopAll()
{
}

void AUD_NULLDevice::lock()
{
}

void AUD_NULLDevice::unlock()
{
}

float AUD_NULLDevice::getVolume() const
{
	return std::numeric_limits<float>::quiet_NaN();
}

void AUD_NULLDevice::setVolume(float volume)
{
}
