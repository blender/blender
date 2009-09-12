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

#include "AUD_SinusReader.h"
#include "AUD_Buffer.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

AUD_SinusReader::AUD_SinusReader(double frequency, AUD_SampleRate sampleRate)
{
	m_frequency = frequency;
	m_position = 0;
	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
	m_sampleRate = sampleRate;
}

AUD_SinusReader::~AUD_SinusReader()
{
	delete m_buffer; AUD_DELETE("buffer")
}

bool AUD_SinusReader::isSeekable()
{
	return true;
}

void AUD_SinusReader::seek(int position)
{
	m_position = position;
}

int AUD_SinusReader::getLength()
{
	return -1;
}

int AUD_SinusReader::getPosition()
{
	return m_position;
}

AUD_Specs AUD_SinusReader::getSpecs()
{
	AUD_Specs specs;
	specs.rate = m_sampleRate;
	specs.format = AUD_FORMAT_S16;
	specs.channels = AUD_CHANNELS_STEREO;
	return specs;
}

AUD_ReaderType AUD_SinusReader::getType()
{
	return AUD_TYPE_STREAM;
}

bool AUD_SinusReader::notify(AUD_Message &message)
{
	return false;
}

void AUD_SinusReader::read(int & length, sample_t* & buffer)
{
	// resize if necessary
	if(m_buffer->getSize() < length*4)
		m_buffer->resize(length*4);

	// fill with sine data
	short* buf = (short*) m_buffer->getBuffer();
	for(int i=0; i < length; i++)
	{
		buf[i*2] = sin((m_position + i) * 2.0 * M_PI * m_frequency /
					   (float)m_sampleRate) * 32700;
		buf[i*2+1] = buf[i*2];
	}

	buffer = (sample_t*)buf;
	m_position += length;
}
