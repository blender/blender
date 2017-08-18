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

#include "fx/LoopReader.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

LoopReader::LoopReader(std::shared_ptr<IReader> reader, int loop) :
		EffectReader(reader), m_count(loop), m_left(loop)
{
}

void LoopReader::seek(int position)
{
	int len = m_reader->getLength();
	if(len < 0)
		m_reader->seek(position);
	else
	{
		if(m_count >= 0)
		{
			m_left = m_count - (position / len);
			if(m_left < 0)
				m_left = 0;
		}
		m_reader->seek(position % len);
	}
}

int LoopReader::getLength() const
{
	if(m_count < 0)
		return -1;
	return m_reader->getLength() * m_count;
}

int LoopReader::getPosition() const
{
	return m_reader->getPosition() * (m_count < 0 ? 1 : m_count);
}

void LoopReader::read(int& length, bool& eos, sample_t* buffer)
{
	const Specs specs = m_reader->getSpecs();

	int len = length;

	m_reader->read(length, eos, buffer);

	if(length < len && eos && m_left)
	{
		int pos = length;
		length = len;

		while(pos < length && eos && m_left)
		{
			if(m_left > 0)
				m_left--;

			m_reader->seek(0);

			len = length - pos;
			m_reader->read(len, eos, buffer + pos * specs.channels);

			// prevent endless loop
			if(!len)
				break;

			pos += len;
		}

		length = pos;
	}
}

AUD_NAMESPACE_END
