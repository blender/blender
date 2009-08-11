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

#include "AUD_StreamBufferFactory.h"
#include "AUD_BufferReader.h"
#include "AUD_Buffer.h"

#include <cstring>

AUD_StreamBufferFactory::AUD_StreamBufferFactory(AUD_IFactory* factory)
{
	AUD_IReader* reader = factory->createReader();

	if(reader == NULL)
		AUD_THROW(AUD_ERROR_READER);

	m_specs = reader->getSpecs();
	m_buffer = AUD_Reference<AUD_Buffer>(new AUD_Buffer()); AUD_NEW("buffer")

	int sample_size = AUD_SAMPLE_SIZE(m_specs);
	int length;
	int index = 0;
	sample_t* buffer;

	// get an aproximated size if possible
	int size = reader->getLength();

	if(size <= 0)
		size = AUD_BUFFER_RESIZE_BYTES / sample_size;
	else
		size += m_specs.rate;

	// as long as we fill our buffer to the end
	while(index == m_buffer.get()->getSize() / sample_size)
	{
		// increase
		m_buffer.get()->resize(size*sample_size, true);

		// read more
		length = size-index;
		reader->read(length, buffer);
		memcpy(m_buffer.get()->getBuffer()+index*sample_size,
			   buffer,
			   length*sample_size);
		size += AUD_BUFFER_RESIZE_BYTES / sample_size;
		index += length;
	}

	m_buffer.get()->resize(index*sample_size, true);
	delete reader; AUD_DELETE("reader")
}

AUD_IReader* AUD_StreamBufferFactory::createReader()
{
	AUD_IReader* reader = new AUD_BufferReader(m_buffer, m_specs);
	AUD_NEW("reader")
	return reader;
}
