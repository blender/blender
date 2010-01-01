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

#include "AUD_HighpassReader.h"
#include "AUD_Buffer.h"

#include <cstring>
#include <cmath>

#define CC channels + channel

AUD_HighpassReader::AUD_HighpassReader(AUD_IReader* reader, float frequency,
									   float Q) :
		AUD_EffectReader(reader)
{
	AUD_Specs specs = reader->getSpecs();
	int samplesize = AUD_SAMPLE_SIZE(specs);

	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")

	m_outvalues = new AUD_Buffer(samplesize * AUD_HIGHPASS_ORDER);
	AUD_NEW("buffer")
	memset(m_outvalues->getBuffer(), 0, samplesize * AUD_HIGHPASS_ORDER);

	m_invalues = new AUD_Buffer(samplesize * AUD_HIGHPASS_ORDER);
	AUD_NEW("buffer")
	memset(m_invalues->getBuffer(), 0, samplesize * AUD_HIGHPASS_ORDER);

	m_position = 0;

	// calculate coefficients
	float w0 = 2.0 * M_PI * frequency / specs.rate;
	float alpha = sin(w0) / (2 * Q);
	float norm = 1 + alpha;
	m_coeff[0][0] = 0;
	m_coeff[0][1] = -2 * cos(w0) / norm;
	m_coeff[0][2] = (1 - alpha) / norm;
	m_coeff[1][2] = m_coeff[1][0] = (1 + cos(w0)) / (2 * norm);
	m_coeff[1][1] = (-1 - cos(w0)) / norm;
}

AUD_HighpassReader::~AUD_HighpassReader()
{
	delete m_buffer; AUD_DELETE("buffer")

	delete m_outvalues; AUD_DELETE("buffer")
	delete m_invalues; AUD_DELETE("buffer");
}

void AUD_HighpassReader::read(int & length, sample_t* & buffer)
{
	sample_t* buf;
	sample_t* outvalues;
	sample_t* invalues;

	outvalues = m_outvalues->getBuffer();
	invalues = m_invalues->getBuffer();

	AUD_Specs specs = m_reader->getSpecs();

	m_reader->read(length, buf);

	if(m_buffer->getSize() < length * AUD_SAMPLE_SIZE(specs))
		m_buffer->resize(length * AUD_SAMPLE_SIZE(specs));

	buffer = m_buffer->getBuffer();
	int channels = specs.channels;

	for(int channel = 0; channel < channels; channel++)
	{
		for(int i = 0; i < length; i++)
		{
			invalues[m_position * CC] = buf[i * CC];
			outvalues[m_position * CC] = 0;

			for(int j = 0; j < AUD_HIGHPASS_ORDER; j++)
			{
				outvalues[m_position * CC] += m_coeff[1][j] *
						invalues[((m_position + j) % AUD_HIGHPASS_ORDER) * CC] -
						m_coeff[0][j] *
						outvalues[((m_position + j) % AUD_HIGHPASS_ORDER) * CC];
			}

			buffer[i * CC] = outvalues[m_position * CC];

			m_position = (m_position + AUD_HIGHPASS_ORDER-1) %
						 AUD_HIGHPASS_ORDER;
		}
	}
}
