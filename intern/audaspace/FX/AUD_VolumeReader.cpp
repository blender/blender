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

#include "AUD_VolumeReader.h"
#include "AUD_Buffer.h"

#include <cstring>

AUD_VolumeReader::AUD_VolumeReader(AUD_IReader* reader, float volume) :
		AUD_EffectReader(reader),
		m_volume(volume)
{
	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
}

AUD_VolumeReader::~AUD_VolumeReader()
{
	delete m_buffer; AUD_DELETE("buffer")
}

bool AUD_VolumeReader::notify(AUD_Message &message)
{
	if(message.type == AUD_MSG_VOLUME)
	{
		m_volume = message.volume;

		m_reader->notify(message);

		return true;
	}
	return m_reader->notify(message);
}

void AUD_VolumeReader::read(int & length, sample_t* & buffer)
{
	sample_t* buf;
	AUD_Specs specs = m_reader->getSpecs();

	m_reader->read(length, buf);
	if(m_buffer->getSize() < length*AUD_SAMPLE_SIZE(specs))
		m_buffer->resize(length*AUD_SAMPLE_SIZE(specs));

	buffer = m_buffer->getBuffer();

	for(int i = 0; i < length * specs.channels; i++)
		buffer[i] = buf[i] * m_volume;
}
