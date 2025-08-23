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
 * @file AnimateableTimeStretchPitchScale.h
 * @ingroup fx
 * The AnimateableTimeStretchPitchScale class.
 */

#include "fx/Effect.h"
#include "fx/TimeStretchPitchScale.h"
#include "sequence/AnimateableProperty.h"

AUD_NAMESPACE_BEGIN

/**
 * This sound allows a sound to be time-stretched and pitch scaled with animation support
 * \note The reader has to be seekable.
 */
class AUD_API AnimateableTimeStretchPitchScale : public Effect
{
private:
	/**
	 * The FPS of the animation system.
	 */
	float m_fps;

	/**
	 * The animateable time-stretch property.
	 */
	std::shared_ptr<AnimateableProperty> m_timeStretch;

	/**
	 * The animateable pitch-scale property.
	 */
	std::shared_ptr<AnimateableProperty> m_pitchScale;

	/**
	 * Rubberband stretcher quality options.
	 */
	StretcherQuality m_quality;

	/**
	 * Whether to preserve the vocal formants for the stretcher.
	 */
	bool m_preserveFormant;

	// delete copy constructor and operator=
	AnimateableTimeStretchPitchScale(const AnimateableTimeStretchPitchScale&) = delete;
	AnimateableTimeStretchPitchScale& operator=(const AnimateableTimeStretchPitchScale&) = delete;

public:
	/**
	 * Creates a new time-stretch, pitch-scaled sound that can be animated.
	 * \param sound The input sound.
	 * \param fps The fps of the animation system.
	 * \param timeRatio The starting factor by which to stretch or compress time.
	 * \param pitchScale The starting factor by which to adjust the pitch.
	 * \param quality The processing quality level of the stretcher.
	 * \param preserveFormant Whether to preserve the vocal formants for the stretcher.
	 */
	AnimateableTimeStretchPitchScale(std::shared_ptr<ISound> sound, float fps, float timeStretch, float pitchScale, StretcherQuality quality, bool preserveFormant);

	/**
	 * Creates a new time-stretch, pitch-scaled sound that can be animated.
	 * \param sound The input sound.
	 * \param fps The fps of the anumation system.
	 * \param timeRatio The animateable time-stretch property.
	 * \param pitchScale The animateable pitch-scale property.
	 * \param quality The processing quality level of the stretcher.
	 * \param preserveFormant Whether to preserve the vocal formants for the stretcher.
	 */
	AnimateableTimeStretchPitchScale(std::shared_ptr<ISound> sound, float fps, std::shared_ptr<AnimateableProperty> timeStretch, std::shared_ptr<AnimateableProperty> pitchScale,
	                                 StretcherQuality quality, bool preserveFormant);

	/**
	 * Returns whether formant preservation is enabled.
	 */
	bool getPreserveFormant() const;

	/**
	 * Returns the quality of the stretcher.
	 */
	StretcherQuality getStretcherQuality() const;

	/**
	 * Retrieves one of the animated properties of the sound.
	 * \param type Which animated property to retrieve.
	 * \return A shared pointer to the animated property
	 */
	std::shared_ptr<AnimateableProperty> getAnimProperty(AnimateablePropertyType type);

	/**
	 * Retrieves the animation system's FPS.
	 * \return The animation system's FPS.
	 */
	float getFPS() const;

	/**
	 * Sets the animation system's FPS.
	 * \param fps The new FPS.
	 */
	void setFPS(float fps);

	virtual std::shared_ptr<IReader> createReader();
};

AUD_NAMESPACE_END