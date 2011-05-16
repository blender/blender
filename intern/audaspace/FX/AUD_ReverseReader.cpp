/*
 * $Id$
 *
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

/** \file audaspace/FX/AUD_ReverseReader.cpp
 *  \ingroup audfx
 */


#include "AUD_ReverseReader.h"

#include <cstring>

static const char* props_error = "AUD_ReverseReader: The reader has to be "
								 "seekable and a finite length.";

AUD_ReverseReader::AUD_ReverseReader(AUD_IReader* reader) :
		AUD_EffectReader(reader),
		m_length(reader->getLength()),
		m_position(0)
{
	if(m_length < 0 || !reader->isSeekable())
		AUD_THROW(AUD_ERROR_PROPS, props_error);
}

void AUD_ReverseReader::seek(int position)
{
	m_position = position;
}

int AUD_ReverseReader::getLength() const
{
	return m_length;
}

int AUD_ReverseReader::getPosition() const
{
	return m_position;
}

void AUD_ReverseReader::read(int & length, sample_t* & buffer)
{
	// first correct the length
	if(m_position + length > m_length)
		length = m_length - m_position;

	if(length <= 0)
	{
		length = 0;
		return;
	}

	AUD_Specs specs = getSpecs();
	int samplesize = AUD_SAMPLE_SIZE(specs);

	// resize buffer if needed
	if(m_buffer.getSize() < length * samplesize)
		m_buffer.resize(length * samplesize);

	buffer = m_buffer.getBuffer();

	sample_t* buf;
	int len = length;

	// read from reader
	m_reader->seek(m_length - m_position - len);
	m_reader->read(len, buf);

	// set null if reader didn't give enough data
	if(len < length)
	{
		memset(buffer, 0, (length - len) * samplesize);
		buffer += (length - len) * specs.channels;
	}

	// copy the samples reverted
	for(int i = 0; i < len; i++)
		memcpy(buffer + i * specs.channels,
			   buf + (len - 1 - i) * specs.channels,
			   samplesize);

	m_position += length;

	buffer = m_buffer.getBuffer();
}
