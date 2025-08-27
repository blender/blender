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
 * @file AnimateableTimeStretchPitchScaleReader.h
 * @ingroup fx
 * The AnimateableTimeStretchPitchScaleReader class.
 */
#include "fx/AnimateableTimeStretchPitchScale.h"
#include "fx/TimeStretchPitchScaleReader.h"

AUD_NAMESPACE_BEGIN

/**
 * This class reads from another reader and applies time-stretching and pitch scaling with support for animating both properties.
 */
class AUD_API AnimateableTimeStretchPitchScaleReader : public TimeStretchPitchScaleReader
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

	// delete copy constructor and operator=
	AnimateableTimeStretchPitchScaleReader(const AnimateableTimeStretchPitchScaleReader&) = delete;
	AnimateableTimeStretchPitchScaleReader& operator=(const AnimateableTimeStretchPitchScaleReader&) = delete;

public:
	/**
	 * Creates a new animateable time-stretch, pitch scale reader.
	 * \param reader The input reader.
	 * \param fps The FPS of the animation system.
	 * \param timeStretch The animateable time-stretch property.
	 * \param pitchScale The animateable pitch-scale property.
	 * \param quality The stretcher quality options.
	 * \param preserveFormant Whether to preserve vocal formants.
	 */
	AnimateableTimeStretchPitchScaleReader(std::shared_ptr<IReader> reader, float fp, std::shared_ptr<AnimateableProperty> timeStretch,
	                                       std::shared_ptr<AnimateableProperty> pitchScale, StretcherQuality quality, bool preserveFormant);

	virtual void read(int& length, bool& eos, sample_t* buffer) override;

	virtual void seek(int position) override;
};

AUD_NAMESPACE_END