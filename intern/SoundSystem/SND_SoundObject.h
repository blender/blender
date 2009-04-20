/*
 * SND_SoundObject.h
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

#ifndef __SND_SOUNDOBJECT_H
#define __SND_SOUNDOBJECT_H

#include "SND_Object.h"
#include "STR_String.h"

/**
 *  SND_SoundObject is a class for api independent sounddata storage conected to an actuator
 */

class SND_SoundObject : public SND_Object
{
private:
	STR_String			m_samplename;		/* name of the sample */
	STR_String			m_objectname;		/* name of the object */
	unsigned int		m_buffer;
	
	bool				m_active;			/* is the object active or not? */
	int					m_id;
	MT_Scalar			m_lifespan;			/* the lifespan of the sound seconds */
	MT_Scalar			m_timestamp;

	MT_Scalar			m_length;			/* length of the sample in seconds */

	MT_Scalar			m_gain;				/* the gain of the object */
	MT_Scalar			m_rollofffactor;	/* the scaling factor to increase or decrease the range
												of a source by decreasing or increasing the
												attenuation, respectively */
	MT_Scalar			m_referencedistance;/* the distance at which the listener will experience
												gain */
	MT_Scalar			m_mingain;			/* indicates the minimal gain which is always guaranteed
												for this source */
	MT_Scalar			m_maxgain;			/* indicates the maximal gain which is always guaranteed
												for this source */

	MT_Scalar			m_pitch;			/* the pitch of the object */
	MT_Vector3			m_position;			/* position; left/right, up/down, in/out */
	MT_Vector3			m_velocity;			/* velocity of the object */
	MT_Matrix3x3		m_orientation;		/* orientation of the object */
	unsigned int		m_loopmode;			/* loop normal or bidirectional? */
	unsigned int		m_loopstart;		/* start of looppoint in samples! */
	unsigned int		m_loopend;			/* end of looppoint in samples! */
	bool				m_is3d;				/* is the object 3D or 2D? */
	int					m_playstate;		/* flag for current state of object */
	bool				m_modified;
	unsigned int		m_running;
	bool				m_highpriority;		/* may the sound be ditched when we run out of voices? */

public:

	SND_SoundObject();
	~SND_SoundObject();

	void SetBuffer(unsigned int buffer);
	void SetActive(bool active);
	
	void StartSound();
	void StopSound();
	void PauseSound();
	void DeleteWhenFinished();

	void SetObjectName(STR_String objectname);
	void SetSampleName(STR_String samplename);
	void SetLength(MT_Scalar length);
	
	void SetPitch(MT_Scalar pitch);
	void SetGain(MT_Scalar gain);
	void SetMinGain(MT_Scalar mingain);
	void SetMaxGain(MT_Scalar maxgain);
	void SetRollOffFactor(MT_Scalar rollofffactor);
	void SetReferenceDistance(MT_Scalar distance);
	void SetPosition(const MT_Vector3& pos);
	void SetVelocity(const MT_Vector3& vel);
	void SetOrientation(const MT_Matrix3x3& orient);
	void SetLoopMode(unsigned int loopmode);
	void SetLoopStart(unsigned int loopstart);
	void SetLoopEnd(unsigned int loopend);
	void Set3D(bool threedee);
	void SetPlaystate(int playstate);
	void SetHighPriority(bool priority);

	void SetId(int id);
	void SetLifeSpan();
	void SetTimeStamp(MT_Scalar timestamp);

	void SetModified(bool modified);

	bool IsLifeSpanOver(MT_Scalar curtime) const;
	bool IsActive() const;
	bool IsModified() const;
	bool IsHighPriority() const;

	void InitRunning();
	bool IsRunning() const;
	void AddRunning();

	int					GetId() const;
	MT_Scalar			GetLifeSpan() const;
	MT_Scalar			GetTimestamp() const;

	unsigned int		GetBuffer();
	const STR_String&	GetSampleName();
	const STR_String&	GetObjectName();

	MT_Scalar			GetLength() const;
	MT_Scalar			GetGain() const;
	MT_Scalar			GetPitch() const;

	MT_Scalar			GetMinGain() const;
	MT_Scalar			GetMaxGain() const;
	MT_Scalar			GetRollOffFactor() const;
	MT_Scalar			GetReferenceDistance() const;
	
	MT_Vector3			GetPosition() const;
	MT_Vector3			GetVelocity() const;
	MT_Matrix3x3		GetOrientation() const;
	unsigned int		GetLoopMode() const;
	unsigned int		GetLoopStart() const;
	unsigned int		GetLoopEnd() const;
	bool				Is3D() const;
	int					GetPlaystate() const;
	
};

#endif //__SND_SOUNDOBJECT_H

