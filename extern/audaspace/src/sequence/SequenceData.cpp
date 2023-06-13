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

#include "sequence/SequenceData.h"
#include "sequence/SequenceReader.h"
#include "sequence/SequenceEntry.h"

#include <mutex>

AUD_NAMESPACE_BEGIN

SequenceData::SequenceData(Specs specs, float fps, bool muted) :
	m_specs(specs),
	m_status(0),
	m_entry_status(0),
	m_id(0),
	m_muted(muted),
	m_fps(fps),
	m_speed_of_sound(343.3f),
	m_doppler_factor(1),
	m_distance_model(DISTANCE_MODEL_INVERSE_CLAMPED),
	m_volume(1, 1.0f),
	m_location(3),
	m_orientation(4)
{
	Quaternion q;
	m_orientation.write(q.get());
	float f = 1;
	m_volume.write(&f);
}

SequenceData::~SequenceData()
{
}

void SequenceData::lock()
{
	m_mutex.lock();
}

void SequenceData::unlock()
{
	m_mutex.unlock();
}

Specs SequenceData::getSpecs()
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	return m_specs;
}

void SequenceData::setSpecs(Specs specs)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_specs = specs;
	m_status++;
}

float SequenceData::getFPS() const
{
	return m_fps;
}

void SequenceData::setFPS(float fps)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_fps = fps;
}

void SequenceData::mute(bool muted)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_muted = muted;
}

bool SequenceData::isMuted() const
{
	return m_muted;
}

float SequenceData::getSpeedOfSound() const
{
	return m_speed_of_sound;
}

void SequenceData::setSpeedOfSound(float speed)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_speed_of_sound = speed;
	m_status++;
}

float SequenceData::getDopplerFactor() const
{
	return m_doppler_factor;
}

void SequenceData::setDopplerFactor(float factor)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_doppler_factor = factor;
	m_status++;
}

DistanceModel SequenceData::getDistanceModel() const
{
	return m_distance_model;
}

void SequenceData::setDistanceModel(DistanceModel model)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_distance_model = model;
	m_status++;
}

AnimateableProperty* SequenceData::getAnimProperty(AnimateablePropertyType type)
{
	switch(type)
	{
	case AP_VOLUME:
		return &m_volume;
	case AP_LOCATION:
		return &m_location;
	case AP_ORIENTATION:
		return &m_orientation;
	default:
		return nullptr;
	}
}

std::shared_ptr<SequenceEntry> SequenceData::add(std::shared_ptr<ISound> sound, std::shared_ptr<SequenceData> sequence_data, double begin, double end, double skip)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	std::shared_ptr<SequenceEntry> entry = std::shared_ptr<SequenceEntry>(new SequenceEntry(sound, begin, end, skip, sequence_data, m_id++));

	m_entries.push_back(entry);
	m_entry_status++;

	return entry;
}

void SequenceData::remove(std::shared_ptr<SequenceEntry> entry)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	m_entries.remove(entry);
	m_entry_status++;
}

AUD_NAMESPACE_END
