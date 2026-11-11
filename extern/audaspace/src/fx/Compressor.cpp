/*******************************************************************************
 * Copyright 2026 Mayank
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

#include "fx/Compressor.h"

#include "fx/CompressorReader.h"

AUD_NAMESPACE_BEGIN

Compressor::Compressor(std::shared_ptr<ISound> sound, std::shared_ptr<ISound> sidechain, float threshold, float ratio, float attack, float release, float makeupGain, float knee) :
    Effect(sound), m_sidechain(sidechain), m_threshold(threshold), m_ratio(ratio), m_attack(attack), m_release(release), m_makeupGain(makeupGain), m_knee(knee)
{
}

float Compressor::getThreshold() const
{
	return m_threshold;
}

float Compressor::getRatio() const
{
	return m_ratio;
}

float Compressor::getAttack() const
{
	return m_attack;
}

float Compressor::getRelease() const
{
	return m_release;
}

float Compressor::getMakeupGain() const
{
	return m_makeupGain;
}

float Compressor::getKnee() const
{
	return m_knee;
}

std::shared_ptr<ISound> Compressor::getSidechain() const
{
	return m_sidechain;
}

std::shared_ptr<IReader> Compressor::createReader()
{
	std::shared_ptr<IReader> reader = getReader();
	std::shared_ptr<IReader> sidechain_reader = m_sidechain->createReader();

	return std::make_shared<CompressorReader>(reader, sidechain_reader, m_threshold, m_ratio, m_attack, m_release, m_makeupGain, m_knee);
}

AUD_NAMESPACE_END
