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

#include "AUD_SilenceReader.h"

#include <cstring>

AUD_SilenceReader::AUD_SilenceReader() :
	m_position(0)
{
}

bool AUD_SilenceReader::isSeekable() const
{
	return true;
}

void AUD_SilenceReader::seek(int position)
{
	m_position = position;
}

int AUD_SilenceReader::getLength() const
{
	return -1;
}

int AUD_SilenceReader::getPosition() const
{
	return m_position;
}

AUD_Specs AUD_SilenceReader::getSpecs() const
{
	AUD_Specs specs;
	specs.rate = AUD_RATE_44100;
	specs.channels = AUD_CHANNELS_MONO;
	return specs;
}

void AUD_SilenceReader::read(int & length, sample_t* & buffer)
{
	// resize if necessary
	if(m_buffer.getSize() < length * sizeof(sample_t))
	{
		m_buffer.resize(length * sizeof(sample_t));
		memset(m_buffer.getBuffer(), 0, m_buffer.getSize());
	}

	buffer = m_buffer.getBuffer();
	m_position += length;
}
