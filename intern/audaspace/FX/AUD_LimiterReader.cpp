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

/** \file audaspace/FX/AUD_LimiterReader.cpp
 *  \ingroup audfx
 */


#include "AUD_LimiterReader.h"
#include "AUD_Buffer.h"

AUD_LimiterReader::AUD_LimiterReader(boost::shared_ptr<AUD_IReader> reader,
									 float start, float end) :
		AUD_EffectReader(reader),
		m_start(start),
		m_end(end)
{
	if(m_start > 0)
	{
		AUD_Specs specs = m_reader->getSpecs();
		AUD_Specs specs2;

		if(m_reader->isSeekable())
			m_reader->seek(m_start * specs.rate);
		else
		{
			// skip first m_start samples by reading them
			int length = AUD_DEFAULT_BUFFER_SIZE;
			AUD_Buffer buffer(AUD_DEFAULT_BUFFER_SIZE * AUD_SAMPLE_SIZE(specs));
			bool eos = false;
			for(int len = m_start * specs.rate;
				length > 0 && !eos;
				len -= length)
			{
				if(len < AUD_DEFAULT_BUFFER_SIZE)
					length = len;

				m_reader->read(length, eos, buffer.getBuffer());

				specs2 = m_reader->getSpecs();
				if(specs2.rate != specs.rate)
				{
					len = len * specs2.rate / specs.rate;
					specs.rate = specs2.rate;
				}

				if(specs2.channels != specs.channels)
				{
					specs = specs2;
					buffer.assureSize(AUD_DEFAULT_BUFFER_SIZE * AUD_SAMPLE_SIZE(specs));
				}
			}
		}
	}
}

void AUD_LimiterReader::seek(int position)
{
	m_reader->seek(position + m_start * m_reader->getSpecs().rate);
}

int AUD_LimiterReader::getLength() const
{
	int len = m_reader->getLength();
	AUD_SampleRate rate = m_reader->getSpecs().rate;
	if(len < 0 || (len > m_end * rate && m_end >= 0))
		len = m_end * rate;
	return len - m_start * rate;
}

int AUD_LimiterReader::getPosition() const
{
	int pos = m_reader->getPosition();
	AUD_SampleRate rate = m_reader->getSpecs().rate;
	return AUD_MIN(pos, m_end * rate) - m_start * rate;
}

void AUD_LimiterReader::read(int& length, bool& eos, sample_t* buffer)
{
	eos = false;
	if(m_end >= 0)
	{
		int position = m_reader->getPosition();
		AUD_SampleRate rate = m_reader->getSpecs().rate;

		if(position + length > m_end * rate)
		{
			length = m_end * rate - position;
			eos = true;
		}

		if(position < m_start * rate)
		{
			int len2 = length;
			for(int len = m_start * rate - position;
				len2 == length && !eos;
				len -= length)
			{
				if(len < length)
					len2 = len;

				m_reader->read(len2, eos, buffer);
				position += len2;
			}

			if(position < m_start * rate)
			{
				length = 0;
				return;
			}
		}

		if(length < 0)
		{
			length = 0;
			return;
		}
	}
	if(eos)
	{
		m_reader->read(length, eos, buffer);
		eos = true;
	}
	else
		m_reader->read(length, eos, buffer);
}
