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
#include <stdio.h>

AUD_LoopReader::AUD_LoopReader(AUD_IReader* reader, int loop) :
		AUD_EffectReader(reader), m_loop(loop)
{
	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
}

AUD_LoopReader::~AUD_LoopReader()
{
	delete m_buffer; AUD_DELETE("buffer")
}

AUD_ReaderType AUD_LoopReader::getType()
{
	if(m_loop < 0)
		return AUD_TYPE_STREAM;
	return m_reader->getType();
}

bool AUD_LoopReader::notify(AUD_Message &message)
{
	if(message.type == AUD_MSG_LOOP)
	{
		m_loop = message.loopcount;

		m_reader->notify(message);

		return true;
	}
	return m_reader->notify(message);
}

void AUD_LoopReader::read(int & length, sample_t* & buffer)
{
	int samplesize = AUD_SAMPLE_SIZE(m_reader->getSpecs());

	int len = length;

	m_reader->read(len, buffer);

	if(len < length && m_loop != 0)
	{
		int pos = 0;

		if(m_buffer->getSize() < length*samplesize)
			m_buffer->resize(length*samplesize);

		memcpy(m_buffer->getBuffer() + pos * samplesize,
			   buffer, len * samplesize);

		pos += len;

		while(pos < length && m_loop != 0)
		{
			if(m_loop > 0)
				m_loop--;

			m_reader->seek(0);

			len = length - pos;
			m_reader->read(len, buffer);
			// prevent endless loop
			if(!len)
				break;

			memcpy(m_buffer->getBuffer() + pos * samplesize,
				   buffer, len * samplesize);

			pos += len;
		}

		length = pos;
		buffer = m_buffer->getBuffer();
	}
	else
		length = len;
}
