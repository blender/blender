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

#include "AUD_LoopReader.h"
#include "AUD_Buffer.h"

#include <cstring>

AUD_LoopReader::AUD_LoopReader(AUD_IReader* reader, int loop) :
		AUD_EffectReader(reader), m_count(loop), m_left(loop)
{
}

void AUD_LoopReader::seek(int position)
{
	int len = m_reader->getLength();
	if(len < 0)
		m_reader->seek(position);
	else
	{
		if(m_count >= 0)
		{
			m_left = m_count - (position / len);
			if(m_left < 0)
				m_left = 0;
		}
		m_reader->seek(position % len);
	}
}

int AUD_LoopReader::getLength() const
{
	if(m_count < 0)
		return -1;
	return m_reader->getLength() * m_count;
}

int AUD_LoopReader::getPosition() const
{
	return m_reader->getPosition() * (m_count < 0 ? 1 : m_count);
}

void AUD_LoopReader::read(int & length, sample_t* & buffer)
{
	AUD_Specs specs = m_reader->getSpecs();
	int samplesize = AUD_SAMPLE_SIZE(specs);

	int len = length;

	m_reader->read(len, buffer);

	if(len < length && m_left)
	{
		int pos = 0;

		if(m_buffer.getSize() < length * samplesize)
			m_buffer.resize(length * samplesize);

		sample_t* buf = m_buffer.getBuffer();

		memcpy(buf + pos * specs.channels, buffer, len * samplesize);

		pos += len;

		while(pos < length && m_left)
		{
			if(m_left > 0)
				m_left--;

			m_reader->seek(0);

			len = length - pos;
			m_reader->read(len, buffer);

			// prevent endless loop
			if(!len)
				break;

			memcpy(buf + pos * specs.channels, buffer, len * samplesize);

			pos += len;
		}

		length = pos;
		buffer = buf;
	}
	else
		length = len;
}
