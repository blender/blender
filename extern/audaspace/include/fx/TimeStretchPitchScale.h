/*******************************************************************************
 * Copyright 2009-2025 Jörg Müller
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
 * @file TimeStretchPitchScale.h
 * @ingroup fx
 * The TimeStretchPitchScale class.
 */

#include "fx/Effect.h"

AUD_NAMESPACE_BEGIN

enum class StretcherQuality
{
	HIGH = 0,      // Prioritize high-quality pitch processing
	FAST = 1,      // Prioritize speed over audio quality
	CONSISTENT = 2 // Prioritize consistency for dynamic pitch changes
};

/**
 * This sound allows a sound to be time-stretched and pitch scaled.
 * \note The reader has to be seekable.
 */
class AUD_API TimeStretchPitchScale : public Effect
{
private:
	/**
	 * The factor by which to stretch or compress time.
	 */
	double m_timeRatio;

	/**
	 * The factor by which to adjust the pitch.
	 */
	double m_pitchScale;

	/**
	 * Rubberband stretcher quality.
	 */
	StretcherQuality m_quality;

	/**
	 * Whether to preserve the vocal formants during pitch-shifting
	 */
	bool m_preserveFormant;

	// delete copy constructor and operator=
	TimeStretchPitchScale(const TimeStretchPitchScale&) = delete;
	TimeStretchPitchScale& operator=(const TimeStretchPitchScale&) = delete;

public:
	/**
	 * Creates a new time-stretch, pitch scaled sound.
	 * \param sound The input sound.
	 * \param timeRatio The factor by which to stretch or compress time.
	 * \param pitchScale The factor by which to adjust the pitch.
	 * \param quality The processing quality level.
	 * \param preserveFormant Whether to preserve the vocal formants for the stretcher.
	 */
	TimeStretchPitchScale(std::shared_ptr<ISound> sound, double timeRatio, double pitchScale, StretcherQuality quality, bool preserveFormant);

	/**
	 * Returns the time ratio.
	 */
	double getTimeRatio() const;

	/**
	 * Returns the pitch scale.
	 */
	double getPitchScale() const;

	/**
	 * Returns whether formant preservation is enabled.
	 */
	bool getPreserveFormant() const;
	virtual std::shared_ptr<IReader> createReader();
};

AUD_NAMESPACE_END
