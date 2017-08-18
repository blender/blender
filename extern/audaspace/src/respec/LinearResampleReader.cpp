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

#include "respec/LinearResampleReader.h"

#include <cmath>
#include <cstring>

AUD_NAMESPACE_BEGIN

LinearResampleReader::LinearResampleReader(std::shared_ptr<IReader> reader, SampleRate rate) :
	ResampleReader(reader, rate),
	m_channels(reader->getSpecs().channels),
	m_cache_pos(0),
	m_cache_ok(false)
{
	Specs specs = { rate, m_channels };
	m_cache.resize(2 * AUD_SAMPLE_SIZE(specs));
}

void LinearResampleReader::seek(int position)
{
	position = std::floor(position * double(m_reader->getSpecs().rate) / double(m_rate));
	m_reader->seek(position);
	m_cache_ok = false;
	m_cache_pos = 0;
}

int LinearResampleReader::getLength() const
{
	return std::floor(m_reader->getLength() * double(m_rate) / double(m_reader->getSpecs().rate));
}

int LinearResampleReader::getPosition() const
{
	return std::floor((m_reader->getPosition() + (m_cache_ok ? m_cache_pos - 1 : 0))
				 * m_rate / m_reader->getSpecs().rate);
}

Specs LinearResampleReader::getSpecs() const
{
	Specs specs = m_reader->getSpecs();
	specs.rate = m_rate;
	return specs;
}

void LinearResampleReader::read(int& length, bool& eos, sample_t* buffer)
{
	if(length == 0)
		return;

	Specs specs = m_reader->getSpecs();

	int samplesize = AUD_SAMPLE_SIZE(specs);
	int size = length;
	float factor = m_rate / m_reader->getSpecs().rate;
	float spos = 0.0f;
	sample_t low, high;
	eos = false;

	// check for channels changed

	if(specs.channels != m_channels)
	{
		m_cache.resize(2 * samplesize);
		m_channels = specs.channels;
		m_cache_ok = false;
	}

	if(factor == 1 && (!m_cache_ok || m_cache_pos == 1))
	{
		// can read directly!
		m_reader->read(length, eos, buffer);

		if(length > 0)
		{
			std::memcpy(m_cache.getBuffer() + m_channels, buffer + m_channels * (length - 1), samplesize);
			m_cache_pos = 1;
			m_cache_ok = true;
		}

		return;
	}

	int len;
	sample_t* buf;

	if(m_cache_ok)
	{
		int need = std::ceil(length / factor + m_cache_pos) - 1;

		len = need;

		m_buffer.assureSize((len + 2) * samplesize);
		buf = m_buffer.getBuffer();

		std::memcpy(buf, m_cache.getBuffer(), 2 * samplesize);
		m_reader->read(len, eos, buf + 2 * m_channels);

		if(len < need)
			length = std::floor((len + 1 - m_cache_pos) * factor);
	}
	else
	{
		m_cache_pos = 1 - 1 / factor;

		int need = std::ceil(length / factor + m_cache_pos);

		len = need;

		m_buffer.assureSize((len + 1) * samplesize);
		buf = m_buffer.getBuffer();

		std::memset(buf, 0, samplesize);
		m_reader->read(len, eos, buf + m_channels);

		if(len == 0)
		{
			length = 0;
			return;
		}

		if(len < need)
		{
			length = std::floor((len - m_cache_pos) * factor);
		}

		m_cache_ok = true;
	}

	if(length == 0)
		return;

	for(int channel = 0; channel < m_channels; channel++)
	{
		for(int i = 0; i < length; i++)
		{
			spos = (i + 1) / factor + m_cache_pos;

			low = buf[(int)std::floor(spos) * m_channels + channel];
			high = buf[(int)std::ceil(spos) * m_channels + channel];

			buffer[i * m_channels + channel] = low + (spos - std::floor(spos)) * (high - low);
		}
	}

	if(std::floor(spos) == spos)
	{
		std::memcpy(m_cache.getBuffer() + m_channels, buf + int(std::floor(spos)) * m_channels, samplesize);
		m_cache_pos = 1;
	}
	else
	{
		std::memcpy(m_cache.getBuffer(), buf + int(std::floor(spos)) * m_channels, 2 * samplesize);
		m_cache_pos = spos - std::floor(spos);
	}

	eos &= length < size;
}

AUD_NAMESPACE_END
