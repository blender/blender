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

#include "fx/LimiterReader.h"
#include "util/Buffer.h"

#include <algorithm>

AUD_NAMESPACE_BEGIN

LimiterReader::LimiterReader(std::shared_ptr<IReader> reader, double start, double end) :
		EffectReader(reader),
		m_start(start),
		m_end(end)
{
	if(m_start > 0)
	{
		Specs specs = m_reader->getSpecs();
		Specs specs2;

		if(m_reader->isSeekable())
			m_reader->seek(m_start * specs.rate);
		else
		{
			// skip first m_start samples by reading them
			int length = AUD_DEFAULT_BUFFER_SIZE;
			Buffer buffer(AUD_DEFAULT_BUFFER_SIZE * AUD_SAMPLE_SIZE(specs));
			bool eos = false;
			for(int len = m_start * specs.rate;
				length > 0 && !eos;
				len -= length)
			{
				if(len < AUD_DEFAULT_BUFFER_SIZE)
					length = len;

				m_reader->read(length, eos, buffer.getBuffer());

				specs2 = m_reader->getSpecs();
				if(specs2.rate != specs.rate)
				{
					len = len * specs2.rate / specs.rate;
					specs.rate = specs2.rate;
				}

				if(specs2.channels != specs.channels)
				{
					specs = specs2;
					buffer.assureSize(AUD_DEFAULT_BUFFER_SIZE * AUD_SAMPLE_SIZE(specs));
				}
			}
		}
	}
}

void LimiterReader::seek(int position)
{
	m_reader->seek(position + m_start * m_reader->getSpecs().rate);
}

int LimiterReader::getLength() const
{
	int len = m_reader->getLength();
	SampleRate rate = m_reader->getSpecs().rate;
	if(len < 0 || (len > m_end * rate && m_end >= 0))
		len = m_end * rate;
	return len - m_start * rate;
}

int LimiterReader::getPosition() const
{
	int pos = m_reader->getPosition();
	SampleRate rate = m_reader->getSpecs().rate;
	return std::min(pos, int(m_end * rate)) - m_start * rate;
}

void LimiterReader::read(int& length, bool& eos, sample_t* buffer)
{
	eos = false;
	if(m_end >= 0)
	{
		int position = m_reader->getPosition();
		SampleRate rate = m_reader->getSpecs().rate;

		if(position + length > m_end * rate)
		{
			length = m_end * rate - position;
			eos = true;
		}

		if(position < int(m_start * rate))
		{
			int len2 = length;
			for(int len = int(m_start * rate) - position;
				len2 == length && !eos;
				len -= length)
			{
				if(len < length)
					len2 = len;

				m_reader->read(len2, eos, buffer);
				position += len2;
			}

			if(position < m_start * rate)
			{
				length = 0;
				return;
			}
		}

		if(length < 0)
		{
			length = 0;
			return;
		}
	}
	if(eos)
	{
		m_reader->read(length, eos, buffer);
		eos = true;
	}
	else
		m_reader->read(length, eos, buffer);
}

AUD_NAMESPACE_END
