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

#include "AUD_ConverterReader.h"
#include "AUD_Buffer.h"

AUD_ConverterReader::AUD_ConverterReader(AUD_IReader* reader,
										 AUD_DeviceSpecs specs) :
		AUD_EffectReader(reader)
{
	m_specs.specs = reader->getSpecs();

	int bigendian = 1;
	bigendian = (((char*)&bigendian)[0]) ? 0: 1; // 1 if Big Endian

	switch(specs.format)
	{
	case AUD_FORMAT_U8:
		m_convert = AUD_convert_float_u8;
		break;
	case AUD_FORMAT_S16:
		m_convert = AUD_convert_float_s16;
		break;
	case AUD_FORMAT_S24:
		if(bigendian)
			m_convert = AUD_convert_float_s24_be;
		else
			m_convert = AUD_convert_float_s24_le;
		break;
	case AUD_FORMAT_S32:
		m_convert = AUD_convert_float_s32;
		break;
	case AUD_FORMAT_FLOAT32:
		m_convert = AUD_convert_copy<float>;
		break;
	case AUD_FORMAT_FLOAT64:
		m_convert = AUD_convert_float_double;
		break;
	default:
		break;
	}

	m_specs.format = specs.format;

	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
}

AUD_ConverterReader::~AUD_ConverterReader()
{
	delete m_buffer; AUD_DELETE("buffer")
}

AUD_Specs AUD_ConverterReader::getSpecs()
{
	return m_specs.specs;
}

void AUD_ConverterReader::read(int & length, sample_t* & buffer)
{
	m_reader->read(length, buffer);

	int samplesize = AUD_SAMPLE_SIZE(m_specs);

	if(m_buffer->getSize() < length*samplesize)
		m_buffer->resize(length*samplesize);

	m_convert((data_t*)m_buffer->getBuffer(), (data_t*)buffer,
			  length * m_specs.channels);

	buffer = m_buffer->getBuffer();
}
