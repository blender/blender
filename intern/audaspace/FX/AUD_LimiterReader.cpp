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

#include "AUD_LimiterReader.h"
#include "AUD_Buffer.h"

#include <iostream>

AUD_LimiterReader::AUD_LimiterReader(AUD_IReader* reader,
									 float start, float end) :
		AUD_EffectReader(reader)
{
	m_end = (int)(end * reader->getSpecs().rate);

	if(start <= 0)
		m_start = 0;
	else
	{
		m_start = (int)(start * reader->getSpecs().rate);
		if(m_reader->isSeekable())
			m_reader->seek(m_start);
		else
		{
			// skip first m_start samples by reading them
			int length;
			sample_t* buffer;
			for(int i = m_start;
				i >= AUD_DEFAULT_BUFFER_SIZE;
				i -= AUD_DEFAULT_BUFFER_SIZE)
			{
				length = AUD_DEFAULT_BUFFER_SIZE;
				m_reader->read(length, buffer);
				length = i;
			}
			m_reader->read(length, buffer);
		}
	}
}

void AUD_LimiterReader::seek(int position)
{
	m_reader->seek(position + m_start);
}

int AUD_LimiterReader::getLength()
{
	int len = m_reader->getLength();
	if(m_reader->getType() != AUD_TYPE_BUFFER || len < 0 ||
	   (len > m_end && m_end >= 0))
		len = m_end;
	return len - m_start;
}

int AUD_LimiterReader::getPosition()
{
	return m_reader->getPosition() - m_start;
}

void AUD_LimiterReader::read(int & length, sample_t* & buffer)
{
	if(m_end >= 0)
	{
		int position = m_reader->getPosition();
		if(position+length > m_end)
			length = m_end - position;
		if(length < 0)
		{
			length = 0;
			return;
		}
	}
	m_reader->read(length, buffer);
}
