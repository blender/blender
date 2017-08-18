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

#include "respec/Mixer.h"

#include <algorithm>
#include <cstring>

AUD_NAMESPACE_BEGIN

Mixer::Mixer(DeviceSpecs specs) :
	m_specs(specs)
{
	switch(m_specs.format)
	{
	case FORMAT_U8:
		m_convert = convert_float_u8;
		break;
	case FORMAT_S16:
		m_convert = convert_float_s16;
		break;
	case FORMAT_S24:

#ifdef __BIG_ENDIAN__
		m_convert = convert_float_s24_be;
#else
		m_convert = convert_float_s24_le;
#endif
		break;
	case FORMAT_S32:
		m_convert = convert_float_s32;
		break;
	case FORMAT_FLOAT32:
		m_convert = convert_copy<float>;
		break;
	case FORMAT_FLOAT64:
		m_convert = convert_float_double;
		break;
	default:
		break;
	}
}

DeviceSpecs Mixer::getSpecs() const
{
	return m_specs;
}

void Mixer::setSpecs(Specs specs)
{
	m_specs.specs = specs;
}

void Mixer::clear(int length)
{
	m_buffer.assureSize(length * m_specs.channels * AUD_SAMPLE_SIZE(m_specs));

	m_length = length;

	std::memset(m_buffer.getBuffer(), 0, length * m_specs.channels * AUD_SAMPLE_SIZE(m_specs));
}

void Mixer::mix(sample_t* buffer, int start, int length, float volume)
{
	sample_t* out = m_buffer.getBuffer();

	length = (std::min(m_length, length + start) - start) * m_specs.channels;
	start *= m_specs.channels;

	for(int i = 0; i < length; i++)
		out[i + start] += buffer[i] * volume;
}

void Mixer::mix(sample_t* buffer, int start, int length, float volume_to, float volume_from)
{
	sample_t* out = m_buffer.getBuffer();

	length = (std::min(m_length, length + start) - start);

	for(int i = 0; i < length; i++)
	{
		float volume = volume_from * (1.0f - i / float(length)) + volume_to * (i / float(length));

		for(int c = 0; c < m_specs.channels; c++)
			out[(i + start) * m_specs.channels + c] += buffer[i * m_specs.channels + c] * volume;
	}
}

void Mixer::read(data_t* buffer, float volume)
{
	sample_t* out = m_buffer.getBuffer();

	for(int i = 0; i < m_length * m_specs.channels; i++)
		out[i] *= volume;

	m_convert(buffer, (data_t*) out, m_length * m_specs.channels);
}

AUD_NAMESPACE_END
