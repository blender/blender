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

/** \file audaspace/intern/AUD_ChannelMapperReader.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_ChannelMapperReader.h"

AUD_ChannelMapperReader::AUD_ChannelMapperReader(AUD_IReader* reader,
												 float **mapping) :
		AUD_EffectReader(reader)
{
	m_specs = reader->getSpecs();

	int channels = -1;
	m_rch = m_specs.channels;
	while(mapping[++channels] != 0);

	m_mapping = new float*[channels];
	m_specs.channels = (AUD_Channels)channels;

	float sum;
	int i;

	while(channels--)
	{
		m_mapping[channels] = new float[m_rch];
		sum = 0.0f;
		for(i=0; i < m_rch; i++)
			sum += mapping[channels][i];
		for(i=0; i < m_rch; i++)
			m_mapping[channels][i] = sum > 0.0f ?
									 mapping[channels][i]/sum : 0.0f;
	}
}

AUD_ChannelMapperReader::~AUD_ChannelMapperReader()
{
	int channels = m_specs.channels;

	while(channels--)
	{
		delete[] m_mapping[channels];
	}

	delete[] m_mapping;
}

AUD_Specs AUD_ChannelMapperReader::getSpecs() const
{
	return m_specs;
}

void AUD_ChannelMapperReader::read(int & length, sample_t* & buffer)
{
	sample_t* in = buffer;

	m_reader->read(length, in);

	if(m_buffer.getSize() < length * AUD_SAMPLE_SIZE(m_specs))
		m_buffer.resize(length * AUD_SAMPLE_SIZE(m_specs));

	buffer = m_buffer.getBuffer();
	sample_t sum;

	for(int i = 0; i < length; i++)
	{
		for(int j = 0; j < m_specs.channels; j++)
		{
			sum = 0;
			for(int k = 0; k < m_rch; k++)
				sum += m_mapping[j][k] * in[i * m_rch + k];
			buffer[i * m_specs.channels + j] = sum;
		}
	}
}
