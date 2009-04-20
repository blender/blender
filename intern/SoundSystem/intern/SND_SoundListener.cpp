/*
 * SND_SoundListener.cpp
 *
 * A SoundListener is for sound what a camera is for vision.
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

#include "SND_SoundListener.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

SND_SoundListener::SND_SoundListener()
{
	m_modified = true;
	m_gain = 1.0;
	m_dopplerfactor = 1.0;
	m_dopplervelocity = 1.0;
	m_scale = 1.0;
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
}


SND_SoundListener::~SND_SoundListener()
{
	; /* intentionally empty */

}



void SND_SoundListener::SetGain(MT_Scalar gain)
{
	m_gain = gain;
	m_modified = true;
}



void SND_SoundListener::SetPosition (const MT_Vector3& pos)
{
	m_position = pos;
}



void SND_SoundListener::SetVelocity(const MT_Vector3& vel)
{
	m_velocity = vel;
}



void SND_SoundListener::SetOrientation(const MT_Matrix3x3& ori)
{
	m_orientation = ori;
}



void SND_SoundListener::SetDopplerFactor(MT_Scalar dopplerfactor)
{
	m_dopplerfactor = dopplerfactor;
	m_modified = true;
}



void SND_SoundListener::SetDopplerVelocity(MT_Scalar dopplervelocity)
{
	m_dopplervelocity = dopplervelocity;
	m_modified = true;
}



void SND_SoundListener::SetScale(MT_Scalar scale)
{
	m_scale = scale;
	m_modified = true;
}



MT_Scalar SND_SoundListener::GetGain() const
{
	return m_gain;
}



MT_Vector3 SND_SoundListener::GetPosition() const
{
	return m_position;
}



MT_Vector3 SND_SoundListener::GetVelocity() const
{
	return m_velocity;
}



MT_Matrix3x3 SND_SoundListener::GetOrientation()
{
	return m_orientation;
}



MT_Scalar SND_SoundListener::GetDopplerFactor() const
{
	return m_dopplerfactor;
}



MT_Scalar SND_SoundListener::GetDopplerVelocity() const
{
	return m_dopplervelocity;
}



MT_Scalar SND_SoundListener::GetScale() const
{
	return m_scale;
}



bool SND_SoundListener::IsModified() const
{
	return m_modified;	
}



void SND_SoundListener::SetModified(bool modified)
{
	m_modified = modified;
}
