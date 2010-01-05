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

#include "AUD_Mixer.h"
#include "AUD_SRCResampleFactory.h"
#include "AUD_ChannelMapperFactory.h"
#include "AUD_IReader.h"
#include "AUD_Buffer.h"

#include <cstring>

AUD_Mixer::AUD_Mixer()
{
	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")

	m_resampler = NULL;
	m_mapper = NULL;
}

AUD_Mixer::~AUD_Mixer()
{
	delete m_buffer; AUD_DELETE("buffer")


	if(m_resampler)
	{
		delete m_resampler; AUD_DELETE("factory")
	}
	if(m_mapper)
	{
		delete m_mapper; AUD_DELETE("factory")
	}
}

AUD_IReader* AUD_Mixer::prepare(AUD_IReader* reader)
{
	m_resampler->setReader(reader);
	reader = m_resampler->createReader();

	if(reader->getSpecs().channels != m_specs.channels)
	{
		m_mapper->setReader(reader);
		reader = m_mapper->createReader();
	}

	return reader;
}

void AUD_Mixer::setSpecs(AUD_DeviceSpecs specs)
{
	m_specs = specs;

	if(m_resampler)
	{
		delete m_resampler; AUD_DELETE("factory")
	}
	if(m_mapper)
	{
		delete m_mapper; AUD_DELETE("factory")
	}

	m_resampler = new AUD_SRCResampleFactory(specs); AUD_NEW("factory")
	m_mapper = new AUD_ChannelMapperFactory(specs); AUD_NEW("factory")

	int bigendian = 1;
	bigendian = (((char*)&bigendian)[0]) ? 0: 1; // 1 if Big Endian

	switch(m_specs.format)
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
}

void AUD_Mixer::add(sample_t* buffer, int length, float volume)
{
	AUD_MixerBuffer buf;
	buf.buffer = buffer;
	buf.length = length;
	buf.volume = volume;
	m_buffers.push_back(buf);
}

void AUD_Mixer::superpose(data_t* buffer, int length, float volume)
{
	AUD_MixerBuffer buf;

	int channels = m_specs.channels;

	if(m_buffer->getSize() < length * channels * 4)
		m_buffer->resize(length * channels * 4);

	sample_t* out = m_buffer->getBuffer();
	sample_t* in;

	memset(out, 0, length * channels * 4);

	int end;

	while(!m_buffers.empty())
	{
		buf = m_buffers.front();
		m_buffers.pop_front();

		end = buf.length*channels;
		in = buf.buffer;

		for(int i = 0; i < end; i++)
			out[i] += in[i]*buf.volume * volume;
	}

	m_convert(buffer, (data_t*) out, length * channels);
}
