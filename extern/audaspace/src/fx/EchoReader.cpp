/*******************************************************************************
 * Copyright 2009-2025 Jörg Müller
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

#include "fx/EchoReader.h"

#include <cstring>

#include "IReader.h"

#include "util/Buffer.h"

AUD_NAMESPACE_BEGIN

EchoReader::EchoReader(std::shared_ptr<IReader> reader, float delay, float feedback, float mix, bool resetBuffer) :
    EffectReader(reader), m_delay(delay), m_feedback(feedback), m_mix(mix), m_resetBuffer(resetBuffer)
{
}

void EchoReader::read(int& length, bool& eos, sample_t* buffer)
{
	auto specs = m_reader->getSpecs();
	auto delaySamples = static_cast<int>(m_delay * specs.rate);

	m_inBuffer.assureSize(length * AUD_SAMPLE_SIZE(specs));

	// Note: should this ever do something another time than in the beginning,
	// it will likely cause an audible glitch, as samples are not reordered
	m_delayBuffer.assureSize(delaySamples * AUD_SAMPLE_SIZE(specs));

	m_reader->read(length, eos, m_inBuffer.getBuffer());

	sample_t* delayBuffer = m_delayBuffer.getBuffer();

	for(int i = 0; i < length; i++)
	{
		for(int channel = 0; channel < specs.channels; channel++)
		{
			int delayPosition = ((m_writePosition + i) % delaySamples) * specs.channels + channel;

			sample_t inSample = m_inBuffer.getBuffer()[i * specs.channels + channel];
			sample_t delayedSample = delayPosition < m_samplesAvailable * specs.channels ? delayBuffer[delayPosition] : 0;

			sample_t outSample = inSample + delayedSample * m_feedback;
			buffer[i * specs.channels + channel] = inSample * (1.0f - m_mix) + outSample * m_mix;

			// Update delay buffer with feedback
			delayBuffer[delayPosition] = outSample;
		}
	}

	m_writePosition = (m_writePosition + length) % delaySamples;
	m_samplesAvailable = std::min(delaySamples, m_samplesAvailable + length);
}

void EchoReader::seek(int position)
{
	m_reader->seek(position);

	if(m_resetBuffer)
	{
		m_samplesAvailable = 0;
		m_writePosition = 0;
	}
}

AUD_NAMESPACE_END
