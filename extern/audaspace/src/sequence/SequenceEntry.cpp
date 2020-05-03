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

#include "sequence/SequenceEntry.h"
#include "sequence/SequenceReader.h"

#include <limits>
#include <mutex>

AUD_NAMESPACE_BEGIN

SequenceEntry::SequenceEntry(std::shared_ptr<ISound> sound, double begin, double end, double skip, int id) :
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
	m_volume(1, 1.0f),
	m_pitch(1, 1.0f),
	m_location(3),
	m_orientation(4)
{
	Quaternion q;
	m_orientation.write(q.get());
	float f = 1;
	m_volume.write(&f);
	m_pitch.write(&f);
}

SequenceEntry::~SequenceEntry()
{
}

void SequenceEntry::lock()
{
	m_mutex.lock();
}

void SequenceEntry::unlock()
{
	m_mutex.unlock();
}

std::shared_ptr<ISound> SequenceEntry::getSound()
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	return m_sound;
}

void SequenceEntry::setSound(std::shared_ptr<ISound> sound)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if(m_sound.get() != sound.get())
	{
		m_sound = sound;
		m_sound_status++;
	}
}

void SequenceEntry::move(double begin, double end, double skip)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if(m_begin != begin || m_skip != skip || m_end != end)
	{
		m_begin = begin;
		m_skip = skip;
		m_end = end;
		m_pos_status++;
	}
}

bool SequenceEntry::isMuted()
{
	return m_muted;
}

void SequenceEntry::mute(bool mute)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_muted = mute;
}

int SequenceEntry::getID() const
{
	return m_id;
}

AnimateableProperty* SequenceEntry::getAnimProperty(AnimateablePropertyType type)
{
	switch(type)
	{
	case AP_VOLUME:
		return &m_volume;
	case AP_PITCH:
		return &m_pitch;
	case AP_PANNING:
		return &m_panning;
	case AP_LOCATION:
		return &m_location;
	case AP_ORIENTATION:
		return &m_orientation;
	default:
		return nullptr;
	}
}

bool SequenceEntry::isRelative()
{
	return m_relative;
}

void SequenceEntry::setRelative(bool relative)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if(m_relative != relative)
	{
		m_relative = relative;
		m_status++;
	}
}

float SequenceEntry::getVolumeMaximum()
{
	return m_volume_max;
}

void SequenceEntry::setVolumeMaximum(float volume)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_volume_max = volume;
	m_status++;
}

float SequenceEntry::getVolumeMinimum()
{
	return m_volume_min;
}

void SequenceEntry::setVolumeMinimum(float volume)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_volume_min = volume;
	m_status++;
}

float SequenceEntry::getDistanceMaximum()
{
	return m_distance_max;
}

void SequenceEntry::setDistanceMaximum(float distance)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_distance_max = distance;
	m_status++;
}

float SequenceEntry::getDistanceReference()
{
	return m_distance_reference;
}

void SequenceEntry::setDistanceReference(float distance)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_distance_reference = distance;
	m_status++;
}

float SequenceEntry::getAttenuation()
{
	return m_attenuation;
}

void SequenceEntry::setAttenuation(float factor)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_attenuation = factor;
	m_status++;
}

float SequenceEntry::getConeAngleOuter()
{
	return m_cone_angle_outer;
}

void SequenceEntry::setConeAngleOuter(float angle)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_cone_angle_outer = angle;
	m_status++;
}

float SequenceEntry::getConeAngleInner()
{
	return m_cone_angle_inner;
}

void SequenceEntry::setConeAngleInner(float angle)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_cone_angle_inner = angle;
	m_status++;
}

float SequenceEntry::getConeVolumeOuter()
{
	return m_cone_volume_outer;
}

void SequenceEntry::setConeVolumeOuter(float volume)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_cone_volume_outer = volume;
	m_status++;
}

AUD_NAMESPACE_END
