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

#include "sequence/Sequence.h"
#include "sequence/SequenceReader.h"
#include "sequence/SequenceData.h"

AUD_NAMESPACE_BEGIN

Sequence::Sequence(Specs specs, float fps, bool muted)
{
	m_sequence = std::shared_ptr<SequenceData>(new SequenceData(specs, fps, muted));
}

Specs Sequence::getSpecs()
{
	return m_sequence->getSpecs();
}

void Sequence::setSpecs(Specs specs)
{
	m_sequence->setSpecs(specs);
}

float Sequence::getFPS() const
{
	return m_sequence->getFPS();
}

void Sequence::setFPS(float fps)
{
	m_sequence->setFPS(fps);
}

void Sequence::mute(bool muted)
{
	m_sequence->mute(muted);
}

bool Sequence::isMuted() const
{
	return m_sequence->isMuted();
}

float Sequence::getSpeedOfSound() const
{
	return m_sequence->getSpeedOfSound();
}

void Sequence::setSpeedOfSound(float speed)
{
	m_sequence->setSpeedOfSound(speed);
}

float Sequence::getDopplerFactor() const
{
	return m_sequence->getDopplerFactor();
}

void Sequence::setDopplerFactor(float factor)
{
	m_sequence->setDopplerFactor(factor);
}

DistanceModel Sequence::getDistanceModel() const
{
	return m_sequence->getDistanceModel();
}

void Sequence::setDistanceModel(DistanceModel model)
{
	m_sequence->setDistanceModel(model);
}

AnimateableProperty* Sequence::getAnimProperty(AnimateablePropertyType type)
{
	return m_sequence->getAnimProperty(type);
}

std::shared_ptr<SequenceEntry> Sequence::add(std::shared_ptr<ISound> sound, double begin, double end, double skip)
{
	return m_sequence->add(sound, m_sequence, begin, end, skip);
}

void Sequence::remove(std::shared_ptr<SequenceEntry> entry)
{
	m_sequence->remove(entry);
}

std::shared_ptr<IReader> Sequence::createQualityReader()
{
	return std::shared_ptr<IReader>(new SequenceReader(m_sequence, true));
}

std::shared_ptr<IReader> Sequence::createReader()
{
	return std::shared_ptr<IReader>(new SequenceReader(m_sequence));
}

AUD_NAMESPACE_END
