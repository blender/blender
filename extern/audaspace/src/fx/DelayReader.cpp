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

#include "fx/DelayReader.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

DelayReader::DelayReader(std::shared_ptr<IReader> reader, float delay) :
	EffectReader(reader),
	m_delay(int((SampleRate)delay * reader->getSpecs().rate)),
	m_remdelay(int((SampleRate)delay * reader->getSpecs().rate))
{
}

void DelayReader::seek(int position)
{
	if(position < m_delay)
	{
		m_remdelay = m_delay - position;
		m_reader->seek(0);
	}
	else
	{
		m_remdelay = 0;
		m_reader->seek(position - m_delay);
	}
}

int DelayReader::getLength() const
{
	int len = m_reader->getLength();
	if(len < 0)
		return len;
	return len + m_delay;
}

int DelayReader::getPosition() const
{
	if(m_remdelay > 0)
		return m_delay - m_remdelay;
	return m_reader->getPosition() + m_delay;
}

void DelayReader::read(int& length, bool& eos, sample_t* buffer)
{
	if(m_remdelay > 0)
	{
		Specs specs = m_reader->getSpecs();
		int samplesize = AUD_SAMPLE_SIZE(specs);

		if(length > m_remdelay)
		{
			std::memset(buffer, 0, m_remdelay * samplesize);

			int len = length - m_remdelay;
			m_reader->read(len, eos, buffer + m_remdelay * specs.channels);

			length = m_remdelay + len;

			m_remdelay = 0;
		}
		else
		{
			std::memset(buffer, 0, length * samplesize);
			m_remdelay -= length;
		}
	}
	else
		m_reader->read(length, eos, buffer);
}

AUD_NAMESPACE_END
