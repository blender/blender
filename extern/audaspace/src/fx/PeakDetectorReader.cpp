/*******************************************************************************
 * Copyright 2026 Mayank
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

#include "fx/PeakDetectorReader.h"

#include <algorithm>
#include <cmath>

AUD_NAMESPACE_BEGIN

PeakDetectorReader::PeakDetectorReader(std::shared_ptr<IReader> reader) : EffectReader(reader)
{
}

void PeakDetectorReader::read(int& length, bool& eos, sample_t* buffer)
{
	Specs specs = m_reader->getSpecs();
	const Channels channels = specs.channels;

	if(channels == CHANNELS_MONO)
	{
		m_reader->read(length, eos, buffer);
		for(int i = 0; i < length; ++i)
		{
			buffer[i] = std::abs(buffer[i]);
		}
		return;
	}

	m_buffer.assureSize(length * AUD_SAMPLE_SIZE(specs));
	sample_t* buf = m_buffer.getBuffer();
	m_reader->read(length, eos, buf);
	if(length == 0)
		return;

	for(int i = 0; i < length; i++)
	{
		// Find peak amplitude across all channels for the current frame.
		float peak = 0.0f;
		for(int ch = 0; ch < channels; ch++)
		{
			peak = std::max(peak, std::abs(buf[i * channels + ch]));
		}

		buffer[i] = peak;
	}
}

Specs PeakDetectorReader::getSpecs() const
{
	Specs specs = m_reader->getSpecs();
	specs.channels = CHANNELS_MONO;
	return specs;
}

AUD_NAMESPACE_END
