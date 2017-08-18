/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#include "fx/BaseIIRFilterReader.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

BaseIIRFilterReader::BaseIIRFilterReader(std::shared_ptr<IReader> reader, int in, int out) :
	EffectReader(reader),
	m_specs(reader->getSpecs()),
	m_xlen(in), m_ylen(out),
	m_xpos(0), m_ypos(0), m_channel(0)
{
	m_x = new sample_t[m_xlen * m_specs.channels];
	m_y = new sample_t[m_ylen * m_specs.channels];

	std::memset(m_x, 0, sizeof(sample_t) * m_xlen * m_specs.channels);
	std::memset(m_y, 0, sizeof(sample_t) * m_ylen * m_specs.channels);
}

BaseIIRFilterReader::~BaseIIRFilterReader()
{
	delete[] m_x;
	delete[] m_y;
}

void BaseIIRFilterReader::setLengths(int in, int out)
{
	if(in != m_xlen)
	{
		sample_t* xn = new sample_t[in * m_specs.channels];
		std::memset(xn, 0, sizeof(sample_t) * in * m_specs.channels);

		for(m_channel = 0; m_channel < m_specs.channels; m_channel++)
		{
			for(int i = 1; i <= in && i <= m_xlen; i++)
			{
				xn[(in - i) * m_specs.channels + m_channel] = x(-i);
			}
		}

		delete[] m_x;
		m_x = xn;
		m_xpos = 0;
		m_xlen = in;
	}

	if(out != m_ylen)
	{
		sample_t* yn = new sample_t[out * m_specs.channels];
		std::memset(yn, 0, sizeof(sample_t) * out * m_specs.channels);

		for(m_channel = 0; m_channel < m_specs.channels; m_channel++)
		{
			for(int i = 1; i <= out && i <= m_ylen; i++)
			{
				yn[(out - i) * m_specs.channels + m_channel] = y(-i);
			}
		}

		delete[] m_y;
		m_y = yn;
		m_ypos = 0;
		m_ylen = out;
	}
}

void BaseIIRFilterReader::read(int& length, bool& eos, sample_t* buffer)
{
	Specs specs = m_reader->getSpecs();
	if(specs.channels != m_specs.channels)
	{
		m_specs.channels = specs.channels;

		delete[] m_x;
		delete[] m_y;

		m_x = new sample_t[m_xlen * m_specs.channels];
		m_y = new sample_t[m_ylen * m_specs.channels];

		std::memset(m_x, 0, sizeof(sample_t) * m_xlen * m_specs.channels);
		std::memset(m_y, 0, sizeof(sample_t) * m_ylen * m_specs.channels);
	}

	if(specs.rate != m_specs.rate)
	{
		m_specs = specs;
		sampleRateChanged(m_specs.rate);
	}

	m_reader->read(length, eos, buffer);

	for(m_channel = 0; m_channel < m_specs.channels; m_channel++)
	{
		for(int i = 0; i < length; i++)
		{
			m_x[m_xpos * m_specs.channels + m_channel] = buffer[i * m_specs.channels + m_channel];
			m_y[m_ypos * m_specs.channels + m_channel] = buffer[i * m_specs.channels + m_channel] = filter();

			m_xpos = m_xlen ? (m_xpos + 1) % m_xlen : 0;
			m_ypos = m_ylen ? (m_ypos + 1) % m_ylen : 0;
		}
	}
}

void BaseIIRFilterReader::sampleRateChanged(SampleRate rate)
{
}

AUD_NAMESPACE_END
