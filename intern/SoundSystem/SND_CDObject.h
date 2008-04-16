/*
 * SND_CDObject.h
 *
 * Implementation for CD playback
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

#ifndef __SND_CDOBJECT_H
#define __SND_CDOBJECT_H

#include "SND_Object.h"

class SND_CDObject : public SND_Object
{
private:

	/**
	 * Private to enforce singleton
	 */
	SND_CDObject();
	SND_CDObject(const SND_CDObject&);

	static SND_CDObject*	m_instance;
	MT_Scalar				m_gain;			/* the gain of the object */
	int						m_playmode;		/* the way CD is played back (all, random, track, trackloop) */
	int						m_track;		/* the track for 'track' and 'trackloop' */
	int						m_playstate;	/* flag for current state of object */
	bool					m_modified;
	bool					m_used;			/* flag for checking if we used the cd, if not don't 
												call the stop cd at the end */

public:
	static bool	CreateSystem();
	static bool DisposeSystem();
	static SND_CDObject* Instance();

	~SND_CDObject();
	
	void SetGain(MT_Scalar gain);
	void SetPlaymode(int playmode);
	void SetTrack(int track);
	void SetPlaystate(int playstate);
	void SetModified(bool modified);
	void SetUsed();
	bool GetUsed();

	bool IsModified() const;

	int			GetTrack() const;
	MT_Scalar	GetGain() const;
	int			GetPlaymode() const;
	int			GetPlaystate() const;
	
};

#endif //__SND_CDOBJECT_H

