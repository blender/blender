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

#include "AUD_ChannelMapperReader.h"
#include "AUD_Buffer.h"

AUD_ChannelMapperReader::AUD_ChannelMapperReader(AUD_IReader* reader,
												 float **mapping) :
		AUD_EffectReader(reader)
{
	m_specs = reader->getSpecs();

	int channels = -1;
	m_rch = m_specs.channels;
	while(mapping[++channels] != 0);

	m_mapping = new float*[channels]; AUD_NEW("mapping")
	m_specs.channels = (AUD_Channels)channels;

	float sum;
	int i;

	while(channels--)
	{
		m_mapping[channels] = new float[m_rch]; AUD_NEW("mapping")
		sum = 0.0f;
		for(i=0; i < m_rch; i++)
			sum += mapping[channels][i];
		for(i=0; i < m_rch; i++)
			m_mapping[channels][i] = sum > 0.0f ?
									 mapping[channels][i]/sum : 0.0f;
	}

	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
}

AUD_ChannelMapperReader::~AUD_ChannelMapperReader()
{
	int channels = m_specs.channels;

	while(channels--)
	{
		delete[] m_mapping[channels]; AUD_DELETE("mapping")
	}

	delete[] m_mapping; AUD_DELETE("mapping")

	delete m_buffer; AUD_DELETE("buffer")
}

AUD_Specs AUD_ChannelMapperReader::getSpecs()
{
	return m_specs;
}

void AUD_ChannelMapperReader::read(int & length, sample_t* & buffer)
{
	m_reader->read(length, buffer);

	int channels = m_specs.channels;

	if(m_buffer->getSize() < length * 4 * channels)
		m_buffer->resize(length * 4 * channels);

	sample_t* in = buffer;
	sample_t* out = m_buffer->getBuffer();
	sample_t sum;

	for(int i = 0; i < length; i++)
	{
		for(int j = 0; j < channels; j++)
		{
			sum = 0;
			for(int k = 0; k < m_rch; k++)
				sum += m_mapping[j][k] * in[i * m_rch + k];
			out[i * channels + j] = sum;
		}
	}

	buffer = m_buffer->getBuffer();
}
