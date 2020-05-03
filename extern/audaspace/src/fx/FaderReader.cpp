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

#include "fx/FaderReader.h"

#include <cstring>

AUD_NAMESPACE_BEGIN

FaderReader::FaderReader(std::shared_ptr<IReader> reader, FadeType type, double start, double length) :
		EffectReader(reader),
		m_type(type),
		m_start(start),
		m_length(length)
{
}

void FaderReader::read(int& length, bool& eos, sample_t* buffer)
{
	int position = m_reader->getPosition();
	Specs specs = m_reader->getSpecs();
	int samplesize = AUD_SAMPLE_SIZE(specs);

	m_reader->read(length, eos, buffer);

	if((position + length) / specs.rate <= m_start)
	{
		if(m_type != FADE_OUT)
		{
			std::memset(buffer, 0, length * samplesize);
		}
	}
	else if(position / specs.rate >= m_start+m_length)
	{
		if(m_type == FADE_OUT)
		{
			std::memset(buffer, 0, length * samplesize);
		}
	}
	else
	{
		float volume = 1.0f;

		for(int i = 0; i < length * specs.channels; i++)
		{
			if(i % specs.channels == 0)
			{
				volume = float((((position + i) / specs.rate) - m_start) / m_length);
				if(volume > 1.0f)
					volume = 1.0f;
				else if(volume < 0.0f)
					volume = 0.0f;

				if(m_type == FADE_OUT)
					volume = 1.0f - volume;
			}

			buffer[i] = buffer[i] * volume;
		}
	}
}

AUD_NAMESPACE_END
