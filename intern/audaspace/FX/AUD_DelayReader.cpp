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

#include "AUD_DelayReader.h"
#include "AUD_Buffer.h"

#include <cstring>

AUD_DelayReader::AUD_DelayReader(AUD_IReader* reader, float delay) :
		AUD_EffectReader(reader)
{
	m_delay = (int)(delay * reader->getSpecs().rate);
	m_remdelay = m_delay;
	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
}

AUD_DelayReader::~AUD_DelayReader()
{
	delete m_buffer; AUD_DELETE("buffer")
}

void AUD_DelayReader::seek(int position)
{
	if(position < 0)
		return;

	if(position < m_delay)
	{
		m_remdelay = m_delay - position;
		m_reader->seek(0);
	}
	else
	{
		m_remdelay = 0;
		m_reader->seek(position - m_delay);
	}
}

int AUD_DelayReader::getLength()
{
	int len = m_reader->getLength();
	if(len < 0)
		return len;
	return len+m_delay;
}

int AUD_DelayReader::getPosition()
{
	if(m_remdelay > 0)
		return m_delay-m_remdelay;
	return m_reader->getPosition() + m_delay;
}

void AUD_DelayReader::read(int & length, sample_t* & buffer)
{
	if(m_remdelay > 0)
	{
		AUD_Specs specs = m_reader->getSpecs();
		int samplesize = AUD_SAMPLE_SIZE(specs);

		if(m_buffer->getSize() < length * samplesize)
			m_buffer->resize(length * samplesize);

		if(length > m_remdelay)
		{
			memset(m_buffer->getBuffer(), 0, m_remdelay * samplesize);
			int len = length - m_remdelay;
			m_reader->read(len, buffer);
			memcpy(m_buffer->getBuffer() + m_remdelay * specs.channels,
				   buffer, len * samplesize);
			if(len < length-m_remdelay)
				length = m_remdelay + len;
			m_remdelay = 0;
		}
		else
		{
			memset(m_buffer->getBuffer(), 0, length * samplesize);
			m_remdelay -= length;
		}
		buffer = m_buffer->getBuffer();
	}
	else
		m_reader->read(length, buffer);
}
