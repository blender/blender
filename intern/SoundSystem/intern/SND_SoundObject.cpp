/*
 * SND_SoundObject.cpp
 *
 * Implementation of the abstract sound object
 *
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "SND_SoundObject.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

SND_SoundObject::SND_SoundObject()// : m_modified(true)
{
	m_samplename = "";
	m_length = 0;
	m_buffer = 0;

	m_gain = 0.0;
	m_pitch = 1.0;

	m_mingain = 0.0;
	m_maxgain = 1.0;
	m_rollofffactor = 1.0;
	m_referencedistance = 1.0;
	
	m_position[0] = 0.0;
	m_position[1] = 0.0;
	m_position[2] = 0.0;
	m_velocity[0] = 0.0;
	m_velocity[1] = 0.0;
	m_velocity[2] = 0.0;
	m_orientation[0][0] = 1.0;
	m_orientation[0][1] = 0.0;
	m_orientation[0][2] = 0.0;
	m_orientation[1][0] = 0.0;
	m_orientation[1][1] = 1.0;
	m_orientation[1][2] = 0.0;
	m_orientation[2][0] = 0.0;
	m_orientation[2][1] = 0.0;
	m_orientation[2][2] = 1.0;
	
	m_loopstart = 0;
	m_loopend = 0;
	m_loopmode = SND_LOOP_NORMAL;
	m_is3d = true;
	m_playstate = SND_INITIAL;
	m_active = false;
	m_id = -1;
	m_lifespan = 0;
	m_timestamp = 0;
	m_modified = true;
	m_running = 0;
	m_highpriority = false;
}



SND_SoundObject::~SND_SoundObject()
{
}



void SND_SoundObject::StartSound()
{
	m_playstate = SND_MUST_PLAY;
}



void SND_SoundObject::StopSound()
{
	m_playstate = SND_MUST_STOP;
}



void SND_SoundObject::PauseSound()
{
	m_playstate = SND_MUST_PAUSE;
}



void SND_SoundObject::DeleteWhenFinished()
{
	m_playstate = SND_MUST_BE_DELETED;
}



void SND_SoundObject::SetGain(MT_Scalar gain)
{
	m_gain = gain;
	m_modified = true;
}



void SND_SoundObject::SetMinGain(MT_Scalar mingain)
{
	m_mingain = mingain;
	m_modified = true;
}



void SND_SoundObject::SetMaxGain(MT_Scalar maxgain)
{
	m_maxgain = maxgain;
	m_modified = true;
}



void SND_SoundObject::SetRollOffFactor(MT_Scalar rollofffactor)
{
	m_rollofffactor = rollofffactor;
	m_modified = true;
}



void SND_SoundObject::SetReferenceDistance(MT_Scalar referencedistance)
{
	m_referencedistance = referencedistance;
	m_modified = true;
}



void SND_SoundObject::SetPitch(MT_Scalar pitch)
{
	m_pitch = pitch;
	m_modified = true;
}



void SND_SoundObject::SetLoopMode(unsigned int loopmode)
{
	m_loopmode = loopmode;
	m_modified = true;
}



void SND_SoundObject::SetLoopStart(unsigned int loopstart)
{
	m_loopstart = loopstart;
	m_modified = true;
}



void SND_SoundObject::SetLoopEnd(unsigned int loopend)
{
	m_loopend = loopend;
	m_modified = true;
}



void SND_SoundObject::Set3D(bool threedee)
{
	m_is3d = threedee;
}



void SND_SoundObject::SetLifeSpan()
{
	m_lifespan = m_length / m_pitch;
}



bool SND_SoundObject::IsLifeSpanOver(MT_Scalar curtime) const
{
	bool result = false;

	if ((curtime - m_timestamp) > m_lifespan)
		result = true;

	return result;
}



void SND_SoundObject::SetActive(bool active)
{
	m_active = active;

	if (!active)
	{
		m_playstate = SND_STOPPED;
		(this)->remove();
	}
}



void SND_SoundObject::SetBuffer(unsigned int buffer)
{
	m_buffer = buffer;
}



void SND_SoundObject::SetObjectName(STR_String objectname)
{
	m_objectname = objectname;
}



void SND_SoundObject::SetSampleName(STR_String samplename)
{
	m_samplename = samplename;
}



void SND_SoundObject::SetLength(MT_Scalar length)
{
	m_length = length;
}



void SND_SoundObject::SetPosition(const MT_Vector3& pos)
{
	m_position = pos;
}



void SND_SoundObject::SetVelocity(const MT_Vector3& vel)
{
	m_velocity = vel;
}



void SND_SoundObject::SetOrientation(const MT_Matrix3x3& orient)
{
	m_orientation = orient;
}



void SND_SoundObject::SetPlaystate(int playstate)
{
	m_playstate = playstate;
}



void SND_SoundObject::SetId(int id)
{
	m_id = id;
}



void SND_SoundObject::SetTimeStamp(MT_Scalar timestamp)
{
	m_timestamp = timestamp;
}



void SND_SoundObject::SetHighPriority(bool priority)
{
	m_highpriority = priority;
}



bool SND_SoundObject::IsHighPriority() const
{
	return m_highpriority;
}



bool SND_SoundObject::IsActive()const
{
	return m_active;
}



int SND_SoundObject::GetId()const
{
	return m_id;
}



MT_Scalar SND_SoundObject::GetLifeSpan()const
{
	return m_lifespan;
}



MT_Scalar SND_SoundObject::GetTimestamp()const
{
	return m_timestamp;
}



unsigned int SND_SoundObject::GetBuffer()
{
	return m_buffer;
}



const STR_String& SND_SoundObject::GetSampleName()
{
	return m_samplename; 
}



const STR_String& SND_SoundObject::GetObjectName()
{
	return m_objectname;
}



MT_Scalar SND_SoundObject::GetLength() const
{
	return m_length;
}



MT_Scalar SND_SoundObject::GetGain() const
{
	return m_gain;
}



MT_Scalar SND_SoundObject::GetPitch() const
{
	return m_pitch;
}



MT_Scalar SND_SoundObject::GetMinGain() const
{
	return m_mingain;
}



MT_Scalar SND_SoundObject::GetMaxGain() const
{
	return m_maxgain;
}



MT_Scalar SND_SoundObject::GetRollOffFactor() const
{
	return m_rollofffactor;
}



MT_Scalar SND_SoundObject::GetReferenceDistance() const
{
	return m_referencedistance;
}



MT_Vector3 SND_SoundObject::GetPosition() const
{
	return m_position;
}



MT_Vector3 SND_SoundObject::GetVelocity() const
{
	return m_velocity;
}



MT_Matrix3x3 SND_SoundObject::GetOrientation() const
{
	return m_orientation;
}



unsigned int SND_SoundObject::GetLoopMode() const
{
	return m_loopmode;
}



unsigned int SND_SoundObject::GetLoopStart() const
{
	return m_loopstart;
}



unsigned int SND_SoundObject::GetLoopEnd() const
{
	return m_loopend;
}



bool SND_SoundObject::Is3D() const
{
	return m_is3d;
}



int SND_SoundObject::GetPlaystate() const
{
	return m_playstate;
}



bool SND_SoundObject::IsModified() const
{
	return m_modified;
}



void SND_SoundObject::SetModified(bool modified)
{
	m_modified = modified;
}



void SND_SoundObject::InitRunning()
{
	m_running = 0;
}



bool SND_SoundObject::IsRunning() const
{
	bool result = false;

	if (m_running > 100)
		result = true;

	return result;
}



void SND_SoundObject::AddRunning()
{
	++m_running;
}
