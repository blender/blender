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

#include "util/BufferReader.h"
#include "util/Buffer.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

BufferReader::BufferReader(std::shared_ptr<Buffer> buffer,
								   Specs specs) :
	m_position(0), m_buffer(buffer), m_specs(specs)
{
}

bool BufferReader::isSeekable() const
{
	return true;
}

void BufferReader::seek(int position)
{
	m_position = position;
}

int BufferReader::getLength() const
{
	return m_buffer->getSize() / AUD_SAMPLE_SIZE(m_specs);
}

int BufferReader::getPosition() const
{
	return m_position;
}

Specs BufferReader::getSpecs() const
{
	return m_specs;
}

void BufferReader::read(int& length, bool& eos, sample_t* buffer)
{
	eos = false;

	int sample_size = AUD_SAMPLE_SIZE(m_specs);

	sample_t* buf = m_buffer->getBuffer() + m_position * m_specs.channels;

	// in case the end of the buffer is reached
	if(m_buffer->getSize() < (m_position + length) * sample_size)
	{
		length = m_buffer->getSize() / sample_size - m_position;
		eos = true;
	}

	if(length < 0)
	{
		length = 0;
		return;
	}

	m_position += length;
	std::memcpy(buffer, buf, length * sample_size);
}

AUD_NAMESPACE_END
