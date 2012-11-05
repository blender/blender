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

/** \file audaspace/intern/AUD_SequencerEntry.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_SequencerEntry.h"
#include "AUD_SequencerReader.h"
#include "AUD_MutexLock.h"

#include <cmath>
#include <limits>

AUD_SequencerEntry::AUD_SequencerEntry(boost::shared_ptr<AUD_IFactory> sound, float begin, float end, float skip, int id) :
	m_status(0),
	m_pos_status(1),
	m_sound_status(0),
	m_id(id),
	m_sound(sound),
	m_begin(begin),
	m_end(end),
	m_skip(skip),
	m_muted(false),
	m_relative(true),
	m_volume_max(1.0f),
	m_volume_min(0),
	m_distance_max(std::numeric_limits<float>::max()),
	m_distance_reference(1.0f),
	m_attenuation(1.0f),
	m_cone_angle_outer(360),
	m_cone_angle_inner(360),
	m_cone_volume_outer(0),
	m_location(3),
	m_orientation(4)
{
	AUD_Quaternion q;
	m_orientation.write(q.get());
	float f = 1;
	m_volume.write(&f);
	m_pitch.write(&f);

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&m_mutex, &attr);

	pthread_mutexattr_destroy(&attr);
}

AUD_SequencerEntry::~AUD_SequencerEntry()
{
	pthread_mutex_destroy(&m_mutex);
}

void AUD_SequencerEntry::lock()
{
	pthread_mutex_lock(&m_mutex);
}

void AUD_SequencerEntry::unlock()
{
	pthread_mutex_unlock(&m_mutex);
}

void AUD_SequencerEntry::setSound(boost::shared_ptr<AUD_IFactory> sound)
{
	AUD_MutexLock lock(*this);

	if(m_sound.get() != sound.get())
	{
		m_sound = sound;
		m_sound_status++;
	}
}

void AUD_SequencerEntry::move(float begin, float end, float skip)
{
	AUD_MutexLock lock(*this);

	if(m_begin != begin || m_skip != skip || m_end != end)
	{
		m_begin = begin;
		m_skip = skip;
		m_end = end;
		m_pos_status++;
	}
}

void AUD_SequencerEntry::mute(bool mute)
{
	AUD_MutexLock lock(*this);

	m_muted = mute;
}

int AUD_SequencerEntry::getID() const
{
	return m_id;
}

AUD_AnimateableProperty* AUD_SequencerEntry::getAnimProperty(AUD_AnimateablePropertyType type)
{
	switch(type)
	{
	case AUD_AP_VOLUME:
		return &m_volume;
	case AUD_AP_PITCH:
		return &m_pitch;
	case AUD_AP_PANNING:
		return &m_panning;
	case AUD_AP_LOCATION:
		return &m_location;
	case AUD_AP_ORIENTATION:
		return &m_orientation;
	default:
		return NULL;
	}
}

void AUD_SequencerEntry::updateAll(float volume_max, float volume_min, float distance_max,
								   float distance_reference, float attenuation, float cone_angle_outer,
								   float cone_angle_inner, float cone_volume_outer)
{
	AUD_MutexLock lock(*this);

	if(volume_max != m_volume_max)
	{
		m_volume_max = volume_max;
		m_status++;
	}

	if(volume_min != m_volume_min)
	{
		m_volume_min = volume_min;
		m_status++;
	}

	if(distance_max != m_distance_max)
	{
		m_distance_max = distance_max;
		m_status++;
	}

	if(distance_reference != m_distance_reference)
	{
		m_distance_reference = distance_reference;
		m_status++;
	}

	if(attenuation != m_attenuation)
	{
		m_attenuation = attenuation;
		m_status++;
	}

	if(cone_angle_outer != m_cone_angle_outer)
	{
		m_cone_angle_outer = cone_angle_outer;
		m_status++;
	}

	if(cone_angle_inner != m_cone_angle_inner)
	{
		m_cone_angle_inner = cone_angle_inner;
		m_status++;
	}

	if(cone_volume_outer != m_cone_volume_outer)
	{
		m_cone_volume_outer = cone_volume_outer;
		m_status++;
	}
}

bool AUD_SequencerEntry::isRelative()
{
	return m_relative;
}

void AUD_SequencerEntry::setRelative(bool relative)
{
	AUD_MutexLock lock(*this);

	if(m_relative != relative)
	{
		m_relative = relative;
		m_status++;
	}
}

float AUD_SequencerEntry::getVolumeMaximum()
{
	return m_volume_max;
}

void AUD_SequencerEntry::setVolumeMaximum(float volume)
{
	AUD_MutexLock lock(*this);

	m_volume_max = volume;
	m_status++;
}

float AUD_SequencerEntry::getVolumeMinimum()
{
	return m_volume_min;
}

void AUD_SequencerEntry::setVolumeMinimum(float volume)
{
	AUD_MutexLock lock(*this);

	m_volume_min = volume;
	m_status++;
}

float AUD_SequencerEntry::getDistanceMaximum()
{
	return m_distance_max;
}

void AUD_SequencerEntry::setDistanceMaximum(float distance)
{
	AUD_MutexLock lock(*this);

	m_distance_max = distance;
	m_status++;
}

float AUD_SequencerEntry::getDistanceReference()
{
	return m_distance_reference;
}

void AUD_SequencerEntry::setDistanceReference(float distance)
{
	AUD_MutexLock lock(*this);

	m_distance_reference = distance;
	m_status++;
}

float AUD_SequencerEntry::getAttenuation()
{
	return m_attenuation;
}

void AUD_SequencerEntry::setAttenuation(float factor)
{
	AUD_MutexLock lock(*this);

	m_attenuation = factor;
	m_status++;
}

float AUD_SequencerEntry::getConeAngleOuter()
{
	return m_cone_angle_outer;
}

void AUD_SequencerEntry::setConeAngleOuter(float angle)
{
	AUD_MutexLock lock(*this);

	m_cone_angle_outer = angle;
	m_status++;
}

float AUD_SequencerEntry::getConeAngleInner()
{
	return m_cone_angle_inner;
}

void AUD_SequencerEntry::setConeAngleInner(float angle)
{
	AUD_MutexLock lock(*this);

	m_cone_angle_inner = angle;
	m_status++;
}

float AUD_SequencerEntry::getConeVolumeOuter()
{
	return m_cone_volume_outer;
}

void AUD_SequencerEntry::setConeVolumeOuter(float volume)
{
	AUD_MutexLock lock(*this);

	m_cone_volume_outer = volume;
	m_status++;
}
