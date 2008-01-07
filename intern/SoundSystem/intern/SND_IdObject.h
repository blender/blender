/*
 * SND_IdObject.h
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

#ifndef __SND_IDOBJECT_H
#define __SND_IDOBJECT_H

#include "SND_SoundObject.h"
#include "GEN_List.h"
#include "SoundDefines.h"

class SND_IdObject : public GEN_Link
{
	SND_SoundObject*	m_soundObject;
	int					m_id;

public:
	SND_IdObject();
	virtual ~SND_IdObject();

	SND_SoundObject*	GetSoundObject();
	void				SetSoundObject(SND_SoundObject* pObject);

	int					GetId();
	void				SetId(int id);
};

#endif //__SND_OBJECT_H

