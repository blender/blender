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

/** \file audaspace/FX/AUD_DelayReader.cpp
 *  \ingroup audfx
 */


#include "AUD_DelayReader.h"

#include <cstring>

AUD_DelayReader::AUD_DelayReader(boost::shared_ptr<AUD_IReader> reader, float delay) :
		AUD_EffectReader(reader),
		m_delay(int((AUD_SampleRate)delay * reader->getSpecs().rate)),
		m_remdelay(int((AUD_SampleRate)delay * reader->getSpecs().rate))
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

void AUD_DelayReader::read(int& length, bool& eos, sample_t* buffer)
{
	if(m_remdelay > 0)
	{
		AUD_Specs specs = m_reader->getSpecs();
		int samplesize = AUD_SAMPLE_SIZE(specs);

		if(length > m_remdelay)
		{
			memset(buffer, 0, m_remdelay * samplesize);

			int len = length - m_remdelay;
			m_reader->read(len, eos, buffer + m_remdelay * specs.channels);

			length = m_remdelay + len;

			m_remdelay = 0;
		}
		else
		{
			memset(buffer, 0, length * samplesize);
			m_remdelay -= length;
		}
	}
	else
		m_reader->read(length, eos, buffer);
}
