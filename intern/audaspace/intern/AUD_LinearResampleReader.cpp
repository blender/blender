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

/** \file audaspace/intern/AUD_LinearResampleReader.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_LinearResampleReader.h"

#include <cmath>
#include <cstring>

#define CC m_channels + channel

AUD_LinearResampleReader::AUD_LinearResampleReader(boost::shared_ptr<AUD_IReader> reader,
												   AUD_Specs specs) :
	AUD_ResampleReader(reader, specs.rate),
	m_channels(reader->getSpecs().channels),
	m_cache_pos(0),
	m_cache_ok(false)
{
	specs.channels = m_channels;
	m_cache.resize(2 * AUD_SAMPLE_SIZE(specs));
}

void AUD_LinearResampleReader::seek(int position)
{
	position = floor(position * double(m_reader->getSpecs().rate) / double(m_rate));
	m_reader->seek(position);
	m_cache_ok = false;
	m_cache_pos = 0;
}

int AUD_LinearResampleReader::getLength() const
{
	return floor(m_reader->getLength() * double(m_rate) / double(m_reader->getSpecs().rate));
}

int AUD_LinearResampleReader::getPosition() const
{
	return floor((m_reader->getPosition() + (m_cache_ok ? m_cache_pos - 1 : 0))
				 * m_rate / m_reader->getSpecs().rate);
}

AUD_Specs AUD_LinearResampleReader::getSpecs() const
{
	AUD_Specs specs = m_reader->getSpecs();
	specs.rate = m_rate;
	return specs;
}

void AUD_LinearResampleReader::read(int& length, bool& eos, sample_t* buffer)
{
	if(length == 0)
		return;

	AUD_Specs specs = m_reader->getSpecs();

	int samplesize = AUD_SAMPLE_SIZE(specs);
	int size = length;
	float factor = m_rate / m_reader->getSpecs().rate;
	float spos = 0.0f;
	sample_t low, high;
	eos = false;

	// check for channels changed

	if(specs.channels != m_channels)
	{
		m_cache.resize(2 * samplesize);
		m_channels = specs.channels;
		m_cache_ok = false;
	}

	if(factor == 1 && (!m_cache_ok || m_cache_pos == 1))
	{
		// can read directly!
		m_reader->read(length, eos, buffer);

		if(length > 0)
		{
			memcpy(m_cache.getBuffer() + m_channels, buffer + m_channels * (length - 1), samplesize);
			m_cache_pos = 1;
			m_cache_ok = true;
		}

		return;
	}

	int len;
	sample_t* buf;

	if(m_cache_ok)
	{
		int need = ceil(length / factor + m_cache_pos) - 1;

		len = need;

		m_buffer.assureSize((len + 2) * samplesize);
		buf = m_buffer.getBuffer();

		memcpy(buf, m_cache.getBuffer(), 2 * samplesize);
		m_reader->read(len, eos, buf + 2 * m_channels);

		if(len < need)
			length = floor((len + 1 - m_cache_pos) * factor);
	}
	else
	{
		m_cache_pos = 1 - 1 / factor;

		int need = ceil(length / factor + m_cache_pos);

		len = need;

		m_buffer.assureSize((len + 1) * samplesize);
		buf = m_buffer.getBuffer();

		memset(buf, 0, samplesize);
		m_reader->read(len, eos, buf + m_channels);

		if(len == 0)
		{
			length = 0;
			return;
		}

		if(len < need)
		{
			length = floor((len - m_cache_pos) * factor);
		}

		m_cache_ok = true;
	}

	if(length == 0)
		return;

	for(int channel = 0; channel < m_channels; channel++)
	{
		for(int i = 0; i < length; i++)
		{
			spos = (i + 1) / factor + m_cache_pos;

			low = buf[(int)floor(spos) * CC];
			high = buf[(int)ceil(spos) * CC];

			buffer[i * CC] = low + (spos - floor(spos)) * (high - low);
		}
	}

	if(floor(spos) == spos)
	{
		memcpy(m_cache.getBuffer() + m_channels, buf + int(floor(spos)) * m_channels, samplesize);
		m_cache_pos = 1;
	}
	else
	{
		memcpy(m_cache.getBuffer(), buf + int(floor(spos)) * m_channels, 2 * samplesize);
		m_cache_pos = spos - floor(spos);
	}

	eos &= length < size;
}
