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

#include "AUD_DoubleReader.h"
#include "AUD_Buffer.h"

#include <cstring>

AUD_DoubleReader::AUD_DoubleReader(AUD_IReader* reader1,
								   AUD_IReader* reader2) :
		m_reader1(reader1), m_reader2(reader2)
{
	try
	{
		if(!reader1)
			AUD_THROW(AUD_ERROR_READER);

		if(!reader2)
			AUD_THROW(AUD_ERROR_READER);

		AUD_Specs s1, s2;
		s1 = reader1->getSpecs();
		s2 = reader2->getSpecs();
		if(memcmp(&s1, &s2, sizeof(AUD_Specs)) != 0)
			AUD_THROW(AUD_ERROR_READER);
	}

	catch(AUD_Exception)
	{
		if(reader1)
		{
			delete reader1; AUD_DELETE("reader")
		}
		if(reader2)
		{
			delete reader2; AUD_DELETE("reader")
		}

		throw;
	}

	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
	m_finished1 = false;
}

AUD_DoubleReader::~AUD_DoubleReader()
{
	delete m_reader1; AUD_DELETE("reader")
	delete m_reader2; AUD_DELETE("reader")
	delete m_buffer; AUD_DELETE("buffer")
}

bool AUD_DoubleReader::isSeekable()
{
	return false;
}

void AUD_DoubleReader::seek(int position)
{
	int length1 = m_reader1->getLength();

	if(position < 0)
		position = 0;

	if(position < length1)
	{
		m_reader1->seek(position);
		m_reader2->seek(0);
		m_finished1 = false;
	}
	else
	{
		m_reader2->seek(position-length1);
		m_finished1 = true;
	}
}

int AUD_DoubleReader::getLength()
{
	int len1 = m_reader1->getLength();
	int len2 = m_reader2->getLength();
	if(len1 < 0 || len2 < 0)
		return -1;
	return len1 + len2;
}

int AUD_DoubleReader::getPosition()
{
	return m_reader1->getPosition() + m_reader2->getPosition();
}

AUD_Specs AUD_DoubleReader::getSpecs()
{
	return m_reader1->getSpecs();
}

AUD_ReaderType AUD_DoubleReader::getType()
{
	if(m_reader1->getType() == AUD_TYPE_BUFFER &&
	   m_reader2->getType() == AUD_TYPE_BUFFER)
		return AUD_TYPE_BUFFER;
	return AUD_TYPE_STREAM;
}

bool AUD_DoubleReader::notify(AUD_Message &message)
{
	return m_reader1->notify(message) | m_reader2->notify(message);
}

void AUD_DoubleReader::read(int & length, sample_t* & buffer)
{
	if(!m_finished1)
	{
		int len = length;
		m_reader1->read(len, buffer);
		if(len < length)
		{
			AUD_Specs specs = m_reader1->getSpecs();
			int samplesize = AUD_SAMPLE_SIZE(specs);
			if(m_buffer->getSize() < length * samplesize)
				m_buffer->resize(length * samplesize);
			memcpy(m_buffer->getBuffer(), buffer, len * samplesize);
			len = length - len;
			length -= len;
			m_reader2->read(len, buffer);
			memcpy(m_buffer->getBuffer() + length * specs.channels, buffer,
				   len * samplesize);
			length += len;
			buffer = m_buffer->getBuffer();
			m_finished1 = true;
		}
	}
	else
	{
		m_reader2->read(length, buffer);
	}
}
