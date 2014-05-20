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

AUD_SequencerHandle::AUD_SequencerHandle(boost::shared_ptr<AUD_SequencerEntry> entry, AUD_ReadDevice& device) :
	m_entry(entry),
	m_status(0),
	m_pos_status(0),
	m_sound_status(0),
	m_device(device)
{
	if(entry->m_sound.get())
	{
		m_handle = device.play(entry->m_sound, true);
		m_3dhandle = boost::dynamic_pointer_cast<AUD_I3DHandle>(m_handle);
	}
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
}

void AUD_SequencerHandle::update(float position, float frame, float fps)
{
	if(m_handle.get())
	{
		AUD_MutexLock lock(*m_entry);
		if(position >= m_entry->m_end)
			m_handle->pause();
		else if(position >= m_entry->m_begin)
			m_handle->resume();

		if(m_sound_status != m_entry->m_sound_status)
		{
			if(m_handle.get())
				m_handle->stop();

			if(m_entry->m_sound.get())
			{
				m_handle = m_device.play(m_entry->m_sound, true);
				m_3dhandle = boost::dynamic_pointer_cast<AUD_I3DHandle>(m_handle);
			}

			m_sound_status = m_entry->m_sound_status;
			m_pos_status--;
			m_status--;
		}

		if(m_pos_status != m_entry->m_pos_status)
		{
			seek(position);

			m_pos_status = m_entry->m_pos_status;
		}

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
}

void AUD_SequencerHandle::seek(float position)
{
	if(m_handle.get())
	{
		AUD_MutexLock lock(*m_entry);
		if(position >= m_entry->m_end)
		{
			m_handle->pause();
			return;
		}

		float seekpos = position - m_entry->m_begin;
		if(seekpos < 0)
			seekpos = 0;
		seekpos += m_entry->m_skip;
		m_handle->setPitch(1.0f);
		m_handle->seek(seekpos);
		if(position < m_entry->m_begin)
			m_handle->pause();
		else
			m_handle->resume();
	}
}
