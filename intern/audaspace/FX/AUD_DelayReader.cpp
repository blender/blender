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

#include <cstring>

AUD_DelayReader::AUD_DelayReader(AUD_IReader* reader, float delay) :
		AUD_EffectReader(reader),
		m_delay(int(delay * reader->getSpecs().rate)),
		m_remdelay(int(delay * reader->getSpecs().rate)),
		m_empty(true)
{
}

void AUD_DelayReader::seek(int position)
{
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

int AUD_DelayReader::getLength() const
{
	int len = m_reader->getLength();
	if(len < 0)
		return len;
	return len + m_delay;
}

int AUD_DelayReader::getPosition() const
{
	if(m_remdelay > 0)
		return m_delay - m_remdelay;
	return m_reader->getPosition() + m_delay;
}

void AUD_DelayReader::read(int & length, sample_t* & buffer)
{
	if(m_remdelay > 0)
	{
		AUD_Specs specs = m_reader->getSpecs();
		int samplesize = AUD_SAMPLE_SIZE(specs);

		if(m_buffer.getSize() < length * samplesize)
		{
			m_buffer.resize(length * samplesize);
			m_empty = false;
		}

		buffer = m_buffer.getBuffer();

		if(length > m_remdelay)
		{
			if(!m_empty)
				memset(buffer, 0, m_remdelay * samplesize);

			int len = length - m_remdelay;
			sample_t* buf;
			m_reader->read(len, buf);

			memcpy(buffer + m_remdelay * specs.channels,
				   buf, len * samplesize);

			if(len < length-m_remdelay)
				length = m_remdelay + len;

			m_remdelay = 0;
			m_empty = false;
		}
		else
		{
			if(!m_empty)
			{
				memset(buffer, 0, length * samplesize);
				m_empty = true;
			}
			m_remdelay -= length;
		}
	}
	else
		m_reader->read(length, buffer);
}
