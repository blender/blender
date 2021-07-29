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

/** \file audaspace/intern/AUD_StreamBufferFactory.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_StreamBufferFactory.h"
#include "AUD_BufferReader.h"
#include "AUD_Buffer.h"

#include <cstring>

AUD_StreamBufferFactory::AUD_StreamBufferFactory(boost::shared_ptr<AUD_IFactory> factory) :
	m_buffer(new AUD_Buffer())
{
	boost::shared_ptr<AUD_IReader> reader = factory->createReader();

	m_specs = reader->getSpecs();

	int sample_size = AUD_SAMPLE_SIZE(m_specs);
	int length;
	int index = 0;
	bool eos = false;

	// get an approximated size if possible
	int size = reader->getLength();

	if(size <= 0)
		size = AUD_BUFFER_RESIZE_BYTES / sample_size;
	else
		size += m_specs.rate;

	// as long as the end of the stream is not reached
	while(!eos)
	{
		// increase
		m_buffer->resize(size*sample_size, true);

		// read more
		length = size-index;
		reader->read(length, eos, m_buffer->getBuffer() + index * m_specs.channels);
		if(index == m_buffer->getSize() / sample_size)
			size += AUD_BUFFER_RESIZE_BYTES / sample_size;
		index += length;
	}

	m_buffer->resize(index * sample_size, true);
}

boost::shared_ptr<AUD_IReader> AUD_StreamBufferFactory::createReader()
{
	return boost::shared_ptr<AUD_IReader>(new AUD_BufferReader(m_buffer, m_specs));
}
