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
 * @file ADSRReader.h
 * @ingroup fx
 * The ADSRReader class.
 */

#include "fx/EffectReader.h"

AUD_NAMESPACE_BEGIN

/**
 * This class is an ADSR filters.
 */
class AUD_API ADSRReader : public EffectReader
{
private:
	enum ADSRState
	{
		ADSR_STATE_INVALID = 0,		/// Invalid ADSR state or finished.
		ADSR_STATE_ATTACK  = 1,		/// Initial attack state.
		ADSR_STATE_DECAY   = 2,		/// Decay state.
		ADSR_STATE_SUSTAIN = 3,		/// Sustain state.
		ADSR_STATE_RELEASE = 4		/// Release state.
	};

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

	/**
	 * Current state.
	 */
	ADSRState m_state;

	/**
	 * Current level.
	 */
	float m_level;

	// delete copy constructor and operator=
	ADSRReader(const ADSRReader&) = delete;
	ADSRReader& operator=(const ADSRReader&) = delete;

	void AUD_LOCAL nextState(ADSRState state);

public:
	/**
	 * Creates a new ADSR reader.
	 * \param reader The reader to read from.
	 * \param attack The attack time in seconds.
	 * \param decay The decay time in seconds.
	 * \param sustain The sustain level, should be in range [0 - 1].
	 * \param release The release time in seconds.
	 */
	ADSRReader(std::shared_ptr<IReader> reader, float attack, float decay, float sustain, float release);

	virtual ~ADSRReader();

	virtual void read(int& length, bool& eos, sample_t* buffer);

	/**
	 * Triggers the release.
	 */
	void release();
};

AUD_NAMESPACE_END
