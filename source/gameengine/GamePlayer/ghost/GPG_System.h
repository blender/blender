/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file GPG_System.h
 *  \ingroup player
 *  \brief Blender Player system on GHOST.
 */

#ifndef __GPG_SYSTEM_H__
#define __GPG_SYSTEM_H__

#ifdef WIN32
#pragma warning (disable:4786) // suppress stl-MSVC debug info warning
#endif  /* WIN32 */

#include "KX_ISystem.h"

class GHOST_ISystem;

class GPG_System : public KX_ISystem
{
	GHOST_ISystem* m_system;

public:
	GPG_System(GHOST_ISystem* system);

	virtual double GetTimeInSeconds();
};

#endif  /* __GPG_SYSTEM_H__ */
