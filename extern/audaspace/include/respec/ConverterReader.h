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

#pragma once

/**
 * @file ConverterReader.h
 * @ingroup respec
 * The ConverterReader class.
 */

#include "fx/EffectReader.h"
#include "respec/ConverterFunctions.h"
#include "util/Buffer.h"

AUD_NAMESPACE_BEGIN

/**
 * This class converts a sound source from one to another format.
 */
class AUD_API ConverterReader : public EffectReader
{
private:
	/**
	 * The sound output buffer.
	 */
	Buffer m_buffer;

	/**
	 * The target specification.
	 */
	SampleFormat m_format;

	/**
	 * Converter function.
	 */
	convert_f m_convert;

	// delete copy constructor and operator=
	ConverterReader(const ConverterReader&) = delete;
	ConverterReader& operator=(const ConverterReader&) = delete;

public:
	/**
	 * Creates a converter reader.
	 * \param reader The reader to convert.
	 * \param specs The target specification.
	 */
	ConverterReader(std::shared_ptr<IReader> reader, DeviceSpecs specs);

	virtual void read(int& length, bool& eos, sample_t* buffer);
};

AUD_NAMESPACE_END
