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

#include "AUD_SquareReader.h"
#include "AUD_Buffer.h"

#include <cstring>

AUD_SquareReader::AUD_SquareReader(AUD_IReader* reader, float threshold) :
		AUD_EffectReader(reader),
		m_threshold(threshold)
{
	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
}

AUD_SquareReader::~AUD_SquareReader()
{
	delete m_buffer; AUD_DELETE("buffer")
}

void AUD_SquareReader::read(int & length, sample_t* & buffer)
{
	sample_t* buf;
	AUD_Specs specs = m_reader->getSpecs();

	m_reader->read(length, buf);
	if(m_buffer->getSize() < length * AUD_SAMPLE_SIZE(specs))
		m_buffer->resize(length * AUD_SAMPLE_SIZE(specs));

	buffer = m_buffer->getBuffer();

	for(int i = 0; i < length * specs.channels; i++)
	{
		if(buf[i] >= m_threshold)
			buffer[i] = 1.0f;
		else if(buf[i] <= -m_threshold)
			buffer[i] = -1.0f;
		else
			buffer[i] = 0.0f;
	}
}
