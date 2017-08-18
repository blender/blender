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

#include "generator/SineReader.h"

#include <cmath>

AUD_NAMESPACE_BEGIN

SineReader::SineReader(float frequency, SampleRate sampleRate) :
	m_frequency(frequency),
	m_position(0),
	m_sampleRate(sampleRate)
{
}

void SineReader::setFrequency(float frequency)
{
	m_frequency = frequency;
}

bool SineReader::isSeekable() const
{
	return true;
}

void SineReader::seek(int position)
{
	m_position = position;
}

int SineReader::getLength() const
{
	return -1;
}

int SineReader::getPosition() const
{
	return m_position;
}

Specs SineReader::getSpecs() const
{
	Specs specs;
	specs.rate = m_sampleRate;
	specs.channels = CHANNELS_MONO;
	return specs;
}

void SineReader::read(int& length, bool& eos, sample_t* buffer)
{
	// fill with sine data
	for(int i = 0; i < length; i++)
	{
		buffer[i] = std::sin((m_position + i) * 2 * M_PI * m_frequency / m_sampleRate);
	}

	m_position += length;
	eos = false;
}

AUD_NAMESPACE_END
