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

#include "SequenceHandle.h"
#include "sequence/SequenceEntry.h"
#include "devices/ReadDevice.h"
#include "Exception.h"

#include <mutex>

#define KEEP_TIME 10
#define POSITION_EPSILON (1.0 / static_cast<double>(RATE_48000))

AUD_NAMESPACE_BEGIN

void SequenceHandle::start()
{
	// we already tried to start, aborting
	if(!m_valid)
		return;

	// in case the sound is playing, we need to stop first
	stop();

	std::lock_guard<ILockable> lock(*m_entry);

	// let's try playing
	if(m_entry->m_sound.get())
	{
		try
		{
			m_handle = m_device.play(m_entry->m_sound, true);
			m_3dhandle = std::dynamic_pointer_cast<I3DHandle>(m_handle);
		}
		catch(Exception&)
		{
			// handle stays invalid in case we get an exception
		}

		// after starting we have to set the properties, so let's ensure that
		m_status--;
	}

	// if the sound could not be played, we invalidate
	m_valid = m_handle.get();
}

bool SequenceHandle::updatePosition(double position)
{
	std::lock_guard<ILockable> lock(*m_entry);

	if(m_handle.get())
	{
		// we currently have a handle, let's check where we are
		if(position - POSITION_EPSILON >= m_entry->m_end)
		{
			if(position >= m_entry->m_end + KEEP_TIME)
				// far end, stopping
				stop();
			else
			{
				// close end, just pausing
				m_handle->pause();
				return true;
			}
		}
		else if(position + POSITION_EPSILON >= m_entry->m_begin)
		{
			// inside, resuming
			m_handle->resume();
			return true;
		}
		else
		{
			if(position < m_entry->m_begin - KEEP_TIME)
				// far beginning, stopping
				stop();
			else
			{
				// close beginning, just pausing
				m_handle->pause();
				return true;
			}
		}
	}
	else
	{
		// we don't have a handle, let's start if we should be playing
		if(position + POSITION_EPSILON >= m_entry->m_begin && position - POSITION_EPSILON <= m_entry->m_end)
		{
			start();
			return m_valid;
		}
	}

	return false;
}

SequenceHandle::SequenceHandle(std::shared_ptr<SequenceEntry> entry, ReadDevice& device) :
	m_entry(entry),
	m_valid(true),
	m_status(0),
	m_pos_status(0),
	m_sound_status(0),
	m_device(device)
{
}

SequenceHandle::~SequenceHandle()
{
	stop();
}

int SequenceHandle::compare(std::shared_ptr<SequenceEntry> entry) const
{
	if(m_entry->getID() < entry->getID())
		return -1;
	else if(m_entry->getID() == entry->getID())
		return 0;
	return 1;
}

void SequenceHandle::stop()
{
	if(m_handle.get())
		m_handle->stop();
	m_handle = nullptr;
	m_3dhandle = nullptr;
}

void SequenceHandle::update(double position, float frame, float fps)
{
	if(m_sound_status != m_entry->m_sound_status)
	{
		// if a new sound has been set, it's possible to get valid again!
		m_sound_status = m_entry->m_sound_status;
		m_valid = true;

		// stop whatever sound has been playing
		stop();

		// seek starts and seeks to the correct position
		if(!seek(position))
			// no handle, aborting
			return;
	}
	else
	{
		if(!m_valid)
			// invalid, aborting
			return;

		if(m_handle.get())
		{
			// we have a handle, let's update the position
			if(!updatePosition(position))
				// lost handle, aborting
				return;
		}
		else
		{
			// we don't have a handle, let's see if we can start
			if(!seek(position))
				return;
		}
	}

	std::lock_guard<ILockable> lock(*m_entry);
	if(m_pos_status != m_entry->m_pos_status)
	{
		m_pos_status = m_entry->m_pos_status;

		// position changed, need to seek
		if(!seek(position))
			// lost handle, aborting
			return;
	}

	// so far everything alright and handle is there, let's keep going

	if(m_status != m_entry->m_status)
	{
		m_3dhandle->setRelative(m_entry->m_relative);
		m_3dhandle->setVolumeMaximum(m_entry->m_volume_max);
		m_3dhandle->setVolumeMinimum(m_entry->m_volume_min);
		m_3dhandle->setDistanceMaximum(m_entry->m_distance_max);
		m_3dhandle->setDistanceReference(m_entry->m_distance_reference);
		m_3dhandle->setAttenuation(m_entry->m_attenuation);
		m_3dhandle->setConeAngleOuter(m_entry->m_cone_angle_outer);
		m_3dhandle->setConeAngleInner(m_entry->m_cone_angle_inner);
		m_3dhandle->setConeVolumeOuter(m_entry->m_cone_volume_outer);

		m_status = m_entry->m_status;
	}

	float value;

	m_entry->m_volume.read(frame, &value);
	m_handle->setVolume(value);
	m_entry->m_pitch.read(frame, &value);
	m_handle->setPitch(value);
	m_entry->m_panning.read(frame, &value);
	SoftwareDevice::setPanning(m_handle.get(), value);

	Vector3 v, v2;
	Quaternion q;

	m_entry->m_orientation.read(frame, q.get());
	m_3dhandle->setOrientation(q);
	m_entry->m_location.read(frame, v.get());
	m_3dhandle->setLocation(v);
	m_entry->m_location.read(frame + 1, v2.get());
	v2 -= v;
	m_3dhandle->setVelocity(v2 * fps);

	if(m_entry->m_muted)
		m_handle->setVolume(0);
}

bool SequenceHandle::seek(double position)
{
	if(!m_valid)
		// sound not valid, aborting
		return false;

	if(!updatePosition(position))
		// no handle, aborting
		return false;

	std::lock_guard<ILockable> lock(*m_entry);

	double seek_frame = (position - m_entry->m_begin) * m_entry->m_sequence_data->getFPS();

	if(seek_frame < 0)
		seek_frame = 0;

	seek_frame += m_entry->m_skip * m_entry->m_sequence_data->getFPS();

	AnimateableProperty* pitch_property = m_entry->getAnimProperty(AP_PITCH);

	double target_frame = 0;

	if(pitch_property != nullptr)
	{
		int frame_start = (m_entry->m_begin - m_entry->m_skip) * m_entry->m_sequence_data->getFPS();

		for(int i = 0; seek_frame > 0; i++)
		{
			float pitch;
			pitch_property->read(frame_start + i, &pitch);
			const double factor = seek_frame > 1.0 ? 1.0 : seek_frame;
			target_frame += pitch * factor;
			seek_frame--;
		}
	}
	else
	{
		target_frame = seek_frame;
	}

	double seekpos = target_frame / m_entry->m_sequence_data->getFPS();

	m_handle->setPitch(1.0f);
	m_handle->seek(seekpos);

	return true;
}

AUD_NAMESPACE_END
