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

#include "AUD_SuperposeReader.h"
#include "AUD_Buffer.h"

#include <cstring>

AUD_SuperposeReader::AUD_SuperposeReader(AUD_IReader* reader1, AUD_IReader* reader2) :
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
}

AUD_SuperposeReader::~AUD_SuperposeReader()
{
	delete m_reader1; AUD_DELETE("reader")
	delete m_reader2; AUD_DELETE("reader")
	delete m_buffer; AUD_DELETE("buffer")
}

bool AUD_SuperposeReader::isSeekable()
{
	return m_reader1->isSeekable() && m_reader2->isSeekable();
}

void AUD_SuperposeReader::seek(int position)
{
	m_reader1->seek(position);
	m_reader2->seek(position);
}

int AUD_SuperposeReader::getLength()
{
	int len1 = m_reader1->getLength();
	int len2 = m_reader2->getLength();
	if((len1 < 0) || (len2 < 0))
		return -1;
	if(len1 < len2)
		return len2;
	return len1;
}

int AUD_SuperposeReader::getPosition()
{
	int pos1 = m_reader1->getPosition();
	int pos2 = m_reader2->getPosition();
	return AUD_MAX(pos1, pos2);
}

AUD_Specs AUD_SuperposeReader::getSpecs()
{
	return m_reader1->getSpecs();
}

AUD_ReaderType AUD_SuperposeReader::getType()
{
	if(m_reader1->getType() == AUD_TYPE_BUFFER &&
	   m_reader2->getType() == AUD_TYPE_BUFFER)
		return AUD_TYPE_BUFFER;
	return AUD_TYPE_STREAM;
}

bool AUD_SuperposeReader::notify(AUD_Message &message)
{
	return m_reader1->notify(message) | m_reader2->notify(message);
}

void AUD_SuperposeReader::read(int & length, sample_t* & buffer)
{
	AUD_Specs specs = m_reader1->getSpecs();
	int samplesize = AUD_SAMPLE_SIZE(specs);

	if(m_buffer->getSize() < length * samplesize)
		m_buffer->resize(length * samplesize);
	buffer = m_buffer->getBuffer();

	int len1 = length;
	sample_t* buf;
	m_reader1->read(len1, buf);
	memcpy(buffer, buf, len1 * samplesize);

	if(len1 < length)
		memset(buffer + len1 * specs.channels, 0, (length - len1) * samplesize);

	int len2 = length;
	m_reader2->read(len2, buf);

	for(int i = 0; i < len2 * specs.channels; i++)
		buffer[i] += buf[i];

	length = AUD_MAX(len1, len2);
}
