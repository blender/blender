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

/** \file audaspace/intern/AUD_Mixer.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_Mixer.h"
#include "AUD_IReader.h"

#include <cstring>

AUD_Mixer::AUD_Mixer(AUD_DeviceSpecs specs) :
	m_specs(specs)
{
	switch(m_specs.format)
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

AUD_DeviceSpecs AUD_Mixer::getSpecs() const
{
	return m_specs;
}

void AUD_Mixer::setSpecs(AUD_Specs specs)
{
	m_specs.specs = specs;
}

void AUD_Mixer::clear(int length)
{
	m_buffer.assureSize(length * m_specs.channels * AUD_SAMPLE_SIZE(m_specs));

	m_length = length;

	memset(m_buffer.getBuffer(), 0, length * m_specs.channels * AUD_SAMPLE_SIZE(m_specs));
}

void AUD_Mixer::mix(sample_t* buffer, int start, int length, float volume)
{
	sample_t* out = m_buffer.getBuffer();

	length = (AUD_MIN(m_length, length + start) - start) * m_specs.channels;
	start *= m_specs.channels;

	for(int i = 0; i < length; i++)
		out[i + start] += buffer[i] * volume;
}

void AUD_Mixer::read(data_t* buffer, float volume)
{
	sample_t* out = m_buffer.getBuffer();

	for(int i = 0; i < m_length * m_specs.channels; i++)
		out[i] *= volume;

	m_convert(buffer, (data_t*) out, m_length * m_specs.channels);
}
