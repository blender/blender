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

AUD_ConverterReader::AUD_ConverterReader(AUD_IReader* reader, AUD_Specs specs) :
		AUD_EffectReader(reader)
{
	m_specs = reader->getSpecs();

	int bigendian = 1;
	bigendian = (((char*)&bigendian)[0]) ? 0: 1; // 1 if Big Endian

	switch(m_specs.format)
	{
	case AUD_FORMAT_U8:
		switch(specs.format)
		{
		case AUD_FORMAT_U8:
			m_convert = AUD_convert_copy<unsigned char>;
			break;
		case AUD_FORMAT_S16:
			m_convert = AUD_convert_u8_s16;
			break;
		case AUD_FORMAT_S24:
			if(bigendian)
				m_convert = AUD_convert_u8_s24_be;
			else
				m_convert = AUD_convert_u8_s24_le;
			break;
		case AUD_FORMAT_S32:
			m_convert = AUD_convert_u8_s32;
			break;
		case AUD_FORMAT_FLOAT32:
			m_convert = AUD_convert_u8_float;
			break;
		case AUD_FORMAT_FLOAT64:
			m_convert = AUD_convert_u8_double;
			break;
		default:
			break;
		}
		break;
	case AUD_FORMAT_S16:
		switch(specs.format)
		{
		case AUD_FORMAT_U8:
			m_convert = AUD_convert_s16_u8;
			break;
		case AUD_FORMAT_S16:
			m_convert = AUD_convert_copy<int16_t>;
			break;
		case AUD_FORMAT_S24:
			if(bigendian)
				m_convert = AUD_convert_s16_s24_be;
			else
				m_convert = AUD_convert_s16_s24_le;
			break;
		case AUD_FORMAT_S32:
			m_convert = AUD_convert_s16_s32;
			break;
		case AUD_FORMAT_FLOAT32:
			m_convert = AUD_convert_s16_float;
			break;
		case AUD_FORMAT_FLOAT64:
			m_convert = AUD_convert_s16_double;
			break;
		default:
			break;
		}
		break;
	case AUD_FORMAT_S24:
		if(bigendian)
			switch(specs.format)
			{
			case AUD_FORMAT_U8:
				m_convert = AUD_convert_u8_s24_be;
				break;
			case AUD_FORMAT_S16:
				m_convert = AUD_convert_s16_s24_be;
				break;
			case AUD_FORMAT_S24:
				m_convert = AUD_convert_s24_s24;
				break;
			case AUD_FORMAT_S32:
				m_convert = AUD_convert_s32_s24_be;
				break;
			case AUD_FORMAT_FLOAT32:
				m_convert = AUD_convert_float_s24_be;
				break;
			case AUD_FORMAT_FLOAT64:
				m_convert = AUD_convert_double_s24_be;
				break;
			default:
				break;
			}
		else
			switch(specs.format)
			{
			case AUD_FORMAT_U8:
				m_convert = AUD_convert_u8_s24_le;
				break;
			case AUD_FORMAT_S16:
				m_convert = AUD_convert_s16_s24_le;
				break;
			case AUD_FORMAT_S24:
				m_convert = AUD_convert_s24_s24;
				break;
			case AUD_FORMAT_S32:
				m_convert = AUD_convert_s32_s24_le;
				break;
			case AUD_FORMAT_FLOAT32:
				m_convert = AUD_convert_float_s24_le;
				break;
			case AUD_FORMAT_FLOAT64:
				m_convert = AUD_convert_double_s24_le;
				break;
			default:
				break;
			}
		break;
	case AUD_FORMAT_S32:
		switch(specs.format)
		{
		case AUD_FORMAT_U8:
			m_convert = AUD_convert_s32_u8;
			break;
		case AUD_FORMAT_S16:
			m_convert = AUD_convert_s32_s16;
			break;
		case AUD_FORMAT_S24:
			if(bigendian)
				m_convert = AUD_convert_s32_s24_be;
			else
				m_convert = AUD_convert_s32_s24_le;
			break;
		case AUD_FORMAT_S32:
			m_convert = AUD_convert_copy<int32_t>;
			break;
		case AUD_FORMAT_FLOAT32:
			m_convert = AUD_convert_s32_float;
			break;
		case AUD_FORMAT_FLOAT64:
			m_convert = AUD_convert_s32_double;
			break;
		default:
			break;
		}
		break;
	case AUD_FORMAT_FLOAT32:
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
		break;
	case AUD_FORMAT_FLOAT64:
		switch(specs.format)
		{
		case AUD_FORMAT_U8:
			m_convert = AUD_convert_double_u8;
			break;
		case AUD_FORMAT_S16:
			m_convert = AUD_convert_double_s16;
			break;
		case AUD_FORMAT_S24:
			if(bigendian)
				m_convert = AUD_convert_double_s24_be;
			else
				m_convert = AUD_convert_double_s24_le;
			break;
		case AUD_FORMAT_S32:
			m_convert = AUD_convert_double_s32;
			break;
		case AUD_FORMAT_FLOAT32:
			m_convert = AUD_convert_double_float;
			break;
		case AUD_FORMAT_FLOAT64:
			m_convert = AUD_convert_copy<double>;
			break;
		default:
			break;
		}
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
	return m_specs;
}

void AUD_ConverterReader::read(int & length, sample_t* & buffer)
{
	m_reader->read(length, buffer);

	int samplesize = AUD_SAMPLE_SIZE(m_specs);

	if(m_buffer->getSize() < length*samplesize)
		m_buffer->resize(length*samplesize);

	m_convert(m_buffer->getBuffer(), buffer, length*m_specs.channels);

	buffer = m_buffer->getBuffer();
}
