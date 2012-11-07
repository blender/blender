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

/** \file audaspace/FX/AUD_BaseIIRFilterReader.cpp
 *  \ingroup audfx
 */


#include "AUD_BaseIIRFilterReader.h"

#include <cstring>

#define CC m_specs.channels + m_channel

AUD_BaseIIRFilterReader::AUD_BaseIIRFilterReader(boost::shared_ptr<AUD_IReader> reader, int in,
												 int out) :
		AUD_EffectReader(reader),
		m_specs(reader->getSpecs()),
		m_xlen(in), m_ylen(out),
		m_xpos(0), m_ypos(0), m_channel(0)
{
	m_x = new sample_t[m_xlen * m_specs.channels];
	m_y = new sample_t[m_ylen * m_specs.channels];

	memset(m_x, 0, sizeof(sample_t) * m_xlen * m_specs.channels);
	memset(m_y, 0, sizeof(sample_t) * m_ylen * m_specs.channels);
}

AUD_BaseIIRFilterReader::~AUD_BaseIIRFilterReader()
{
	delete[] m_x;
	delete[] m_y;
}

void AUD_BaseIIRFilterReader::setLengths(int in, int out)
{
	if(in != m_xlen)
	{
		sample_t* xn = new sample_t[in * m_specs.channels];
		memset(xn, 0, sizeof(sample_t) * in * m_specs.channels);

		for(m_channel = 0; m_channel < m_specs.channels; m_channel++)
		{
			for(int i = 1; i <= in && i <= m_xlen; i++)
			{
				xn[(in - i) * CC] = x(-i);
			}
		}

		delete[] m_x;
		m_x = xn;
		m_xpos = 0;
		m_xlen = in;
	}

	if(out != m_ylen)
	{
		sample_t* yn = new sample_t[out * m_specs.channels];
		memset(yn, 0, sizeof(sample_t) * out * m_specs.channels);

		for(m_channel = 0; m_channel < m_specs.channels; m_channel++)
		{
			for(int i = 1; i <= out && i <= m_ylen; i++)
			{
				yn[(out - i) * CC] = y(-i);
			}
		}

		delete[] m_y;
		m_y = yn;
		m_ypos = 0;
		m_ylen = out;
	}
}

void AUD_BaseIIRFilterReader::read(int& length, bool& eos, sample_t* buffer)
{
	AUD_Specs specs = m_reader->getSpecs();
	if(specs.channels != m_specs.channels)
	{
		m_specs.channels = specs.channels;

		delete[] m_x;
		delete[] m_y;

		m_x = new sample_t[m_xlen * m_specs.channels];
		m_y = new sample_t[m_ylen * m_specs.channels];

		memset(m_x, 0, sizeof(sample_t) * m_xlen * m_specs.channels);
		memset(m_y, 0, sizeof(sample_t) * m_ylen * m_specs.channels);
	}

	if(specs.rate != m_specs.rate)
	{
		m_specs = specs;
		sampleRateChanged(m_specs.rate);
	}

	m_reader->read(length, eos, buffer);

	for(m_channel = 0; m_channel < m_specs.channels; m_channel++)
	{
		for(int i = 0; i < length; i++)
		{
			m_x[m_xpos * CC] = buffer[i * CC];
			m_y[m_ypos * CC] = buffer[i * CC] = filter();

			m_xpos = (m_xpos + 1) % m_xlen;
			m_ypos = (m_ypos + 1) % m_ylen;
		}
	}
}

void AUD_BaseIIRFilterReader::sampleRateChanged(AUD_SampleRate rate)
{
}
