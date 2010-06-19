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

#include "AUD_ButterworthReader.h"
#include "AUD_Buffer.h"

#include <cstring>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BWPB41 0.76536686473
#define BWPB42 1.84775906502
#define CC channels + channel

AUD_ButterworthReader::AUD_ButterworthReader(AUD_IReader* reader,
											 float frequency) :
		AUD_EffectReader(reader)
{
	AUD_Specs specs = reader->getSpecs();
	int samplesize = AUD_SAMPLE_SIZE(specs);

	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")

	m_outvalues = new AUD_Buffer(samplesize * 5); AUD_NEW("buffer")
	memset(m_outvalues->getBuffer(), 0, samplesize * 5);

	m_invalues = new AUD_Buffer(samplesize * 5); AUD_NEW("buffer")
	memset(m_invalues->getBuffer(), 0, samplesize * 5);

	m_position = 0;

	// calculate coefficients
	float omega = 2 * tan(frequency * M_PI / specs.rate);
	float o2 = omega * omega;
	float o4 = o2 * o2;
	float x1 = o2 + 2 * BWPB41 * omega + 4;
	float x2 = o2 + 2 * BWPB42 * omega + 4;
	float y1 = o2 - 2 * BWPB41 * omega + 4;
	float y2 = o2 - 2 * BWPB42 * omega + 4;
	float o228 = 2 * o2 - 8;
	float norm = x1 * x2;
	m_coeff[0][0] = 0;
	m_coeff[0][1] = (x1 + x2) * o228 / norm;
	m_coeff[0][2] = (x1 * y2 + x2 * y1 + o228 * o228) / norm;
	m_coeff[0][3] = (y1 + y2) * o228 / norm;
	m_coeff[0][4] = y1 * y2 / norm;
	m_coeff[1][4] = m_coeff[1][0] = o4 / norm;
	m_coeff[1][3] = m_coeff[1][1] = 4 * o4 / norm;
	m_coeff[1][2] = 6 * o4 / norm;
}

AUD_ButterworthReader::~AUD_ButterworthReader()
{
	delete m_buffer; AUD_DELETE("buffer")

	delete m_outvalues; AUD_DELETE("buffer")
	delete m_invalues; AUD_DELETE("buffer");
}

void AUD_ButterworthReader::read(int & length, sample_t* & buffer)
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

			for(int j = 0; j < 4; j++)
			{
				outvalues[m_position * CC] += m_coeff[1][j] *
									invalues[((m_position + j) % 5) * CC] -
									m_coeff[0][j] *
									outvalues[((m_position + j) % 5) * CC];
			}

			buffer[i * CC] = outvalues[m_position * CC];

			m_position = (m_position + 4) % 5;
		}
	}
}
