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

#include "AUD_ReverseReader.h"
#include "AUD_Buffer.h"

#include <cstring>

AUD_ReverseReader::AUD_ReverseReader(AUD_IReader* reader) :
		AUD_EffectReader(reader)
{
	if(reader->getType() != AUD_TYPE_BUFFER)
		AUD_THROW(AUD_ERROR_READER);

	m_length = reader->getLength();
	if(m_length < 0)
		AUD_THROW(AUD_ERROR_READER);

	m_position = 0;
	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
}

AUD_ReverseReader::~AUD_ReverseReader()
{
	delete m_buffer; AUD_DELETE("buffer")
}

void AUD_ReverseReader::seek(int position)
{
	m_position = position;
}

int AUD_ReverseReader::getLength()
{
	return m_length;
}

int AUD_ReverseReader::getPosition()
{
	return m_position;
}

void AUD_ReverseReader::read(int & length, sample_t* & buffer)
{
	// first correct the length
	if(m_position+length > m_length)
		length = m_length-m_position;

	if(length <= 0)
	{
		length = 0;
		return;
	}

	int samplesize = AUD_SAMPLE_SIZE(getSpecs());

	// resize buffer if needed
	if(m_buffer->getSize() < length * samplesize)
		m_buffer->resize(length * samplesize);

	buffer = m_buffer->getBuffer();

	sample_t* buf;
	int len = length;

	// read from reader
	m_reader->seek(m_length-m_position-len);
	m_reader->read(len, buf);

	// set null if reader didn't give enough data
	if(len < length)
	{
		if(getSpecs().format == AUD_FORMAT_U8)
			memset(buffer, 0x80, (length-len)*samplesize);
		else
			memset(buffer, 0, (length-len)*samplesize);
		buffer += length-len;
	}

	// copy the samples reverted
	for(int i = 0; i < len; i++)
		memcpy(buffer + i * samplesize,
			   buf + (len - 1 - i) * samplesize,
			   samplesize);

	m_position += length;

	buffer = m_buffer->getBuffer();
}
