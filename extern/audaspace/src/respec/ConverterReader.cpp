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

#include "respec/ConverterReader.h"

AUD_NAMESPACE_BEGIN

ConverterReader::ConverterReader(std::shared_ptr<IReader> reader,
										 DeviceSpecs specs) :
	EffectReader(reader),
	m_format(specs.format)
{
	switch(m_format)
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

void ConverterReader::read(int& length, bool& eos, sample_t* buffer)
{
	Specs specs = m_reader->getSpecs();
	int samplesize = AUD_SAMPLE_SIZE(specs);

	m_buffer.assureSize(length * samplesize);

	m_reader->read(length, eos, m_buffer.getBuffer());

	m_convert((data_t*)buffer, (data_t*)m_buffer.getBuffer(),
	          length * specs.channels);
}

AUD_NAMESPACE_END
