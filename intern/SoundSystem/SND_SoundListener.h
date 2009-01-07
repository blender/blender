/*
 * SND_SoundListener.h
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

#ifndef __SND_SOUNDLISTENER_H
#define __SND_SOUNDLISTENER_H

#include "SND_Object.h"

class SND_SoundListener : public SND_Object
{
public:
	SND_SoundListener();
	virtual ~SND_SoundListener();

	void SetStateFlag(unsigned int stateflags);
	void SetGain(MT_Scalar gain);
	void SetPosition(const MT_Vector3& pos);
	void SetVelocity(const MT_Vector3& vel);
	void SetOrientation(const MT_Matrix3x3& ori);
	void SetDopplerFactor(MT_Scalar dopplerfactor);
	void SetDopplerVelocity(MT_Scalar dopplervelocity);
	void SetScale(MT_Scalar scale);

	void SetModified(bool modified);
	bool IsModified() const;

	unsigned int	GetStateFlags() const;
	MT_Scalar		GetGain() const;
	MT_Vector3		GetPosition() const;
	MT_Vector3		GetVelocity() const;
	MT_Matrix3x3	GetOrientation();

	MT_Scalar		GetDopplerFactor() const;
	MT_Scalar		GetDopplerVelocity() const;
	MT_Scalar		GetScale() const;
	
private:
	void*			m_listener;
	bool			m_modified;

	MT_Scalar		m_gain;				/* overall gain */
	MT_Vector3		m_position;			/* position; left/right, up/down, in/out */
	MT_Vector3		m_velocity;			/* velocity of the listener */
	MT_Matrix3x3	m_orientation;		/* orientation of the listener */

	MT_Scalar		m_dopplerfactor;	/* scaling factor for the Doppler (pitch) shift */
	MT_Scalar		m_dopplervelocity;	/* factor for the reference velocity (for Dopplereffect) */
	MT_Scalar		m_scale;
};

#endif //__SND_SOUNDLISTENER_H

