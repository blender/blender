/*
 * SND_IdObject.cpp
 *
 * Object for storing runtime data, like id's, soundobjects etc
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

#include "SND_IdObject.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

SND_IdObject::SND_IdObject()
{
}



SND_IdObject::~SND_IdObject()
{
}



SND_SoundObject* SND_IdObject::GetSoundObject()
{
	return m_soundObject;
}



void SND_IdObject::SetSoundObject(SND_SoundObject* pObject)
{
	m_soundObject = pObject;
}



int SND_IdObject::GetId()
{
	return m_id;
}



void SND_IdObject::SetId(int id)
{
	m_id = id;
}
