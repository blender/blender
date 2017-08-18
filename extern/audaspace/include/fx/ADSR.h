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
 * @file ADSR.h
 * @ingroup fx
 * The ADSR class.
 */

#include "fx/Effect.h"

AUD_NAMESPACE_BEGIN

/**
 * The ADSR effect implements the Attack-Delay-Sustain-Release behaviour of a sound.
 */
class AUD_API ADSR : public Effect
{
private:
	/**
	 * Attack time.
	 */
	float m_attack;

	/**
	 * Decay time.
	 */
	float m_decay;

	/**
	 * Sustain level.
	 */
	float m_sustain;

	/**
	 * Release time.
	 */
	float m_release;

	// delete copy constructor and operator=
	ADSR(const ADSR&) = delete;
	ADSR& operator=(const ADSR&) = delete;

public:
	/**
	 * Creates a new ADSR object.
	 * @param sound The sound to apply this effect to.
	 * @param attack The attack time in seconds.
	 * @param decay The decay time in seconds.
	 * @param sustain The sustain level as linear volume.
	 * @param release The release time in seconds.
	 */
	ADSR(std::shared_ptr<ISound> sound, float attack, float decay, float sustain, float release);

	/**
	 * Returns the attack time.
	 * @return The attack time in seconds.
	 */
	float getAttack() const;

	/**
	 * Sets the attack time.
	 * @param attack The attack time in seconds.
	 */
	void setAttack(float attack);

	/**
	 * Returns the decay time.
	 * @return The decay time in seconds.
	 */
	float getDecay() const;

	/**
	 * Sets the decay time.
	 * @param decay The decay time in seconds.
	 */
	void setDecay(float decay);

	/**
	 * Returns the sustain level.
	 * @return The sustain level in linear volume.
	 */
	float getSustain() const;

	/**
	 * Sets the sustain level.
	 * @param sustain The sustain level in linear volume.
	 */
	void setSustain(float sustain);

	/**
	 * Returns the release time.
	 * @return The release time in seconds.
	 */
	float getRelease() const;

	/**
	 * Sets the release time.
	 * @param release The release time in seconds.
	 */
	void setRelease(float release);

	virtual std::shared_ptr<IReader> createReader();
};

AUD_NAMESPACE_END
