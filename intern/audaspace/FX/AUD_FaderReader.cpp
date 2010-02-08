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

#include "AUD_FaderReader.h"
#include "AUD_Buffer.h"

#include <cstring>

AUD_FaderReader::AUD_FaderReader(AUD_IReader* reader, AUD_FadeType type,
								 float start,float length) :
		AUD_EffectReader(reader),
		m_type(type),
		m_start(start),
		m_length(length)
{
	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
}

AUD_FaderReader::~AUD_FaderReader()
{
	delete m_buffer; AUD_DELETE("buffer")
}

bool AUD_FaderReader::notify(AUD_Message &message)
{
	return m_reader->notify(message);
}

void AUD_FaderReader::read(int & length, sample_t* & buffer)
{
	int position = m_reader->getPosition();
	AUD_Specs specs = m_reader->getSpecs();
	int samplesize = AUD_SAMPLE_SIZE(specs);

	m_reader->read(length, buffer);

	if(m_buffer->getSize() < length * samplesize)
		m_buffer->resize(length * samplesize);

	if((position + length) / (float)specs.rate <= m_start)
	{
		if(m_type != AUD_FADE_OUT)
		{
			buffer = m_buffer->getBuffer();
			memset(buffer, 0, length * samplesize);
		}
	}
	else if(position / (float)specs.rate >= m_start+m_length)
	{
		if(m_type == AUD_FADE_OUT)
		{
			buffer = m_buffer->getBuffer();
			memset(buffer, 0, length * samplesize);
		}
	}
	else
	{
		sample_t* buf = m_buffer->getBuffer();
		float volume = 1.0f;

		for(int i = 0; i < length * specs.channels; i++)
		{
			if(i % specs.channels == 0)
			{
				volume = (((position+i)/(float)specs.rate)-m_start) / m_length;
				if(volume > 1.0f)
					volume = 1.0f;
				else if(volume < 0.0f)
					volume = 0.0f;

				if(m_type == AUD_FADE_OUT)
					volume = 1.0f - volume;
			}

			buf[i] = buffer[i] * volume;
		}

		buffer = buf;
	}
}
