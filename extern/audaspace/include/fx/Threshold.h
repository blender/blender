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
 * @file Threshold.h
 * @ingroup fx
 * The Threshold class.
 */

#include "fx/Effect.h"

AUD_NAMESPACE_BEGIN

class CallbackIIRFilterReader;

/**
 * This sound Transforms any signal to a square signal by thresholding.
 */
class AUD_API Threshold : public Effect
{
private:
	/**
	 * The threshold.
	 */
	const float m_threshold;

	// delete copy constructor and operator=
	Threshold(const Threshold&) = delete;
	Threshold& operator=(const Threshold&) = delete;

public:
	/**
	 * Creates a new threshold sound.
	 * \param sound The input sound.
	 * \param threshold The threshold.
	 */
	Threshold(std::shared_ptr<ISound> sound, float threshold = 0.0f);

	/**
	 * Returns the threshold.
	 */
	float getThreshold() const;

	virtual std::shared_ptr<IReader> createReader();

	/**
	 * The thresholdFilter function implements the doFilterIIR callback
	 * for the callback IIR filter.
	 * @param reader The CallbackIIRFilterReader that executes the callback.
	 * @param threshold The threshold value.
	 * @return The filtered sample.
	 */
	static sample_t AUD_LOCAL thresholdFilter(CallbackIIRFilterReader* reader, float* threshold);

	/**
	 * The endThresholdFilter function implements the endFilterIIR callback
	 * for the callback IIR filter.
	 * @param threshold The threshold value.
	 */
	static void AUD_LOCAL endThresholdFilter(float* threshold);
};

AUD_NAMESPACE_END
