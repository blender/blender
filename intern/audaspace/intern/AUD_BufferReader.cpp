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

#include "AUD_BufferReader.h"
#include "AUD_Buffer.h"
#include "AUD_Space.h"

AUD_BufferReader::AUD_BufferReader(AUD_Reference<AUD_Buffer> buffer,
								   AUD_Specs specs)
{
	m_position = 0;
	m_buffer = buffer;
	m_specs = specs;
}

bool AUD_BufferReader::isSeekable()
{
	return true;
}

void AUD_BufferReader::seek(int position)
{
	if(position < 0)
		m_position = 0;
	else if(position > m_buffer.get()->getSize() / AUD_SAMPLE_SIZE(m_specs))
		m_position = m_buffer.get()->getSize() / AUD_SAMPLE_SIZE(m_specs);
	else
		m_position = position;
}

int AUD_BufferReader::getLength()
{
	return m_buffer.get()->getSize()/AUD_SAMPLE_SIZE(m_specs);
}

int AUD_BufferReader::getPosition()
{
	return m_position;
}

AUD_Specs AUD_BufferReader::getSpecs()
{
	return m_specs;
}

AUD_ReaderType AUD_BufferReader::getType()
{
	return AUD_TYPE_BUFFER;
}

bool AUD_BufferReader::notify(AUD_Message &message)
{
	return false;
}

void AUD_BufferReader::read(int & length, sample_t* & buffer)
{
	int sample_size = AUD_SAMPLE_SIZE(m_specs);

	buffer = m_buffer.get()->getBuffer() + m_position * m_specs.channels;

	// in case the end of the buffer is reached
	if(m_buffer.get()->getSize() < (m_position + length) * sample_size)
		length = m_buffer.get()->getSize() / sample_size - m_position;

	if(length < 0)
		length = 0;
	m_position += length;
}
