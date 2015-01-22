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

/** \file audaspace/intern/AUD_SequencerHandle.cpp
 *  \ingroup audaspaceintern
 */


#include "AUD_SequencerHandle.h"
#include "AUD_ReadDevice.h"
#include "AUD_MutexLock.h"

#define KEEP_TIME 10

void AUD_SequencerHandle::start()
{
	// we already tried to start, aborting
	if(!m_valid)
		return;

	// in case the sound is playing, we need to stop first
	stop();

	AUD_MutexLock lock(*m_entry);

	// let's try playing
	if(m_entry->m_sound.get())
	{
		try
		{
			m_handle = m_device.play(m_entry->m_sound, true);
			m_3dhandle = boost::dynamic_pointer_cast<AUD_I3DHandle>(m_handle);
		}
		catch(AUD_Exception&)
		{
			// handle stays invalid in case we get an exception
		}

		// after starting we have to set the properties, so let's ensure that
		m_status--;
	}

	// if the sound could not be played, we invalidate
	m_valid = m_handle.get();
}

bool AUD_SequencerHandle::updatePosition(float position)
{
	AUD_MutexLock lock(*m_entry);

	if(m_handle.get())
	{
		// we currently have a handle, let's check where we are
		if(position >= m_entry->m_end)
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
		else if(position >= m_entry->m_begin)
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
		if(position >= m_entry->m_begin && position <= m_entry->m_end)
		{
			start();
			return m_valid;
		}
	}

	return false;
}

AUD_SequencerHandle::AUD_SequencerHandle(boost::shared_ptr<AUD_SequencerEntry> entry, AUD_ReadDevice& device) :
	m_entry(entry),
	m_valid(true),
	m_status(0),
	m_pos_status(0),
	m_sound_status(0),
	m_device(device)
{
}

AUD_SequencerHandle::~AUD_SequencerHandle()
{
	stop();
}

int AUD_SequencerHandle::compare(boost::shared_ptr<AUD_SequencerEntry> entry) const
{
	if(m_entry->getID() < entry->getID())
		return -1;
	else if(m_entry->getID() == entry->getID())
		return 0;
	return 1;
}

void AUD_SequencerHandle::stop()
{
	if(m_handle.get())
		m_handle->stop();
	m_handle = boost::shared_ptr<AUD_IHandle>();
	m_3dhandle = boost::shared_ptr<AUD_I3DHandle>();
}

void AUD_SequencerHandle::update(float position, float frame, float fps)
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

	AUD_MutexLock lock(*m_entry);
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
		m_status = m_entry->m_status;

		m_3dhandle->setRelative(m_entry->m_relative);
		m_3dhandle->setVolumeMaximum(m_entry->m_volume_max);
		m_3dhandle->setVolumeMinimum(m_entry->m_volume_min);
		m_3dhandle->setDistanceMaximum(m_entry->m_distance_max);
		m_3dhandle->setDistanceReference(m_entry->m_distance_reference);
		m_3dhandle->setAttenuation(m_entry->m_attenuation);
		m_3dhandle->setConeAngleOuter(m_entry->m_cone_angle_outer);
		m_3dhandle->setConeAngleInner(m_entry->m_cone_angle_inner);
		m_3dhandle->setConeVolumeOuter(m_entry->m_cone_volume_outer);
	}

	float value;

	m_entry->m_volume.read(frame, &value);
	m_handle->setVolume(value);
	m_entry->m_pitch.read(frame, &value);
	m_handle->setPitch(value);
	m_entry->m_panning.read(frame, &value);
	AUD_SoftwareDevice::setPanning(m_handle.get(), value);

	AUD_Vector3 v, v2;
	AUD_Quaternion q;

	m_entry->m_orientation.read(frame, q.get());
	m_3dhandle->setSourceOrientation(q);
	m_entry->m_location.read(frame, v.get());
	m_3dhandle->setSourceLocation(v);
	m_entry->m_location.read(frame + 1, v2.get());
	v2 -= v;
	m_3dhandle->setSourceVelocity(v2 * fps);

	if(m_entry->m_muted)
		m_handle->setVolume(0);
}

bool AUD_SequencerHandle::seek(float position)
{
	if(!m_valid)
		// sound not valid, aborting
		return false;

	if(!updatePosition(position))
		// no handle, aborting
		return false;

	AUD_MutexLock lock(*m_entry);
	float seekpos = position - m_entry->m_begin;
	if(seekpos < 0)
		seekpos = 0;
	seekpos += m_entry->m_skip;
	m_handle->setPitch(1.0f);
	m_handle->seek(seekpos);

	return true;
}
