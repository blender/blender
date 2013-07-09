/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/FX/AUD_FaderReader.cpp
 *  \ingroup audfx
 */


#include "AUD_FaderReader.h"

#include <cstring>

AUD_FaderReader::AUD_FaderReader(boost::shared_ptr<AUD_IReader> reader, AUD_FadeType type,
								 float start,float length) :
		AUD_EffectReader(reader),
		m_type(type),
		m_start(start),
		m_length(length)
{
}

void AUD_FaderReader::read(int& length, bool& eos, sample_t* buffer)
{
	int position = m_reader->getPosition();
	AUD_Specs specs = m_reader->getSpecs();
	int samplesize = AUD_SAMPLE_SIZE(specs);

	m_reader->read(length, eos, buffer);

	if((position + length) / (float)specs.rate <= m_start)
	{
		if(m_type != AUD_FADE_OUT)
		{
			memset(buffer, 0, length * samplesize);
		}
	}
	else if(position / (float)specs.rate >= m_start+m_length)
	{
		if(m_type == AUD_FADE_OUT)
		{
			memset(buffer, 0, length * samplesize);
		}
	}
	else
	{
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

			buffer[i] = buffer[i] * volume;
		}
	}
}
