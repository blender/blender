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
#include "AUD_IReader.h"
#include "AUD_IFactory.h"

AUD_NULLDevice::AUD_NULLDevice()
{
	m_specs.channels = AUD_CHANNELS_INVALID;
	m_specs.format = AUD_FORMAT_INVALID;
	m_specs.rate = AUD_RATE_INVALID;
}

AUD_Specs AUD_NULLDevice::getSpecs()
{
	return m_specs;
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

bool AUD_NULLDevice::setKeep(AUD_Handle* handle, bool keep)
{
	return false;
}

bool AUD_NULLDevice::sendMessage(AUD_Handle* handle, AUD_Message &message)
{
	return false;
}

bool AUD_NULLDevice::seek(AUD_Handle* handle, float position)
{
	return false;
}

float AUD_NULLDevice::getPosition(AUD_Handle* handle)
{
	return 0.0f;
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

bool AUD_NULLDevice::checkCapability(int capability)
{
	return false;
}

bool AUD_NULLDevice::setCapability(int capability, void *value)
{
	return false;
}

bool AUD_NULLDevice::getCapability(int capability, void *value)
{
	return false;
}
