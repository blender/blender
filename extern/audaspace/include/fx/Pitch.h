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
 * @file Pitch.h
 * @ingroup fx
 * The Pitch class.
 */

#include "fx/Effect.h"

AUD_NAMESPACE_BEGIN

/**
 * This sound changes the pitch of another sound.
 */
class AUD_API Pitch : public Effect
{
private:
	/**
	 * The pitch.
	 */
	const float m_pitch;

	// delete copy constructor and operator=
	Pitch(const Pitch&) = delete;
	Pitch& operator=(const Pitch&) = delete;

public:
	/**
	 * Creates a new pitch sound.
	 * \param sound The input sound.
	 * \param pitch The desired pitch.
	 */
	Pitch(std::shared_ptr<ISound> sound, float pitch);

	virtual std::shared_ptr<IReader> createReader();
};

AUD_NAMESPACE_END
