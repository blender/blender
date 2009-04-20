/*
 * SND_Scene.h
 *
 * The scene for sounds.
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

#ifdef WIN32
#pragma warning (disable:4786) // get rid of stupid stl-visual compiler debug warning
#endif //WIN32

#ifndef __SND_SCENE_H
#define __SND_SCENE_H

#include "SoundDefines.h"
#include "SND_SoundObject.h"
#include "SND_CDObject.h"
#include "SND_SoundListener.h"
#include "SND_WaveSlot.h"

#include "MT_Vector3.h"
#include "MT_Matrix3x3.h"
#include "STR_String.h"

#include <set>


class SND_Scene
{
	std::set<class SND_SoundObject*>	m_soundobjects;

	GEN_List					m_activeobjects;
	class SND_IAudioDevice*		m_audiodevice;
	class SND_WaveCache*		m_wavecache;
	class SND_SoundListener		m_listener;
	bool						m_audio;			// to check if audio works
	bool						m_audioplayback;	// to check if audioplayback is wanted

	void					UpdateListener();
	void					BuildActiveList(MT_Scalar curtime);
	void					UpdateActiveObects();
	void					UpdateCD();

public:
	SND_Scene(SND_IAudioDevice* adi);
	~SND_Scene();

	bool				IsPlaybackWanted();

	void				AddActiveObject(SND_SoundObject* pObject, MT_Scalar curtime);
	void				RemoveActiveObject(SND_SoundObject* pObject);
	void				DeleteObjectWhenFinished(SND_SoundObject* pObject);

	void				Proceed();

	int					LoadSample(const STR_String& samplename,
								   void* memlocation,
								   int size);
	void				RemoveAllSamples();
	bool				CheckBuffer(SND_SoundObject* pObject);
	bool				IsSampleLoaded(STR_String& samplename);

	void				AddObject(SND_SoundObject* pObject);
	bool				SetCDObject(SND_CDObject* cdobject);
	void				DeleteObject(SND_SoundObject* pObject);
	void				RemoveAllObjects();
	void				StopAllObjects();
	int					GetObjectStatus(SND_SoundObject* pObject) const;

	void				SetListenerTransform(const MT_Vector3& pos,
											 const MT_Vector3& vel,
											 const MT_Matrix3x3& mat);

	SND_SoundListener*	GetListener();
};

#endif //__SND_SCENE_H

