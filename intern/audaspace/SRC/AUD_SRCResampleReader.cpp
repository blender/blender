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

/** \file audaspace/SRC/AUD_SRCResampleReader.cpp
 *  \ingroup audsrc
 */


#include "AUD_SRCResampleReader.h"

#include <cmath>
#include <cstring>
#include <cstdio>

static long src_callback(void *cb_data, float **data)
{
	return ((AUD_SRCResampleReader*)cb_data)->doCallback(data);
}

static const char* state_error = "AUD_SRCResampleReader: SRC State couldn't be "
								 "created.";

AUD_SRCResampleReader::AUD_SRCResampleReader(AUD_Reference<AUD_IReader> reader,
											 AUD_Specs specs) :
		AUD_ResampleReader(reader, specs.rate),
		m_channels(reader->getSpecs().channels),
		m_position(0)
{
	int error;
	m_src = src_callback_new(src_callback,
							 SRC_SINC_MEDIUM_QUALITY,
							 m_channels,
							 &error,
							 this);

	if(!m_src)
	{
		// XXX printf("%s\n", src_strerror(error));
		AUD_THROW(AUD_ERROR_SRC, state_error);
	}
}

AUD_SRCResampleReader::~AUD_SRCResampleReader()
{
	src_delete(m_src);
}

long AUD_SRCResampleReader::doCallback(float** data)
{
	AUD_Specs specs;
	specs.channels = m_channels;
	specs.rate = m_rate;

	int length = m_buffer.getSize() / AUD_SAMPLE_SIZE(specs);

	*data = m_buffer.getBuffer();
	m_reader->read(length, m_eos, *data);

	return length;
}

void AUD_SRCResampleReader::seek(int position)
{
	AUD_Specs specs = m_reader->getSpecs();
	double factor = double(m_rate) / double(specs.rate);
	m_reader->seek(position / factor);
	src_reset(m_src);
	m_position = position;
}

int AUD_SRCResampleReader::getLength() const
{
	AUD_Specs specs = m_reader->getSpecs();
	double factor = double(m_rate) / double(specs.rate);
	return m_reader->getLength() * factor;
}

int AUD_SRCResampleReader::getPosition() const
{
	return m_position;
}

AUD_Specs AUD_SRCResampleReader::getSpecs() const
{
	AUD_Specs specs = m_reader->getSpecs();
	specs.rate = m_rate;
	return specs;
}

void AUD_SRCResampleReader::read(int& length, bool& eos, sample_t* buffer)
{
	AUD_Specs specs = m_reader->getSpecs();

	double factor = double(m_rate) / double(specs.rate);

	specs.rate = m_rate;

	int size = length;

	m_buffer.assureSize(length * AUD_SAMPLE_SIZE(specs));

	if(specs.channels != m_channels)
	{
		src_delete(m_src);

		m_channels = specs.channels;

		int error;
		m_src = src_callback_new(src_callback,
								 SRC_SINC_MEDIUM_QUALITY,
								 m_channels,
								 &error,
								 this);

		if(!m_src)
		{
			// XXX printf("%s\n", src_strerror(error));
			AUD_THROW(AUD_ERROR_SRC, state_error);
		}
	}

	m_eos = false;

	length = src_callback_read(m_src, factor, length, buffer);

	m_position += length;

	eos = m_eos && (length < size);
}
