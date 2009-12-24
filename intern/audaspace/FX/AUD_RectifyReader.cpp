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

#include "AUD_RectifyReader.h"
#include "AUD_Buffer.h"

#include <cstring>

AUD_RectifyReader::AUD_RectifyReader(AUD_IReader* reader) :
		AUD_EffectReader(reader)
{
	int bigendian = 1;
	bigendian = (((char*)&bigendian)[0]) ? 0: 1; // 1 if Big Endian

	switch(m_reader->getSpecs().format)
	{
	case AUD_FORMAT_S16:
		m_rectify = AUD_rectify<int16_t>;
		break;
	case AUD_FORMAT_S32:
		m_rectify = AUD_rectify<int32_t>;
		break;
	case AUD_FORMAT_FLOAT32:
		m_rectify = AUD_rectify<float>;
		break;
	case AUD_FORMAT_FLOAT64:
		m_rectify = AUD_rectify<double>;
		break;
	case AUD_FORMAT_U8:
		m_rectify = AUD_rectify_u8;
		break;
	case AUD_FORMAT_S24:
		m_rectify = bigendian ? AUD_rectify_s24_be : AUD_rectify_s24_le;
		break;
	default:
		delete m_reader;
		AUD_THROW(AUD_ERROR_READER);
	}

	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
}

AUD_RectifyReader::~AUD_RectifyReader()
{
	delete m_buffer; AUD_DELETE("buffer")
}

void AUD_RectifyReader::read(int & length, sample_t* & buffer)
{
	sample_t* buf;
	AUD_Specs specs = m_reader->getSpecs();

	m_reader->read(length, buf);
	if(m_buffer->getSize() < length*AUD_SAMPLE_SIZE(specs))
		m_buffer->resize(length*AUD_SAMPLE_SIZE(specs));

	buffer = m_buffer->getBuffer();

	m_rectify(buffer, buf, length * specs.channels);
}
