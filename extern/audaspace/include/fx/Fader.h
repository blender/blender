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
 * @file Fader.h
 * @ingroup fx
 * The Fader class.
 */

#include "fx/Effect.h"
#include "fx/FaderReader.h"

AUD_NAMESPACE_BEGIN

/**
 * This sound fades another sound.
 * If the fading type is FADE_IN, everything before the fading start will be
 * silenced, for FADE_OUT that's true for everything after fading ends.
 */
class AUD_API Fader : public Effect
{
private:
	/**
	 * The fading type.
	 */
	const FadeType m_type;

	/**
	 * The fading start.
	 */
	const float m_start;

	/**
	 * The fading length.
	 */
	const float m_length;

	// delete copy constructor and operator=
	Fader(const Fader&) = delete;
	Fader& operator=(const Fader&) = delete;

public:
	/**
	 * Creates a new fader sound.
	 * \param sound The input sound.
	 * \param type The fading type.
	 * \param start The time where fading should start in seconds.
	 * \param length How long fading should last in seconds.
	 */
	Fader(std::shared_ptr<ISound> sound,
	                 FadeType type = FADE_IN,
	                 float start = 0.0f, float length = 1.0f);

	/**
	 * Returns the fading type.
	 */
	FadeType getType() const;

	/**
	 * Returns the fading start.
	 */
	float getStart() const;

	/**
	 * Returns the fading length.
	 */
	float getLength() const;

	virtual std::shared_ptr<IReader> createReader();
};

AUD_NAMESPACE_END
