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

/** \file audaspace/intern/AUD_ReadDevice.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_ReadDevice.h"
#include "AUD_IReader.h"

#include <cstring>

AUD_ReadDevice::AUD_ReadDevice(AUD_DeviceSpecs specs) :
	m_playing(false)
{
	m_specs = specs;

	create();
}

AUD_ReadDevice::AUD_ReadDevice(AUD_Specs specs) :
	m_playing(false)
{
	m_specs.specs = specs;
	m_specs.format = AUD_FORMAT_FLOAT32;

	create();
}

AUD_ReadDevice::~AUD_ReadDevice()
{
	destroy();
}

bool AUD_ReadDevice::read(data_t* buffer, int length)
{
	if(m_playing)
		mix(buffer, length);
	else
		if(m_specs.format == AUD_FORMAT_U8)
			memset(buffer, 0x80, length * AUD_DEVICE_SAMPLE_SIZE(m_specs));
		else
			memset(buffer, 0, length * AUD_DEVICE_SAMPLE_SIZE(m_specs));
	return m_playing;
}

void AUD_ReadDevice::changeSpecs(AUD_Specs specs)
{
	if(!AUD_COMPARE_SPECS(specs, m_specs.specs))
		setSpecs(specs);
}

void AUD_ReadDevice::playing(bool playing)
{
	m_playing = playing;
}
