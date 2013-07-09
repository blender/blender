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

/** \file audaspace/intern/AUD_ConverterReader.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_ConverterReader.h"

AUD_ConverterReader::AUD_ConverterReader(boost::shared_ptr<AUD_IReader> reader,
										 AUD_DeviceSpecs specs) :
	AUD_EffectReader(reader),
	m_format(specs.format)
{
	switch(m_format)
	{
	case AUD_FORMAT_U8:
		m_convert = AUD_convert_float_u8;
		break;
	case AUD_FORMAT_S16:
		m_convert = AUD_convert_float_s16;
		break;
	case AUD_FORMAT_S24:
#ifdef __BIG_ENDIAN__
		m_convert = AUD_convert_float_s24_be;
#else
		m_convert = AUD_convert_float_s24_le;
#endif
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
}

void AUD_ConverterReader::read(int& length, bool& eos, sample_t* buffer)
{
	AUD_Specs specs = m_reader->getSpecs();
	int samplesize = AUD_SAMPLE_SIZE(specs);

	m_buffer.assureSize(length * samplesize);

	m_reader->read(length, eos, m_buffer.getBuffer());

	m_convert((data_t*)buffer, (data_t*)m_buffer.getBuffer(),
	          length * specs.channels);
}
