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
 * @file Sawtooth.h
 * @ingroup generator
 * The Sawtooth class.
 */

#include "ISound.h"
#include "respec/Specification.h"

AUD_NAMESPACE_BEGIN

/**
 * This sound creates a reader that plays a sawtooth tone.
 */
class AUD_API Sawtooth : public ISound
{
private:
	/**
	 * The frequence of the sawtooth wave.
	 */
	const float m_frequency;

	/**
	 * The target sample rate for output.
	 */
	const SampleRate m_sampleRate;

	// delete copy constructor and operator=
	Sawtooth(const Sawtooth&) = delete;
	Sawtooth& operator=(const Sawtooth&) = delete;

public:
	/**
	 * Creates a new sawtooth sound.
	 * \param frequency The desired frequency.
	 * \param sampleRate The target sample rate for playback.
	 */
	Sawtooth(float frequency, SampleRate sampleRate = RATE_48000);

	/**
	 * Returns the frequency of the sawtooth wave.
	 */
	float getFrequency() const;

	virtual std::shared_ptr<IReader> createReader();
};

AUD_NAMESPACE_END
