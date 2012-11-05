/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * Copyright 2009-2011 Jörg Hermann Müller
 *
 * This file is part of AudaSpace.
 *
 * Audaspace is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * AudaSpace is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Audaspace; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file audaspace/intern/AUD_SequencerFactory.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_SequencerFactory.h"
#include "AUD_SequencerReader.h"
#include "AUD_3DMath.h"
#include "AUD_MutexLock.h"

AUD_SequencerFactory::AUD_SequencerFactory(AUD_Specs specs, float fps, bool muted)
{
	m_sequence = boost::shared_ptr<AUD_Sequencer>(new AUD_Sequencer(specs, fps, muted));
}

/*void AUD_SequencerFactory::lock()
{
	pthread_mutex_lock(&m_mutex);
}

void AUD_SequencerFactory::unlock()
{
	pthread_mutex_unlock(&m_mutex);
}*/

void AUD_SequencerFactory::setSpecs(AUD_Specs specs)
{
	m_sequence->setSpecs(specs);
}

void AUD_SequencerFactory::setFPS(float fps)
{
	m_sequence->setFPS(fps);
}

void AUD_SequencerFactory::mute(bool muted)
{
	m_sequence->mute(muted);
}

bool AUD_SequencerFactory::getMute() const
{
	return m_sequence->getMute();
}

float AUD_SequencerFactory::getSpeedOfSound() const
{
	return m_sequence->getSpeedOfSound();
}

void AUD_SequencerFactory::setSpeedOfSound(float speed)
{
	m_sequence->setSpeedOfSound(speed);
}

float AUD_SequencerFactory::getDopplerFactor() const
{
	return m_sequence->getDopplerFactor();
}

void AUD_SequencerFactory::setDopplerFactor(float factor)
{
	m_sequence->setDopplerFactor(factor);
}

AUD_DistanceModel AUD_SequencerFactory::getDistanceModel() const
{
	return m_sequence->getDistanceModel();
}

void AUD_SequencerFactory::setDistanceModel(AUD_DistanceModel model)
{
	m_sequence->setDistanceModel(model);
}

AUD_AnimateableProperty* AUD_SequencerFactory::getAnimProperty(AUD_AnimateablePropertyType type)
{
	return m_sequence->getAnimProperty(type);
}

boost::shared_ptr<AUD_SequencerEntry> AUD_SequencerFactory::add(boost::shared_ptr<AUD_IFactory> sound, float begin, float end, float skip)
{
	return m_sequence->add(sound, begin, end, skip);
}

void AUD_SequencerFactory::remove(boost::shared_ptr<AUD_SequencerEntry> entry)
{
	m_sequence->remove(entry);
}

boost::shared_ptr<AUD_IReader> AUD_SequencerFactory::createQualityReader()
{
	return boost::shared_ptr<AUD_IReader>(new AUD_SequencerReader(m_sequence, true));
}

boost::shared_ptr<AUD_IReader> AUD_SequencerFactory::createReader()
{
	return boost::shared_ptr<AUD_IReader>(new AUD_SequencerReader(m_sequence));
}
