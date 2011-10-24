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

/** \file audaspace/intern/AUD_SinusReader.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_SinusReader.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

AUD_SinusReader::AUD_SinusReader(float frequency, AUD_SampleRate sampleRate) :
	m_frequency(frequency),
	m_position(0),
	m_sampleRate(sampleRate)
{
}

bool AUD_SinusReader::isSeekable() const
{
	return true;
}

void AUD_SinusReader::seek(int position)
{
	m_position = position;
}

int AUD_SinusReader::getLength() const
{
	return -1;
}

int AUD_SinusReader::getPosition() const
{
	return m_position;
}

AUD_Specs AUD_SinusReader::getSpecs() const
{
	AUD_Specs specs;
	specs.rate = m_sampleRate;
	specs.channels = AUD_CHANNELS_MONO;
	return specs;
}

void AUD_SinusReader::read(int& length, bool& eos, sample_t* buffer)
{
	// fill with sine data
	for(int i = 0; i < length; i++)
	{
		buffer[i] = sin((m_position + i) * 2 * M_PI * m_frequency / m_sampleRate);
	}

	m_position += length;
	eos = false;
}
