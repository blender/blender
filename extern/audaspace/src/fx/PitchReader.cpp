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

#include "fx/PitchReader.h"

AUD_NAMESPACE_BEGIN

PitchReader::PitchReader(std::shared_ptr<IReader> reader, float pitch) :
		EffectReader(reader), m_pitch(pitch)
{
}

Specs PitchReader::getSpecs() const
{
	Specs specs = m_reader->getSpecs();
	specs.rate *= m_pitch;
	return specs;
}

float PitchReader::getPitch() const
{
	return m_pitch;
}

void PitchReader::setPitch(float pitch)
{
	if(pitch > 0.0f)
		m_pitch = pitch;
}

AUD_NAMESPACE_END
