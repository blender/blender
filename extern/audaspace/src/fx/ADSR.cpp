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

#include "fx/ADSR.h"
#include "fx/ADSRReader.h"

AUD_NAMESPACE_BEGIN

ADSR::ADSR(std::shared_ptr<ISound> sound, float attack, float decay, float sustain, float release) :
		Effect(sound),
		m_attack(attack), m_decay(decay), m_sustain(sustain), m_release(release)
{
}

float ADSR::getAttack() const
{
	return m_attack;
}

void ADSR::setAttack(float attack)
{
	m_attack = attack;
}

float ADSR::getDecay() const
{
	return m_decay;
}

void ADSR::setDecay(float decay)
{
	m_decay = decay;
}

float ADSR::getSustain() const
{
	return m_sustain;
}

void ADSR::setSustain(float sustain)
{
	m_sustain = sustain;
}

float ADSR::getRelease() const
{
	return m_release;
}

void ADSR::setRelease(float release)
{
	m_release = release;
}

std::shared_ptr<IReader> ADSR::createReader()
{
	return std::shared_ptr<IReader>(new ADSRReader(getReader(), m_attack, m_decay, m_sustain, m_release));
}

AUD_NAMESPACE_END
