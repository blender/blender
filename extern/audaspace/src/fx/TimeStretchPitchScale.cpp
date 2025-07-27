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

#include "fx/TimeStretchPitchScale.h"

#include "fx/TimeStretchPitchScaleReader.h"

AUD_NAMESPACE_BEGIN

TimeStretchPitchScale::TimeStretchPitchScale(std::shared_ptr<ISound> sound, double timeRatio, double pitchScale, StretcherQuality quality, bool preserveFormant) :
    Effect(sound), m_timeRatio(timeRatio), m_pitchScale(pitchScale), m_quality(quality), m_preserveFormant(preserveFormant)

{
}

std::shared_ptr<IReader> TimeStretchPitchScale::createReader()
{
	return std::shared_ptr<IReader>(new TimeStretchPitchScaleReader(getReader(), m_timeRatio, m_pitchScale, m_quality, m_preserveFormant));
}

double TimeStretchPitchScale::getTimeRatio() const
{
	return m_timeRatio;
}

double TimeStretchPitchScale::getPitchScale() const
{
	return m_pitchScale;
}

bool TimeStretchPitchScale::getPreserveFormant() const
{
	return m_preserveFormant;
}
AUD_NAMESPACE_END