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
 * @file Modulator.h
 * @ingroup fx
 * The Modulator class.
 */

#include "ISound.h"

AUD_NAMESPACE_BEGIN

/**
 * This sound plays two other factories, playing them the same time and modulating/multiplying them.
 * \note Readers from the underlying factories must have the same sample rate
 *       and channel count.
 */
class AUD_API Modulator : public ISound
{
private:
	/**
	 * First played sound.
	 */
	std::shared_ptr<ISound> m_sound1;

	/**
	 * Second played sound.
	 */
	std::shared_ptr<ISound> m_sound2;

	// delete copy constructor and operator=
	Modulator(const Modulator&) = delete;
	Modulator& operator=(const Modulator&) = delete;

public:
	/**
	 * Creates a new modulator sound.
	 * \param sound1 The first input sound.
	 * \param sound2 The second input sound.
	 */
	Modulator(std::shared_ptr<ISound> sound1, std::shared_ptr<ISound> sound2);

	virtual std::shared_ptr<IReader> createReader();
};

AUD_NAMESPACE_END
