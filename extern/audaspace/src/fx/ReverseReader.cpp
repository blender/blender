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

#include "fx/ReverseReader.h"
#include "Exception.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

ReverseReader::ReverseReader(std::shared_ptr<IReader> reader) :
		EffectReader(reader),
		m_length(reader->getLength()),
		m_position(0)
{
	if(m_length < 0 || !reader->isSeekable())
		AUD_THROW(StateException, "A reader has to be seekable and have finite length to be reversible.");
}

void ReverseReader::seek(int position)
{
	m_position = position;
}

int ReverseReader::getLength() const
{
	return m_length;
}

int ReverseReader::getPosition() const
{
	return m_position;
}

void ReverseReader::read(int& length, bool& eos, sample_t* buffer)
{
	// first correct the length
	if(m_position + length > m_length)
		length = m_length - m_position;

	if(length <= 0)
	{
		length = 0;
		eos = true;
		return;
	}

	const Specs specs = getSpecs();
	const int samplesize = AUD_SAMPLE_SIZE(specs);

	sample_t temp[CHANNEL_MAX];

	int len = length;

	// read from reader
	m_reader->seek(m_length - m_position - len);
	m_reader->read(len, eos, buffer);

	// set null if reader didn't give enough data
	if(len < length)
		std::memset(buffer, 0, (length - len) * samplesize);

	// copy the samples reverted
	for(int i = 0; i < length / 2; i++)
	{
		std::memcpy(temp, buffer + (len - 1 - i) * specs.channels, samplesize);
		std::memcpy(buffer + (len - 1 - i) * specs.channels, buffer + i * specs.channels, samplesize);
		std::memcpy(buffer + i * specs.channels, temp, samplesize);
	}

	m_position += length;
	eos = false;
}

AUD_NAMESPACE_END
