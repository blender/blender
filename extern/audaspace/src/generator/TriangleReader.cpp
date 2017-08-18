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

#include "generator/TriangleReader.h"

#include <cmath>

AUD_NAMESPACE_BEGIN

TriangleReader::TriangleReader(float frequency, SampleRate sampleRate) :
	m_frequency(frequency),
	m_position(0),
	m_sample(0.5f),
	m_sampleRate(sampleRate)
{
}

void TriangleReader::setFrequency(float frequency)
{
	m_frequency = frequency;
}

bool TriangleReader::isSeekable() const
{
	return true;
}

void TriangleReader::seek(int position)
{
	m_position = position;
	m_sample = std::fmod(m_position * m_frequency / (float)m_sampleRate + 1.5f, 2.0f) - 1.0f;
}

int TriangleReader::getLength() const
{
	return -1;
}

int TriangleReader::getPosition() const
{
	return m_position;
}

Specs TriangleReader::getSpecs() const
{
	Specs specs;
	specs.rate = m_sampleRate;
	specs.channels = CHANNELS_MONO;
	return specs;
}

void TriangleReader::read(int& length, bool& eos, sample_t* buffer)
{
	float k = 2.0 * m_frequency / m_sampleRate;

	for(int i = 0; i < length; i++)
	{
		m_sample += k;

		if(m_sample >= 1.0f)
			m_sample -= std::floor(m_sample) + 1.0f;

		buffer[i] = std::fabs(m_sample) * 2.0f - 1.0f;
	}

	m_position += length;
	eos = false;
}

AUD_NAMESPACE_END
