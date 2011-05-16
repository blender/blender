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

/** \file audaspace/intern/AUD_BufferReader.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_BufferReader.h"
#include "AUD_Buffer.h"
#include "AUD_Space.h"

AUD_BufferReader::AUD_BufferReader(AUD_Reference<AUD_Buffer> buffer,
								   AUD_Specs specs) :
	m_position(0), m_buffer(buffer), m_specs(specs)
{
}

bool AUD_BufferReader::isSeekable() const
{
	return true;
}

void AUD_BufferReader::seek(int position)
{
	m_position = position;
}

int AUD_BufferReader::getLength() const
{
	return m_buffer.get()->getSize() / AUD_SAMPLE_SIZE(m_specs);
}

int AUD_BufferReader::getPosition() const
{
	return m_position;
}

AUD_Specs AUD_BufferReader::getSpecs() const
{
	return m_specs;
}

void AUD_BufferReader::read(int & length, sample_t* & buffer)
{
	int sample_size = AUD_SAMPLE_SIZE(m_specs);

	buffer = m_buffer.get()->getBuffer() + m_position * m_specs.channels;

	// in case the end of the buffer is reached
	if(m_buffer.get()->getSize() < (m_position + length) * sample_size)
		length = m_buffer.get()->getSize() / sample_size - m_position;

	if(length < 0)
		length = 0;
	m_position += length;
}
