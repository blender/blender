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

#include "AUD_EnvelopeReader.h"
#include "AUD_Buffer.h"

#include <cstring>
#include <cmath>

AUD_EnvelopeReader::AUD_EnvelopeReader(AUD_IReader* reader, float attack,
									   float release, float threshold,
									   float arthreshold) :
		AUD_EffectReader(reader),
		m_threshold(threshold)
{
	AUD_Specs specs = reader->getSpecs();
	int samplesize = AUD_SAMPLE_SIZE(specs);

	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")

	m_envelopes = new AUD_Buffer(samplesize);
	AUD_NEW("buffer")
	memset(m_envelopes->getBuffer(), 0, samplesize);

	m_bAttack = pow(arthreshold, 1.0/(specs.rate * attack));
	m_bRelease = pow(arthreshold, 1.0/(specs.rate * release));
}

AUD_EnvelopeReader::~AUD_EnvelopeReader()
{
	delete m_buffer; AUD_DELETE("buffer")
	delete m_envelopes; AUD_DELETE("buffer")
}

void AUD_EnvelopeReader::read(int & length, sample_t* & buffer)
{
	sample_t* buf;
	sample_t* envelopes;
	envelopes = m_envelopes->getBuffer();

	AUD_Specs specs = m_reader->getSpecs();

	m_reader->read(length, buf);
	if(m_buffer->getSize() < length * AUD_SAMPLE_SIZE(specs))
		m_buffer->resize(length * AUD_SAMPLE_SIZE(specs));

	buffer = m_buffer->getBuffer();

	sample_t value;

	for(int channel = 0; channel < specs.channels; channel++)
	{
		for(int i = 0; i < length; i++)
		{
			value = fabs(buf[i * specs.channels + channel]);
			if(value < m_threshold)
				value = 0.0f;

			buffer[i * specs.channels + channel] = envelopes[channel] =
				((value > envelopes[channel]) ? m_bAttack : m_bRelease) *
				 (envelopes[channel] - value) + value;
		}
	}
}
