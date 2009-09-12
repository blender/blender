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

#include "AUD_FloatMixer.h"
#include "AUD_ReadDevice.h"
#include "AUD_IReader.h"

#include <cstring>

AUD_ReadDevice::AUD_ReadDevice(AUD_Specs specs)
{
	m_specs = specs;

	m_mixer = new AUD_FloatMixer(); AUD_NEW("mixer")
	m_mixer->setSpecs(m_specs);

	m_playing = false;

	create();
}

AUD_ReadDevice::~AUD_ReadDevice()
{
	destroy();
}

bool AUD_ReadDevice::read(sample_t* buffer, int length)
{
	if(m_playing)
		mix(buffer, length);
	else
		if(m_specs.format == AUD_FORMAT_U8)
			memset(buffer, 0x80, length * AUD_SAMPLE_SIZE(m_specs));
		else
			memset(buffer, 0, length * AUD_SAMPLE_SIZE(m_specs));
	return m_playing;
}

void AUD_ReadDevice::playing(bool playing)
{
	m_playing = playing;
}
