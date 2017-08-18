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
 * @file Envelope.h
 * @ingroup fx
 * The Envelope class.
 */

#include "fx/Effect.h"

AUD_NAMESPACE_BEGIN

class CallbackIIRFilterReader;
struct EnvelopeParameters;

/**
 * This sound creates an envelope follower reader.
 */
class AUD_API Envelope : public Effect
{
private:
	/**
	 * The attack value in seconds.
	 */
	const float m_attack;

	/**
	 * The release value in seconds.
	 */
	const float m_release;

	/**
	 * The threshold value.
	 */
	const float m_threshold;

	/**
	 * The attack/release threshold value.
	 */
	const float m_arthreshold;

	// delete copy constructor and operator=
	Envelope(const Envelope&) = delete;
	Envelope& operator=(const Envelope&) = delete;

public:
	/**
	 * Creates a new envelope sound.
	 * \param sound The input sound.
	 * \param attack The attack value in seconds.
	 * \param release The release value in seconds.
	 * \param threshold The threshold value.
	 * \param arthreshold The attack/release threshold value.
	 */
	Envelope(std::shared_ptr<ISound> sound, float attack, float release,
						float threshold, float arthreshold);

	virtual std::shared_ptr<IReader> createReader();

	/**
	 * The envelopeFilter function implements the doFilterIIR callback
	 * for the callback IIR filter.
	 * @param reader The CallbackIIRFilterReader that executes the callback.
	 * @param param The envelope parameters.
	 * @return The filtered sample.
	 */
	static sample_t AUD_LOCAL envelopeFilter(CallbackIIRFilterReader* reader, EnvelopeParameters* param);

	/**
	 * The endEnvelopeFilter function implements the endFilterIIR callback
	 * for the callback IIR filter.
	 * @param param The envelope parameters.
	 */
	static void AUD_LOCAL endEnvelopeFilter(EnvelopeParameters* param);
};

AUD_NAMESPACE_END
