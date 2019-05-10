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

#include "generator/SilenceReader.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

SilenceReader::SilenceReader(SampleRate sampleRate) :
	m_position(0),
	m_sampleRate(sampleRate)
{
}

bool SilenceReader::isSeekable() const
{
	return true;
}

void SilenceReader::seek(int position)
{
	m_position = position;
}

int SilenceReader::getLength() const
{
	return -1;
}

int SilenceReader::getPosition() const
{
	return m_position;
}

Specs SilenceReader::getSpecs() const
{
	Specs specs;
	specs.rate = m_sampleRate;
	specs.channels = CHANNELS_MONO;
	return specs;
}

void SilenceReader::read(int& length, bool& eos, sample_t* buffer)
{
	std::memset(buffer, 0, length * sizeof(sample_t));
	m_position += length;
	eos = false;
}

AUD_NAMESPACE_END
