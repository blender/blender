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
	int bigendian = 1;
	bigendian = (((char*)&bigendian)[0]) ? 0: 1; // 1 if Big Endian

	switch(m_reader->getSpecs().format)
	{
	case AUD_FORMAT_S16:
		m_adjust = AUD_volume_adjust<int16_t>;
		break;
	case AUD_FORMAT_S32:
		m_adjust = AUD_volume_adjust<int32_t>;
		break;
	case AUD_FORMAT_FLOAT32:
		m_adjust = AUD_volume_adjust<float>;
		break;
	case AUD_FORMAT_FLOAT64:
		m_adjust = AUD_volume_adjust<double>;
		break;
	case AUD_FORMAT_U8:
		m_adjust = AUD_volume_adjust_u8;
		break;
	case AUD_FORMAT_S24:
		m_adjust = bigendian ? AUD_volume_adjust_s24_be :
							   AUD_volume_adjust_s24_le;
		break;
	default:
		delete m_reader;
		AUD_THROW(AUD_ERROR_READER);
	}

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

	m_adjust(buffer, buf, length * specs.channels, m_volume);
}
