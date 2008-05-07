/*
 * SND_CDObject.cpp
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

#include "SND_CDObject.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

SND_CDObject* SND_CDObject::m_instance = NULL;

bool SND_CDObject::CreateSystem()
{
	bool result = false;

	if (!m_instance)
	{
		m_instance = new SND_CDObject();
		result = true;
	}

	return result;
}



bool SND_CDObject::DisposeSystem()
{
	bool result = false;

	if (m_instance)
	{
		delete m_instance;
		m_instance = NULL;
		result = true;
	}

	return result;
}



SND_CDObject* SND_CDObject::Instance()
{
	return m_instance;
}



SND_CDObject::SND_CDObject()
{
	m_gain = 1;
	m_playmode = SND_CD_ALL;
	m_track = 1;
	m_playstate = SND_STOPPED;
	m_used = false;

	// don't set the cd standard on modified:
	// if not used, we don't wanna touch it (performance)
	m_modified = false;
}



SND_CDObject::~SND_CDObject()
{
}



void SND_CDObject::SetGain(MT_Scalar gain)
{
	m_gain = gain;
	m_modified = true;
}



void SND_CDObject::SetPlaymode(int playmode)
{
	m_playmode = playmode;
}



void SND_CDObject::SetPlaystate(int playstate)
{
	m_playstate = playstate;
}



void SND_CDObject::SetTrack(int track)
{
	m_track = track;
}



int SND_CDObject::GetTrack() const
{
	return m_track;
}



MT_Scalar SND_CDObject::GetGain() const
{
	return m_gain;
}


int SND_CDObject::GetPlaystate() const
{
	return m_playstate;
}



bool SND_CDObject::IsModified() const
{
	return m_modified;
}



void SND_CDObject::SetModified(bool modified)
{
	m_modified = modified;
}



int SND_CDObject::GetPlaymode() const
{
	return m_playmode;
}



void SND_CDObject::SetUsed()
{
	m_used = true;
}



bool SND_CDObject::GetUsed()
{
	return m_used;
}

