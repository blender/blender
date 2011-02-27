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

/** \file audaspace/intern/AUD_ConverterReader.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_ConverterReader.h"

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
}

AUD_Specs AUD_ConverterReader::getSpecs() const
{
	return m_specs.specs;
}

void AUD_ConverterReader::read(int & length, sample_t* & buffer)
{
	m_reader->read(length, buffer);

	int samplesize = AUD_SAMPLE_SIZE(m_specs);

	if(m_buffer.getSize() < length * samplesize)
		m_buffer.resize(length * samplesize);

	m_convert((data_t*)m_buffer.getBuffer(), (data_t*)buffer,
			  length * m_specs.channels);

	buffer = m_buffer.getBuffer();
}
