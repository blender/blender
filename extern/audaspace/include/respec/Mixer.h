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
 * @file Mixer.h
 * @ingroup respec
 * The Mixer class.
 */

#include "respec/Specification.h"
#include "respec/ConverterFunctions.h"
#include "util/Buffer.h"

#include <memory>

AUD_NAMESPACE_BEGIN

class IReader;

/**
 * This abstract class is able to mix audiosignals with same channel count
 * and sample rate and convert it to a specific output format.
 */
class AUD_API Mixer
{
private:
	// delete copy constructor and operator=
	Mixer(const Mixer&) = delete;
	Mixer& operator=(const Mixer&) = delete;

protected:
	/**
	 * The output specification.
	 */
	DeviceSpecs m_specs;

	/**
	 * The length of the mixing buffer.
	 */
	int m_length;

	/**
	 * The mixing buffer.
	 */
	Buffer m_buffer;

	/**
	 * Converter function.
	 */
	convert_f m_convert;

public:
	/**
	 * Creates the mixer.
	 */
	Mixer(DeviceSpecs specs);

	/**
	 * Destroys the mixer.
	 */
	virtual ~Mixer() {}

	/**
	 * Returns the target specification for superposing.
	 * \return The target specification.
	 */
	DeviceSpecs getSpecs() const;

	/**
	 * Sets the target specification for superposing.
	 * \param specs The target specification.
	 */
	void setSpecs(Specs specs);

	/**
	 * Mixes a buffer.
	 * \param buffer The buffer to superpose.
	 * \param start The start sample of the buffer.
	 * \param length The length of the buffer in samples.
	 * \param volume The mixing volume. Must be a value between 0.0 and 1.0.
	 */
	void mix(sample_t* buffer, int start, int length, float volume);

	/**
	 * Mixes a buffer with linear volume interpolation.
	 * \param buffer The buffer to superpose.
	 * \param start The start sample of the buffer.
	 * \param length The length of the buffer in samples.
	 * \param volume_to The target mixing volume. Must be a value between 0.0 and 1.0.
	 * \param volume_from The start mixing volume. Must be a value between 0.0 and 1.0.
	 */
	void mix(sample_t* buffer, int start, int length, float volume_to, float volume_from);

	/**
	 * Writes the mixing buffer into an output buffer.
	 * \param buffer The target buffer for superposing.
	 * \param volume The mixing volume. Must be a value between 0.0 and 1.0.
	 */
	void read(data_t* buffer, float volume);

	/**
	 * Clears the mixing buffer.
	 * \param length The length of the buffer in samples.
	 */
	void clear(int length);
};

AUD_NAMESPACE_END
