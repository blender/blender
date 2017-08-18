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
 * @file PitchReader.h
 * @ingroup fx
 * The PitchReader class.
 */

#include "fx/EffectReader.h"

AUD_NAMESPACE_BEGIN

/**
 * This class reads another reader and changes it's pitch.
 */
class AUD_API PitchReader : public EffectReader
{
private:
	/**
	 * The pitch level.
	 */
	float m_pitch;

	// delete copy constructor and operator=
	PitchReader(const PitchReader&) = delete;
	PitchReader& operator=(const PitchReader&) = delete;

public:
	/**
	 * Creates a new pitch reader.
	 * \param reader The reader to read from.
	 * \param pitch The pitch value.
	 */
	PitchReader(std::shared_ptr<IReader> reader, float pitch);

	virtual Specs getSpecs() const;

	/**
	 * Retrieves the pitch.
	 * \return The current pitch value.
	 */
	float getPitch() const;

	/**
	 * Sets the pitch.
	 * \param pitch The new pitch value.
	 */
	void setPitch(float pitch);
};

AUD_NAMESPACE_END
