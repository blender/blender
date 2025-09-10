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

#include "fx/AnimateableTimeStretchPitchScale.h"

#include "fx/AnimateableTimeStretchPitchScaleReader.h"

AUD_NAMESPACE_BEGIN

AnimateableTimeStretchPitchScale::AnimateableTimeStretchPitchScale(std::shared_ptr<ISound> sound, float fps, float timeStretch, float pitchScale, StretcherQuality quality,
                                                                   bool preserveFormant) :
    Effect(sound),
    m_fps(fps),
    m_timeStretch(std::make_shared<AnimateableProperty>(1, timeStretch)),
    m_pitchScale(std::make_shared<AnimateableProperty>(1, pitchScale)),
    m_quality(quality),
    m_preserveFormant(preserveFormant)
{
}

AnimateableTimeStretchPitchScale::AnimateableTimeStretchPitchScale(std::shared_ptr<ISound> sound, float fps, std::shared_ptr<AnimateableProperty> timeStretch,
                                                                   std::shared_ptr<AnimateableProperty> pitchScale, StretcherQuality quality, bool preserveFormant) :
    Effect(sound), m_fps(fps), m_timeStretch(timeStretch), m_pitchScale(pitchScale), m_quality(quality), m_preserveFormant(preserveFormant)
{
}

std::shared_ptr<IReader> AnimateableTimeStretchPitchScale::createReader()
{
	return std::make_shared<AnimateableTimeStretchPitchScaleReader>(getReader(), m_fps, m_timeStretch, m_pitchScale, m_quality, m_preserveFormant);
}

bool AnimateableTimeStretchPitchScale::getPreserveFormant() const
{
	return m_preserveFormant;
}

StretcherQuality AnimateableTimeStretchPitchScale::getStretcherQuality() const
{
	return m_quality;
}

std::shared_ptr<AnimateableProperty> AnimateableTimeStretchPitchScale::getAnimProperty(AnimateablePropertyType type)
{
	switch(type)
	{
	case AP_TIME_STRETCH:
		return m_timeStretch;
	case AP_PITCH_SCALE:
		return m_pitchScale;
	default:
		return nullptr;
	}
}

float AnimateableTimeStretchPitchScale::getFPS() const
{
	return m_fps;
}

void AnimateableTimeStretchPitchScale::setFPS(float fps)
{
	m_fps = fps;
}

AUD_NAMESPACE_END