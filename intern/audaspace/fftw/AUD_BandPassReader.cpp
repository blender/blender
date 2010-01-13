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

#include "AUD_BandPassReader.h"
#include "AUD_Buffer.h"

#include <cstring>
#include <stdio.h>

AUD_BandPassReader::AUD_BandPassReader(AUD_IReader* reader, float low,
									   float high) :
		AUD_EffectReader(reader), m_low(low), m_high(high)
{
	m_buffer = new AUD_Buffer(); AUD_NEW("buffer")
	m_in = new AUD_Buffer(); AUD_NEW("buffer")
	m_out = new AUD_Buffer(); AUD_NEW("buffer")
	m_length = 0;
}

AUD_BandPassReader::~AUD_BandPassReader()
{
	if(m_length != 0)
	{
		fftw_destroy_plan(m_forward);
		fftw_destroy_plan(m_backward);
	}

	delete m_buffer; AUD_DELETE("buffer")
	delete m_in; AUD_DELETE("buffer")
	delete m_out; AUD_DELETE("buffer")
}

AUD_ReaderType AUD_BandPassReader::getType()
{
	return m_reader->getType();
}

void AUD_BandPassReader::read(int & length, sample_t* & buffer)
{
	AUD_Specs specs = m_reader->getSpecs();

	m_reader->read(length, buffer);

	if(length > 0)
	{
		if(length * AUD_SAMPLE_SIZE(specs) > m_buffer->getSize())
			m_buffer->resize(length * AUD_SAMPLE_SIZE(specs));

		if(length != m_length)
		{
			if(m_length != 0)
			{
				fftw_destroy_plan(m_forward);
				fftw_destroy_plan(m_backward);
			}

			m_length = length;

			if(m_length * sizeof(double) > m_in->getSize())
			{
				m_in->resize(m_length * sizeof(double));
				m_out->resize((m_length / 2 + 1) * sizeof(fftw_complex));
			}

			m_forward = fftw_plan_dft_r2c_1d(m_length,
											 (double*)m_in->getBuffer(),
											 (fftw_complex*)m_out->getBuffer(),
											 FFTW_ESTIMATE);
			m_backward = fftw_plan_dft_c2r_1d(m_length,
											  (fftw_complex*)m_out->getBuffer(),
											  (double*)m_in->getBuffer(),
											  FFTW_ESTIMATE);
		}

		double* target = (double*) m_in->getBuffer();
		sample_t* target2 = m_buffer->getBuffer();
		fftw_complex* complex = (fftw_complex*) m_out->getBuffer();
		float frequency;

		for(int channel = 0; channel < specs.channels; channel++)
		{
			for(int i = 0; i < m_length; i++)
				target[i] = buffer[i * specs.channels + channel];

			fftw_execute(m_forward);

			for(int i = 0; i < m_length / 2 + 1; i++)
			{
				frequency = i * specs.rate / (m_length / 2.0f + 1.0f);
				if((frequency < m_low) || (frequency > m_high))
					complex[i][0] = complex[i][1] = 0.0;
			}

			fftw_execute(m_backward);

			for(int i = 0; i < m_length; i++)
				target2[i * specs.channels + channel] = target[i] / m_length;
		}
	}

	buffer = m_buffer->getBuffer();
}
