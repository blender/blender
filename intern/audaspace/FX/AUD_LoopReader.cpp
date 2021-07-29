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

/** \file audaspace/FX/AUD_LoopReader.cpp
 *  \ingroup audfx
 */


#include "AUD_LoopReader.h"
#include "AUD_Buffer.h"

#include <cstring>

AUD_LoopReader::AUD_LoopReader(boost::shared_ptr<AUD_IReader> reader, int loop) :
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

void AUD_LoopReader::read(int& length, bool& eos, sample_t* buffer)
{
	const AUD_Specs specs = m_reader->getSpecs();

	int len = length;

	m_reader->read(length, eos, buffer);

	if(length < len && eos && m_left)
	{
		int pos = length;
		length = len;

		while(pos < length && eos && m_left)
		{
			if(m_left > 0)
				m_left--;

			m_reader->seek(0);

			len = length - pos;
			m_reader->read(len, eos, buffer + pos * specs.channels);

			// prevent endless loop
			if(!len)
				break;

			pos += len;
		}

		length = pos;
	}
}
