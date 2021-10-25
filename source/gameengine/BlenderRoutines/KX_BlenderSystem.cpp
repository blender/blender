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

/** \file gameengine/BlenderRoutines/KX_BlenderSystem.cpp
 *  \ingroup blroutines
 */


#include "KX_ISystem.h"

#ifdef _MSC_VER
#  pragma warning (disable:4786)
#endif

#ifdef WIN32
#include <windows.h>
#endif

#include <iostream>
#include <stdio.h>
#include "KX_BlenderInputDevice.h"
#include "KX_BlenderSystem.h"

#include "PIL_time.h"

KX_BlenderSystem::KX_BlenderSystem()
: KX_ISystem()
{
	m_starttime = PIL_check_seconds_timer();
}

double KX_BlenderSystem::GetTimeInSeconds()
{
	return PIL_check_seconds_timer() - m_starttime;
}
