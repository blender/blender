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

/** \file audaspace/intern/AUD_Sequencer.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_Sequencer.h"
#include "AUD_SequencerReader.h"
#include "AUD_3DMath.h"
#include "AUD_MutexLock.h"

AUD_Sequencer::AUD_Sequencer(AUD_Specs specs, float fps, bool muted) :
	m_specs(specs),
	m_status(0),
	m_entry_status(0),
	m_id(0),
	m_muted(muted),
	m_fps(fps),
	m_speed_of_sound(434),
	m_doppler_factor(1),
	m_distance_model(AUD_DISTANCE_MODEL_INVERSE_CLAMPED),
	m_volume(1, 1.0f),
	m_location(3),
	m_orientation(4)
{
	AUD_Quaternion q;
	m_orientation.write(q.get());
	float f = 1;
	m_volume.write(&f);

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&m_mutex, &attr);

	pthread_mutexattr_destroy(&attr);
}

AUD_Sequencer::~AUD_Sequencer()
{
	pthread_mutex_destroy(&m_mutex);
}

void AUD_Sequencer::lock()
{
	pthread_mutex_lock(&m_mutex);
}

void AUD_Sequencer::unlock()
{
	pthread_mutex_unlock(&m_mutex);
}

void AUD_Sequencer::setSpecs(AUD_Specs specs)
{
	AUD_MutexLock lock(*this);

	m_specs = specs;
	m_status++;
}

void AUD_Sequencer::setFPS(float fps)
{
	AUD_MutexLock lock(*this);

	m_fps = fps;
}

void AUD_Sequencer::mute(bool muted)
{
	AUD_MutexLock lock(*this);

	m_muted = muted;
}

bool AUD_Sequencer::getMute() const
{
	return m_muted;
}

float AUD_Sequencer::getSpeedOfSound() const
{
	return m_speed_of_sound;
}

void AUD_Sequencer::setSpeedOfSound(float speed)
{
	AUD_MutexLock lock(*this);

	m_speed_of_sound = speed;
	m_status++;
}

float AUD_Sequencer::getDopplerFactor() const
{
	return m_doppler_factor;
}

void AUD_Sequencer::setDopplerFactor(float factor)
{
	AUD_MutexLock lock(*this);

	m_doppler_factor = factor;
	m_status++;
}

AUD_DistanceModel AUD_Sequencer::getDistanceModel() const
{
	return m_distance_model;
}

void AUD_Sequencer::setDistanceModel(AUD_DistanceModel model)
{
	AUD_MutexLock lock(*this);

	m_distance_model = model;
	m_status++;
}

AUD_AnimateableProperty* AUD_Sequencer::getAnimProperty(AUD_AnimateablePropertyType type)
{
	switch(type)
	{
	case AUD_AP_VOLUME:
		return &m_volume;
	case AUD_AP_LOCATION:
		return &m_location;
	case AUD_AP_ORIENTATION:
		return &m_orientation;
	default:
		return NULL;
	}
}

boost::shared_ptr<AUD_SequencerEntry> AUD_Sequencer::add(boost::shared_ptr<AUD_IFactory> sound, float begin, float end, float skip)
{
	AUD_MutexLock lock(*this);

	boost::shared_ptr<AUD_SequencerEntry> entry = boost::shared_ptr<AUD_SequencerEntry>(new AUD_SequencerEntry(sound, begin, end, skip, m_id++));

	m_entries.push_back(entry);
	m_entry_status++;

	return entry;
}

void AUD_Sequencer::remove(boost::shared_ptr<AUD_SequencerEntry> entry)
{
	AUD_MutexLock lock(*this);

	m_entries.remove(entry);
	m_entry_status++;
}
